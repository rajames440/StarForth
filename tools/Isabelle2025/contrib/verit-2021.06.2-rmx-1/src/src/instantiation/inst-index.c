/* Debugging
   0: no debugging
   1: prints size of indexes
   2: prints indexes
   3: prints signature table */
#define DEBUG_INDEX 0
#define DEBUG_DELETION 0

#include "instantiation/inst-index.h"

#include "congruence/congruence.h"
#include "instantiation/free-vars.h"
#include "instantiation/inst-del.h"
#include "instantiation/inst-man.h"
#include "limits.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-stat.h"
#include "symbolic/DAG.h"
#include "symbolic/recursion.h"
#include "utils/options.h"
#include "utils/statistics.h"

/*
  --------------------------------------------------------------
  Options
  --------------------------------------------------------------
*/

/**
   \addtogroup arguments_user
   - --index-fresh-SAT

   Index ground terms in quantified formulas in SAT stack */
static bool index_fresh_SAT;

/**
   \addtogroup arguments_user
   - --index-fresh-sorts

   Index ground terms in quantified formulas in SAT stack for enum. inst */
static bool index_fresh_sorts;

/*
  --------------------------------------------------------------
  Stats
  --------------------------------------------------------------
*/

/** \brief time spent computing indexes */
static unsigned index_stats_time;
/** \brief the deepest level of instantiations */
static unsigned index_stats_deepest_lvl;
/** \brief how many literals deleted from index (cummulative) */
static unsigned index_stats_deleted;
/** \brief how many literals removed from prime implicant  (cummulative) */
static unsigned index_stats_prime;
/** \brief how many literals removed from input pruning  (cummulative) */
static unsigned index_stats_prune_cnf;

/** \brief Biggest arity of a function symbol occurring in the index CC */
static unsigned index_stats_arity_max = 0;

/*
  --------------------------------------------------------------
  "Deleting" literals from index consideration
  --------------------------------------------------------------
*/

/**
   All literals should be associated with a value indicating the instantiation
   round in which it was produced. 0 denotes either a literal from the original
   formula or one from an instance which participated in a conflict and was
   "promoted".

   Before building the index the instantiation module checks the activity of the
   clauses from instantiations, promoting all literals from the "active" ones
   (i.e., were part of a conflict)

   TODO: Guarantee that activity really indicates participating in a conflict,
   not just propagation etc. */

/* TODO: use less than an unsigned?? */
/** \brief keeps the level of a literal for the instantiation module */
typedef struct Tinst_lvl
{
	unsigned lvl : 31; /*< instantiation level; default is 0 */
	bool promoted : 1; /*< whether was promoted to level 0; default is 0 */
} Tinst_lvl;

Tinst_lvl* inst_var_lvl;

/** \brief marks the last instantiation round lvl (default is 0) */
unsigned last_lvl;
/**
   \brief marks the deepest instantiation round which occurred
   TODO: Would this be an issue with true restarts of the solver? */
unsigned deepest_lvl = 0;

unsigned
get_var_lvl(Tvar var)
{
	return inst_var_lvl[var].lvl;
}

/* TODO: workaround while this is done *after* the SAT solver has
   backtracked */
void
set_var_lvl_arg(Tvar var, unsigned lvl)
{
	assert(lvl >= 0);
	/* TODO: Do I really need this promoted thing? */
	if (inst_var_lvl[var].promoted) return;
	inst_var_lvl[var].lvl =
		inst_var_lvl[var].lvl ? MIN(inst_var_lvl[var].lvl, lvl) : lvl;
}

void
set_var_lvl(Tvar var)
{
	assert(last_lvl >= 0);
	inst_var_lvl[var].lvl = last_lvl;
}

void
promote_var_lvl(Tvar var)
{
	/* TODO: Avoid calling it on literals already in root */
	if (inst_var_lvl[var].promoted) return;
#if DEBUG_DELETION
	my_message(
		"promote: promoting var %d from lvl %d to root\n", var,
		inst_var_lvl[var].lvl);
#endif
	inst_var_lvl[var].lvl = 0;
	inst_var_lvl[var].promoted = true;
}

unsigned
get_last_lvl(void)
{
	return last_lvl;
}

unsigned
get_deepest_lvl(void)
{
	return deepest_lvl;
}

bool
update_lvl_next(void)
{
	if (last_lvl + 1 > deepest_lvl) return false;
#if DEBUG_DELETION
	my_message("inst_lvl_next: From level %d to %d\n", last_lvl, last_lvl + 1);
#endif
	SAT_set_markup();
	++last_lvl;
	return true;
}

void
update_lvl_up(void)
{
#if DEBUG_DELETION
	my_message("inst_lvl_up: From level %d to %d\n", last_lvl, last_lvl + 1);
#endif
	++last_lvl;
	assert(last_lvl > 0);
	if (last_lvl > deepest_lvl) deepest_lvl = last_lvl;
	stats_counter_set(index_stats_deepest_lvl, deepest_lvl);
}

/**
   \brief decrements last instantiation
   \remark invoked by SAT solver when backtracks beyond a marked point */
void
update_lvl_down(void)
{
#if DEBUG_DELETION
	my_message("inst_lvl_down: From level %d to %d\n\n", last_lvl, last_lvl - 1);
#endif
	--last_lvl;
	assert(last_lvl >= 0);
}

/**
   \brief sets the level of each new literal to 0
   \remark should be called *after* literal_DAG_hook_resize
   \remark the number of literals is the double the number of DAGs? */
static void
lit_lvl_DAG_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	MY_REALLOC(inst_var_lvl, new_alloc * sizeof(Tinst_lvl));
	/* HB necessary because DAG_done sends new_alloc with size 0 */
	/*
     DD the following assertion is not correct, because having
     DD new_alloc = 0 does not entail that MY_REALLOC sets
     DD inst_var_lvl to NULL:
     DD assert(!inst_var_lvl || (new_alloc > old_alloc));
     DD if (inst_var_lvl)
     DD   memset(inst_var_lvl + old_alloc, 0,
     DD          (new_alloc - old_alloc) * sizeof(Tinst_lvl));
     DD I rewrote the assert and the if condition to test
     DD the original conditions instead.
  */
	assert(new_alloc == 0 || (new_alloc > old_alloc));
	if (new_alloc > old_alloc)
		memset(
			inst_var_lvl + old_alloc, 0, (new_alloc - old_alloc) * sizeof(Tinst_lvl));
}

