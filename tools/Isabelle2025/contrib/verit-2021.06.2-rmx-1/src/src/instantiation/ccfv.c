#include "instantiation/ccfv.h"

#include "congruence/congruence.h"
#include "instantiation/ccfv-bckt.h"
#include "instantiation/free-vars.h"
#include "instantiation/inst-index.h"
#include "instantiation/inst-man.h"
#include "instantiation/inst-symbs.h"
#include "instantiation/unify.h"
#include "limits.h"
#include "pre/pre.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG.h"
#include "utils/options.h"
#include "utils/statistics.h"

/* Debugging
   0: no debugging
   1: prints time; how many conflicting instances found
   2: prints info to track conflicting instances search
   3: prints info to track E-unification */
#if defined(DEBUG_HEURISTICS)
#define DEBUG_CCFV 1
#endif

/** For debugging combinatiorial explosions (unfeasible with debugger); prints
    at maximal level at specific rounds and DAG */
#define DEBUG_EXP 0

#if DEBUG_EXP
bool print_exp = false;
unsigned ccfv_round = 8;
unsigned clause = 0;
#endif

/*
  --------------------------------------------------------------
  Stats
  --------------------------------------------------------------
*/

/* Stats
   0: no stats
   1: general: time, rounds, insts
   2: euni: explosions, big index
   3: structure: redundants, fo uni, existentials */
#define STATS_CCFV

#ifdef STATS_CCFV

static unsigned found_unifiers;

/* \brief How many unifiers have sort with empty ground terms */
static unsigned ccfv_stats_ungroundable;

static unsigned ccfv_undef_preds;
static unsigned ccfv_undef_terms;
static unsigned ccfv_undef_half_eq;

/** \brief Time spent on CCFV (cumulative) */
static unsigned ccfv_stats_time;
/** \brief Max time spent on a CCFV round */
static float ccfv_stats_max_time;
/* for interal control */
static float ccfv_time;

/** \brief How many rounds of CCFV there were. */
unsigned ccfv_stats_rounds;

/** \brief How many times it gave up because of explosion */
static unsigned ccfv_stats_explode;

/** \brief How many times it gave up because of index (pos) */
static unsigned ccfv_stats_hopeless_index;

/** \brief How many times it gave up because of index (neg) */
static unsigned ccfv_stats_hopeless_index_full;

/** \brief How many times it gave up because of existential */
static unsigned ccfv_stats_hopeless_existential;
#endif

/*
  --------------------------------------------------------------
  User configurable options
  --------------------------------------------------------------
*/

/**
   \addtogroup arguments_developer

   - --ccfv-propagate

   If search for CI fails, try to propagate disjunction of equalities over terms
   known by CC if there is any */
bool ccfv_propagate;

/**
   \addtogroup arguments_developer

   - --ccfv-ujobs-off

   Deactivate memoization of unification jobs */
bool ccfv_ujobs_off;

/**
   \addtogroup arguments_developer

   - --ccfv-score

   Attributes scoring for ordering literals to be unified [optimize]  */
static bool ccfv_score;

/**
   \addtogroup arguments_user

   - --ccfv-all-CI

   Only derive CIs (default may generate non-CIs), i.e., when deriving
   substitutions from the unifiers enforces that if a variable is asserted
   disequal to a term "t" it can only be instantiated with a term "t'" if the
   ground model implies "t != t'" */
bool ccfv_all_CI;

/**
   \addtogroup arguments_developer

   - --ccfv-breadth

   Find solutions in breadth-first manner. */
static bool ccfv_breadth;

/**
   \addtogroup arguments_developer

   - --ccfv-comps-off

   Do not split (initial) constraints into components. */
static bool ccfv_comps_off;

/**
   \addtogroup arguments_developer

   - --ccfv-cnf

   Don't compute CNF if it would have more nodes than threshold */
int ccfv_cnf_threshold;

/**
   \addtogroup arguments_user

   - --ccfv-exp

   Sets a limit to the exponential merge of unifier sets, giving up if potential
   result is above threshold. The default value is 10^5 [optmize] */
int ccfv_exp_threshold;

/**
   \addtogroup arguments_user

   - --ccfv-index

   Sets a limit to the size of indexes considered during E-unification, giving
   up if index size is above threshold. The default value is 10^3 [optmize] */
int ccfv_index_threshold;

/**
   \addtogroup arguments_user

   - --ccfv-index-full

   Sets a limit to the size of indexes considered during E-unification of <fx,
   gy>, giving up if index size is above threshold. The default value is 10^2
   [optmize] */
static int ccfv_index_threshold_full;

/**
   \addtogroup arguments_user

   - --ccfv-index-inc

   Use indexes up to threshold rather than not at all if their size is abovi
   threshold */
static bool ccfv_index_inc;

/*
  --------------------------------------------------------------
  Data structures
  --------------------------------------------------------------
*/

/** \brief a literal to be unified */
typedef struct Tulit
{
	TDAG DAG; /*< the atom */
	bool pol; /*< polarity */
	unsigned score; /*< the smaller the score the sooner the literal is considered
                    for unification */
} Tulit;

TSstack(_ulit, Tulit); /* typedefs Tstack_ulit */

/*
  --------------------------------------------------------------
  Global state
  --------------------------------------------------------------
*/

/**
   \brief accumulates current disequalities in SAT solver stack
   \remark necessary because they are not kept in CC */
static Tstack_DAG CC_negative;

/**
   \brief unifiers embodying the conflicting instantiations for set of
   non-ground literals */
Tstack_unifier unifiers;

/**
   \brief backup to be used if assertion of constraints is active */
Tstack_unifier bkp_unifiers;

Tstack_ulit ulits;
/** \brief current unifier during depth-first search */
Tunifier current_unifier;

/*
  --------------------------------------------------------------
  Auxiliary stuff
  --------------------------------------------------------------
*/

#define NO_CONSTRAINT UINT_MAX

/**
   \brief ordering function on DAGs' score: returns neg, zero or pos value like
   a compare for qsort */
int
ulit_cmp_q_score(Tulit* Pulit1, Tulit* Pulit2)
{
	return (int)Pulit1->score - (int)Pulit2->score;
}

/*
  --------------------------------------------------------------
  E-Unification
  --------------------------------------------------------------
*/

static Tstack_unifier CCFV_unify_equality(TDAG D0, TDAG D1);

/**
   \brief checks whether at least one of given DAGs is an ITE
   \param D0 a term
   \param D1 a term
   \return true if has ITE, false otherwise
   \remark this function is meant to prevent doing unification on terms with
   ITEs, which will fail. They could be in-processed, however, so this hard
   condition could be lifted */
