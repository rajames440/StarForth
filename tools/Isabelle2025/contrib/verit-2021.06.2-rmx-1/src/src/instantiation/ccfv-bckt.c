#define DEBUG_ASSIGN 0
#define DEBUG_CONSTR 0
#define DEBUG_CCFV_BCKT 0

#include "instantiation/ccfv-bckt.h"

#include "congruence/congruence.h"
#include "instantiation/free-vars.h"
#include "instantiation/inst-index.h"
#include "instantiation/unify.h"
#include "limits.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG.h"
#include "undo.h"
#include "utils/options.h"
#include "utils/statistics.h"

/*
  --------------------------------------------------------------
  User options
  --------------------------------------------------------------
*/

/**
   \addtogroup arguments_developer

   - --ccfv-depth-eager-discard */
bool ccfv_depth_eager_discard;

/**
   \addtogroup arguments_developer

   - --ccfv-depth-order-during */
static bool ccfv_depth_order_during;

/**
   \addtogroup arguments_developer

   - --ccfv-depth-branches=X */
static int ccfv_branches;

/** \brief to control number of branches explored */
static unsigned found_unifiers;
static unsigned branches_open;

/* TODO: Workaround since options are set in inst-man */
extern int CIs_bound;
extern bool ccfv_all_CI;

/*
  --------------------------------------------------------------
  Global state
  --------------------------------------------------------------
*/

static bool (*CCFV_function)(Tunifier unifier) = NULL;

/*
  --------------------------------------------------------------
  Search
  --------------------------------------------------------------
*/

/** Assumes a solution and set of constraints in context */
#define YIELD(r) \
	do { \
		unify_free(solution); \
		stack_free(constraints); \
		return r; \
	} while (0)

static Tstack_constr
build_args_ctrs(Tunifier solution, TDAG D0, TDAG D1)
{
	unsigned i;
	TDAG arg0, arg1;
	Tconstr constr;
	Tstack_constr constraints;
	if (DAG_arity(D0) != DAG_arity(D1)) {
		assert(DAG_sort_arity(DAG_symb_sort(DAG_symb(D0))) == DAG_SORT_NARY);
		assert(DAG_sort_arity(DAG_symb_sort(DAG_symb(D1))) == DAG_SORT_NARY);
		return NULL;
	}
	stack_INIT(constraints);
	assert(DAG_arity(D0) && DAG_symb(D0) == DAG_symb(D1));
	for (i = 0; i < DAG_arity(D0); ++i) {
		if (
			DAG_symb(DAG_arg(D0, i)) == FUNCTION_ITE ||
			DAG_symb(DAG_arg(D1, i)) == FUNCTION_ITE)
			break;
		if (DAG_sort(DAG_arg(D0, i)) != DAG_sort(DAG_arg(D1, i))) break;
		/* Retrieve SIGs of arguments at position, if any */
		if (
			ground_mod(DAG_arg(D0, i)) && ground_mod(DAG_arg(D1, i)) &&
			((ground(DAG_arg(D0, i)) || has_sig(DAG_arg(D0, i))) &&
			 (ground(DAG_arg(D0, i)) || has_sig(DAG_arg(D0, i))))) {
#if DEBUG_CONSTR
			constr = create_constr_eq(DAG_arg(D0, i), DAG_arg(D1, i), solution);
			assert(constr.type == CCFV_GROUND_SIG);
			my_DAG_message("Constraint with unifier\n");
			unify_print(solution);
			my_DAG_message("of <%D,%D>:\n", DAG_arg(D0, i), DAG_arg(D1, i));
			print_constr(UINT_MAX, constr);
#endif
			arg0 = ground(DAG_arg(D0, i)) ? DAG_arg(D0, i)
																		: DAGs_modulo[DAG_arg(D0, i)]->term;
			arg1 = ground(DAG_arg(D1, i)) ? DAG_arg(D1, i)
																		: DAGs_modulo[DAG_arg(D1, i)]->term;
			if (congruent(arg0, arg1)) continue;
			break;
		}
		constr = create_constr_eq(DAG_arg(D0, i), DAG_arg(D1, i), solution);
#if DEBUG_CONSTR
		my_DAG_message("Constraint with unifier\n");
		unify_print(solution);
		my_DAG_message("of <%D,%D>:\n", DAG_arg(D0, i), DAG_arg(D1, i));
		print_constr(UINT_MAX, constr);
#endif
#if DEBUG_CCFV_BCKT > 1
		my_message("build_args_depth: adding constr:\n");
		print_constr(UINT_MAX, constr);
#endif
		stack_push(constraints, constr);
	}
	if (i != DAG_arity(D0)) stack_free(constraints);
	return constraints;
}

/**
   \brief match args of terms with args of each terms from a set, one at a time
   \remark destructive for solution and constraints, non-destructive for
   terms */
