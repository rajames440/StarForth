#include "instantiation/inst-trigger-selection.h"

#include "instantiation/free-vars.h"
#include "pre/qnt-trigger.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/polarities.h"
#include "utils/options.h"
#include "utils/statistics.h"

/*
  --------------------------------------------------------------
  Options
  --------------------------------------------------------------
*/

/**
   \addtogroup arguments_developer

   - --triggers-nested-off

   Don't collect nested triggers */
bool triggers_nested_off;

/**
   \addtogroup arguments_developer

   - --triggers-new

   Uses new triggers (include nested ones). */
bool triggers_new;

/**
   \addtogroup arguments_developer

   - --

    */
static bool ignore_interpreted;

/**
   \addtogroup arguments_developer

   - --

    */
static bool rm_more_specific_triggers;

/**
   \addtogroup arguments_developer

   - --

    */
static bool rm_subterm_triggers;

/**
   \addtogroup arguments_developer

   - --

    */
static bool multi_triggers_off;

/*
  --------------------------------------------------------------
  Stats
  --------------------------------------------------------------
*/

static unsigned triggers_sel_stats_time;

/*
  --------------------------------------------------------------
  Synctatic matching infrastructure
  --------------------------------------------------------------
*/

static Tstack_DAG current_vars;

static unsigned bvars;
static unsigned var_offset;
static unsigned* var_indices;

#define var_ind(v) var_indices[v - var_offset]

typedef struct TSval
{
	TDAG var; /*< var DAG */
	TDAG term; /*< term var is equal to; default is DAG_NULL */
} TSval;

/**
   \brief synctatic matcher over fixed set of variables
   \remark the set of variables is static, sorted and fixed
   \remark assigned_vars is a bitmask for the assignments of the matcher:
   whenever a variable is assigned to a term its position in the mask is set,
   s.t. a matcher is assigned iff (assigned_vars = 2^{size} - 1). This should
   only be used if no more than 32 variables in the unifier (for overcoming this
   one should use a bitset) */
typedef struct TSmatcher
{
	unsigned size;
	unsigned assigned_vars;
	TSval val[];
} * TSmatcher;

TSstack(_Smatcher, TSmatcher); /* typedefs Tstack_Smatcher */

#define VAR_INDEX_NOT_FOUND UINT_MAX

/**
   \brief get index of variable in ordered global set of variables
   \param unfier a unifier
   \param var a variable
   \return the var index or UNIFY_VAR_INDEX_NOT_FOUND if not found */
static inline unsigned
var_index(TDAG var)
{
	/* PF,HB: int is mandatory: we can have -1 either with size = 0 or
     imid = 0 and imax gets imid - 1 */
	int imid, imin = 0, imax = stack_size(current_vars) - 1;
	while (imin <= imax) {
		imid = (imax + imin) / 2;
		if (var == stack_get(current_vars, imid)) return imid;
		if (var < stack_get(current_vars, imid))
			imax = imid - 1;
		else if (var > stack_get(current_vars, imid))
			imin = imid + 1;
	}
	return VAR_INDEX_NOT_FOUND;
}

static inline TSmatcher
matcher_new(Tstack_DAG vars)
{
	unsigned i;
	TSmatcher matcher;
	assert(stack_size(vars));
	/* HB Remember the memory alignment */
	MY_MALLOC(matcher, sizeof(TSmatcher) + stack_size(vars) * sizeof(TSval));
	matcher->size = stack_size(vars);
	matcher->assigned_vars = 0;
	for (i = 0; i < matcher->size; ++i) {
		assert(!i || stack_get(vars, i - 1) < stack_get(vars, i));
		matcher->val[i].var = stack_get(vars, i);
		matcher->val[i].term = DAG_NULL;
	}
	return matcher;
}