static inline bool
hopeless_ITE(TDAG D0, TDAG D1)
{
	return (DAG_symb(D0) == FUNCTION_ITE || DAG_symb(D1) == FUNCTION_ITE);
}

/**
   \brief orders pair of terms s.t. the first is non-ground
   \param NGDAG first term of ordered pair
   \param UDAG second term of ordered pair
   \param D0 a term
   \param D1 a term
   \remark if both are non-ground and at least one of them is a variable then
   NGDAG is a variable
   \remark assumes that either D0 or D1 is non-ground */
static inline void
order_pair(TDAG* NGDAG, TDAG* UDAG, TDAG D0, TDAG D1)
{
	assert(!ground(D0) || !ground(D1));
	if (ground(D0)) {
		*NGDAG = D1;
		*UDAG = D0;
		return;
	}
	if (ground(D1)) {
		*NGDAG = D0;
		*UDAG = D1;
		return;
	}
	if (!DAG_arity(D1)) {
		*NGDAG = D1;
		*UDAG = D0;
		return;
	}
	*NGDAG = D0;
	*UDAG = D1;
}

/**
   \brief checks if set of indexed terms is too big to try Eunification on
   \param i position in incremental traversal of index
   \param terms terms being traversed
   \param full whether this is being done in a <fx, gy> unification job
   \return true if above threshold, false otherwise
   \remark whether the test is made with "i" or with "terms" is dependent on the
   user options */
static inline bool
hopeless_index(unsigned i, Tstack_DAG terms, bool full)
{
	if (
		!full &&
		((ccfv_index_inc && i > ccfv_index_threshold) ||
		 (!ccfv_index_inc && (stack_size(terms) > ccfv_index_threshold)))) {
#ifdef STATS_CCFV
		stats_counter_inc(ccfv_stats_hopeless_index);
#endif
		return true;
	}
	if (
		full &&
		((ccfv_index_inc && i > ccfv_index_threshold_full) ||
		 (!ccfv_index_inc && (stack_size(terms) > ccfv_index_threshold_full)))) {
#ifdef STATS_CCFV
		stats_counter_inc(ccfv_stats_hopeless_index_full);
#endif
		return true;
	}
	return false;
}

/**
   \brief matchs NGDAG with a set of ground terms
   \param result set accumulating all resulting unifiers
   \param NGDAG a non-ground term
   \param terms a set of ground terms */
void
match_DAGs(Tstack_unifier* result, TDAG NGDAG, Tstack_DAG terms)
{
	unsigned i;
	Tstack_unifier unis_NGDAG;
	assert(*result);
	for (i = 0; i < stack_size(terms) && !hopeless_index(i, terms, false); ++i) {
		unis_NGDAG = CCFV_unify_args(NGDAG, stack_get(terms, i));
		if (!unis_NGDAG) continue;
		stack_merge(*result, unis_NGDAG);
		stack_free(unis_NGDAG);
	}
}

/**
   \brief combine a set of and a single unifier
   \param result set accumulating all resulting unifiers
   \param base a set of unifiers
   \param new a unifier
   \remark all non consistent unifiers in (base X new) are discarded
   \remark destructive for "new" */
void
combine_unifiers_one(Tstack_unifier* result, Tstack_unifier base, Tunifier new)
{
	unsigned i;
	Tunifier unifier;
	for (i = 0; i < stack_size(base); ++i) {
		unifier = unify_copy(stack_get(base, i));
		if (unify_merge(unifier, new))
			stack_push(*result, unifier);
		else
			unify_free(unifier);
	}
	unify_free(new);
}

/**
   \brief combine two sets of unifiers
   \param result set accumulating all resulting unifiers
   \param base a set of unifiers
   \param new a set of unifiers
   \remark all non consistent unifiers in (base X new) are discarded
   \remark destructive for "new" */
void
combine_unifiers(
	Tstack_unifier* result, Tstack_unifier base, Tstack_unifier new)
{
	unsigned i;
	Tunifier unifier, new_unifier;
	Tstack_unifier new_unifiers;
	if (stack_size(base) * stack_size(new) > ccfv_exp_threshold) {
#ifdef STATS_CCFV
		stats_counter_inc(ccfv_stats_explode);
#endif
		stack_apply(new, unify_free);
		stack_free(new);
		return;
	}
	stack_INIT(new_unifiers);
	while (!stack_is_empty(new)) {
		new_unifier = stack_pop(new);
		for (i = 0; i < stack_size(base); ++i) {
			unifier = unify_copy(stack_get(base, i));
			if (unify_merge(unifier, new_unifier))
				stack_push(new_unifiers, unifier);
			else
				unify_free(unifier);
		}
		unify_free(new_unifier);
	}
	stack_merge(*result, new_unifiers);
	stack_free(new_unifiers);
	stack_free(new);
}

/**
   \brief updates the set of instantiations falsifying given clause
   \param new set of unifiers from last evaluated literal
   \return true if combining new set of unifiers with the one from previous
   literals results in a non-empty set, false otherwise
   \remark destructive for "new" */
static bool update_unifiers(Tstack_unifier new)
{
	Tstack_unifier old_unifiers;
#if DEBUG_UNIFYING
	if (new_subs) {
		my_DAG_message("update_unifiers: in with insts\n");
		print_Tstack_unifier(new_subs);
	} else
		my_DAG_message("update_unifiers: no insts; destroy current unifiers\n");
#endif
	if (!new || stack_is_empty(new)) {
		stack_apply(unifiers, unify_free);
		stack_reset(unifiers);
		if (new) stack_free(new);
		return false;
	}
	/* First set of unifiers */
	if (stack_is_empty(unifiers)) {
		/* TODO: Why this led to worse results? */
		/* stack_merge(unifiers, new); */
		/* stack_free(new); */
		/* return true; */
		Tunifier unifier = unify_new(current_vars);
		stack_push(unifiers, unifier);
	}
	stack_COPY(old_unifiers, unifiers);
	stack_reset(unifiers);
	combine_unifiers(&unifiers, old_unifiers, new);
	stack_apply(old_unifiers, unify_free);
	stack_free(old_unifiers);
	return !stack_is_empty(unifiers);
}

/* TODO: this could be extended to a notion of "broader sort": an int value
   could be used to instantiate a real variable, but not the other way around,
   e.g. */
#define COMPATIBLE_SORTS(D0, D1) (DAG_sort(D0) == DAG_sort(D1))

/**
   \brief given two function applications computes the unifiers of their
   arguments
   \param D0 a function application
   \param D1 a function application
   \return all unifiers simultaneously pairwise unifying the arguments of D0 and
   D1
   \remark at least one of the inputs is non-ground, neither is nullary and both
   have the same arity */