bool
match_DAGs_args(
	TDAG DAG, Tstack_DAG terms, Tunifier solution, Tstack_constr constraints)
{
	unsigned i;
	bool result;
	Tstack_constr args_ctrs, cp_ctrs;
	assert(DAG_arity(DAG));
	result = false;
	for (i = 0; i < stack_size(terms); ++i) {
		args_ctrs = build_args_ctrs(solution, DAG, stack_get(terms, i));
		if (!args_ctrs) continue;
		/* TODO: could use args_ctrs directly if order didn't matter... */
		/* TODO: why COPY broken??? */
		/* stack_COPY(cp_ctrs, constraints); */
		stack_INIT(cp_ctrs);
		stack_merge(cp_ctrs, constraints);
		stack_merge(cp_ctrs, args_ctrs);
		stack_free(args_ctrs);
		result = CCFV_entail_constraint(unify_copy(solution), cp_ctrs) || result;
		/* Backtrack control */
		if (needs_backtrack(solution)) BACKTRACK_TO(solution);
		/* Bound controls */
		if (found_unifiers >= CIs_bound || branches_open > ccfv_branches) {
			/* if (branches_open > ccfv_branches) */
			/*   stats_counter_inc(ccfv_stats_depth_above_failed_threshold); */
			break;
		}
	}
	YIELD(result);
}

/** fx != gy */
static bool
CCFV_entail_euni_fapps_diseq(
	TDAG NGDAG, TDAG UDAG, Tunifier solution, Tstack_constr constraints)
{
	unsigned i;
	bool result;
	TDAG term_class;
	Findex index, index_UDAG;
	Tstack_DAG terms_UDAG;
	Tconstr constr_ematch;
	Tstack_constr cp_ctrs;
	assert(
		DAG_arity(NGDAG) && !ground_mod(NGDAG) && DAG_arity(UDAG) &&
		!ground_mod(UDAG));
	if (
		!get_Findex(DAG_symb(NGDAG), &index) || !index.signatures ||
		!get_Findex(DAG_symb(UDAG), &index_UDAG) || !index_UDAG.signatures)
		YIELD(false);
	result = false;
	/* For each class C in I(f), match all terms in I(g) different from C */
	for (i = 0; i < stack_size(index.signatures); ++i) {
		/* Term marking class from I(f) being evaluated */
		term_class = stack_get(index.signatures, i);
		/* Retrieve all gt disequal to term_class */
		terms_UDAG = find_class_terms_diseq(index_UDAG, term_class);
		if (terms_UDAG) {
			/* <fx, term_class> + <y, t>, for each gt != term_class */
			/* stack_COPY(cp_ctrs, constraints); */
			stack_INIT(cp_ctrs);
			stack_merge(cp_ctrs, constraints);
			/* <fx, term_class> */
			constr_ematch = create_constr(NGDAG, term_class, CCFV_EMATCH);
#if DEBUG_CONSTR
			Tconstr constr = create_constr_eq(NGDAG, term_class, solution);
			assert(constr.type == CCFV_EMATCH);
#endif
			stack_push(cp_ctrs, constr_ematch);
			/* <y, t> for each gt */
			result =
				match_DAGs_args(UDAG, terms_UDAG, unify_copy(solution), cp_ctrs) ||
				result;
			stack_free(terms_UDAG);
		}
		/* Backtrack control */
		if (needs_backtrack(solution)) BACKTRACK_TO(solution);
		/* Bound controls */
		if (found_unifiers >= CIs_bound || branches_open > ccfv_branches)
			YIELD(result);
		/* Go to last term in [term_class] */
		while (i + 1 < stack_size(index.signatures) &&
					 congruent(term_class, stack_get(index.signatures, i + 1)))
			++i;
	}
	YIELD(result);
}

/** fx != t */
static bool
CCFV_entail_ematch_diseq(
	TDAG NGDAG, TDAG UDAG, Tunifier solution, Tstack_constr constraints)
{
	bool result;
	Findex index;
	Tstack_DAG terms;
	assert(DAG_arity(NGDAG) && ground(UDAG));
	/* "f" has empty index */
	if (!get_Findex(DAG_symb(NGDAG), &index) || !index.signatures) YIELD(false);
	/* fx != t, retrieve from I(f) all ft' s.t t != ft' */
	terms = find_class_terms_diseq(index, UDAG);
	if (!terms) YIELD(false);
	/* Try <x, t'> for all ft' */
	result = match_DAGs_args(NGDAG, terms, solution, constraints);
	stack_free(terms);
	return result;
}