static inline TSmatcher
matcher_copy(TSmatcher matcher)
{
	unsigned i;
	TSmatcher new_matcher;
	assert(matcher);
	MY_MALLOC(new_matcher, sizeof(TSmatcher) + matcher->size * sizeof(TSval));
	new_matcher->size = matcher->size;
	new_matcher->assigned_vars = matcher->assigned_vars;
	for (i = 0; i < matcher->size; ++i) {
		new_matcher->val[i].var = matcher->val[i].var;
		new_matcher->val[i].term = matcher->val[i].term;
	}
	return new_matcher;
}

static inline void
matcher_free(TSmatcher matcher)
{
	assert(matcher);
	free(matcher);
}

static inline bool
matcher_union(TSmatcher matcher, TDAG D0, TDAG D1)
{
	unsigned ind;
	assert(!DAG_arity(D0) && DAG_symb_type(DAG_symb(D0)) & SYMB_VARIABLE);
	assert(var_index(D0) != VAR_INDEX_NOT_FOUND);
	ind = var_index(D0);
	if (!matcher->val[ind].term) {
		matcher->val[ind].term = D1;
		return true;
	}
	return matcher->val[ind].term == D1;
}

/*
  --------------------------------------------------------------
  Trigger subsumption
  --------------------------------------------------------------
*/

/* TODO: Use DAG_tmp to traverse as DAG rather than as a tree? */
/* TODO: Only check in dest if vars of src contained in vars of dest? */
static bool
subterm(TDAG src, TDAG dest)
{
	unsigned i;
	if (src == dest) return true;
	for (i = 0; i < DAG_arity(dest); ++i)
		if (subterm(src, DAG_arg(dest, i))) return true;
	return false;
}

static bool
is_contained(Tstack_DAG src, unsigned index, Tstack_DAG dest)
{
	unsigned i;
	/* All terms in src are subterms */
	if (index == stack_size(src)) {
#if DEBUG_TRIGGERS_SEL > 2
		my_message("is_contained: \n");
		print_Tstack_DAG(src);
		my_message("is_contained: in\n");
		print_Tstack_DAG(dest);
#endif
		return true;
	}

	for (i = 0; i < stack_size(dest); ++i) {
		if (
			subterm(stack_get(src, index), stack_get(dest, i)) &&
			is_contained(src, index + 1, dest))
			return true;
	}
	return false;
}

static bool
matches(TSmatcher matcher, TDAG D0, TDAG D1)
{
	unsigned i;
	if (DAG_symb_type(DAG_symb(D0)) & SYMB_VARIABLE)
		return matcher_union(matcher, D0, D1);
	if (DAG_symb(D0) == DAG_symb(D1)) {
		i = 0;
		while (i < DAG_arity(D0) &&
					 matches(matcher, DAG_arg(D0, i), DAG_arg(D1, i)))
			++i;
		return i == DAG_arity(D0);
	}
	return false;
}

static bool
subsumes(
	TSmatcher solution, Tstack_DAG src, unsigned index, Tstack_DAG dest,
	unsigned available)
{
	unsigned i;
	TSmatcher bkp_solution;
	/* All terms in src matched */
	if (index == stack_size(src)) {
#if DEBUG_TRIGGERS_SEL > 2
		my_message("subsumption: from\n");
		print_Tstack_DAG(src);
		my_message("subsumption: to\n");
		print_Tstack_DAG(dest);
#endif
		matcher_free(solution);
		return true;
	}
	for (i = 0; i < stack_size(dest); ++i) {
		if (!(available & (1 << i))) continue;
		bkp_solution = matcher_copy(solution);
		if (matches(bkp_solution, stack_get(src, index), stack_get(dest, i))) {
			if (!subsumes(bkp_solution, src, index + 1, dest, available - (1 << i)))
				continue;
			matcher_free(solution);
			return true;
		}
		matcher_free(bkp_solution);
	}
	matcher_free(solution);
	return false;
}

