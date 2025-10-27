#include "symbolic/DAG-subst.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/dll-DAG.h"
#include "utils/general.h"
#include "utils/options.h"
#include "utils/veriT-qsort.h"

#include <limits.h>
#include <string.h>

#define DAG_cluster ((Tdll_DAG*)DAG_tmp)

#include "pre/simp-sym.h"

/* TODO: order is not good.
   Use a dynamic order, using already used symmetry-breaking constants */
/* TODO: check for binders */

/* #define DEBUG_SYM */
/* #define PRINT_SYM_ASSUMPTIONS */
/* #define PRINT_SYM_STATS */
/* #define SAVE_SYM_ASSUMPTIONS */

static bool disable_simp_sym = false;

TSstack(_dll_DAG, Tdll_DAG);

/*
  --------------------------------------------------------------
  Checking for clusters
  --------------------------------------------------------------
*/

/**
   \brief Call simp_sym_notify(n, cts) with all clusters of n cts of
   variables related by an X != Y
   \param DAG a conjuction where to find the X != Y
   \remark used as a heuristic to find candidates for symmetries */
static void
find_clusters(TDAG DAG)
{
	unsigned i;
	Tstack_DAG constants;
	Tstack_dll_DAG clusters;
	if (DAG_symb(DAG) != CONNECTOR_AND) return;
	stack_INIT(constants);
	stack_INIT(clusters);
	DAG_tmp_reserve();
	/* create all possible clusters */
	for (i = 0; i < DAG_arity(DAG); i++)
		if (
			DAG_symb(DAG_arg(DAG, i)) == CONNECTOR_NOT &&
			DAG_symb(DAG_arg0(DAG_arg(DAG, i))) == PREDICATE_EQ) {
			TDAG tmp, eq = DAG_arg0(DAG_arg(DAG, i));
			tmp = DAG_arg0(eq);
			if (
				DAG_arity(tmp) || (DAG_symb_type(DAG_symb(tmp)) & SYMB_INTERPRETED) ||
				DAG_sort(tmp) == SORT_BOOLEAN || DAG_sort_arity(DAG_sort(tmp)))
				continue;
			tmp = DAG_arg1(eq);
			if (
				DAG_arity(tmp) || (DAG_symb_type(DAG_symb(tmp)) & SYMB_INTERPRETED) ||
				DAG_sort(tmp) == SORT_BOOLEAN || DAG_sort_arity(DAG_sort(tmp)))
				continue;
			tmp = DAG_arg0(eq);
			if (!DAG_cluster[tmp]) {
				DAG_cluster[tmp] = dll_DAG_cons(tmp, DLL_NULL);
				stack_push(constants, tmp);
			}
			tmp = DAG_arg1(eq);
			if (!DAG_cluster[tmp]) {
				DAG_cluster[tmp] = dll_DAG_cons(tmp, DLL_NULL);
				stack_push(constants, tmp);
			}
			if (!dll_DAG_same(DAG_cluster[DAG_arg0(eq)], DAG_cluster[DAG_arg1(eq)]))
				dll_DAG_merge(DAG_cluster[DAG_arg0(eq)], DAG_cluster[DAG_arg1(eq)]);
		}
	/* collect clusters */
	while (stack_size(constants)) {
		TDAG tmp = stack_pop(constants);
		Tdll_DAG cluster = DAG_cluster[tmp];
		Tdll_DAG dll_tmp = cluster;
		if (!cluster) continue;
		dll_tmp = cluster;
		do {
			DAG_cluster[dll_car(dll_tmp)] = DLL_NULL;
			dll_tmp = dll_cdr(dll_tmp);
		} while (dll_tmp != cluster);
		stack_push(clusters, cluster);
	}
	stack_free(constants);
	/* IMPROVE eliminate elements that are not really != to all others */
	for (i = 0; i < stack_size(clusters); i++) {
		unsigned j, n;
		TDAG* cts;
		Tdll_DAG cluster = stack_get(clusters, i);
		n = dll_DAG_length(cluster);
		assert(n >= 2);
		MY_MALLOC(cts, n * sizeof(TDAG));
		for (j = 0; j < n; j++, cluster = dll_DAG_cdr(cluster))
			cts[j] = dll_DAG_car(cluster);
		simp_sym_notify(n, cts);
		dll_DAG_free(&cluster);
	}
	DAG_tmp_release();
	stack_free(clusters);
}