Tstack_unifier
CCFV_unify_args(TDAG D0, TDAG D1)
{
	unsigned i;
	Tstack_DAG unify_queue;
	Tstack_unifier result = NULL, old_result, result_pair;
	assert(
		(!ground(D0) || !ground(D1)) &&
		(DAG_arity(D0) && DAG_arity(D0) == DAG_arity(D1)));
	stack_INIT(unify_queue);
	/* Collects arguments that need to be unified; incompatible
     arguments rule out unification */
	for (i = 0; i < DAG_arity(D0); ++i) {
		if (hopeless_ITE(DAG_arg(D0, i), DAG_arg(D1, i))) break;
		if (
			ground(DAG_arg(D0, i)) && ground(DAG_arg(D1, i)) &&
			!congruent(DAG_arg(D0, i), DAG_arg(D1, i)))
			break;
		if (!COMPATIBLE_SORTS(DAG_arg(D0, i), DAG_arg(D1, i))) break;
		if (congruent(DAG_arg(D0, i), DAG_arg(D1, i))) continue;
		stack_push(unify_queue, DAG_arg(D0, i));
		stack_push(unify_queue, DAG_arg(D1, i));
	}
	if (i != DAG_arity(D0)) {
		stack_free(unify_queue);
		return result;
	}
	stack_INIT(result);
	for (i = 0; i < stack_size(unify_queue); i = i + 2) {
		result_pair = CCFV_unify_equality(
			stack_get(unify_queue, i), stack_get(unify_queue, i + 1));
		/* If unification of a pair failed, give up */
		if (stack_is_empty(result_pair)) {
			stack_apply(result, unify_free);
			stack_reset(result);
			stack_free(result_pair);
			break;
		}
		/* This is the first unification found  */
		if (stack_is_empty(result)) {
			stack_merge(result, result_pair);
			stack_free(result_pair);
			continue;
		}
		/* Combine each new set of unifications with previous ones */
		stack_COPY(old_result, result);
		stack_reset(result);
		combine_unifiers(&result, old_result, result_pair);
		stack_apply(old_result, unify_free);
		stack_free(old_result);
		if (stack_is_empty(result)) break;
	}
	stack_free(unify_queue);
	if (stack_is_empty(result)) stack_free(result);
	return result;
}

extern bool ematch_on;

/**
   \brief computes the unifiers solving the E-unification problem of the given
   terms
   \param D0 a term
   \param D1 a term
   \return all unifiers solving the given E-unification problem, if any
   \remark at least one of the inputs is non-ground
   \remark according to the arity and "groundness" of the inputs, as well as
   the user options, there are several different cases, each handled in a
   specific way, as denoted by the comments
   \remark only stores the result in ujobs_index if the result is non-trivial,
   i.e., for all cases but the first */
static Tstack_unifier
CCFV_unify_equality(TDAG D0, TDAG D1)
{
	unsigned i, j;
	TDAG NGDAG, UDAG, fapp_DAG, var_DAG, term, term_class;
	Tstack_DAG terms, terms_UDAG;
	Findex index, index_UDAG;
	Tunifier unifier;
	Tstack_unifier subs_NGDAG, subs_UDAG, result;
	order_pair(&NGDAG, &UDAG, D0, D1);
	assert(!ground(NGDAG));
#if DEBUG_CCFV > 2
	my_DAG_message("CCFV_unify_eq: In with {%d,%d}<%D, %D>\n", D0, D1, D0, D1);
#elif DEBUG_EXP
	if (print_exp)
		my_DAG_message("CCFV_unify_eq: In with {%d,%d}<%D, %D>\n", D0, D1, D0, D1);
#endif
	/* Whether NGDAG = UDAG was already done */
	if (
		!ccfv_ujobs_off && retrieve_ujob(&result, NGDAG, UDAG, true, current_vars))
		return result;
	/* Whether either term has functional ITEs; TODO: Should be done outside */
	if (hopeless_ITE(D0, D1)) return NULL;
	stack_INIT(result);
	/* x = y or x = t */
	if (
		!DAG_arity(NGDAG) &&
		((!ground(UDAG) && !DAG_arity(UDAG)) || ground(UDAG))) {
		unifier = unify_new(current_vars);
		unify_union(unifier, NGDAG, UDAG);
		stack_push(result, unifier);
		return result;
	}
	/* x = gy or fx = y */
	if (
		!ground(UDAG) &&
		((DAG_arity(NGDAG) && !DAG_arity(UDAG)) ||
		 (!DAG_arity(NGDAG) && DAG_arity(UDAG)))) {
		fapp_DAG = DAG_arity(UDAG) ? UDAG : NGDAG;
		var_DAG = fapp_DAG == UDAG ? NGDAG : UDAG;
		if (!get_Findex(DAG_symb(fapp_DAG), &index) || !index.signatures)
			return result;
		/* TODO: Use match_DAGs */
		for (i = 0; i < stack_size(index.signatures) &&
				 !hopeless_index(i, index.signatures, false);
				 ++i) {
			term = stack_get(index.signatures, i);
			assert(ground(term));
			/* <UDAG, term> + <x, term> */
			subs_NGDAG = CCFV_unify_equality(fapp_DAG, term);
			if (!stack_is_empty(subs_NGDAG)) {
				unifier = unify_new(current_vars);
				unify_union(unifier, var_DAG, term);
				combine_unifiers_one(&result, subs_NGDAG, unifier);
			}
			stack_apply(subs_NGDAG, unify_free);
			stack_free(subs_NGDAG);
		}
		if (!ccfv_ujobs_off) set_ujob(NGDAG, UDAG, true, result);
		return result;
	}
	/* fx = gy */
	if (!ground(UDAG)) {
		/* fx = fy; does not consider ground terms  */
		if (DAG_symb(NGDAG) == DAG_symb(UDAG)) {
			subs_UDAG = CCFV_unify_args(NGDAG, UDAG);
			if (subs_UDAG) {
				stack_merge(result, subs_UDAG);
				stack_free(subs_UDAG);
			}
		}
		if (
			!get_Findex(DAG_symb(NGDAG), &index) || !index.signatures ||
			!get_Findex(DAG_symb(UDAG), &index_UDAG) || !index_UDAG.signatures)
			return result;
		/* fx = gy; does E-matching of both DAGs to a common term from
         I(f) */
		stack_INIT(subs_UDAG);
		for (i = 0; i < stack_size(index.signatures) &&
				 !hopeless_index(i, index.signatures, true);
				 ++i) {
			term_class = stack_get(index.signatures, i);
			assert(ground(term_class));
			if (!class_has_symbol(term_class, DAG_symb(UDAG))) continue;
			terms_UDAG = find_class_terms(index_UDAG.signatures, term_class);
			if (!terms_UDAG) continue;
			match_DAGs(&subs_UDAG, UDAG, terms_UDAG);
			if (stack_is_empty(subs_UDAG)) {
				/* Ignore all terms in [term] */
				while (i + 1 < stack_size(index.signatures) &&
							 congruent(term_class, stack_get(index.signatures, i + 1)))
					++i;
				stack_free(terms_UDAG);
				continue;
			}
			/* All terms in [term] may yield unifications for NGDAG */
			for (j = i; j < stack_size(index.signatures) &&
					 congruent(term_class, stack_get(index.signatures, j));
					 ++j) {
				/* Continue while in same class */
				subs_NGDAG = CCFV_unify_equality(NGDAG, stack_get(index.signatures, j));
				if (stack_is_empty(subs_NGDAG)) {
					stack_free(subs_NGDAG);
					continue;
				}
				combine_unifiers(&result, subs_UDAG, subs_NGDAG);
			}
			assert(j > i);
			i = j - 1;
			stack_free(terms_UDAG);
			stack_apply(subs_UDAG, unify_free);
			stack_reset(subs_UDAG);
		}
		stack_free(subs_UDAG);
		if (!ccfv_ujobs_off) set_ujob(NGDAG, UDAG, true, result);
		return result;
	}
	/* fx = t */
	assert(DAG_arity(NGDAG) && ground(UDAG));
	stack_INIT(terms);
	/* Do fx=ft regardless of ground model */
	if (DAG_symb(NGDAG) == DAG_symb(UDAG)) stack_push(terms, UDAG);
	/* Retrieve all ft in [t] */
	if (
		get_Findex(DAG_symb(NGDAG), &index) && index.signatures &&
		class_has_symbol(UDAG, DAG_symb(NGDAG))) {
		terms_UDAG = find_class_terms(index.signatures, UDAG);
		/* TODO: Big workaround, which I don't even know if desirable */
		if (ematch_on && index.diseq_terms) {
			Tstack_DAG tmp = find_class_terms(index.diseq_terms, UDAG);
			if (!terms_UDAG)
				terms_UDAG = tmp;
			else if (tmp) {
				stack_merge(terms_UDAG, tmp);
				stack_free(tmp);
			}
		}
		if (terms_UDAG) {
			/* TODO: there is no guarantee that UDAG will be in the index even if
             it is known by CC, since the ground model is minimized */
			/* Remove UDAG so it's not repeated */
			stack_reset(terms);
			stack_merge(terms, terms_UDAG);
			stack_free(terms_UDAG);
		}
	}
	match_DAGs(&result, NGDAG, terms);
	stack_free(terms);
	if (!ccfv_ujobs_off) set_ujob(NGDAG, UDAG, true, result);
	return result;
}