static void
clean_multi_triggers(Tstack_DAGstack* multi_triggers)
{
	unsigned i, j;
	bool* good;
	Tstack_DAG src, dest;
	Tstack_DAGstack result;
	if (rm_more_specific_triggers) {
		MY_MALLOC(good, stack_size(*multi_triggers) * sizeof(bool));
		memset(good, true, stack_size(*multi_triggers) * sizeof(bool));
		/* Test each trigger against all remaining good ones for subsumption*/
		/* TODO: should jump !good[i] also? */
		for (i = 0; i < stack_size(*multi_triggers); ++i)
			for (j = 0; j < stack_size(*multi_triggers); ++j) {
				dest = stack_get(*multi_triggers, i);
				src = stack_get(*multi_triggers, j);
				/* TODO: check if having same size is indeed a requirement */
				if (j == i || !good[j] || stack_size(src) != stack_size(dest)) continue;
				if (subsumes(
							matcher_new(current_vars), src, 0, dest,
							(1 << stack_size(dest)) - 1)) {
					good[i] = false;
					break;
				}
			}
		/* Test each trigger against all remaining good ones for being
         contained */
		if (rm_subterm_triggers)
			for (i = 0; i < stack_size(*multi_triggers); ++i) {
				for (j = 0; j < stack_size(*multi_triggers); ++j) {
					dest = stack_get(*multi_triggers, i);
					src = stack_get(*multi_triggers, j);
					/* TODO: check if having same size is indeed a requirement */
					if (j == i || !good[j] || stack_size(src) != stack_size(dest))
						continue;
					if (is_contained(src, 0, dest)) {
						good[i] = false;
						break;
					}
				}
			}
		/* Getting still standnig triggers */
		stack_INIT(result);
		for (i = 0; i < stack_size(*multi_triggers); ++i)
			if (good[i]) {
				stack_inc(result);
				stack_INIT(stack_top(result));
				stack_merge(stack_top(result), stack_get(*multi_triggers, i));
			}
		stack_apply(*multi_triggers, stack_free);
		stack_reset(*multi_triggers);
		stack_merge(*multi_triggers, result);
		stack_free(result);
		free(good);
	}
	for (i = 0; i < stack_size(*multi_triggers); ++i)
		stack_apply(stack_get(*multi_triggers, i), DAG_dup);
}

/**
   \brief remove redundant single-triggers
   \param single_triggers a set of terms/predicates
   \param vars the set of variables
   \return the set of non-redundant single triggers
   \remark "redundant" is defined as a term being contained in another trigger,
   or vice-versa */
static void
clean_single_triggers(Tstack_DAG* single_triggers)
{
	unsigned i, j;
	bool* good;
	Tstack_DAG result;
	TSmatcher current_matcher;
	/* Removing triggers that are matchable by another */
	if (rm_more_specific_triggers) {
		MY_MALLOC(good, stack_size(*single_triggers) * sizeof(bool));
		memset(good, true, stack_size(*single_triggers) * sizeof(bool));
		for (i = 0; i < stack_size(*single_triggers); ++i) {
			for (j = 0; j < stack_size(*single_triggers); ++j) {
				if (j == i || !good[j]) continue;
				current_matcher = matcher_new(current_vars);
				if (matches(
							current_matcher, stack_get(*single_triggers, j),
							stack_get(*single_triggers, i))) {
					matcher_free(current_matcher);
					good[i] = false;
					break;
				} else
					matcher_free(current_matcher);
			}
		}
		/* Getting still standnig triggers */
		stack_INIT(result);
		for (i = 0; i < stack_size(*single_triggers); ++i)
			if (good[i]) stack_push(result, stack_get(*single_triggers, i));
		stack_reset(*single_triggers);
		stack_merge(*single_triggers, result);
		stack_free(result);
		free(good);
	}
	stack_apply(*single_triggers, DAG_dup);
}

/*
  --------------------------------------------------------------
  Creating candidate triggers (new)
  --------------------------------------------------------------
*/

#define mask DAG_tmp_unsigned

static inline void
set_mask(TDAG DAG)
{
	unsigned i;
	assert(!ground(DAG));
	mask[DAG] = 0;
	for (i = 0; i < stack_size(get_fvars(DAG)); ++i)
		mask[DAG] |= 1 << var_ind(stack_get(get_fvars(DAG), i));
}