/** x != gy */
static bool
CCFV_entail_euni_var_diseq(
	TDAG var_DAG, TDAG fapp_DAG, Tunifier solution, Tstack_constr constraints)
{
	unsigned i, j;
	bool result;
	Findex index;
	TDAG term_class;
	Tstack_DAG diseqs;
	Tconstr constr_ematch, constr_assign;
	Tstack_constr cp_ctrs;
	assert(
		unify_is_var(solution, var_DAG) && !ground_mod(fapp_DAG) &&
		DAG_arity(fapp_DAG));
	if (!get_Findex(DAG_symb(fapp_DAG), &index) || !index.signatures)
		YIELD(false);
	result = false;
	for (i = 0; i < stack_size(index.signatures); ++i) {
		/* Term marking class from I(g) being evaluated */
		term_class = stack_get(index.signatures, i);
		diseqs = CC_diseqs(term_class);
		if (diseqs) {
			/* <gy, term_class> + <x, t> for each t in diseqs([term_class]) */
			for (j = 0; j < stack_size(diseqs); ++j) {
				/* stack_COPY(cp_ctrs, constraints); */
				stack_INIT(cp_ctrs);
				stack_merge(cp_ctrs, constraints);
				/* <gy, term_class> */
				constr_ematch = create_constr(fapp_DAG, term_class, CCFV_EMATCH);
				stack_push(cp_ctrs, constr_ematch);
				/* <x, t> */
				constr_assign =
					create_constr(var_DAG, stack_get(diseqs, j), CCFV_ASSIGN);
				stack_push(cp_ctrs, constr_assign);
#if DEBUG_CONSTR
				Tconstr constr0 = create_constr_eq(fapp_DAG, term_class, solution);
				Tconstr constr1 =
					create_constr_eq(var_DAG, stack_get(diseqs, j), solution);
				assert(constr0.type == CCFV_EMATCH && constr1.type == CCFV_ASSIGN);
#endif
				result =
					CCFV_entail_constraint(unify_copy(solution), cp_ctrs) || result;
				/* Backtrack control */
				if (needs_backtrack(solution)) BACKTRACK_TO(solution);
				/* Bound controls */
				if (found_unifiers >= CIs_bound || branches_open > ccfv_branches)
					YIELD(result);
			}
		}
		/* Go to last term in [term_class] */
		while (i + 1 < stack_size(index.signatures) &&
					 congruent(term_class, stack_get(index.signatures, i + 1)))
			++i;
	}
	YIELD(result);
}

/** x != t or x != y */
static bool
CCFV_entail_var_diseq(
	TDAG var_DAG, TDAG UDAG, Tunifier solution, Tstack_constr constraints)
{
	assert(ground(UDAG) || unify_is_var(solution, UDAG));
	if (unify_union_diff(solution, var_DAG, UDAG))
		return CCFV_entail_constraint(solution, constraints);
	YIELD(false);
}

/** fx = gy */
static bool
CCFV_entail_euni_fapps_eq(
	TDAG NGDAG, TDAG UDAG, Tunifier solution, Tstack_constr constraints)
{
	unsigned i;
	bool result;
	TDAG term_class;
	Findex index, index_UDAG;
	Tstack_DAG terms_UDAG;
	Tconstr constr_ematch;
	Tstack_constr args_ctrs, cp_ctrs;
	assert(
		DAG_arity(NGDAG) && !ground_mod(NGDAG) && DAG_arity(UDAG) &&
		!ground_mod(UDAG));
	result = false;
	/* fx = fy; does not consider ground terms  */
	if (DAG_symb(NGDAG) == DAG_symb(UDAG)) {
		args_ctrs = build_args_ctrs(solution, NGDAG, UDAG);
		if (args_ctrs) {
			/* TODO: why COPY broken??? */
			/* stack_COPY(cp_ctrs, constraints); */
			stack_INIT(cp_ctrs);
			stack_merge(cp_ctrs, constraints);
			stack_merge(cp_ctrs, args_ctrs);
			stack_free(args_ctrs);
			result = CCFV_entail_constraint(unify_copy(solution), cp_ctrs);
		}
		/* Backtrack control */
		if (needs_backtrack(solution)) BACKTRACK_TO(solution);
		/* Bound controls */
		if (found_unifiers >= CIs_bound || branches_open > ccfv_branches)
			YIELD(result);
	}
	/* If no possible common term, return whether could syntactically unify */
	if (
		!get_Findex(DAG_symb(NGDAG), &index) || !index.signatures ||
		!get_Findex(DAG_symb(UDAG), &index_UDAG) || !index_UDAG.signatures)
		YIELD(result);
	/* fx = gy; does matching of both DAGs to a common term from I(f) */
	for (i = 0; i < stack_size(index.signatures); ++i) {
		term_class = stack_get(index.signatures, i);
		/* Ignore classes without g applications */
		if (!class_has_symbol(term_class, DAG_symb(UDAG))) continue;
		/* Collect all gt in [term_class] */
		terms_UDAG = find_class_terms(index_UDAG.signatures, term_class);
		if (terms_UDAG) {
			/* <fx, term_class> + <gy, term_class> */
			/* TODO: why COPY not working??? */
			/* stack_COPY(cp_ctrs, constraints); */
			stack_INIT(cp_ctrs);
			stack_merge(cp_ctrs, constraints);
			/* <fx, term_class> */
			constr_ematch = create_constr(NGDAG, term_class, CCFV_EMATCH);
			stack_push(cp_ctrs, constr_ematch);
#if DEBUG_CONSTR
			Tconstr constr = create_constr_eq(NGDAG, term_class, solution);
			assert(constr.type == CCFV_EMATCH);
#endif
			/* <y, t> for each gt */
			result =
				match_DAGs_args(UDAG, terms_UDAG, unify_copy(solution), cp_ctrs) ||
				result;
			stack_free(terms_UDAG);
		}
		/* Backtrack control */
		if (needs_backtrack(solution)) BACKTRACK_TO(solution);
		/* Bound controls */
		if (found_unifiers >= CIs_bound || branches_open > ccfv_branches)
			YIELD(result);
		/* Go to last term in [term_class] */
		while (i + 1 < stack_size(index.signatures) &&
					 congruent(term_class, stack_get(index.signatures, i + 1)))
			++i;
	}
	YIELD(result);
}