/*
  --------------------------------------------------------------
  Handling ground model indexes
  --------------------------------------------------------------
*/

/** \brief Auxiliar data structure for index computation */
typedef union Indexes
{
	Findex f_index;
	Pindex p_index;
} Indexes;

/** \brief Accumulator for indexes of all function and predicate symbols */
typedef struct SymbIndex
{
	Indexes** symb_indexes; /*< each indexed symbol is associated with
                                 respective function or predicate index */
	Tstack_DAG indexed_fsymbols; /*< which function symbols have been indexed */
	Tstack_DAG indexed_psymbols; /*< which predicate symbols have been indexed */
} SymbIndex;

SymbIndex index_model;

#if DEBUG_INDEX
/**
   \brief Debugging function to count size of Findexes, also marking the
   maximum. */
static unsigned
get_all_Findex_size(void)
{
	unsigned i, size = 0;
	Tsymb symbol;
	for (i = 0; i < stack_size(index_model.indexed_fsymbols); ++i) {
		symbol = stack_get(index_model.indexed_fsymbols, i);
		if (index_model.symb_indexes[symbol]->f_index.signatures)
			size += stack_size(index_model.symb_indexes[symbol]->f_index.signatures);
	}
	return size;
}

/**
   \brief Debugging function to count size of Pindexes, also marking
   the maximum. */
static unsigned
get_all_Pindex_size(void)
{
	unsigned i, size = 0;
	Tsymb symbol;
	for (i = 0; i < stack_size(index_model.indexed_psymbols); ++i) {
		symbol = stack_get(index_model.indexed_psymbols, i);
		if (index_model.symb_indexes[symbol]->p_index.signatures[0])
			size +=
				stack_size(index_model.symb_indexes[symbol]->p_index.signatures[0]);
		if (index_model.symb_indexes[symbol]->p_index.signatures[1])
			size +=
				stack_size(index_model.symb_indexes[symbol]->p_index.signatures[1]);
	}
	return size;
}

#endif

void
unset_model_index()
{
	Tsymb symbol;
	if (!index_model.symb_indexes) return;
	assert(index_model.indexed_fsymbols && index_model.indexed_psymbols);
	while (!stack_is_empty(index_model.indexed_psymbols)) {
		symbol = stack_pop(index_model.indexed_psymbols);
		if (index_model.symb_indexes[symbol]->p_index.signatures[0])
			stack_free(index_model.symb_indexes[symbol]->p_index.signatures[0]);
		if (index_model.symb_indexes[symbol]->p_index.signatures[1])
			stack_free(index_model.symb_indexes[symbol]->p_index.signatures[1]);
		free(index_model.symb_indexes[symbol]);
	}
	stack_free(index_model.indexed_psymbols);
	while (!stack_is_empty(index_model.indexed_fsymbols)) {
		symbol = stack_pop(index_model.indexed_fsymbols);
		if (index_model.symb_indexes[symbol]->f_index.signatures)
			stack_free(index_model.symb_indexes[symbol]->f_index.signatures);
		if (index_model.symb_indexes[symbol]->f_index.diseq_terms)
			stack_free(index_model.symb_indexes[symbol]->f_index.diseq_terms);
		free(index_model.symb_indexes[symbol]);
	}
	stack_free(index_model.indexed_fsymbols);
	free(index_model.symb_indexes);
	index_model.symb_indexes = NULL;
}

bool
get_Findex(Tsymb symbol, Findex* f_index)
{
	if (!index_model.symb_indexes[symbol]) return false;
	*f_index = index_model.symb_indexes[symbol]->f_index;
	return true;
}

bool
get_Pindex(Tsymb symbol, Pindex* p_index)
{
	if (!index_model.symb_indexes[symbol]) return false;
	*p_index = index_model.symb_indexes[symbol]->p_index;
	return true;
}

Tstack_DAG
find_class_terms(Tstack_DAG terms, TDAG DAG)
{
	Tstack_DAG result;
	unsigned i;
	int imid, imin = 0, imax = stack_size(terms) - 1;
	while (imin <= imax) {
		imid = imin + (imax - imin) / 2;
		if (CC_abstract(DAG) == CC_abstract(stack_get(terms, imid))) break;
		if (CC_abstract(DAG) < CC_abstract(stack_get(terms, imid))) imax = imid - 1;
		if (CC_abstract(DAG) > CC_abstract(stack_get(terms, imid))) imin = imid + 1;
	}
	if (imin > imax) return NULL;
	stack_INIT(result);
	for (i = imid; i < stack_size(terms) && congruent(stack_get(terms, i), DAG);
			 ++i)
		stack_push(result, stack_get(terms, i));
	if (imid > 0)
		for (i = imid - 1; congruent(stack_get(terms, i), DAG); --i) {
			stack_push(result, stack_get(terms, i));
			if (i == 0) break;
		}
	if (stack_is_empty(result)) stack_free(result);
	return result;
}

Tstack_DAG
find_class_terms_diseq(Findex f_index, TDAG DAG)
{
	unsigned i;
	TDAG diseq_DAG;
	Tstack_DAG tmp_stack, diseqs, result = NULL;
	diseqs = CC_diseqs(DAG);
	if (!diseqs) return result;
	stack_INIT(result);
	for (i = 0; i < stack_size(diseqs); ++i) {
		diseq_DAG = stack_get(diseqs, i);
		/* Symbol not in class */
		if (!class_has_symbol(diseq_DAG, f_index.symbol)) continue;
		/* Symbol may or may not be there. If it is then it may find terms */
		tmp_stack = find_class_terms(f_index.signatures, diseq_DAG);
		if (!tmp_stack) continue;
		assert(!stack_is_empty(tmp_stack));
		stack_merge(result, tmp_stack);
		stack_free(tmp_stack);
	}
	if (stack_is_empty(result)) stack_free(result);
	return result;
}

int
DAG_cmp_q_class(const TDAG *PDAG1, const TDAG *PDAG2)
{
 const TDAG CC1 = CC_abstract(*PDAG1);
 const TDAG CC2 = CC_abstract(*PDAG2);
 return (CC1 > CC2) - (CC1 < CC2);
}