typedef struct Tcand_trigger
{
	unsigned free_vars;
	Tstack_DAG var_to_terms;
	Tstack_unsigned term_to_vars;
	Tstack_DAG terms;
} * Tcand_trigger;

TSstack(_cand_trigger, Tcand_trigger); /* typedefs Tstack_cand_trigger */

/* returns true if not redundant and therefore branch should remain, false
   otherwise, indicating that branch should be discarded */
static inline bool
add_term(Tcand_trigger trigger, TDAG DAG)
{
	unsigned i, j, owned;
	TDAG var, term;
	assert(trigger->terms);
	/* add term to trigger, make term own its vars */
	stack_push(trigger->terms, DAG);
	stack_push(trigger->term_to_vars, mask[DAG]);
	/* update variables of trigger */
	trigger->free_vars |= mask[DAG];
	/* for each variable in new term, make it point to new term and remove it from
     owned vars of previous term, if any */
	for (i = 0; i < stack_size(get_fvars(DAG)); ++i) {
		var = stack_get(get_fvars(DAG), i);
		term = stack_get(trigger->var_to_terms, var_ind(var));
		/* remove var from owned vars of term */
		if (term)
			for (j = 0; j < stack_size(trigger->terms); ++j)
				if (stack_get(trigger->terms, j) == term) {
					assert(stack_get(trigger->term_to_vars, j) >= var_ind(var));
					owned = stack_get(trigger->term_to_vars, j) - (1 << var_ind(var));
					/* if term owns no vars, trigger is redundant */
					if (!owned) return false;
					stack_set(trigger->term_to_vars, j, owned);
				}
		/* var points to new term */
		stack_set(trigger->var_to_terms, var_ind(var), DAG);
	}
	return true;
}

static inline Tcand_trigger
cand_trigger_new(void)
{
	unsigned i;
	Tcand_trigger trigger;
	/* HB Remember the memory alignment */
	MY_MALLOC(trigger, 4 * sizeof(Tcand_trigger));
	trigger->free_vars = 0;
	stack_INIT(trigger->var_to_terms);
	for (i = 0; i < stack_size(current_vars); ++i) {
		stack_inc(trigger->var_to_terms);
		stack_top(trigger->var_to_terms) = DAG_NULL;
	}
	stack_INIT(trigger->term_to_vars);
	stack_INIT(trigger->terms);
	return trigger;
}

static inline Tcand_trigger
cand_trigger_copy(Tcand_trigger trigger)
{
	Tcand_trigger new_trigger;
	MY_MALLOC(new_trigger, 4 * sizeof(Tcand_trigger));
	new_trigger->free_vars = trigger->free_vars;
	stack_INIT(new_trigger->var_to_terms);
	stack_merge(new_trigger->var_to_terms, trigger->var_to_terms);
	stack_INIT(new_trigger->term_to_vars);
	stack_merge(new_trigger->term_to_vars, trigger->term_to_vars);
	stack_INIT(new_trigger->terms);
	stack_merge(new_trigger->terms, trigger->terms);
	/* stack_COPY(new_trigger->var_to_terms, trigger->var_to_terms); */
	/* stack_COPY(new_trigger->term_to_vars, trigger->term_to_vars); */
	/* stack_COPY(new_trigger->terms, trigger->terms); */
	return new_trigger;
}

static inline void
cand_trigger_free(Tcand_trigger trigger)
{
	stack_free(trigger->var_to_terms);
	stack_free(trigger->term_to_vars);
	stack_free(trigger->terms);
	free(trigger);
}

static Tstack_cand_trigger found_triggers;

static void
combine_candidates(
	unsigned index, Tstack_DAG candidates, Tcand_trigger trigger_to_be)
{
	unsigned i;
	TDAG term;
	Tcand_trigger tmp_trigger;
	for (i = index; i < stack_size(candidates); ++i) {
		term = stack_get(candidates, i);
		/* adding can only happen if DAG has vars not yet in trigger */
		if ((trigger_to_be->free_vars & mask[term]) == mask[term]) continue;
		tmp_trigger = cand_trigger_copy(trigger_to_be);
		if (!add_term(tmp_trigger, term)) {
			/* trigger became non-parsimonious */
			cand_trigger_free(tmp_trigger);
			continue;
		}
		/* trigger complete */
		if (tmp_trigger->free_vars == bvars) {
			stack_push(found_triggers, tmp_trigger);
			continue;
		}
		/* combine with next candidates */
		combine_candidates(i + 1, candidates, tmp_trigger);
	}
	cand_trigger_free(trigger_to_be);
}