/*
  --------------------------------------------------------------
  Checking invariance by permutation
  --------------------------------------------------------------
*/

static TDAG
normalize_aux(TDAG src)
{
	unsigned i, j;
	TDAG* tmp;
	TDAG dest;
	if (DAG_symb(src) != CONNECTOR_AND && DAG_symb(src) != CONNECTOR_OR)
		return src;
	MY_MALLOC(tmp, DAG_arity(src) * sizeof(TDAG));
	for (i = 0; i < DAG_arity(src); i++) tmp[i] = DAG_arg(src, i);
	veriT_qsort(tmp, DAG_arity(src), sizeof(TDAG), (TFcmp)DAG_cmp_q);
	for (i = 1, j = 0; i < DAG_arity(src); i++)
		if (tmp[i] != tmp[j]) tmp[++j] = tmp[i];
	++j;
	dest = DAG_dup(DAG_new(DAG_symb(src), j, tmp));
	DAG_free(src);
	return dest;
}

/**
   \brief normalizes disjunctions and conjunctions (by sorting their
   arguments).
   \param src a DAG
   \return the normalized form of src */
static TDAG
normalize(TDAG src)
{
	return structural_recursion(src, normalize_aux);
}

static int
permutation_invariant(TDAG src, unsigned n, TDAG* cts)
{
	TDAG tmp, dest;
	unsigned i;
	int res;
#ifdef DEBUG_SYM
	my_message("permutation_invariant in\n");
#endif
	assert(n >= 2);
	DAG_tmp_reserve();
	DAG_tmp_DAG[cts[0]] = cts[1];
	DAG_tmp_DAG[cts[1]] = cts[0];
	DAG_tmp_subst(src);
	tmp = DAG_dup(DAG_tmp_DAG[src]);
	DAG_tmp_reset_DAG(src);
	DAG_tmp_DAG[cts[0]] = DAG_NULL;
	DAG_tmp_DAG[cts[1]] = DAG_NULL;
	DAG_tmp_release();
	dest = normalize(tmp);
	res = (dest == src);
	DAG_free(tmp);
	DAG_free(dest);
	if (!res) return 0;
	if (n == 2) return 1;
	DAG_tmp_reserve();
	for (i = 0; i < n - 1; i++) DAG_tmp_DAG[cts[i]] = cts[i + 1];
	DAG_tmp_DAG[cts[n - 1]] = cts[0];
	DAG_tmp_subst(src);
	tmp = DAG_dup(DAG_tmp_DAG[src]);
	DAG_tmp_reset_DAG(src);
	DAG_tmp_release();
	dest = normalize(tmp);
	res = (dest == src);
	DAG_free(tmp);
	DAG_free(dest);
	return res;
}

/*
  --------------------------------------------------------------
  Choosing good terms
  --------------------------------------------------------------
*/

typedef struct Tsimp_sym
{
	unsigned n;
	TDAG* cts;
} Tsimp_sym;

TSstack(_simp_sym, Tsimp_sym);
static Tstack_simp_sym simp_sym_stack = NULL;

typedef struct Tchosen_term
{
	unsigned cts_occurrences;
	unsigned clause_count;
	TDAG DAG;
} Tchosen_term;
typedef Tchosen_term* TPchosen_term;

TSstack(_chosen_term, TPchosen_term);

#if 0
static int
chosen_term_sort(Tchosen_term ** PPchosen_term1, Tchosen_term ** PPchosen_term2)
{
  Tchosen_term * Pchosen_term1 = *PPchosen_term1;
  Tchosen_term * Pchosen_term2 = *PPchosen_term2;
  int tmp, c1 = 0, c2 = 0;
  if (!Pchosen_term1->cts_occurrences && !Pchosen_term2->cts_occurrences)
    {
      tmp = Pchosen_term2->clause_count - Pchosen_term1->clause_count;
      if (!tmp)
        {
          if (Pchosen_term1->cts_occurrences < Pchosen_term2->cts_occurrences)
            return -1;
          if (Pchosen_term1->cts_occurrences > Pchosen_term2->cts_occurrences)
            return 1;
          return 0;
        }
      return tmp;
    }
  if (Pchosen_term1->cts_occurrences && !Pchosen_term2->cts_occurrences)
    return 1;
  if (!Pchosen_term1->cts_occurrences && Pchosen_term2->cts_occurrences)
    return -1;
  tmp = Pchosen_term1->cts_occurrences;
  while (tmp) { c1 += (tmp & 1); tmp>>=1; }
  tmp = Pchosen_term2->cts_occurrences;
  while (tmp) { c2 += (tmp & 1); tmp>>=1; }
  if (c1 == c2)
    {
      tmp = Pchosen_term2->clause_count - Pchosen_term1->clause_count;
      if (!tmp)
        {
          if (Pchosen_term1->cts_occurrences < Pchosen_term2->cts_occurrences)
            return -1;
          if (Pchosen_term1->cts_occurrences > Pchosen_term2->cts_occurrences)
            return 1;
          return 0;
        }
      return tmp;
    }
  return Pchosen_term2->clause_count - Pchosen_term1->clause_count;
  if (c1 < c2)
    return -1;
  return 1;
}
#endif