/**
   \brief computes the unifiers reducing a non-ground disequality to a groundly
   asserted one
   \param D0 a term
   \param D1 a term
   \return all unifiers reducing the disequality to one asserted by the ground
   model, if any
   \remark at least one of the inputs is non-ground
   \remark according to the arity and "groundness" of the inputs, as well as the
   user options, there are several different cases, each handled in a specific
   way, as denoted by the comments
   \remark only stores the result in ujobs_index if the result is non-trivial,
   i.e., for all cases but the first */
static Tstack_unifier
CCFV_unify_ineq(TDAG D0, TDAG D1)
{
	unsigned i, j, k;
	TDAG NGDAG, UDAG, fapp_DAG, var_DAG, term_class;
	Tstack_DAG terms, terms_UDAG;
	Findex index, index_UDAG;
	Tunifier unifier;
	Tstack_unifier subs_NGDAG, subs_UDAG, result;
	order_pair(&NGDAG, &UDAG, D0, D1);
	assert(!ground(NGDAG));
#if DEBUG_CCFV > 2
	my_DAG_message("CCFV_unify_ineq: In with {%d,%d}<%D, %D>\n", D0, D1, D0, D1);
#elif DEBUG_EXP
	if (print_exp)
		my_DAG_message(
			"CCFV_unify_ineq: In with {%d,%d}<%D, %D>\n", D0, D1, D0, D1);
#endif
	/* Whether NGDAG != UDAG was already done */
	if (
		!ccfv_ujobs_off && retrieve_ujob(&result, NGDAG, UDAG, false, current_vars))
		return result;
	/* Whether either term has functional ITEs; TODO: Should be done outside */
	if (hopeless_ITE(D0, D1)) return NULL;
	stack_INIT(result);
	/* x != y or x != t as a blocking constraint */
	if (!DAG_arity(NGDAG) && (ground(UDAG) || !DAG_arity(UDAG))) {
		unifier = unify_new(current_vars);
		unify_union_diff(unifier, NGDAG, UDAG);
		stack_push(result, unifier);
		return result;
	}
	/* x != gy or fx != y */
	if (
		!ground(UDAG) &&
		((DAG_arity(NGDAG) && !DAG_arity(UDAG)) ||
		 (!DAG_arity(NGDAG) && DAG_arity(UDAG)))) {
		fapp_DAG = DAG_arity(UDAG) ? UDAG : NGDAG;
		var_DAG = fapp_DAG == UDAG ? NGDAG : UDAG;
		if (!get_Findex(DAG_symb(fapp_DAG), &index) || !index.signatures)
			return result;
		for (i = 0; i < stack_size(index.signatures) &&
				 !hopeless_index(i, index.signatures, false);
				 ++i) {
			/* Term marking class from I(g) being evaluated */
			term_class = stack_get(index.signatures, i);
			terms = CC_diseqs(term_class);
			/* No disequalties for [term] */
			if (!terms) {
				/* Ignore all terms in this class */
				while (i + 1 < stack_size(index.signatures) &&
							 congruent(term_class, stack_get(index.signatures, i + 1)))
					++i;
				continue;
			}
			/* All terms in [term] may yield unifications for fapp_DAG */
			for (j = i; j < stack_size(index.signatures) &&
					 congruent(term_class, stack_get(index.signatures, j));
					 ++j) {
				/* Continue while in same class */
				subs_NGDAG =
					CCFV_unify_equality(fapp_DAG, stack_get(index.signatures, j));
				/* <fapp, term> + <var, terms[i]> */
				if (!stack_is_empty(subs_NGDAG))
					for (k = 0; k < stack_size(terms); ++k) {
						unifier = unify_new(current_vars);
						unify_union(unifier, var_DAG, stack_get(terms, k));
						combine_unifiers_one(&result, subs_NGDAG, unifier);
					}
				stack_apply(subs_NGDAG, unify_free);
				stack_free(subs_NGDAG);
			}
			assert(j > i);
			i = j - 1;
		}
		if (!ccfv_ujobs_off) set_ujob(NGDAG, UDAG, false, result);
		return result;
	}
	if (!get_Findex(DAG_symb(NGDAG), &index) || !index.signatures) return result;
	/* If fx != t, from I(f) get subs [<fx,t'>] s.t t != t'  */
	if (ground(UDAG)) {
		terms = find_class_terms_diseq(index, UDAG);
		if (!terms) {
			if (!ccfv_ujobs_off) set_ujob(NGDAG, UDAG, false, result);
			return result;
		}
		match_DAGs(&result, NGDAG, terms);
		stack_free(terms);
		if (!ccfv_ujobs_off) set_ujob(NGDAG, UDAG, false, result);
		return result;
	}
	/* fx != gy */
	if (!get_Findex(DAG_symb(UDAG), &index_UDAG) || !index_UDAG.signatures)
		return result;
	/* For each class C in I(f) index, match all terms in I(g) different
     from C */
	for (i = 0; i < stack_size(index.signatures) &&
			 !hopeless_index(i, index.signatures, true);
			 ++i) {
		/* Term marking class from I(f) being evaluated */
		term_class = stack_get(index.signatures, i);
		terms_UDAG = find_class_terms_diseq(index_UDAG, term_class);
		/* No terms in I(g) different from [term] */
		if (!terms_UDAG) {
			/* Ignore all terms in this class */
			while (i + 1 < stack_size(index.signatures) &&
						 congruent(term_class, stack_get(index.signatures, i + 1)))
				++i;
			continue;
		}
		/* All terms in [term] may yield unifications for NGDAG */
		for (j = i; j < stack_size(index.signatures) &&
				 congruent(term_class, stack_get(index.signatures, j));
				 ++j) {
			/* Continue while in same class */
			subs_NGDAG = CCFV_unify_equality(NGDAG, stack_get(index.signatures, j));
			/* TODO: organize this better so it's not so redundant. Ujobs? */
			/* <NGDAG, term> + <UDAG, terms_UDAG[i]> */
			if (!stack_is_empty(subs_NGDAG))
				for (k = 0;
						 k < stack_size(terms_UDAG) && !hopeless_index(k, terms_UDAG, true);
						 ++k) {
					subs_UDAG = CCFV_unify_equality(UDAG, stack_get(terms_UDAG, k));
					if (stack_is_empty(subs_UDAG)) {
						stack_free(subs_UDAG);
						continue;
					}
					combine_unifiers(&result, subs_NGDAG, subs_UDAG);
				}
			stack_apply(subs_NGDAG, unify_free);
			stack_free(subs_NGDAG);
		}
		assert(j > i);
		i = j - 1;
		stack_free(terms_UDAG);
	}
	if (!ccfv_ujobs_off) set_ujob(NGDAG, UDAG, false, result);
	return result;
}