/*
  --------------------------------------------------------------
  Indexing signature table
  --------------------------------------------------------------
*/

/**
   \brief indexes a signature from CC
   \param DAG the signature being indexed
   \param index accumulator of all symbol indexes
   \remark literals not activated in CC are ignored
   \remark the terms appearing in disequalities are used to set the association
   between congruence classes and disequalities. */
static void
set_SIG_index_aux(TDAG DAG)
{
	unsigned i;
	TDAG term;
	Findex f_index;
	Pindex p_index;
	Tboolean_value bvalue = CC_abstract_p(DAG);
	/* TODO: Make sure that this means this is an unactivated predicate
     (unintepreted or (dis)equality) */
	if (bvalue == 2 && DAG_sort(DAG) == SORT_BOOLEAN) return;
	/* Ignores equalities */
	if (DAG_symb(DAG) == PREDICATE_EQ && bvalue == 1) return;
	/* Disequality */
	if (DAG_symb(DAG) == PREDICATE_EQ) {
		CC_set_diseqs(DAG);
		/* Collect terms in disequality; useful for Ematching */
		for (i = 0; i < DAG_arity(DAG); ++i) {
			term = DAG_arg(DAG, i);
			f_index.symbol = DAG_symb(DAG_arg(DAG, i));
			f_index.signatures = NULL;
			f_index.diseq_terms = NULL;
			if (index_model.symb_indexes[DAG_symb(term)])
				f_index = index_model.symb_indexes[DAG_symb(term)]->f_index;
			else {
				MY_MALLOC(index_model.symb_indexes[DAG_symb(term)], sizeof(Indexes));
				stack_push(index_model.indexed_fsymbols, DAG_symb(term));
			}
			if (!f_index.diseq_terms) stack_INIT(f_index.diseq_terms);
			stack_push(f_index.diseq_terms, term);
			index_model.symb_indexes[DAG_symb(term)]->f_index = f_index;
		}
		return;
	}
	/* Predicate */
	if (DAG_sort(DAG) == SORT_BOOLEAN) {
		p_index.symbol = DAG_symb(DAG);
		p_index.signatures[0] = NULL;
		p_index.signatures[1] = NULL;
		if (index_model.symb_indexes[DAG_symb(DAG)])
			p_index = index_model.symb_indexes[DAG_symb(DAG)]->p_index;
		else {
			MY_MALLOC(index_model.symb_indexes[DAG_symb(DAG)], sizeof(Indexes));
			stack_push(index_model.indexed_psymbols, DAG_symb(DAG));
		}
		if (!p_index.signatures[bvalue]) stack_INIT(p_index.signatures[bvalue]);
		stack_push(p_index.signatures[bvalue], DAG);
		index_model.symb_indexes[DAG_symb(DAG)]->p_index = p_index;
		return;
	}
	/* term that appeared in an equality literal at some point. I guess;
     TODO: Does this not also get the terms in diseqs? */
	f_index.symbol = DAG_symb(DAG);
	CC_set_symbols(DAG);
	f_index.signatures = NULL;
	f_index.diseq_terms = NULL;
	if (index_model.symb_indexes[DAG_symb(DAG)])
		f_index = index_model.symb_indexes[DAG_symb(DAG)]->f_index;
	else {
		MY_MALLOC(index_model.symb_indexes[DAG_symb(DAG)], sizeof(Indexes));
		stack_push(index_model.indexed_fsymbols, DAG_symb(DAG));
	}
	if (!f_index.signatures) stack_INIT(f_index.signatures);
	stack_push(f_index.signatures, DAG);
	index_model.symb_indexes[DAG_symb(DAG)]->f_index = f_index;
}

void
set_SIG_index(void)
{
	unsigned i;
	Tsymb symbol;
	stats_timer_start(index_stats_time);
	assert(!index_model.symb_indexes);
	MY_MALLOC(
		index_model.symb_indexes, stack_size(DAG_symb_stack) * (sizeof(Indexes)));
	memset(
		index_model.symb_indexes, 0,
		stack_size(DAG_symb_stack) * (sizeof(Indexes)));
	stack_INIT(index_model.indexed_fsymbols);
	stack_INIT(index_model.indexed_psymbols);
	CC_sig_apply(set_SIG_index_aux);
	stack_sort(index_model.indexed_fsymbols, DAG_cmp_q);
	stack_uniq(index_model.indexed_fsymbols);
	stack_sort(index_model.indexed_psymbols, DAG_cmp_q);
	stack_uniq(index_model.indexed_psymbols);
	/* Sort f_indexes by congruence class */
	for (i = 0; i < stack_size(index_model.indexed_fsymbols); ++i) {
		symbol = stack_get(index_model.indexed_fsymbols, i);
		if (index_model.symb_indexes[symbol]->f_index.signatures)
			stack_sort(
				index_model.symb_indexes[symbol]->f_index.signatures, DAG_cmp_q_class);
		if (index_model.symb_indexes[symbol]->f_index.diseq_terms) {
			/* Remove duplicates */
			stack_sort(
				index_model.symb_indexes[symbol]->f_index.diseq_terms, DAG_cmp_q);
			stack_uniq(index_model.symb_indexes[symbol]->f_index.diseq_terms);
			stack_sort(
				index_model.symb_indexes[symbol]->f_index.diseq_terms, DAG_cmp_q_class);
		}
	}
	stats_timer_stop(index_stats_time);
#if DEBUG_INDEX
	my_message("F_Indexes size: %d\n", get_all_Findex_size());
	my_message("P_Indexes size: %d\n", get_all_Pindex_size());
#if DEBUG_INDEX > 1
	for (i = 0; i < stack_size(index_model.indexed_fsymbols); ++i)
		print_Findex(
			index_model.symb_indexes[stack_get(index_model.indexed_fsymbols, i)]
				->f_index);
	for (i = 0; i < stack_size(index_model.indexed_psymbols); ++i)
		print_Pindex(
			index_model.symb_indexes[stack_get(index_model.indexed_psymbols, i)]
				->p_index);
	for (int gambi = 50; gambi--; printf("-"))
		;
	printf("\n\n");
#endif
#endif
}

/*
  --------------------------------------------------------------
  Indexing SAT stack
  --------------------------------------------------------------
*/