/**
   \brief searches for triggers for a set of variables among given candidates
   \param bound_vars a set of variables
   \param candidates a set of non-ground terms/predicate applications
   \return a non-empty set of triggers, if any, NULL otherwise */
Tstack_DAGstack
set_triggers_qnt(Tstack_DAG candidates)
{
	unsigned i;
	Tstack_DAGstack triggers = NULL;
	assert(stack_size(current_vars));
	/* Avoid spurious repetitions */
	stack_sort(candidates, DAG_cmp_q);
	stack_uniq(candidates);
#if DEBUG_TRIGGERS_SEL > 1
	my_message("Candidates:\n");
	print_Tstack_DAG(candidates);
#endif
	if (multi_triggers_off) {
		clean_single_triggers(&candidates);
		stack_INIT(triggers);
		while (!stack_is_empty(candidates)) {
			stack_inc(triggers);
			stack_INIT(stack_top(triggers));
			stack_push(stack_top(triggers), stack_pop(candidates));
		}
		stack_free(candidates);
		if (stack_is_empty(triggers)) stack_free(triggers);
		return triggers;
	}
	/* Set indices for vars masks */
	bvars = (1 << stack_size(current_vars)) - 1u;
	var_offset = stack_get(current_vars, 0);
	MY_MALLOC(
		var_indices, (stack_top(current_vars) - var_offset + 1) * sizeof(unsigned));
	for (i = 0; i < stack_size(current_vars); ++i)
		var_ind(stack_get(current_vars, i)) = i;
	DAG_tmp_reserve();
	stack_apply(candidates, set_mask);
	stack_INIT(found_triggers);
	combine_candidates(0, candidates, cand_trigger_new());
	free(var_indices);
	for (i = 0; i < stack_size(candidates); ++i)
		mask[stack_get(candidates, i)] = 0;
	stack_free(candidates);
	DAG_tmp_release();
	/* Build triggers */
	stack_INIT(triggers);
	for (i = 0; i < stack_size(found_triggers); ++i) {
		stack_inc(triggers);
		stack_COPY(stack_top(triggers), stack_get(found_triggers, i)->terms);
	}
	clean_multi_triggers(&triggers);
	stack_apply(found_triggers, cand_trigger_free);
	stack_free(found_triggers);
	if (stack_is_empty(triggers)) stack_free(triggers);
	return triggers;
}

/*
  --------------------------------------------------------------
  Selecting terms for candidates
  --------------------------------------------------------------
*/

/* terms s.t. non-variable, non-ground, top symbol not interpreted */
static void
set_candidates_multi(TDAG src, Tstack_DAG* candidates)
{
	unsigned i;
	if (quantifier(DAG_symb(src)) || ground(src) || DAG_symb(src) == FUNCTION_ITE)
		return;
	if (
		!ground(src) && DAG_arity(src) && !boolean_connector(DAG_symb(src)) &&
		(!ignore_interpreted || !(DAG_symb_type(DAG_symb(src)) & SYMB_INTERPRETED)))
		stack_push(*candidates, src);
	for (i = 0; i < DAG_arity(src); ++i)
		set_candidates_multi(DAG_arg(src, i), candidates);
}

/**
    \brief checks recursively if any non-ground sub-term/predicate application
    is a candidate single-trigger for the given variables
    \param DAG a non-ground term or predicate application
    \param candidates accumulator for non-ground terms/predicates
    \return true if the given DAG is a single-trigger for the given variables,
    false otherwise
    \remark global variable current_vars holds the bound variables
    \remark it only does the recursive search if the "triggers_single_weak"
    option is on, otherwise taking the initial DAG given as the single
    trigger (unless it is an equality, which has a special treatment) */