/**
   \brief computes the unifiers reducing a non-ground predicate application to a
   groundly asserted one
   \param DAG a non-ground predicate application
   \param pol the predicate application's polarity
   \return all unifiers matching DAG to a predicate application asserted by the
   ground model, according to the polarity, if any */
static Tstack_unifier
CCFV_unify_predicate(TDAG DAG, bool pol)
{
	unsigned i;
	Pindex index;
	Tstack_unifier local_result, result;
#if DEBUG_CCFV > 2
	my_DAG_message("CCFV_unify_pred: In with {%d}<%D>\n", DAG, DAG);
#elif DEBUG_EXP
	if (print_exp)
		my_DAG_message("CCFV_unify_pred: In with {%d}<%D>\n", DAG, DAG);
#endif
	assert(!ground(DAG));
	/* Whether unifications for DAG at given polarity have already been
     searched */
	if (
		!ccfv_ujobs_off && retrieve_ujob(&result, DAG, DAG_NULL, pol, current_vars))
		return result;
	/* Avoid trying unification on indexes with too many terms */
	if (!get_Pindex(DAG_symb(DAG), &index) || !index.signatures[pol]) return NULL;
	stack_INIT(result);
	for (i = 0; i < stack_size(index.signatures[pol]) &&
			 !hopeless_index(i, index.signatures[pol], false);
			 ++i) {
		local_result = CCFV_unify_args(DAG, stack_get(index.signatures[pol], i));
		if (!local_result) continue;
		stack_merge(result, local_result);
		stack_free(local_result);
	}
	if (!ccfv_ujobs_off) set_ujob(DAG, DAG_NULL, pol, result);
	return result;
}

/**
   \brief checks whether ground literals are entailed by ground model
   \param DAG a literal
   \param pol the literal's polarity
   \return true if ground model entails given literal with given polarity, false
   otherwise
   \remark this could be done so that it checks not if a ground literal is
   implied but if it is satisfiable together with ground model. This would
   amount to handle fresh ground terms in CCFV, which has been avoided so far
   for simplicity */
static inline bool
CCFV_entail_ground(TDAG DAG, bool pol)
{
	Tboolean_value bvalue;
#ifdef STATS_CCFV
	if (DAG_symb(DAG) == PREDICATE_EQ) {
		unsigned eq = stats_counter_get(ccfv_undef_terms);
		if (!CC_abstract(DAG_arg0(DAG))) stats_counter_inc(ccfv_undef_terms);
		if (!CC_abstract(DAG_arg1(DAG))) stats_counter_inc(ccfv_undef_terms);
		if (eq == (stats_counter_get(ccfv_undef_terms) + 1))
			stats_counter_inc(ccfv_undef_half_eq);
	}
#endif
	/* Equality */
	if (DAG_symb(DAG) == PREDICATE_EQ && pol)
		return congruent(DAG_arg0(DAG), DAG_arg1(DAG));
	/* Disequality */
	if (DAG_symb(DAG) == PREDICATE_EQ)
		return stack_DAG_contains(CC_negative, DAG);
	bvalue = CC_abstract_p(DAG);
	/* Predicate is fresh */
	if (bvalue == BOOL_UNDEFINED) {
#if DEBUG_CCFV > 1
		my_DAG_message(
			"CCFV_set_p: got undef {%d}%D with value %d\n", DAG, DAG, bvalue);
#endif
#ifdef STATS_CCFV
		stats_counter_inc(ccfv_undef_preds);
#endif
		return false;
	}
	assert(CC_abstract(DAG));
	if ((pol ? (bvalue) == BOOL_TRUE : (bvalue) == BOOL_FALSE)) return true;
	return false;
}