/** fx = t */
static bool
CCFV_entail_ematch_eq(
	TDAG NGDAG, TDAG UDAG, Tunifier solution, Tstack_constr constraints)
{
	bool result;
	Findex index;
	Tstack_DAG terms, terms_UDAG;
	assert(
		DAG_arity(NGDAG) && (ground(UDAG) || (ground_mod(UDAG) && !has_sig(UDAG))));
	stack_INIT(terms);
	/* To assure fx = ft will be attempted. There is no guarantee that UDAG will
     be in the index even if it is known by CC, since the ground model is
     minimized */
	if (DAG_symb(NGDAG) == DAG_symb(UDAG)) stack_push(terms, UDAG);
	/* Retrieve all classes in I(f) if [t] pontentially has f applications */
	if (
		class_has_symbol(UDAG, DAG_symb(NGDAG)) &&
		get_Findex(DAG_symb(NGDAG), &index) && index.signatures) {
		/* Get all ft in [t] */
		terms_UDAG = find_class_terms(index.signatures, UDAG);
		if (terms_UDAG) {
			/* TODO: if there is no guarantee that ft will be attempted even if
             it is known by CC why am I removing it here? */
			/* Remove UDAG so it's not repeated */
			stack_reset(terms);
			stack_merge(terms, terms_UDAG);
			stack_free(terms_UDAG);
		}
	}
	result = match_DAGs_args(NGDAG, terms, solution, constraints);
	stack_free(terms);
	return result;
}

/** x = g(y) */
static bool
CCFV_entail_euni_var_eq(
	TDAG var_DAG, TDAG fapp_DAG, Tunifier solution, Tstack_constr constraints)
{
	unsigned i;
	bool result;
	Findex index;
	TDAG term_class;
	Tconstr constr_ematch, constr_assign;
	Tstack_constr cp_ctrs;
	assert(
		unify_is_var(solution, var_DAG) && !ground_mod(fapp_DAG) &&
		DAG_arity(fapp_DAG));
#if DEBUG_ASSIGN
	my_DAG_message("%D occurs in %D\n", var_DAG, fapp_DAG);
#endif
	if (!get_Findex(DAG_symb(fapp_DAG), &index) || !index.signatures)
		YIELD(false);
	result = false;
	for (i = 0; i < stack_size(index.signatures); ++i) {
		term_class = stack_get(index.signatures, i);
		/* <x, term_class> + <fapp, term_class> */
		/* TODO: why COPY broken? */
		/* stack_COPY(cp_ctrs, constraints); */
		stack_INIT(cp_ctrs);
		stack_merge(cp_ctrs, constraints);
		/* <fapp, term_class> */
		constr_ematch = create_constr(fapp_DAG, term_class, CCFV_EMATCH);
		stack_push(cp_ctrs, constr_ematch);
		/* <x, term_class> */
		constr_assign = create_constr(var_DAG, term_class, CCFV_ASSIGN);
		stack_push(cp_ctrs, constr_assign);
#if DEBUG_CONSTR
		Tconstr constr0 = create_constr_eq(fapp_DAG, term_class, solution);
		Tconstr constr1 = create_constr_eq(var_DAG, term_class, solution);
		assert(constr1.type == CCFV_EMATCH && constr1.type == CCFV_ASSIGN);
#endif
		result = CCFV_entail_constraint(unify_copy(solution), cp_ctrs) || result;
		/* Backtrack control */
		if (needs_backtrack(solution)) BACKTRACK_TO(solution);
		/* Bound controls */
		if (found_unifiers >= CIs_bound || branches_open > ccfv_branches)
			YIELD(result);
		/* Go to last term in [term_class] */
		while (i + 1 < stack_size(index.signatures) &&
					 congruent(term_class, stack_get(index.signatures, i + 1)))
			++i;
	}
	YIELD(result);
}