static bool
set_candidates_single(TDAG DAG, Tstack_DAG* candidates)
{
	unsigned i;
	bool has_subterm_candidate;
	assert(DAG_arity(DAG) && !ground(DAG));
	if (
		stack_size(get_fvars(DAG)) != stack_size(current_vars) ||
		quantifier(DAG_symb(DAG)) || DAG_symb(DAG) == FUNCTION_ITE)
		return false;
		/* DAG or one its sub-DAGs is a single-trigger */
#if DEBUG
	Tstack_DAG stack = stack_DAG_intersect(get_fvars(DAG), current_vars);
	assert(stack_size(stack) == stack_size(current_vars));
	stack_free(stack);
#endif
	has_subterm_candidate = false;
	/* Whether a non-variable sub-term is a single-trigger */
	for (i = 0; i < DAG_arity(DAG); ++i) {
		if (!DAG_arity(DAG_arg(DAG, i)) || ground(DAG_arg(DAG, i))) continue;
		has_subterm_candidate =
			set_candidates_single(DAG_arg(DAG, i), candidates) ||
			has_subterm_candidate;
	}
	if (!has_subterm_candidate) stack_push(*candidates, DAG);
	return true;
}

/**
   \brief sets candidates for single and multi-triggers
   \param DAG a DAG
   \param bound_vars a set of variables
   \param candidates accumulator for non-ground terms/predicates
   \remark broadly, candidates are non-ground, according to the given set of
   variables, terms/predicate applications. Whether strong (maximal depth) or
   weak (minimal) candidates are considered is up to the user options; at this
   level the selection is done for single triggers, while for multi-triggers
   the computation is downstream
   \remark does not consider candidates inside nested quantifiers or functional
   ITEs */
static void
set_candidates(TDAG DAG, Tstack_DAG* candidates)
{
	unsigned i;
	/* Variables already computed or quant subformulas; TODO: What about
     functional ITEs? */
	if (quantifier(DAG_symb(DAG)) || ground(DAG) || DAG_symb(DAG) == FUNCTION_ITE)
		return;
	if (boolean_connector(DAG_symb(DAG))) {
		for (i = 0; i < DAG_arity(DAG); ++i)
			set_candidates(DAG_arg(DAG, i), candidates);
		return;
	}
	assert(DAG_literal(DAG));
	if (multi_triggers_off) {
		if (stack_size(get_fvars(DAG)) == stack_size(current_vars))
			set_candidates_single(DAG, candidates);
		return;
	}
	if (!ground(DAG)) set_candidates_multi(DAG, candidates);
}

/**
   \brief searchs for triggers for a set of variables within the given DAG
   \param body_DAG the body of a quantified formula
   \param bound_vars a set of variables
   \return a set of triggers
   \remark uses DAG_tmp to store variables of each DAG not under the scope of a
   nested quantifier (or functional ITE) */
static Tstack_DAGstack
compute_triggers_vars(TDAG body_DAG, Tstack_DAG bound_vars)
{
	Tstack_DAG candidates;
	Tstack_DAGstack result;
	stack_INIT(candidates);
	current_vars = bound_vars;
	/* Setting variables and candidates for triggers in non-quant subformulas */
	set_candidates(body_DAG, &candidates);
#if DEBUG
	unsigned i;
	for (i = 0; i < stack_size(candidates); ++i)
		assert(!ground(stack_get(candidates, i)));
#endif
	/* Searching triggers in non-quant subformulas */
	result = set_triggers_qnt(candidates);
	assert(!result || !stack_is_empty(result));
	return result;
}

/*
  --------------------------------------------------------------
  Interface
  --------------------------------------------------------------
*/