static double
chosen_term_value(Tchosen_term* Pchosen_term, unsigned cts_occurences_mask)
{
	unsigned tmp = Pchosen_term->cts_occurrences & ~cts_occurences_mask;
	unsigned c = 0;
	while (tmp) {
		c += (tmp & 1);
		tmp >>= 1;
	}
	/*
    my_DAG_message("chosen_term_value %D %x %d %d %f\n", Pchosen_term->DAG,
    Pchosen_term->cts_occurrences & cts_occurences_mask,
    c, Pchosen_term->clause_count,
    Pchosen_term->clause_count * 1. / (c + 1));
  */
	return Pchosen_term->clause_count * 1. / (c + 1);
}

static void
chosen_terms_sort(Tstack_chosen_term* Pchosen_terms, unsigned nb_cts)
{
	unsigned j = 0;
	unsigned cts_occurences_mask = 0;
	while (cts_occurences_mask != ((1u << nb_cts) - 1u)) {
		unsigned i;
		unsigned selected_term = UINT_MAX;
		int selected_is_ct_free = 0;
		double selected_value = 0;
		Tchosen_term* Pchosen_term;
		for (i = j; i < stack_size(*Pchosen_terms); i++) {
			double tmp;
			Pchosen_term = stack_get(*Pchosen_terms, i);
			tmp = chosen_term_value(Pchosen_term, cts_occurences_mask);
			if (
				(!Pchosen_term->cts_occurrences &&
				 (!selected_is_ct_free || tmp > selected_value)) ||
				(Pchosen_term->cts_occurrences &&
				 (!selected_is_ct_free && tmp > selected_value))) {
				selected_term = i;
				selected_value = tmp;
				selected_is_ct_free = !Pchosen_term->cts_occurrences;
			}
		}
		if (selected_term == UINT_MAX) break;
		Pchosen_term = stack_get(*Pchosen_terms, selected_term);
		stack_set(*Pchosen_terms, selected_term, stack_get(*Pchosen_terms, j));
		stack_set(*Pchosen_terms, j, Pchosen_term);
		cts_occurences_mask |= Pchosen_term->cts_occurrences;
		for (i = 0; i < nb_cts; i++)
			if (!((1u << i) & cts_occurences_mask)) {
				cts_occurences_mask |= 1u << i;
				break;
			}
		j++;
	}
}

static void
count_occurrences(TDAG src)
{
	unsigned i;
	if (DAG_tmp_unsigned[src] & 1) return;
	DAG_tmp_unsigned[src] |= 1;
	if (DAG_tmp_unsigned[src] >> 1) DAG_tmp_unsigned[src] += 1 << 1;
	for (i = 0; i < DAG_arity(src); i++) count_occurrences(DAG_arg(src, i));
}

static void
count_occurrences_reset(TDAG src)
{
	unsigned i;
	if (!(DAG_tmp_unsigned[src] & 1)) return;
	DAG_tmp_unsigned[src] ^= 1;
	for (i = 0; i < DAG_arity(src); i++) count_occurrences_reset(DAG_arg(src, i));
}

static void
set_occur_bv(TDAG src)
{
	unsigned i;
	if (DAG_tmp_unsigned[src]) return;
	for (i = 0; i < DAG_arity(src); i++) {
		set_occur_bv(DAG_arg(src, i));
		DAG_tmp_unsigned[src] |= DAG_tmp_unsigned[DAG_arg(src, i)];
	}
}