/** x=t or x=y or x = g(y) */
static bool
CCFV_entail_var_eq(
	TDAG var_DAG, TDAG UDAG, Tunifier solution, Tstack_constr constraints)
{
	unsigned i, grounded, rep_var, previous_grounds = solution->ground_vars;
	/* If UDAG is ground, var_DAG is not equal to a ng fapp */
	assert(!ground_mod(UDAG) || unify_is_var(solution, var_DAG));
	stack_reset(grounded_var_classes);
	if (unify_union(solution, var_DAG, UDAG)) {
		/* Set all grounded vars in DAGs_modulo  */
		if (stack_size(grounded_var_classes)) {
			/* check all previously free vars for newly grounded (e.g. x=var_DAG, var_DAG=t; need
             to update x and predecessors); if they have no ground term associated,
             build it (e.g. x=g(var_DAG)) */
#if DEBUG_ASSIGN
			my_DAG_message("Grounded classes from <%D,%D>:\n", var_DAG, UDAG);
#endif
			while (!stack_is_empty(grounded_var_classes)) {
				grounded = stack_pop(grounded_var_classes);
				assert(
					!((previous_grounds >> grounded) & 1u) &&
					unify_ground_var(solution, solution->val[grounded].var));
				rep_var = unify_find(solution, grounded);
				assert(solution->val[rep_var].term);
#if DEBUG_ASSIGN
				my_DAG_message("\tclass of %D\n", solution->val[grounded].var);
#endif
				for (i = 0; i < solution->size; ++i) {
					/* If was already ground or not congruent to grounded var rep */
					if (
						((previous_grounds >> i) & 1u) ||
						unify_find(solution, i) != rep_var)
						continue;
					assert(unify_ground_var(solution, solution->val[i].var));
					if (
						!ground(solution->val[rep_var].term) &&
						has_sig(solution->val[rep_var].term))
						solution->val[rep_var].term =
							DAGs_modulo[solution->val[rep_var].term]->term;
					set_var_modulo(solution->val[i].var, solution->val[rep_var].term);
					/* TODO: without this things break. I DON'T KNOW WHY */
					set_ground_var(solution, var_pos(solution->val[i].var));
				}
			}
#ifdef DEBUG
			/* TODO: This invariant breaks for quantified formulas with more than
             32 variables. Tunifier is not robust for that. */
			for (i = 0; i < solution->size; ++i)
				assert(
					!unify_ground_var(solution, solution->val[i].var) ||
					ground_mod(solution->val[i].var));
#endif
			if (ccfv_depth_order_during) {
#if DEBUG_CCFV_BCKT
				my_message("Previous constraints:\n");
				print_Tstack_constr(constraints);
				for (i = 0; i < stack_size(constraints); ++i)
					update_constr(&stack_get(constraints, i), solution);
				my_message("New constraints:\n");
				print_Tstack_constr(constraints);
				my_message("Ordered constraints:\n");
				stack_sort(constraints, constrs_cmp_q_t_score);
				print_Tstack_constr(constraints);
				my_message("-----------------------------\n");
#else
				for (i = 0; i < stack_size(constraints); ++i)
					update_constr(&stack_get(constraints, i), solution);
				stack_sort(constraints, constrs_cmp_q_t_score);
#endif
			}
			/* TODO: extend this for equality constraints as well, not only predicates */
			if (ccfv_depth_eager_discard && !check_ground_apps(constraints))
				YIELD(false);
		}
		/* TODO: aaaaawful workaround because x=fy may happen, no grounding, and
         necessary updates */
		if (ccfv_depth_order_during && DAG_arity(UDAG) && !ground(UDAG))
			for (i = 0; i < stack_size(constraints); ++i)
				update_constr(&stack_get(constraints, i), solution);
		return CCFV_entail_constraint(solution, constraints);
	}
	YIELD(false);
}

/** t1 != t2 */
static bool
CCFV_entail_ground_diseq(
	TDAG D0, TDAG D1, Tunifier solution, Tstack_constr constraints)
{
	assert(ground(D0) && ground(D1));
	if (CC_disequal(D0, D1)) return CCFV_entail_constraint(solution, constraints);
	YIELD(false);
}

/** t1 = t2 */
static bool
CCFV_entail_ground_eq(
	TDAG D0, TDAG D1, Tunifier solution, Tstack_constr constraints)
{
	assert(ground(D0) && ground(D1));
	if (congruent(D0, D1)) return CCFV_entail_constraint(solution, constraints);
	YIELD(false);
}

/** p(x) */
static bool
CCFV_entail_pred(
	TDAG DAG, bool pol, Tunifier solution, Tstack_constr constraints)
{
	Pindex index;
	/* If no indexed application, fails */
	if (!get_Pindex(DAG_symb(DAG), &index) || !index.signatures[pol])
		YIELD(false);
	/* Try matching with arguments of different applications */
	return match_DAGs_args(DAG, index.signatures[pol], solution, constraints);
}

/** p(t) */
static bool
CCFV_entail_ground_pred(
	TDAG DAG, bool pol, Tunifier solution, Tstack_constr constraints)
{
	assert(ground(DAG));
	Tboolean_value bvalue = CC_abstract_p(DAG);
	/* If predicate is fresh value should be undefined */
	assert(CC_abstract(DAG) || bvalue == BOOL_UNDEFINED);
	if (
		bvalue == BOOL_UNDEFINED ||
		(pol ? (bvalue) != BOOL_TRUE : (bvalue) != BOOL_FALSE))
		YIELD(false);
	return CCFV_entail_constraint(solution, constraints);
}