/**
   \brief checks if there are unifiers with which the given literal is entaild
   by the ground model
   \param DAG a non-ground literal
   \param pol the literal's polarity
   \return true if there are such unifiers, false otherwise
   \remark There are three cases, defined by the given literal
   and polarity:
   1) u = v
   - find unifiers that solve the E-unification problem for u and v
   2) u != v
   - find unifiers such that u and v are are reduced to terms asserted disequal
   by the ground model
   3) p(u) or not(p(u))
   - find unifiers for u for which the predicate is asserted by the ground model
   according to the polarity
   \remark Each of the auxiliary E-unification functions used throughout the
   module make have use of the indexed signature table from CC for selecting
   which ground terms should be used in the E-matching sub-problems
   \remark For now the DAG structure itself is used during E-unification, rather
   than a dedicated index (such as fingerprint trees). This could be improved
   eventually
   \remark The search is not complete due to thresholds imposed for performance
   reasons */
static bool
CCFV_unify_lit(TDAG DAG, bool pol)
{
#if DEBUG_CCFV > 1
	my_DAG_message(
		"CCFV_unify_lit: In with {%d} %s%D\n", DAG, pol ? "" : "not", DAG);
#endif
	assert(DAG_literal(DAG) && !DAG_quant(DAG));
	assert(!ground(DAG));
	if (DAG_symb(DAG) == PREDICATE_EQ) {
		if (pol)
			return update_unifiers(CCFV_unify_equality(DAG_arg0(DAG), DAG_arg1(DAG)));
		return update_unifiers(CCFV_unify_ineq(DAG_arg0(DAG), DAG_arg1(DAG)));
	}
	return update_unifiers(CCFV_unify_predicate(DAG, pol));
}

/*
  --------------------------------------------------------------
  Conflicting instances search
  --------------------------------------------------------------
*/

/**

   \brief checks whether it is hopeless* to try finding conflicting
   instantiations for given literal
   \param DAG a literal
   \return true if hopeless, false otherwise
   \remark "hopeless" as of now means quantifier alternation and ground
   literals whose negation is not implied by the ground model */
static bool
hopeless_DAG(TDAG DAG)
{
	if (DAG_symb(DAG) == QUANTIFIER_EXISTS) {
#ifdef STATS_CCFV
		stats_counter_inc(ccfv_stats_hopeless_existential);
#endif
		return true;
	}
	if (DAG_quant(DAG)) return true;
	if (ground(DAG))
		return DAG_symb(DAG) == CONNECTOR_NOT
			? !CCFV_entail_ground(DAG_arg0(DAG), true)
			: !CCFV_entail_ground(DAG, false);
	return false;
}

/**
    \brief checks whether a given clause has conflicting instantiations
    \param clause a non-ground clause
    \return true if the clause is falsifiable, false otherwise
    \remark the search proceeds by attempting to solve the E-unification problem
    for the clause's negation
    \remark if the clause is falsifiable all the respective instantiations are
    stored in the "unifiers" set */
static bool
CCFV_falsify_clause_breadth(Tstack_DAG clause)
{
	unsigned i;
	Tulit ulit;
	/* TODO: global, for now, because of the depth first search. Fix it
     eventually to have this as a parameter */
	/* Tstack_ulit ulits; */
#if DEBUG_CCFV > 1
	my_DAG_message("CCFV_falsify_clause_breadth: In with\n");
	print_Tstack_DAG(clause);
#endif
#if DEBUG_EXP && defined(STATS_CCFV)
	if (
		stats_counter_get(ccfv_stats_rounds) == ccfv_round &&
		(!clause || DAG == clause))
		print_exp = true;
#endif
	/* Collects literals for falsifying, unless trivially hopeless */
	stack_INIT(ulits);
	for (i = 0; i < stack_size(clause); ++i) {
		if (hopeless_DAG(stack_get(clause, i))) {
			stack_free(ulits);
			return false;
		}
		if (ground(stack_get(clause, i))) continue;
		if (DAG_symb(stack_get(clause, i)) == CONNECTOR_NOT) {
			ulit.DAG = DAG_arg0(stack_get(clause, i));
			ulit.pol = true;
		} else {
			ulit.DAG = stack_get(clause, i);
			ulit.pol = false;
		}
		stack_push(ulits, ulit);
	}
	assert(stack_size(ulits));
	/* Initialize variables list */
	set_context_vars(clause);
	/* TODO: what should happen here??? */
	if (stack_is_empty(current_vars)) return false;
	/* Sort unifying DAGs by lesser unification potential (smaller
     indexes); positives kept in front */
	if (ccfv_score) {
		DAG_tmp_reserve();
		for (i = 0; i < stack_size(ulits); ++i) {
			ulit = stack_get(ulits, i);
			if (!DAG_prop_check(ulit.DAG, DAG_PROP_SYMBS))
				set_symbs(ulit.DAG, true, current_vars);
			ulit.score = score_lit(ulit.DAG, ulit.pol);
			stack_set(ulits, i, ulit);
		}
		for (i = 0; i < stack_size(ulits); ++i)
			DAG_tmp_reset_symbs(stack_get(ulits, i).DAG);
		DAG_tmp_release();
#if DEBUG_CCFV > 2
		for (i = 0; i < stack_size(ulits); ++i) {
			ulit = stack_get(ulits, i);
			if (!DAG_prop_check(ulit.DAG, DAG_PROP_SYMBS))
				my_DAG_error("CCFV_clause: could not retrieve DAG symbols\n");
			my_DAG_message(
				"Unifying_DAG: {scores: %d} %s%D\n", ulit.score, ulit.pol ? "" : "not",
				ulit.DAG);
		}
		for (int gambi = 50; gambi--; printf("-"))
			;
		printf("\n\n");
#endif
	}
	/* Succesive unificiation of literals */
	for (i = 0; i < stack_size(ulits); ++i)
		if (!CCFV_unify_lit(stack_get(ulits, i).DAG, stack_get(ulits, i).pol)) {
#if DEBUG_CCFV > 1
			my_DAG_message(
				"CCFV: Failed to unify {%d}%s%D\n\n", stack_get(ulits, i).DAG,
				stack_get(ulits, i).pol ? "" : "not", stack_get(ulits, i).DAG);
			for (int gambi = 50; gambi--; printf("="))
				;
			printf("\n\n");
#endif
			stack_free(ulits);
			return false;
		}
	stack_free(ulits);
#if DEBUG_CCFV > 1
	my_DAG_message("CCFV_falsify_clause_breadth: Unifications for clause:\n");
	print_Tstack_unifier(unifiers);
	my_message_return();
#endif
	return true;
}