static inline Tpol
update_pol(TDAG src, unsigned i, Tpol pol)
{
	if (
		DAG_symb(src) == CONNECTOR_NOT ||
		(DAG_symb(src) == CONNECTOR_IMPLIES && i == 0)) {
		assert(DAG_symb(src) == CONNECTOR_NOT || DAG_arity(src) == 2);
		return INV_POL(pol);
	}
	/* Last case is for atoms and terms */
	if (
		(DAG_symb(src) == CONNECTOR_ITE && i == 0) ||
		DAG_symb(src) == CONNECTOR_EQUIV || DAG_symb(src) == CONNECTOR_XOR ||
		(!boolean_connector(DAG_symb(src)) && !binder(DAG_symb(src)))) {
		assert(
			DAG_symb(src) == CONNECTOR_ITE || DAG_arity(src) == 2 ||
			DAG_literal(src));
		return POL_BOTH;
	}
	return pol;
}

/**
    \brief compute triggers for a weak quantifier
    \param src a formula
    \param previous_vars bound vars in scope
    \remark finds nested triggers whene there are no top-level ones. Such
    triggers potentially instantiate variables for the nested quantifiers as
    well, s.t. the variables of nested quantifiers in the same level must not
    intersect. */
void
set_triggers(
	TDAG src, Tstack_DAG previous_vars, Tstack_DAGstack* triggers, Tpol pol)
{
	unsigned i;
	Tstack_DAG local_bound_vars;
	Tstack_DAGstack qnt_triggers, nested_triggers;
	assert(!previous_vars || !stack_is_empty(previous_vars));
	if (!DAG_quant(src) || (DAG_literal(src) && !QUANTIFIED_WEAK(src, pol)))
		return;
	if (QUANTIFIED_WEAK(src, pol)) {
		/* Don't compute triggers for annotated quantifiers: trust the user */
		if (DAG_prop_check(src, DAG_PROP_TRIGGER)) return;
		stack_INIT(local_bound_vars);
		for (i = 0; i < DAG_arity(src) - 1; ++i)
			stack_push(local_bound_vars, DAG_arg(src, i));
		stack_sort(local_bound_vars, DAG_cmp_q);
		/* If not nested, get triggers to quantifier */
		if (!previous_vars) {
			/* TODO: Have not only the triggers but, in case of failure, the
             candidates for multi-patterns. Idea is to handle something like

             forall xy. p(x) or forall z. q(y,z) or q(x,z) */
			qnt_triggers = compute_triggers_vars(DAG_arg_last(src), local_bound_vars);
			/* Found local triggers */
			if (qnt_triggers) {
#if DEBUG_TRIGGERS_SEL
				my_DAG_message("set_triggers: local for {%d}%D:\n", src, src);
				print_Tstack_DAGstack(qnt_triggers);
#endif
				DAG_prop_set(src, DAG_PROP_TRIGGER, &qnt_triggers);
				stack_free(local_bound_vars);
				return;
			}
		} else if (ground(src)) {
			/* Can only search for triggers if quantifier is nonground (may have
             occurrences of the variables from the outer quantifier) */
			stack_free(local_bound_vars);
			return;
		} else {
			stack_merge(local_bound_vars, previous_vars);
			stack_sort(local_bound_vars, DAG_cmp_q);
			nested_triggers =
				compute_triggers_vars(DAG_arg_last(src), local_bound_vars);
			/* If found, done; don't search in deeper quantifiers */
			if (nested_triggers) {
				stack_merge(*triggers, nested_triggers);
				stack_free(nested_triggers);
				stack_free(local_bound_vars);
				return;
			}
		}
		/* Search for triggers for this quantifier in nested ones */
		stack_INIT(qnt_triggers);
		set_triggers(DAG_arg_last(src), local_bound_vars, &qnt_triggers, pol);
		/* Found nested triggers */
		if (!stack_is_empty(qnt_triggers)) {
#if DEBUG_TRIGGERS_SEL
			my_DAG_message("set_triggers: nested for {%d}%D:\n", src, src);
			print_Tstack_DAGstack(qnt_triggers);
#endif
			DAG_prop_set(src, DAG_PROP_TRIGGER, &qnt_triggers);
		} else
			stack_free(qnt_triggers);
		stack_free(local_bound_vars);
		return;
	}
	for (i = 0; i < DAG_arity(src); ++i)
		set_triggers(
			DAG_arg(src, i), previous_vars, triggers, update_pol(src, i, pol));
}