/**
   \brief filters out terms with same signature, keeping a single one
   \param terms a set of terms
   \remark changes are made directly to the given set
   \remark assume terms are ordered by class */
static void
merge_signatures(Tstack_DAG* terms)
{
	unsigned i, j, k;
	TDAG term;
	Tstack_DAG tmp_terms;
	stack_COPY(tmp_terms, *terms);
	stack_reset(*terms);
	DAG_tmp_reserve();
	for (i = 0; i < stack_size(tmp_terms); ++i) {
		term = stack_get(tmp_terms, i);
		if (DAG_tmp_bool[term]) continue;
		stack_push(*terms, term);
		for (j = i + 1;
				 j < stack_size(tmp_terms) && congruent(term, stack_get(tmp_terms, j));
				 ++j) {
			for (k = 0; k < DAG_arity(term); ++k)
				if (!congruent(DAG_arg(term, k), DAG_arg(stack_get(tmp_terms, j), k)))
					break;
			/* Same signature */
			if (k == DAG_arity(term)) DAG_tmp_bool[stack_get(tmp_terms, j)] = 1;
		}
	}
	for (i = 0; i < stack_size(tmp_terms); ++i)
		DAG_tmp_bool[stack_get(tmp_terms, i)] = 0;
	DAG_tmp_release();
	stack_free(tmp_terms);
}

static void
index_term(TDAG DAG)
{
	unsigned i;
	Findex f_index;
	assert(DAG_sort(DAG) != SORT_BOOLEAN);
	f_index.symbol = DAG_symb(DAG);
	CC_set_symbols(DAG);
	f_index.signatures = NULL;
	f_index.diseq_terms = NULL;
	if (index_model.symb_indexes[DAG_symb(DAG)])
		f_index = index_model.symb_indexes[DAG_symb(DAG)]->f_index;
	else {
		MY_MALLOC(index_model.symb_indexes[DAG_symb(DAG)], sizeof(Indexes));
		stack_push(index_model.indexed_fsymbols, DAG_symb(DAG));
		if (DAG_arity(DAG) > stats_counter_get(index_stats_arity_max))
			stats_counter_set(index_stats_arity_max, DAG_arity(DAG));
	}
	if (!f_index.signatures) stack_INIT(f_index.signatures);
	stack_push(f_index.signatures, DAG);
	index_model.symb_indexes[DAG_symb(DAG)]->f_index = f_index;
	for (i = 0; i < DAG_arity(DAG); ++i)
		if (DAG_arity(DAG_arg(DAG, i)) && !DAG_tmp_bool[DAG_arg(DAG, i)]) {
			DAG_tmp_bool[DAG_arg(DAG, i)] = 1;
			index_term(DAG_arg(DAG, i));
		}
}

extern bool opt_bool_required_off;
extern bool prime_implicant_off;
extern bool prune_cnf_off;