static Tstack_DAG
choose_terms(TDAG DAG, unsigned n, TDAG* cts)
{
	unsigned i;
	Tstack_chosen_term chosen_terms;
	Tstack_DAG terms;
	Tstack_DAG misc_set;
	stack_INIT(terms);
	/* PF Assume the formula as previously been put in large conjunction form */
	if (DAG_symb(DAG) != CONNECTOR_AND) return terms;
	DAG_tmp_reserve();
	for (i = 0; i < n; i++) DAG_tmp_unsigned[cts[i]] = 1u << i;
	stack_INIT(chosen_terms);
	/* PF first prevent to choose terms with value already set */
	stack_INIT(misc_set);
	for (i = 0; i < DAG_arity(DAG); i++)
		if (DAG_symb(DAG_arg(DAG, i)) == PREDICATE_EQ) {
			TDAG eq = DAG_arg(DAG, i);
			if (DAG_tmp_unsigned[DAG_arg0(eq)] && !DAG_tmp_unsigned[DAG_arg1(eq)]) {
				DAG_tmp_unsigned[DAG_arg1(eq)] = 1u << n;
				stack_push(misc_set, DAG_arg1(eq));
			}
			if (DAG_tmp_unsigned[DAG_arg1(eq)] && !DAG_tmp_unsigned[DAG_arg0(eq)]) {
				DAG_tmp_unsigned[DAG_arg0(eq)] = 1u << n;
				stack_push(misc_set, DAG_arg0(eq));
			}
		}
	/* PF collect all terms that have values in the set of the cts */
	for (i = 0; i < DAG_arity(DAG); i++) {
		unsigned j, t;
		TDAG term;
		if (DAG_symb(DAG_arg(DAG, i)) != CONNECTOR_OR) continue;
		if (DAG_arity(DAG_arg(DAG, i)) != n) continue;
		for (j = 0, t = 0, term = DAG_NULL; j < n; j++) {
			TDAG eq;
			if (DAG_symb(DAG_arg(DAG_arg(DAG, i), j)) != PREDICATE_EQ) break;
			eq = DAG_arg(DAG_arg(DAG, i), j);
			if (DAG_tmp_unsigned[DAG_arg0(eq)]) {
				if (DAG_tmp_unsigned[DAG_arg0(eq)] & t) break;
				if (!term) term = DAG_arg1(eq);
				if (DAG_arg1(eq) != term) break;
				t |= DAG_tmp_unsigned[DAG_arg0(eq)];
			} else if (DAG_tmp_unsigned[DAG_arg1(eq)]) {
				if (DAG_tmp_unsigned[DAG_arg1(eq)] & t) break;
				if (!term) term = DAG_arg0(eq);
				if (DAG_arg0(eq) != term) break;
				t |= DAG_tmp_unsigned[DAG_arg1(eq)];
			} else
				break;
		}
		if (t == ((1u << n) - 1u) && !DAG_tmp_unsigned[term]) {
			Tchosen_term* Pchosen_term;
			MY_MALLOC(Pchosen_term, sizeof(Tchosen_term));
			Pchosen_term->DAG = term;
			Pchosen_term->cts_occurrences = 0;
			Pchosen_term->clause_count = 0;
			DAG_tmp_unsigned[term] = 1u << n;
			stack_push(chosen_terms, Pchosen_term);
		}
	}
	for (i = 0; i < n; i++) DAG_tmp_unsigned[cts[i]] = 0;
	while (stack_size(misc_set)) DAG_tmp_unsigned[stack_pop(misc_set)] = 0;
	stack_free(misc_set);

	/* PF count the number of clauses that contain the terms */
	for (i = 0; i < stack_size(chosen_terms); i++)
		DAG_tmp_unsigned[stack_get(chosen_terms, i)->DAG] = 1 << 1;
	for (i = 0; i < DAG_arity(DAG); i++) {
		count_occurrences(DAG_arg(DAG, i));
		count_occurrences_reset(DAG_arg(DAG, i));
	}
	for (i = 0; i < stack_size(chosen_terms); i++)
		stack_get(chosen_terms, i)->clause_count =
			DAG_tmp_unsigned[stack_get(chosen_terms, i)->DAG] >> 1;
	for (i = 0; i < stack_size(chosen_terms); i++)
		DAG_tmp_unsigned[stack_get(chosen_terms, i)->DAG] = 0;
	DAG_tmp_release();

	/* PF collects the occurrences of constants within the terms */
	DAG_tmp_reserve();
	for (i = 0; i < n; i++) DAG_tmp_unsigned[cts[i]] = 1u << i;
	for (i = 0; i < stack_size(chosen_terms); i++) {
		Tchosen_term* Pchosen_term = stack_get(chosen_terms, i);
		set_occur_bv(Pchosen_term->DAG);
		Pchosen_term->cts_occurrences = DAG_tmp_unsigned[Pchosen_term->DAG];
	}
	for (i = 0; i < stack_size(chosen_terms); i++)
		DAG_tmp_reset_unsigned(stack_get(chosen_terms, i)->DAG);
	for (i = 0; i < n; i++) DAG_tmp_unsigned[cts[i]] = 0;
	DAG_tmp_release();
	chosen_terms_sort(&chosen_terms, n);
	/* PF collect only the terms */
	for (i = 0; i < stack_size(chosen_terms); i++) {
		Tchosen_term* Pchosen_term = stack_get(chosen_terms, i);
		if (i < n) stack_push(terms, Pchosen_term->DAG);
		free(Pchosen_term);
	}
	stack_free(chosen_terms);
#ifdef DEBUG_SYM
	my_message("choose_terms : \n");
	for (i = 0; i < stack_size(terms); i++)
		my_DAG_message("  %D\n", stack_get(terms, i));
#endif
	return terms;
}