static bool
CCFV_entail_constraint_type(
	Tconstr constr, Tunifier solution, Tstack_constr constraints)
{
	/* TODO: probably have this handled right after creation */
	if (constr.type == CCFV_UNDEF) YIELD(false);
	/* Handle predicates */
	if (is_predicate(constr)) {
		if (constr.type == CCFV_GROUND_PRED)
			return CCFV_entail_ground_pred(
				constr.D0, constr.pol, solution, constraints);
		assert(constr.type == CCFV_PRED);
		return CCFV_entail_pred(constr.D0, constr.pol, solution, constraints);
	}
	/* t1 = t2 or t1 != t2 with signatures*/
	if (constr.type == CCFV_GROUND_SIG)
		return constr.pol
			? CCFV_entail_ground_eq(constr.D0, constr.D1, solution, constraints)
			: CCFV_entail_ground_diseq(constr.D0, constr.D1, solution, constraints);
	/* x = y or x = t or x != t or x != y */
	if (constr.type == CCFV_ASSIGN)
		return constr.pol
			? CCFV_entail_var_eq(constr.D0, constr.D1, solution, constraints)
			: CCFV_entail_var_diseq(constr.D0, constr.D1, solution, constraints);
	/* t1 = t2 with either fresh (if at only one, it's t1) */
	if (constr.type == CCFV_EMATCH_FRESH)
		return CCFV_entail_ematch_eq(constr.D0, constr.D1, solution, constraints);
	/* fx = t or fx != t */
	if (constr.type == CCFV_EMATCH)
		return constr.pol
			? CCFV_entail_ematch_eq(constr.D0, constr.D1, solution, constraints)
			: CCFV_entail_ematch_diseq(constr.D0, constr.D1, solution, constraints);
	/* x = gx or x != gy */
	if (constr.type == CCFV_EUNI_VAR)
		return constr.pol
			? CCFV_entail_euni_var_eq(constr.D0, constr.D1, solution, constraints)
			: CCFV_entail_euni_var_diseq(constr.D0, constr.D1, solution, constraints);
	/* fx = gy or fx != gy */
	if (constr.type == CCFV_EUNI_FAPP)
		return constr.pol
			? CCFV_entail_euni_fapps_eq(constr.D0, constr.D1, solution, constraints)
			: CCFV_entail_euni_fapps_diseq(
					constr.D0, constr.D1, solution, constraints);
	/* x = fy */
	assert(constr.type == CCFV_ASSIGN_FAPP);
	return CCFV_entail_var_eq(constr.D0, constr.D1, solution, constraints);
}

/**
   \brief retrieves a pending constraint and, according to its structure,
   invokes the respective handling function
   \param solution the current solution
   \param constraints pending constraints
   \return true if all remaining pending constraints are met; false otherwise
   \remark the test of "groundness" will have to account for the current
   solution; probably there shouldn't be DAGs in the constraint, but structures
   which, in themselves, have the groundness information (which would have been
   updated when a given assignement occurred in this branch) */