/*
  --------------------------------------------------------------
  To search with backtrackable CCFV
  --------------------------------------------------------------
*/

/* TODO: Awful workarounds */
extern int CIs_bound;

int current_component;
Tstack_comp components;

bool
solves_component(Tunifier unifier)
{
	bool result;
	Tstack_constr constraints;
	if ((current_component + 1) == stack_size(components)) {
		stack_push(unifiers, unifier);
		return true;
	}
	current_component++;
	/* my_message("%d, %d\n", current_component, stack_size(components)); */
	assert(((unsigned)current_component) < stack_size(components));
	stack_COPY(constraints, stack_get(components, current_component).constrs);
#if DEBUG_CCFV > 1
	my_message("Entail constraints:\n");
	print_Tstack_constr(constraints);
#endif
	result = CCFV_entail_constraint(unifier, constraints);
	current_component--;
	return result;
}

static bool
CCFV_falsify_clause_comps(Tstack_DAG clause)
{
	unsigned i;
	Tstack_constr constraints;
	Tunifier solution;
	CCFV_bckt_cycle_init(clause, solves_component);
	solution = unify_new(current_vars);
	stack_INIT(constraints);
	for (i = 0; i < stack_size(clause); ++i)
		stack_push(constraints, create_constr_lit(stack_get(clause, i), solution));
	components = sort_constraints(constraints);
	current_component = -1;
	/* Look for solutions for each component sequentially */
	solves_component(solution);
	for (i = 0; i < stack_size(components); ++i)
		stack_free(stack_get(components, i).constrs);
	stack_free(components);
	CCFV_bckt_cycle_done(clause);
#if DEBUG_CCFV > 1
	if (!stack_is_empty(unifiers)) {
		my_DAG_message("CCFV_falsify_components: Unifications for clause:\n");
		print_Tstack_unifier(unifiers);
		my_message_return();
	} else {
		my_DAG_message("CCFV_falsify_components: Failed to falsify clause\n");
		for (int gambi = 50; gambi--; printf("="))
			;
		printf("\n\n");
	}
#endif
	return !stack_is_empty(unifiers);
}

bool
store_unifier(Tunifier unifier)
{
	stack_push(unifiers, unifier);
	return true;
}

/**
    \brief checks whether a given clause has conflicting instantiations
    \param clause a non-ground clause
    \return true if the clause is falsifiable, false otherwise
    \remark the search proceeds by attempting to solve the E-unification problem
    for the clause's negation
    \remark if the clause is falsifiable all the respective instantiations are
    stored in the "unifiers" set */
static bool
CCFV_falsify_clause(Tstack_DAG clause)
{
	unsigned i;
	Tstack_constr constraints;
	Tunifier solution;
#if DEBUG_CCFV > 1
	my_DAG_message("CCFV_falsify_clause: In with\n");
	print_Tstack_DAG(clause);
#endif
	/* TODO: remove from the CNF clauses with nested existentials... */
	for (i = 0; i < stack_size(clause); ++i)
		if (DAG_quant(stack_get(clause, i))) return false;
	/* Initialize variables list */
	set_context_vars(clause);
	/* TODO: what should happen here??? */
	if (stack_is_empty(current_vars)) return false;
	/* Collects literals for falsifying */
	if (!ccfv_comps_off) return CCFV_falsify_clause_comps(clause);
	CCFV_bckt_cycle_init(clause, store_unifier);
	stack_INIT(constraints);
	solution = unify_new(current_vars);
	for (i = 0; i < stack_size(clause); ++i)
		stack_push(constraints, create_constr_lit(stack_get(clause, i), solution));
	/* Succesive unificiation of literals */
	if (!CCFV_entail_constraint(solution, constraints)) {
#if DEBUG_CCFV > 1
		my_DAG_message("CCFV_falsify_clause: Failed to falsify clause\n");
		for (int gambi = 50; gambi--; printf("="))
			;
		printf("\n\n");
#endif
		CCFV_bckt_cycle_done(clause);
		return false;
	}
#if DEBUG_CCFV > 1
	my_DAG_message("CCFV_falsify_clause: Unifications for clause:\n");
	print_Tstack_unifier(unifiers);
	my_message_return();
#endif
	CCFV_bckt_cycle_done(clause);
	return true;
}

/*
  --------------------------------------------------------------
  Interface
  --------------------------------------------------------------
*/

/**
   \brief initializes context dependent information at each instantiation
   cycle. */
void
CCFV_cycle_init()
{
	unsigned i;
	stack_INIT(unifiers);
	stack_INIT(current_vars);
	stack_INIT(grounded_var_classes);
	vars_pos = NULL;
	/* Retrieves disequalities from SAT model */
	stack_INIT(CC_negative);
	for (i = 0; i < SAT_literal_stack_n; ++i)
		if (
			!lit_pol(SAT_literal_stack[i]) &&
			DAG_symb(var_to_DAG(lit_var(SAT_literal_stack[i]))) == PREDICATE_EQ)
			stack_push(CC_negative, var_to_DAG(lit_var(SAT_literal_stack[i])));
	stack_sort(CC_negative, DAG_cmp_q);
#if DEBUG_CCFV > 3
	my_DAG_message("CC_quantified:\n");
	print_Tstack_DAG(CC_quantified);
	my_DAG_message("CC_negative:\n");
	print_Tstack_DAG(CC_negative);
	my_message_return();
#endif
}

/**
   \brief Releases context dependent information at the end of each
   instantiation cycle. */
void
CCFV_cycle_done()
{
	if (!ccfv_ujobs_off) ujobs_done_cycle();
	stack_free(unifiers);
	stack_free(current_vars);
	if (vars_pos) free(vars_pos);
	stack_free(grounded_var_classes);
	stack_free(CC_negative);
}