void
set_SAT_index(unsigned delete_lvl)
{
	unsigned i, j;
	bool pol;
	TDAG DAG;
	Pindex p_index;
	Tsymb symbol;
	stats_timer_start(index_stats_time);
	MY_MALLOC(
		index_model.symb_indexes, stack_size(DAG_symb_stack) * (sizeof(Indexes)));
	memset(
		index_model.symb_indexes, 0,
		stack_size(DAG_symb_stack) * (sizeof(Indexes)));
	stack_INIT(index_model.indexed_fsymbols);
	stack_INIT(index_model.indexed_psymbols);
	DAG_tmp_reserve();
	for (i = 0; i < SAT_literal_stack_n; ++i) {
		DAG = lit_to_DAG(SAT_literal_stack[i]);
		pol = lit_pol(SAT_literal_stack[i]);
#ifndef POLARITY_FILTER
		if (
			DAG == DAG_NULL || DAG == DAG_TRUE || boolean_connector(DAG_symb(DAG)) ||
			!DAG_arity(DAG) || DAG_quant(DAG) || DAG_sort(DAG) != SORT_BOOLEAN)
			continue;
#else
		/* TODO: amend this */
		if (
			(!opt_bool_required_off && !bool_required[SAT_literal_stack[i]]) ||
			(DAG == DAG_NULL || DAG == DAG_TRUE || boolean_connector(DAG_symb(DAG)) ||
			 !DAG_arity(DAG) || DAG_quant(DAG) || DAG_sort(DAG) != SORT_BOOLEAN))
			continue;
#endif
		/* Disequalities should be set no matter the level, model etc; I think */
		if (!pol && DAG_symb(DAG) == PREDICATE_EQ) CC_set_diseqs(DAG);
		if (!prime_implicant_off && !prime_required[SAT_literal_stack[i]]) {
			stats_counter_inc(index_stats_prime);
			continue;
		}
		if (!prune_cnf_off && !original_required[SAT_literal_stack[i]]) {
			stats_counter_inc(index_stats_prune_cnf);
			continue;
		}
		/* Filter literals from instantiations downstream */
		if (delete_lvl < inst_var_lvl[lit_var(SAT_literal_stack[i])].lvl) {
#if DEBUG_DELETION
			my_DAG_message(
				"set_prime_index: deleting lit %d, {%d}%D\n", SAT_literal_stack[i], DAG,
				DAG);
#endif
			stats_counter_inc(index_stats_deleted);
			continue;
		}
		assert(!DAG_tmp_bool[DAG]);
		DAG_tmp_bool[DAG] = 1;
		/* Index all terms */
		for (j = 0; j < DAG_arity(DAG); ++j)
			if (DAG_arity(DAG_arg(DAG, j)) && !DAG_tmp_bool[DAG_arg(DAG, j)]) {
				DAG_tmp_bool[DAG_arg(DAG, j)] = 1;
				index_term(DAG_arg(DAG, j));
			}
		/* Equality literal */
		if (DAG_symb(DAG) == PREDICATE_EQ) {
			/* Disequality */
			if (!pol) CC_set_diseqs(DAG);
			continue;
		}
		/* Uninterpreted predicate */
		p_index.symbol = DAG_symb(DAG);
		p_index.signatures[0] = NULL;
		p_index.signatures[1] = NULL;
		if (index_model.symb_indexes[DAG_symb(DAG)])
			p_index = index_model.symb_indexes[DAG_symb(DAG)]->p_index;
		else {
			MY_MALLOC(index_model.symb_indexes[DAG_symb(DAG)], sizeof(Indexes));
			stack_push(index_model.indexed_psymbols, DAG_symb(DAG));
		}
		if (!p_index.signatures[pol]) stack_INIT(p_index.signatures[pol]);
		stack_push(p_index.signatures[pol], DAG);
		index_model.symb_indexes[DAG_symb(DAG)]->p_index = p_index;
	}
	for (i = 0; i < SAT_literal_stack_n; ++i)
		DAG_tmp_reset_bool(lit_to_DAG(SAT_literal_stack[i]));
	DAG_tmp_release();
	/* Delete repeated symbols */
	stack_sort(index_model.indexed_fsymbols, DAG_cmp_q);
	stack_uniq(index_model.indexed_fsymbols);
	stack_sort(index_model.indexed_psymbols, DAG_cmp_q);
	stack_uniq(index_model.indexed_psymbols);
	/* Sort by congruence class, remove duplicates, merge terms with same
     signature */
	for (i = 0; i < stack_size(index_model.indexed_fsymbols); ++i) {
		symbol = stack_get(index_model.indexed_fsymbols, i);
		if (index_model.symb_indexes[symbol]->f_index.signatures) {
			stack_sort(
				index_model.symb_indexes[symbol]->f_index.signatures, DAG_cmp_q);
			stack_uniq(index_model.symb_indexes[symbol]->f_index.signatures);
			stack_sort(
				index_model.symb_indexes[symbol]->f_index.signatures, DAG_cmp_q_class);
			merge_signatures(&index_model.symb_indexes[symbol]->f_index.signatures);
		}
		assert(!index_model.symb_indexes[symbol]->f_index.diseq_terms);
	}
	for (i = 0; i < stack_size(index_model.indexed_psymbols); ++i) {
		symbol = stack_get(index_model.indexed_psymbols, i);
		for (j = 0; j < 2; ++j)
			if (index_model.symb_indexes[symbol]->p_index.signatures[j]) {
				stack_sort(
					index_model.symb_indexes[symbol]->p_index.signatures[j], DAG_cmp_q);
				stack_uniq(index_model.symb_indexes[symbol]->p_index.signatures[j]);
				stack_sort(
					index_model.symb_indexes[symbol]->p_index.signatures[j],
					DAG_cmp_q_class);
				/* TODO: this should be improved by having an external CC that merges the
               signatures at the beginning of each instantiation cycle

               Another option is to have an incremental CC to which equalities
               (and disequalities?) are added according to the inst level of
               literals */
				merge_signatures(
					&index_model.symb_indexes[symbol]->p_index.signatures[j]);
			}
	}
#if DEBUG_INDEX
	my_message("F_Indexes size: %d\n", get_all_Findex_size());
	my_message("P_Indexes size: %d\n", get_all_Pindex_size());
#if DEBUG_INDEX > 1
	for (i = 0; i < stack_size(index_model.indexed_fsymbols); ++i)
		print_Findex(
			index_model.symb_indexes[stack_get(index_model.indexed_fsymbols, i)]
				->f_index);
	for (i = 0; i < stack_size(index_model.indexed_psymbols); ++i)
		print_Pindex(
			index_model.symb_indexes[stack_get(index_model.indexed_psymbols, i)]
				->p_index);
	for (int gambi = 50; gambi--; printf("-"))
		;
	printf("\n\n");
#endif
#endif
}

/*
  --------------------------------------------------------------
  Indexing SAT stack and associating terms to lits
  --------------------------------------------------------------
*/

/* TODO: This should not be a duplication, but the standard. Keeping like this
   for now for not introducing bugs in what concerns the competition */

extern Tstack_var* index_lits;

static inline void
free_index_lits(TDAG DAG)
{
	assert(index_lits[DAG]);
	stack_free(index_lits[DAG]);
}

/* as above, but also frees index_lits of indexed SIGs */
void
unset_model_lit_index(void)
{
	Tsymb symbol;
	if (!index_model.symb_indexes) return;
	assert(index_model.indexed_fsymbols && index_model.indexed_psymbols);
	while (!stack_is_empty(index_model.indexed_psymbols)) {
		symbol = stack_pop(index_model.indexed_psymbols);
		if (index_model.symb_indexes[symbol]->p_index.signatures[0]) {
			stack_apply(
				index_model.symb_indexes[symbol]->p_index.signatures[0],
				free_index_lits);
			stack_free(index_model.symb_indexes[symbol]->p_index.signatures[0]);
		}
		if (index_model.symb_indexes[symbol]->p_index.signatures[1]) {
			stack_apply(
				index_model.symb_indexes[symbol]->p_index.signatures[1],
				free_index_lits);
			stack_free(index_model.symb_indexes[symbol]->p_index.signatures[1]);
		}
		free(index_model.symb_indexes[symbol]);
	}
	stack_free(index_model.indexed_psymbols);
	while (!stack_is_empty(index_model.indexed_fsymbols)) {
		symbol = stack_pop(index_model.indexed_fsymbols);
		if (index_model.symb_indexes[symbol]->f_index.signatures) {
			stack_apply(
				index_model.symb_indexes[symbol]->f_index.signatures, free_index_lits);
			stack_free(index_model.symb_indexes[symbol]->f_index.signatures);
		}
		if (index_model.symb_indexes[symbol]->f_index.diseq_terms) {
			stack_apply(
				index_model.symb_indexes[symbol]->f_index.diseq_terms, free_index_lits);
			stack_free(index_model.symb_indexes[symbol]->f_index.diseq_terms);
		}
		free(index_model.symb_indexes[symbol]);
	}
	stack_free(index_model.indexed_fsymbols);
	free(index_model.symb_indexes);
	index_model.symb_indexes = NULL;
#ifdef DEBUG
	unsigned i;
	for (i = 0; i < stack_size(DAG_table); ++i) assert(!index_lits[i]);
#endif
}