bool
CCFV_entail_constraint(Tunifier solution, Tstack_constr constraints)
{
	Tconstr constr;
	bool pol;
	TDAG NGDAG, UDAG, var_DAG, fapp_DAG, sig_D0, sig_D1, fresh_D0, fresh_D1;
	NGDAG = UDAG = var_DAG = fapp_DAG = sig_D0 = sig_D1 = fresh_D0 = fresh_D1 =
		DAG_NULL;
	/* Successfully solved all constraints */
	if (stack_is_empty(constraints)) {
		if (!CCFV_function(unify_copy(solution))) YIELD(false);
		++found_unifiers;
		YIELD(true);
	}
	/* TODO: this is where things will be optimized: does not need to get top,
     but whoever has lesser score */
	constr = stack_pop(constraints);
#if DEBUG_CCFV_BCKT > 1
	my_message("CCFV_entail_constraint: in with constr\n");
	print_constr(UINT_MAX, constr);
	my_message("and solution and remaining ctrs:\n");
	unify_print(solution);
	print_Tstack_constr(constraints);
#endif
	pol = constr.pol;

	/* TODO: this should have been removed before */
	if (
		DAG_symb(constr.D0) == FUNCTION_ITE || DAG_symb(constr.D1) == FUNCTION_ITE)
		YIELD(false);
	if (ccfv_depth_order_during)
		return CCFV_entail_constraint_type(constr, solution, constraints);
	/* Handle predicates */
	if (is_predicate(constr)) {
		if (ground_mod(constr.D0)) {
			/* Fresh ground predicates are undefined, thus cannot be entailed */
			if (!ground(constr.D0) && !has_sig(constr.D0)) YIELD(false);
			UDAG = ground(constr.D0) ? constr.D0 : DAGs_modulo[constr.D0]->term;
			return CCFV_entail_ground_pred(UDAG, pol, solution, constraints);
		}
		return CCFV_entail_pred(constr.D0, constr.pol, solution, constraints);
	}
	/* t1 = t2 or t1 != t2 */
	if (ground_mod(constr.D0) && ground_mod(constr.D1)) {
		/* If term has FVs and SIG, retrieve it, otherwise term itself unless
         variable */
		if (!ground(constr.D0)) {
			if (has_sig(constr.D0))
				sig_D0 = DAGs_modulo[constr.D0]->term;
			else
				fresh_D0 =
					DAG_arity(constr.D0) ? constr.D0 : DAGs_modulo[constr.D0]->term;
		} else
			sig_D0 = constr.D0;
		if (!ground(constr.D1)) {
			if (has_sig(constr.D1))
				sig_D1 = DAGs_modulo[constr.D1]->term;
			else
				fresh_D1 =
					DAG_arity(constr.D1) ? constr.D1 : DAGs_modulo[constr.D1]->term;
		} else
			sig_D1 = constr.D1;
		/* sig/fresh are exclusive conditions */
		assert(
			(!sig_D0 || !fresh_D0) && (sig_D0 || fresh_D0) &&
			(!sig_D1 || !fresh_D1) && (sig_D1 || fresh_D1));
		/* either both have sigs, both fresh or alternated */
		assert(
			(sig_D0 && sig_D1) || (fresh_D0 && fresh_D1) || (fresh_D0 && sig_D1) ||
			(fresh_D1 && sig_D0));
		/* If sigs, just check with ground model, otherwise try E-matching args */
		if (pol) {
			if (sig_D0 && sig_D1)
				return CCFV_entail_ground_eq(sig_D0, sig_D1, solution, constraints);
			return fresh_D0
				? CCFV_entail_ematch_eq(
						fresh_D0, fresh_D1 ? fresh_D1 : sig_D1, solution, constraints)
				: CCFV_entail_ematch_eq(fresh_D1, sig_D0, solution, constraints);
		}
		/* TODO: simplify this in terms of sig/fresh? */
		/* constr.DAG is a var, leave to be handled later */
		if (!ccfv_all_CI && !ground(constr.D0) && !DAG_arity(constr.D0)) {
			/* TODO: really do this? */
			if (fresh_D1) YIELD(false);
			return CCFV_entail_var_diseq(constr.D0, sig_D1, solution, constraints);
		} else if (!ccfv_all_CI && !ground(constr.D1) && !DAG_arity(constr.D1)) {
			if (fresh_D0) YIELD(false);
			return CCFV_entail_var_diseq(constr.D1, sig_D0, solution, constraints);
		}
		/* If either grounded is fresh, constraint can't be entailed */
		if (!sig_D0 || !sig_D1) YIELD(false);
		return CCFV_entail_ground_diseq(sig_D0, sig_D1, solution, constraints);
	}
	ORDER_CONSTRAINT(NGDAG, UDAG, constr.D0, constr.D1);
#ifdef DEBUG
	/* fx = y or fx != y can't happen due to constraint ordering */
	assert(!(DAG_arity(NGDAG) && !ground_mod(UDAG) && !DAG_arity(UDAG)));

	TDAG rep_NGDAG = unify_find_DAG(solution, NGDAG);
	TDAG rep_UDAG = unify_find_DAG(solution, UDAG);
	if (!DAG_arity(NGDAG))
		assert(
			unify_is_var(solution, NGDAG) ||
			(DAG_arity(rep_NGDAG) && !ground_mod(rep_NGDAG)));
	if (!ground_mod(UDAG) && !DAG_arity(UDAG))
		assert(
			unify_is_var(solution, UDAG) ||
			(DAG_arity(rep_UDAG) && !ground_mod(rep_UDAG)));

#endif
	if (pol) {
		/* TODO: put this in another function... */
		/* x = y or x = t */
		if (!DAG_arity(NGDAG) && (ground_mod(UDAG) || !DAG_arity(UDAG))) {
			/* get sig or fresh DAG */
			if (ground_mod(UDAG)) {
				UDAG = ground(UDAG)
					? UDAG
					: ((has_sig(UDAG) || !DAG_arity(UDAG)) ? DAGs_modulo[UDAG]->term
																								 : UDAG);
				if (unify_is_var(solution, NGDAG))
					return CCFV_entail_var_eq(NGDAG, UDAG, solution, constraints);
				return CCFV_entail_ematch_eq(
					unify_find_DAG(solution, NGDAG), UDAG, solution, constraints);
			}
			/* UDAG is some var y */
			/* x has no fz in its class */
			if (unify_is_var(solution, NGDAG)) {
				/* x = y and y = f(... x ...) */
				if (
					!unify_is_var(solution, UDAG) && unify_occurs(solution, NGDAG, UDAG))
					return CCFV_entail_euni_var_eq(
						NGDAG, unify_find_DAG(solution, UDAG), solution, constraints);
				return CCFV_entail_var_eq(NGDAG, UDAG, solution, constraints);
			}
			/* y has some gz' in its class */
			if (!unify_is_var(solution, UDAG))
				return CCFV_entail_euni_fapps_eq(
					unify_find_DAG(solution, NGDAG), unify_find_DAG(solution, UDAG),
					solution, constraints);
			/* x = y and x = g(...y...) */
			if (unify_occurs(solution, UDAG, NGDAG))
				return CCFV_entail_euni_var_eq(
					UDAG, unify_find_DAG(solution, NGDAG), solution, constraints);
			return CCFV_entail_euni_var_eq(UDAG, NGDAG, solution, constraints);
		}
		/* x = gy */
		if (!DAG_arity(NGDAG) && !ground_mod(UDAG)) {
			assert(DAG_arity(UDAG));
			if (!unify_is_var(solution, NGDAG))
				return CCFV_entail_euni_fapps_eq(
					unify_find_DAG(solution, NGDAG), UDAG, solution, constraints);
			if (unify_occurs(solution, NGDAG, UDAG))
				return CCFV_entail_euni_var_eq(NGDAG, UDAG, solution, constraints);
			return CCFV_entail_var_eq(NGDAG, UDAG, solution, constraints);
		}
		/* fx = t */
		assert(DAG_arity(NGDAG));
		if (ground_mod(UDAG)) {
			UDAG = ground(UDAG)
				? UDAG
				: ((has_sig(UDAG) || !DAG_arity(UDAG)) ? DAGs_modulo[UDAG]->term
																							 : UDAG);
			return CCFV_entail_ematch_eq(NGDAG, UDAG, solution, constraints);
		}
		/* fx = gy */
		assert(DAG_arity(UDAG));
		return CCFV_entail_euni_fapps_eq(NGDAG, UDAG, solution, constraints);
	}
	/* TODO: only call if y is true var */
	/* x != y or x != t as a blocking constraint */
	if (!DAG_arity(NGDAG) && (ground_mod(UDAG) || !DAG_arity(UDAG))) {
		if (ground_mod(UDAG)) {
			/* UDAG is fresh */
			if (!ground(UDAG) && !has_sig(UDAG)) YIELD(false);
			UDAG = ground(UDAG) ? UDAG : DAGs_modulo[UDAG]->term;
			if (unify_is_var(solution, NGDAG))
				return CCFV_entail_var_diseq(NGDAG, UDAG, solution, constraints);
			return CCFV_entail_ematch_diseq(
				unify_find_DAG(solution, NGDAG), UDAG, solution, constraints);
		}
		/* UDAG is some var y */
		/* x has no fz in its class */
		if (unify_is_var(solution, NGDAG)) {
			/* y = gz' */
			if (!unify_is_var(solution, UDAG))
				return CCFV_entail_euni_var_diseq(
					NGDAG, unify_find_DAG(solution, UDAG), solution, constraints);
			return CCFV_entail_var_diseq(NGDAG, UDAG, solution, constraints);
		}
		/* y has some gz' in its class */
		if (!unify_is_var(solution, UDAG))
			return CCFV_entail_euni_fapps_diseq(
				unify_find_DAG(solution, NGDAG), unify_find_DAG(solution, UDAG),
				solution, constraints);
		return CCFV_entail_euni_var_diseq(
			UDAG, unify_find_DAG(solution, NGDAG), solution, constraints);
	}
	/* x != gy */
	assert(DAG_arity(NGDAG) || ground_mod(UDAG) || DAG_arity(UDAG));
	if (!DAG_arity(NGDAG) && !ground_mod(UDAG)) {
		if (unify_is_var(solution, NGDAG))
			return CCFV_entail_euni_var_diseq(NGDAG, UDAG, solution, constraints);
		return CCFV_entail_euni_fapps_diseq(
			unify_find_DAG(solution, NGDAG), UDAG, solution, constraints);
	}
	/* fx != t */
	if (ground_mod(UDAG)) {
		if (!ground(UDAG) && !has_sig(UDAG)) YIELD(false);
		UDAG = ground(UDAG) ? UDAG : DAGs_modulo[UDAG]->term;
		return CCFV_entail_ematch_diseq(NGDAG, UDAG, solution, constraints);
	}
	/* fx != gy */
	return CCFV_entail_euni_fapps_diseq(NGDAG, UDAG, solution, constraints);
}

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