void
triggers_sel_init(void)
{
	/* TODO: have this option taken into account in generation */
	triggers_nested_off = false;
	options_new(
		0, "triggers-nested-off",
		"Uses old triggers but include nested ones [unstable]",
		&triggers_nested_off);

	triggers_new = false;
	options_new(
		0, "triggers-new",
		"Uses new triggers (include nested ones, multi-triggers) [unstable]",
		&triggers_new);

	/* triggers_single_weak = false; */
	/* options_new(0, "triggers-single-weak", */
	/*             "Uses weak multi-triggers (minimal depths)", */
	/*             &triggers_single_weak); */

	/* triggers_multi_weak = false; */
	/* options_new(0, "triggers-multi-weak", */
	/*             "Uses weak multi-triggers (minimal depths)", */
	/*             &triggers_multi_weak); */

	ignore_interpreted = false;
	options_new(
		0, "triggers-no-interpreted",
		"Do not consider interpreted terms as candidate terms",
		&ignore_interpreted);

	rm_more_specific_triggers = false;
	options_new(
		0, "triggers-sel-rm-specific", "Remove more specifc triggers",
		&rm_more_specific_triggers);

	rm_subterm_triggers = false;
	options_new(
		0, "triggers-sel-rm-subterms",
		"Remove triggers which are subterm from others", &rm_subterm_triggers);

	multi_triggers_off = true;
	options_new(
		0, "triggers-multi-off", "Do not generate multi-triggers",
		&multi_triggers_off);

	triggers_sel_stats_time = stats_timer_new(
		"triggers_sel_time", "Triggers Selection time", "%7.2f", STATS_TIMER_ALL);
}

void
triggers_sel_done(void)
{}

/*
  --------------------------------------------------------------
  Old triggers
  --------------------------------------------------------------
*/

/**
   \brief inspects the whole formula, adds trigger to every quantified formula
   without triggers, and ensures the triggers are on the formula itself, not on
   the body
   \param src the formula
   \remark sets the DAG_PROP_TRIGGER property if triggers are found  */
void
set_triggers_old(TDAG DAG)
{
	unsigned i;
	Tstack_DAGstack triggers, *Ptriggers;
	if (!DAG_quant(DAG)) return;
	if (DAG_symb_type(DAG_symb(DAG)) & SYMB_BOOLEAN_CONNECTOR)
		for (i = 0; i < DAG_arity(DAG); ++i) set_triggers_old(DAG_arg(DAG, i));
	if (!quantifier(DAG_symb(DAG))) return;
	Ptriggers = DAG_prop_get(DAG, DAG_PROP_TRIGGER);
	if (!Ptriggers) {
		triggers = qnt_triggers(DAG);
		if (!triggers) return;
		DAG_prop_set(DAG, DAG_PROP_TRIGGER, &triggers);
	}
}

/**
   \brief inspects the whole formula, adds trigger to every quantified
   formula without triggers, and ensures the triggers are on the
   formula itself, not on the body
   \param src the formula */
static void
inst_add_triggers(TDAG src)
{
	unsigned i;
	Tstack_DAGstack* Pannot;

	if (!DAG_quant(src)) return;
	if (DAG_symb_type(DAG_symb(src)) & SYMB_BOOLEAN_CONNECTOR)
		for (i = 0; i < DAG_arity(src); ++i) inst_add_triggers(DAG_arg(src, i));
	if (!quantifier(DAG_symb(src))) return;
	Pannot = DAG_prop_get(src, DAG_PROP_TRIGGER);
	if (!Pannot) {
		Tstack_DAGstack annot = qnt_triggers(src);
		if (!annot) return;
		DAG_prop_set(src, DAG_PROP_TRIGGER, &annot);
	}
}

void
inst_pre(TDAG src)
{
	inst_add_triggers(src);
}