/* As above, but frees index_lits of DAGs that were filtered out */
static void
merge_lit_signatures(Tstack_DAG* terms)
{
	unsigned i, j, k;
	TDAG term;
	Tstack_DAG tmp_terms;
	stack_COPY(tmp_terms, *terms);
	stack_reset(*terms);
	DAG_tmp_reserve();
	for (i = 0; i < stack_size(tmp_terms); ++i) {
		term = stack_get(tmp_terms, i);
		if (DAG_tmp_bool[term]) {
			free_index_lits(term);
			continue;
		}
		stack_push(*terms, term);
		for (j = i + 1;
				 j < stack_size(tmp_terms) && congruent(term, stack_get(tmp_terms, j));
				 ++j) {
			for (k = 0; k < DAG_arity(term); ++k)
				if (!congruent(DAG_arg(term, k), DAG_arg(stack_get(tmp_terms, j), k)))
					break;
			/* Same signature */
			if (k == DAG_arity(term)) DAG_tmp_bool[stack_get(tmp_terms, j)] = 1;
		}
	}
	for (i = 0; i < stack_size(tmp_terms); ++i)
		DAG_tmp_bool[stack_get(tmp_terms, i)] = 0;
	DAG_tmp_release();
	stack_free(tmp_terms);
}

/* as above, but associates DAG to respective var */
static void
index_lit_term(Tvar var, TDAG DAG)
{
	unsigned i;
	Findex f_index;
	assert(DAG_sort(DAG) != SORT_BOOLEAN);
	/* Associate DAG to var */
	if (!index_lits[DAG]) stack_INIT(index_lits[DAG]);
	stack_push(index_lits[DAG], var);

	f_index.symbol = DAG_symb(DAG);
	CC_set_symbols(DAG);
	f_index.signatures = NULL;
	f_index.diseq_terms = NULL;
	if (index_model.symb_indexes[DAG_symb(DAG)])
		f_index = index_model.symb_indexes[DAG_symb(DAG)]->f_index;
	else {
		MY_MALLOC(index_model.symb_indexes[DAG_symb(DAG)], sizeof(Indexes));
		stack_push(index_model.indexed_fsymbols, DAG_symb(DAG));
	}
	if (!f_index.signatures) stack_INIT(f_index.signatures);
	stack_push(f_index.signatures, DAG);
	index_model.symb_indexes[DAG_symb(DAG)]->f_index = f_index;
	for (i = 0; i < DAG_arity(DAG); ++i)
		if (DAG_arity(DAG_arg(DAG, i)) && !DAG_tmp_bool[DAG_arg(DAG, i)]) {
			DAG_tmp_bool[DAG_arg(DAG, i)] = 1;
			index_lit_term(var, DAG_arg(DAG, i));
		}
}

/* as above, but calls modified functions  */
void
set_SAT_lit_index(unsigned delete_lvl)
{
	unsigned i, j;
	bool pol;
	TDAG DAG;
	Pindex p_index;
	Tsymb symbol;
	stats_timer_start(index_stats_time);
	MY_MALLOC(
		index_model.symb_indexes, stack_size(DAG_symb_stack) * (sizeof(Indexes)));
	memset(
		index_model.symb_indexes, 0,
		stack_size(DAG_symb_stack) * (sizeof(Indexes)));
	stack_INIT(index_model.indexed_fsymbols);
	stack_INIT(index_model.indexed_psymbols);
	DAG_tmp_reserve();
	for (i = 0; i < SAT_literal_stack_n; ++i) {
		DAG = lit_to_DAG(SAT_literal_stack[i]);
		pol = lit_pol(SAT_literal_stack[i]);
#ifndef POLARITY_FILTER
		if (
			DAG == DAG_NULL || DAG == DAG_TRUE || boolean_connector(DAG_symb(DAG)) ||
			!DAG_arity(DAG) || DAG_quant(DAG) || DAG_sort(DAG) != SORT_BOOLEAN)
			continue;
#else
		/* TODO: amend this */
		if (
			(!opt_bool_required_off && !bool_required[SAT_literal_stack[i]]) ||
			(DAG == DAG_NULL || DAG == DAG_TRUE || boolean_connector(DAG_symb(DAG)) ||
			 !DAG_arity(DAG) || DAG_quant(DAG) || DAG_sort(DAG) != SORT_BOOLEAN))
			continue;
#endif
		/* Disequalities should be set no matter the level, model etc; I think */
		if (!pol && DAG_symb(DAG) == PREDICATE_EQ) CC_set_diseqs(DAG);
		if (!prime_implicant_off && !prime_required[SAT_literal_stack[i]]) {
			stats_counter_inc(index_stats_prime);
			continue;
		}
		if (!prune_cnf_off && !original_required[SAT_literal_stack[i]]) {
			stats_counter_inc(index_stats_prune_cnf);
			continue;
		}
		/* Filter literals from instantiations downstream */
		if (delete_lvl < inst_var_lvl[lit_var(SAT_literal_stack[i])].lvl) {
#if DEBUG_DELETION
			my_DAG_message(
				"set_prime_index: deleting lit %d, {%d}%D\n", SAT_literal_stack[i], DAG,
				DAG);
#endif
			stats_counter_inc(index_stats_deleted);
			continue;
		}
		assert(!DAG_tmp_bool[DAG]);
		DAG_tmp_bool[DAG] = 1;
		/* Index all terms */
		for (j = 0; j < DAG_arity(DAG); ++j)
			if (DAG_arity(DAG_arg(DAG, j)) && !DAG_tmp_bool[DAG_arg(DAG, j)]) {
				DAG_tmp_bool[DAG_arg(DAG, j)] = 1;
				index_lit_term(lit_var(SAT_literal_stack[i]), DAG_arg(DAG, j));
			}
		/* Equality literal */
		if (DAG_symb(DAG) == PREDICATE_EQ) {
			/* Disequality */
			if (!pol) CC_set_diseqs(DAG);
			continue;
		}
		/* Uninterpreted predicate */
		p_index.symbol = DAG_symb(DAG);
		p_index.signatures[0] = NULL;
		p_index.signatures[1] = NULL;
		if (index_model.symb_indexes[DAG_symb(DAG)])
			p_index = index_model.symb_indexes[DAG_symb(DAG)]->p_index;
		else {
			MY_MALLOC(index_model.symb_indexes[DAG_symb(DAG)], sizeof(Indexes));
			stack_push(index_model.indexed_psymbols, DAG_symb(DAG));
		}
		if (!p_index.signatures[pol]) stack_INIT(p_index.signatures[pol]);
		stack_push(p_index.signatures[pol], DAG);
		index_model.symb_indexes[DAG_symb(DAG)]->p_index = p_index;
		/* Associate DAG to var */
		if (!index_lits[DAG]) stack_INIT(index_lits[DAG]);
		stack_push(index_lits[DAG], lit_var(SAT_literal_stack[i]));
	}
	for (i = 0; i < SAT_literal_stack_n; ++i)
		DAG_tmp_reset_bool(lit_to_DAG(SAT_literal_stack[i]));
	DAG_tmp_release();
	/* Delete repeated symbols */
	stack_sort(index_model.indexed_fsymbols, DAG_cmp_q);
	stack_uniq(index_model.indexed_fsymbols);
	stack_sort(index_model.indexed_psymbols, DAG_cmp_q);
	stack_uniq(index_model.indexed_psymbols);
	/* Sort by congruence class, remove duplicates, merge terms with same
     signature */
	for (i = 0; i < stack_size(index_model.indexed_fsymbols); ++i) {
		symbol = stack_get(index_model.indexed_fsymbols, i);
		if (index_model.symb_indexes[symbol]->f_index.signatures) {
			stack_sort(
				index_model.symb_indexes[symbol]->f_index.signatures, DAG_cmp_q);
			stack_uniq(index_model.symb_indexes[symbol]->f_index.signatures);
			stack_sort(
				index_model.symb_indexes[symbol]->f_index.signatures, DAG_cmp_q_class);
			merge_lit_signatures(
				&index_model.symb_indexes[symbol]->f_index.signatures);
		}
		assert(!index_model.symb_indexes[symbol]->f_index.diseq_terms);
	}
	for (i = 0; i < stack_size(index_model.indexed_psymbols); ++i) {
		symbol = stack_get(index_model.indexed_psymbols, i);
		for (j = 0; j < 2; ++j)
			if (index_model.symb_indexes[symbol]->p_index.signatures[j]) {
				stack_sort(
					index_model.symb_indexes[symbol]->p_index.signatures[j], DAG_cmp_q);
				stack_uniq(index_model.symb_indexes[symbol]->p_index.signatures[j]);
				stack_sort(
					index_model.symb_indexes[symbol]->p_index.signatures[j],
					DAG_cmp_q_class);
				/* TODO: this should be improved by having an external CC that merges the
               signatures at the beginning of each instantiation cycle

               Another option is to have an incremental CC to which equalities
               (and disequalities?) are added according to the inst level of
               literals */
				merge_lit_signatures(
					&index_model.symb_indexes[symbol]->p_index.signatures[j]);
			}
	}
#if DEBUG_INDEX
	my_message("F_Indexes size: %d\n", get_all_Findex_size());
	my_message("P_Indexes size: %d\n", get_all_Pindex_size());
#if DEBUG_INDEX > 1
	for (i = 0; i < stack_size(index_model.indexed_fsymbols); ++i)
		print_Findex(
			index_model.symb_indexes[stack_get(index_model.indexed_fsymbols, i)]
				->f_index);
	for (i = 0; i < stack_size(index_model.indexed_psymbols); ++i)
		print_Pindex(
			index_model.symb_indexes[stack_get(index_model.indexed_psymbols, i)]
				->p_index);
	for (int gambi = 50; gambi--; printf("-"))
		;
	printf("\n\n");
#endif
#endif
}