/*
  --------------------------------------------------------------
  Interface
  --------------------------------------------------------------
*/

void
simp_sym_init(void)
{
	options_new(
		0, "disable-simp-sym", "Disable simplification for symmetries.",
		&disable_simp_sym);
	stack_INIT(simp_sym_stack);
}

/**
   \brief notifies of a possible symmetry between cts
   \param n the number of constants
   \param cts the constants
   \remark checked to verify cts satisfy all necessary conditions
   \remark conditions are: all elements of cts are "simple nice cts",
   they are pairwise different, no existing copy has been notified
   yet */
void
simp_sym_notify(unsigned n, TDAG* cts)
{
	unsigned i, j;
	if (n < 2) {
		free(cts);
		return;
	}
	/* check if simple sort for every cts[i] */
	for (i = 0; i < n; i++)
		if (
			DAG_arity(cts[i]) ||
			(DAG_symb_type(DAG_symb(cts[i])) & SYMB_INTERPRETED) ||
			(DAG_symb_type(DAG_symb(cts[i])) & SYMB_VARIABLE) ||
			DAG_sort(cts[i]) == SORT_BOOLEAN || DAG_sort_arity(DAG_sort(cts[i]))) {
			free(cts);
			return;
		}
	/* sort by DAG_order, and eliminate duplicates */
	veriT_qsort(cts, n, sizeof(TDAG), (TFcmp)DAG_cmp_q);
	for (i = 0, j = 1; i + 1 < n; i++)
		if (cts[i + 1] != cts[i]) cts[j++] = cts[i + 1];
	n = j;
	/* check if not already in list */
	for (i = 0; i < stack_size(simp_sym_stack); i++)
		if (
			stack_get(simp_sym_stack, i).n == n &&
			memcmp(stack_get(simp_sym_stack, i).cts, cts, n * sizeof(TDAG)) == 0) {
			free(cts);
			return;
		}
	stack_inc(simp_sym_stack);
	stack_top(simp_sym_stack).n = n;
	for (i = 0; i < n; i++) DAG_dup(cts[i]);
	stack_top(simp_sym_stack).cts = cts;
}

static int
simp_sym_decrease(Tsimp_sym* Psimp_sym1, Tsimp_sym* Psimp_sym2)
{
	return (int)Psimp_sym2->n - (int)Psimp_sym1->n;
}