Tstack_DAGinst
CCFV()
{
	unsigned i, j;
	bool keep_looking;
	TDAG qt_formula;
	Tstack_unifier grounds;
	Tstack_DAGstack* qt_CNF;
	Tstack_DAGinst result;
	CCFV_cycle_init();
	stack_INIT(result);
#ifdef STATS_CCFV
	found_unifiers = 0;
	stats_timer_start(ccfv_stats_time);
	stats_counter_inc(ccfv_stats_rounds);
#endif
	keep_looking = true;
	for (i = 0; i < stack_size(CC_quantified) && keep_looking; ++i) {
		qt_formula = stack_get(CC_quantified, i);
		qt_CNF = (Tstack_DAGstack*)DAG_prop_get(qt_formula, DAG_PROP_CNF);
		if (!qt_CNF) continue;
#if DEBUG_CCFV > 1
		my_DAG_message("CIs: #%d:{%d}%D\n", i, qt_formula, qt_formula);
		print_Tstack_DAGstack(*qt_CNF);
#endif
		/* Each clause may yield instantiations */
		for (j = 0; j < stack_size(*qt_CNF) && keep_looking; ++j) {
			if ((!ccfv_breadth
						 ? CCFV_falsify_clause(stack_get(*qt_CNF, j))
						 : CCFV_falsify_clause_breadth(stack_get(*qt_CNF, j)))) {
				stack_inc(result);
				stack_top(result).qform = qt_formula;
				stack_top(result).clause = stack_get(*qt_CNF, j);
				stack_INIT(stack_top(result).insts);
				while (!stack_is_empty(unifiers)) {
					/* TODO: If it takes one or many or all grounds should be up
                     to options. But it has an effect on checking afterwards: if
                     {x/x} is grounded to {x/a}, {x/b} and only {x/a} is
                     selected, in a next instantiation on this clause with {x/x}
                     it shouldn't be generated an intantiation with {x/a}, under
                     the risk of having an infinite chain, maybe? */
					grounds = unify_ground(stack_pop(unifiers), UINT_MAX, ccfv_all_CI);
					/* TODO: Does this ever happen? Run in debug mode with an
                     assert */
					if (!grounds) {
#if DEBUG_CCFV > 2
						my_DAG_message("CIs: unifier ungroundable\n");
#endif
						stats_counter_inc(ccfv_stats_ungroundable);
						continue;
					}
					/* TODO: this control should not be done here?; Bound
                     controls */
					if (stack_size(grounds) >= CIs_bound) keep_looking = false;
					stack_merge(stack_top(result).insts, grounds);
					stack_free(grounds);
				}
				stack_reset(unifiers);
#ifdef STATS_CCFV
				found_unifiers += stack_size(stack_top(result).insts);
#endif
			}
		}
	}
	CCFV_cycle_done();
#ifdef STATS_CCFV
#if DEBUG_CCFV
	my_DAG_message(
		"\t\t(%2.2fs) CIs %d: got %d unifiers\n",
		stats_timer_get(ccfv_stats_time) - ccfv_time,
		stats_counter_get(ccfv_stats_rounds), found_unifiers);
#endif
	if (ccfv_stats_max_time < (stats_timer_get(ccfv_stats_time) - ccfv_time))
		ccfv_stats_max_time = stats_timer_get(ccfv_stats_time) - ccfv_time;
	ccfv_time = stats_timer_get(ccfv_stats_time);
	stats_timer_stop(ccfv_stats_time);
#endif
	if (stack_is_empty(result)) stack_free(result);
	return result;
}

/*
  --------------------------------------------------------------
  Initialization
  --------------------------------------------------------------
*/

void
CCFV_init()
{
	ccfv_time = 0;
	ccfv_stats_max_time = 0;
	if (!ccfv_ujobs_off) ujobs_init();
	ccfv_propagate = false;
	/* options_new(0, "ccfv-propagate", */
	/*             "Assert single non-falsified literal in clause [unstable]",
                 &ccfv_propagate); */
	ccfv_ujobs_off = false;
	options_new(
		0, "ccfv-ujobs-off", "Turn off prevention of redundant unification jobs",
		&ccfv_ujobs_off);
	ccfv_score = false;
	/* options_new(0, "ccfv-score", */
	/*             "Order literals to be unified through scoring function
                 [Naive]", */
	/*             &ccfv_score); */
	ccfv_all_CI = false;
	/* options_new(0, "ccfv-all-CI", */
	/*             "Only derive CIs in CCFV [stable?]", */
	/*             &ccfv_all_CI); */
	ccfv_breadth = false;
	options_new(
		0, "ccfv-breadth", "Find solutions in breadth-first manner.",
		&ccfv_breadth);

	ccfv_comps_off = false;
	options_new(
		0, "ccfv-comps-off", "Do not split (initial) constraints into components.",
		&ccfv_comps_off);

	options_new_int(
		0, "ccfv-cnf", "Limit to potential number of nodes in CNF", "10^3",
		&ccfv_cnf_threshold);
	ccfv_cnf_threshold = 1000;

	options_new_int(
		0, "ccfv-exp", "Limit to potential number of unifiers", "10^6",
		&ccfv_exp_threshold);
	ccfv_exp_threshold = 1000000;
	options_new_int(
		0, "ccfv-index", "Limit to size of indexes considered in E-uni", "10^3",
		&ccfv_index_threshold);
	ccfv_index_threshold = 1000;
	options_new_int(
		0, "ccfv-index-full",
		"Limit to size of indexes considered in <fx,gy> E-uni", "10^2",
		&ccfv_index_threshold_full);
	ccfv_index_threshold_full = 100;
	ccfv_index_inc = false;
	options_new(
		0, "ccfv-index-inc", "Use indexes up to threshold", &ccfv_index_inc);
#ifdef STATS_CCFV
	ccfv_stats_time =
		stats_timer_new("ccfv_time", "CCFV time", "%7.2f", STATS_TIMER_ALL);
	ccfv_stats_ungroundable = stats_counter_new(
		"ccfv/ungroundable",
		"how many unifiers range over variables with empty sorts", "%6d");
	ccfv_undef_preds = stats_counter_new(
		"ccfv/undef_preds", "how many undef_preds (total)", "%6d");
	ccfv_undef_terms = stats_counter_new(
		"ccfv/undef_terms", "how many undef_terms (total)", "%6d");
	ccfv_undef_half_eq = stats_counter_new(
		"ccfv/undef_halfeq",
		"how many equalities in which only half is undef (total)", "%6d");
	ccfv_stats_rounds = stats_counter_new(
		"ccfv/rounds", "how many rounds of CCFV there were", "%6d");
	ccfv_stats_explode = stats_counter_new(
		"ccfv/exp", "how many times gave up due to combinatorial explosion", "%6d");
	ccfv_stats_hopeless_index = stats_counter_new(
		"ccfv/hpl_idx", "how many times gave up because of index", "%6d");
	ccfv_stats_hopeless_index_full = stats_counter_new(
		"ccfv/hpl_idx_full", "how many times gave up because of index (full E-uni)",
		"%6d");
	ccfv_stats_hopeless_existential = stats_counter_new(
		"ccfv/hpl_exists", "how many times gave up because of index (neg)", "%6d");
#endif
}

void
CCFV_done()
{
	if (!ccfv_ujobs_off) ujobs_done();
	stats_float(
		"ccfv/time_max", "max time CCFV spent in a round ", "%7.2f",
		ccfv_stats_max_time);
}