/*
  --------------------------------------------------------------
  Indexing Sort classes
  --------------------------------------------------------------
*/

typedef struct TDAGdepth
{
	TDAG DAG;
	unsigned depth;
} TDAGdepth;

TSstack(_DAGdepth, TDAGdepth); /* typedefs Tstack_DAGdepth */

Tstack_DAGdepth* sorts_index;

static int
DAGdepth_cmp_q(TDAGdepth* PDAG1, TDAGdepth* PDAG2)
{
	return (int)PDAG1->depth - (int)PDAG2->depth;
}

Tstack_DAG
get_sort_terms_shallow(Tsort sort)
{
	unsigned i, depth;
	Tstack_DAG result = NULL;
	if (sorts_index[sort]) {
		stack_INIT(result);
		assert(!stack_is_empty(sorts_index[sort]));
		stack_push(result, stack_get(sorts_index[sort], 0).DAG);
		depth = stack_get(sorts_index[sort], 0).depth;
		for (i = 1; i < stack_size(sorts_index[sort]) &&
				 stack_get(sorts_index[sort], i).depth == depth;
				 ++i)
			stack_push(result, stack_get(sorts_index[sort], i).DAG);
	}
	assert(!result || !stack_is_empty(result));
	return result;
}

Tstack_DAG
get_sort_terms(Tsort sort)
{
	unsigned i;
	Tstack_DAG result = NULL;
	if (sorts_index[sort]) {
		stack_INIT(result);
		for (i = 0; i < stack_size(sorts_index[sort]); ++i)
			stack_push(result, stack_get(sorts_index[sort], i).DAG);
	}
	assert(!result || !stack_is_empty(result));
	return result;
}

/**
   \brief index set of terms of given sort according to depth
   \param sort a sort */
static void
index_by_depth(Tsort sort)
{
	unsigned i;
	TDAG DAG;
	for (i = 0; i < stack_size(sorts_index[sort]); ++i) {
		/* TODO: Is this indirection necessary? It would only be if DAG_depth
         modified sorts_index, no? */
		DAG = stack_get(sorts_index[sort], i).DAG;
		stack_get(sorts_index[sort], i).depth = DAG_depth(DAG);
	}
	stack_sort(sorts_index[sort], DAGdepth_cmp_q);
}

static void
index_term_sort_rec(TDAG DAG)
{
	unsigned i;
	Tsort sort;
	if (DAG_tmp_bool[CC_abstract(DAG)]) return;
	sort = DAG_sort(DAG);
	DAG_tmp_bool[CC_abstract(DAG)] = 1;
	if (!sorts_index[sort]) stack_INIT(sorts_index[sort]);
	stack_inc(sorts_index[sort]);
	stack_top(sorts_index[sort]).DAG = CC_abstract(DAG);
	for (i = 0; i < DAG_arity(DAG); i++) index_term_sort_rec(DAG_arg(DAG, i));
}