TDAG
simp_sym(TDAG src)
{
	unsigned i;
	TDAG norm_src;
#ifdef SAVE_SYM_ASSUMPTIONS
	FILE* sym_file = fopen("sym.txt", "w");
#endif
	src = DAG_dup(src);
	if (disable_simp_sym) return src;
	if (DAG_symb(src) != CONNECTOR_AND) return src;
	find_clusters(src);
	norm_src = normalize(src);
	stack_sort(simp_sym_stack, (TFcmp)simp_sym_decrease);
	for (i = 0; i < stack_size(simp_sym_stack); i++) {
		Tsimp_sym* Psimp_sym = &stack_get(simp_sym_stack, i);
		Tstack_DAG terms;
		TDAG *DAGs, dest;
		unsigned j, n = 0, t = 0;
		if (!permutation_invariant(norm_src, Psimp_sym->n, Psimp_sym->cts))
			continue;
		terms = choose_terms(src, Psimp_sym->n, Psimp_sym->cts);
		if (!stack_size(terms)) {
			stack_free(terms);
			continue;
		}
		MY_MALLOC(DAGs, (stack_size(terms) + DAG_arity(src)) * sizeof(TDAG));
		DAG_tmp_reserve();
		for (i = 0; i < Psimp_sym->n; i++)
			DAG_tmp_unsigned[Psimp_sym->cts[i]] = 1u << i;
		for (t = 0, j = 0; j < stack_size(terms); j++) {
			unsigned tmp, c, k, l;
			TDAG term = stack_get(terms, j);
			TDAG* disjuncts;
			/* all constants in the term will appear in disjunction */
			set_occur_bv(term);
			t |= DAG_tmp_unsigned[term];
			/* a new constant will appear in disjunction */
			for (tmp = t, k = 0; tmp & 1; tmp >>= 1, k++)
				;
			t |= 1u << k;
			/* count the size of disjunction */
			for (tmp = t, c = 0; tmp; tmp >>= 1) c += (tmp & 1);
			if (c >= Psimp_sym->n) break;
			/* build disjunction */
			MY_MALLOC(disjuncts, c * sizeof(TDAG));
			for (tmp = t, k = 0, l = 0; tmp; tmp >>= 1, k++)
				if (tmp & 1) disjuncts[l++] = DAG_eq(term, Psimp_sym->cts[k]);
			assert(l && l == c);
			if (l == 1) {
				DAGs[n++] = disjuncts[0];
				free(disjuncts);
			} else
				DAGs[n++] = DAG_new(CONNECTOR_OR, l, disjuncts);
#if defined(DEBUG_SYM) || defined(PRINT_SYM_ASSUMPTIONS)
			my_DAG_message("New disjunction: %D\n", DAGs[n - 1]);
#endif
#ifdef SAVE_SYM_ASSUMPTIONS
			fprintf(sym_file, "(assert ");
			DAG_fprint(sym_file, DAGs[n - 1]);
			fprintf(sym_file, ")\n");
			/*
            fprintf(sym_file, ":assumption ");
            DAG_fprint(sym_file, DAGs[n - 1]);
            fprintf(sym_file, "\n"); */
#endif
		}
		for (j = 0; j < stack_size(terms); j++)
			DAG_tmp_reset_unsigned(stack_get(terms, j));
		for (i = 0; i < Psimp_sym->n; i++) DAG_tmp_unsigned[Psimp_sym->cts[i]] = 0;
		DAG_tmp_release();
		stack_free(terms);
#ifdef PRINT_SYM_STATS
		printf("%d, %d\n", Psimp_sym->n, n);
#endif
		for (j = 0; j < DAG_arity(src); j++) DAGs[n++] = DAG_arg(src, j);
		MY_REALLOC(DAGs, n * sizeof(TDAG));
		dest = DAG_dup(DAG_new(CONNECTOR_AND, n, DAGs));
		DAG_free(src);
		src = dest;
	}
	DAG_free(norm_src);
#ifdef DEBUG_SYM
	DAG_fprint_smt2_bench(stdout, src, "unknown");
#endif
#ifdef PRINT_SYM_STATS
	exit(0);
#endif
#ifdef SAVE_SYM_ASSUMPTIONS
	fclose(sym_file);
	exit(0);
#endif
	return src;
}

void
simp_sym_done(void)
{
	unsigned i;
	for (i = 0; i < stack_size(simp_sym_stack); i++) {
		unsigned j;
		Tsimp_sym* Psimp_sym = &stack_get(simp_sym_stack, i);
		for (j = 0; j < Psimp_sym->n; j++) {
			DAG_free(Psimp_sym->cts[j]);
			Psimp_sym->cts[j] = 1234;
		}
		free(Psimp_sym->cts);
	}
	stack_free(simp_sym_stack);
}