/**
   \brief */
void
CCFV_bckt_cycle_init(Tstack_DAG lits, bool (*f)(Tunifier))
{
	found_unifiers = 0;
	branches_open = 0;
	init_undo_lvl = undo_level;
	undo_level_new();
	CCFV_function = f;
	stack_apply(lits, set_DAGs_modulo);
}

/**
   \brief */
void
CCFV_bckt_cycle_done(Tstack_DAG lits)
{
	/* [TOOD] I don't think that I should assert this because of cases like
     "L={x=a, x=b}". There are no branching rules to delete levels */
	/* assert(undo_level == init_undo_lvl + 1); */
	undo_level_del_to_level(init_undo_lvl);
	stack_apply(lits, unset_DAGs_modulo);
}

void
CCFV_bckt_init(void)
{
	CCFV_mod_init();

	ccfv_depth_eager_discard = false;
	/* options_new(0, "ccfv-depth-eager-discard", */
	/*             "Backtrack as soon as predicate arg grounded into fresh app", */
	/*             &ccfv_depth_eager_discard); */

	ccfv_depth_order_during = false;
	/* options_new(0, "ccfv-depth-order-during", */
	/*             "Order constraints in branch after each assignment. [unstable]", */
	/*             &ccfv_depth_order_during); */

	options_new_int(
		0, "ccfv-branches",
		"Limit max number of branches explored in backtrackable search", "UNIT_MAX",
		&ccfv_branches);
	ccfv_branches = UINT_MAX;
}

void
CCFV_bckt_done(void)
{
	CCFV_mod_done();
}