static void
index_ground_term_sort_rec(TDAG DAG)
{
	unsigned i;
	Tsort sort;
	/* Term known by congruence closure */
	if (CC_abstract(DAG)) {
		assert(ground(DAG));
		index_term_sort_rec(DAG);
		return;
	}
	if (quantifier(DAG_symb(DAG))) {
		index_ground_term_sort_rec(DAG_arg_last(DAG));
		return;
	}
	/* TODO: why am I not using DAG_tmp? */
	/* if (DAG_tmp_bool[DAG]) */
	/*   return; */
	if (DAG_sort(DAG) != SORT_BOOLEAN && ground(DAG)) {
		/* DAG_tmp_bool[DAG] = 1; */
		sort = DAG_sort(DAG);
		if (!sorts_index[sort]) stack_INIT(sorts_index[sort]);
		stack_inc(sorts_index[sort]);
		stack_top(sorts_index[sort]).DAG = DAG;
	}
	for (i = 0; i < DAG_arity(DAG); i++)
		index_ground_term_sort_rec(DAG_arg(DAG, i));
}

void
set_sorts_index(unsigned delete_lvl)
{
	unsigned i, j;
	TDAG DAG;
	stats_timer_start(index_stats_time);
	/* Prune boolean model according to options */
	MY_MALLOC(sorts_index, stack_size(DAG_sort_stack) * sizeof(Tstack_DAGdepth));
	memset(sorts_index, 0, stack_size(DAG_sort_stack) * sizeof(Tstack_DAGdepth));
	DAG_tmp_reserve();
	for (i = 0; i < SAT_literal_stack_n; ++i) {
		DAG = lit_to_DAG(SAT_literal_stack[i]);
#ifndef POLARITY_FILTER
		if (
			DAG == DAG_NULL || DAG == DAG_TRUE || boolean_connector(DAG_symb(DAG)) ||
			!DAG_arity(DAG) || DAG_quant(DAG) || DAG_sort(DAG) != SORT_BOOLEAN)
			continue;
#else
		/* TODO: amend this */
		if (
			(!opt_bool_required_off && !bool_required[SAT_literal_stack[i]]) ||
			(DAG == DAG_NULL || DAG == DAG_TRUE || boolean_connector(DAG_symb(DAG)) ||
			 !DAG_arity(DAG) ||
			 ((DAG_sort(DAG) != SORT_BOOLEAN || DAG_quant(DAG)) &&
				!index_fresh_sorts)))
			continue;
#endif
		if (!prime_implicant_off && !prime_required[SAT_literal_stack[i]]) {
			stats_counter_inc(index_stats_prime);
			continue;
		}
		if (!prune_cnf_off && !original_required[SAT_literal_stack[i]]) {
			stats_counter_inc(index_stats_prune_cnf);
			continue;
		}
		/* Filter literals from instantiations downstream */
		if (delete_lvl < inst_var_lvl[lit_var(SAT_literal_stack[i])].lvl) {
#if DEBUG_DELETION
			my_DAG_message(
				"set_prime_index: filtering out lit %d, {%d}%D\n", SAT_literal_stack[i],
				DAG, DAG);
#endif
			stats_counter_inc(index_stats_deleted);
			continue;
		}
		/* Index ground terms in quantified formula */
		if (index_fresh_sorts && quantifier(DAG_symb(DAG)))
			index_ground_term_sort_rec(DAG_arg_last(DAG));
		else /* index all terms */
			for (j = 0; j < DAG_arity(DAG); ++j) index_term_sort_rec(DAG_arg(DAG, j));
	}
	/* if (index_fresh_sorts) */
	/*   for (i = 0; i < SAT_literal_stack_n; ++i) */
	/*     if (quantifier(DAG_symb(lit_to_DAG(SAT_literal_stack[i])))) */
	/*       DAG_tmp_reset_bool(DAG_arg_last(lit_to_DAG(SAT_literal_stack[i]))); */
	for (i = 0; i < stack_size(DAG_sort_stack); ++i)
		if (sorts_index[i])
			for (j = 0; j < stack_size(sorts_index[i]); ++j)
				DAG_tmp_reset_bool(stack_get(sorts_index[i], j).DAG);
	DAG_tmp_release();
	for (i = 0; i < stack_size(DAG_sort_stack); ++i)
		if (sorts_index[i]) index_by_depth(i);
	stats_timer_stop(index_stats_time);
}

void
unset_sorts_index(void)
{
	unsigned i;
	for (i = 0; i < stack_size(DAG_sort_stack); ++i)
		if (sorts_index[i]) stack_free(sorts_index[i]);
	free(sorts_index);
}

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

void
inst_index_init(void)
{
	index_model.symb_indexes = NULL;
	last_lvl = 0;
	SAT_markup_function = update_lvl_down;
	inst_var_lvl = NULL;
	DAG_set_hook_resize(lit_lvl_DAG_hook_resize);

	index_fresh_SAT = false;
	options_new(
		0, "index-fresh-SAT", "Index ground terms in quant formulas in SAT stack",
		&index_fresh_SAT);
	index_fresh_sorts = false;
	options_new(
		0, "index-fresh-sorts",
		"Use ground terms under quantifiers in enum. instantiation (requires --index-sorts).", &index_fresh_sorts);

	index_stats_time =
		stats_timer_new("index_time", "Indexing time", "%7.2f", STATS_TIMER_ALL);
	index_stats_deepest_lvl = stats_counter_new(
		"del/deepest", "max deepth of inst round with deletion", "%6d");
	index_stats_deleted = stats_counter_new(
		"del/deleted", "how many literals deleted from index (cumulative)", "%6d");
	index_stats_prime = stats_counter_new(
		"index/prime", "how many literals prime implicant removed (cumulative)",
		"%6d");
	index_stats_prune_cnf = stats_counter_new(
		"index/prune_cnf", "how many literals pruning CNF removed (cumulative)",
		"%6d");
	index_stats_arity_max = stats_counter_new(
		"index/arity_max", "biggerst arity of indexed function symbol", "%6d");
}

void
inst_index_done(void)
{
	/* stats_unsigned("index/arity_max", */
	/*                "biggerst arity of indexed function symbol", */
	/*                "%6d", index_stats_arity_max); */
}
