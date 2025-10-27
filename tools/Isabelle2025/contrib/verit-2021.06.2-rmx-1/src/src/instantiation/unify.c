#include "instantiation/unify.h"

#include "congruence/congruence.h"
#include "instantiation/inst-man.h"
#include "limits.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-subst.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"

#define DEBUG_UNIFY 0

#ifdef DEBUG
/**
   \brief checks if DAG has variables in unifier's domain
   \param unifier the unifier providing the context
   \param DAG a term
   \return true if DAG has at least one free variable among those of the
   unifier, false otherwise
   \remark both vars stack and unifier's variables are sorted */
static bool
ground_in_unifier(Tunifier unifier, TDAG DAG)
{
	unsigned i = 0, j = 0;
	if (DAG_fvars[DAG]) {
		/* Checks if at least one of vars is in unifier */
		while (i < unifier->size && j < stack_size(DAG_fvars[DAG]))
			if (unifier->val[i].var < stack_get(DAG_fvars[DAG], j))
				i++;
			else if (unifier->val[i].var > stack_get(DAG_fvars[DAG], j))
				j++;
			else
				return false;
	}
	return true;
}

static inline bool
ground_u(Tunifier unifier, TDAG DAG)
{
	assert(ground_in_unifier(unifier, DAG) == ground(DAG));
	return ground(DAG);
}
#else
#define ground_u(u, D) ground(D)
#endif

/*
  --------------------------------------------------------------
  Auxiliary functions
  --------------------------------------------------------------
*/

Tstack_DAG current_vars;
unsigned var_offset, *vars_pos;

void
set_context_vars(Tstack_DAG DAGs)
{
	unsigned i;
	stack_reset(current_vars);
	for (i = 0; i < stack_size(DAGs); ++i)
		if (!ground(stack_get(DAGs, i)))
			stack_merge(current_vars, get_fvars(stack_get(DAGs, i)));
	stack_sort(current_vars, DAG_cmp_q);
	stack_uniq(current_vars);
	if (stack_is_empty(current_vars)) return;
	if (vars_pos) free(vars_pos);
	/* Associate var DAGs with indices, so access is constant time */
	var_offset = stack_get(current_vars, 0);
	MY_MALLOC(
		vars_pos, (stack_top(current_vars) - var_offset + 1) * sizeof(unsigned));
	for (i = 0; i < stack_size(current_vars); ++i)
		var_pos(stack_get(current_vars, i)) = i;
}

/**
   \brief checks if a variable being equal to a DAG is inconsistent w.r.t. the
   terms that variable is asserted different to in the unifier
   \param unifier the unifier in which the assertion is made
   \param var_pos the position of the variable to have its diff checked
   \param DAG the term, which can be a variable, to be checked if representative
   is in diff
   \param force_disequal whether it should be enforced that DAG is disequal to
   all terms in diff
   \return true either if the term which is the representative of DAG is in the
   "diff" of the var position or if disequal is on and given term is not
   disequal from all terms in "diff"; false otherwise
   \remark since variables may be in "diff" as well the terms of the latter are
   tested using their representatives in the unifier
   \remark if at some point CCFV also asserts ground equality literals these
   will need to be considered as well. In fact, not only in this function but
   throughout the module */
bool
diff_breaks(Tunifier unifier, unsigned var_pos, TDAG DAG, bool force_disequal)
{
	unsigned i;
	TDAG rep_DAG, rep_diff;
	/* DAG is either a var or a ground term */
	assert(
		unifier->val[var_pos].rep && (ground_u(unifier, DAG) || !DAG_arity(DAG)));
	if (!unifier->val[var_pos].diff) return false;
	rep_DAG = unify_find_DAG(unifier, DAG);
	for (i = 0; i < stack_size(unifier->val[var_pos].diff); ++i) {
		rep_diff =
			unify_find_DAG(unifier, stack_get(unifier->val[var_pos].diff, i));
		if (congruent(rep_diff, rep_DAG)) return true;
		if (!force_disequal) continue;
		/* <diff, DAG>: <t1, t2> or <x, t2> or or <t1, y> or <x, y> */
		/* If both ground, they must be disequal in CC */
		if (!ground_u(unifier, rep_diff) || !ground_u(unifier, rep_DAG)) continue;
		if (!CC_disequal(rep_diff, rep_DAG)) return true;
	}
	return false;
}

/**
   \brief transfer diff in uf[i2] to uf[i1]
   \param unifier the unifier where the transfer occurs
   \param i1 the index of the valuation to which the transfer is done
   \param i2 the index of the valuation from which the transfer is done
   \remark destructive to diff from uf[i2] */
static inline void
merge_diff(Tunifier unifier, unsigned i1, unsigned i2)
{
	assert(unifier->val[i1].rep && unifier->val[i2].rep);
	if (!unifier->val[i2].diff) return;
	if (!unifier->val[i1].diff) stack_INIT(unifier->val[i1].diff);
	stack_merge(unifier->val[i1].diff, unifier->val[i2].diff);
	stack_free(unifier->val[i2].diff);
}

/**
   \brief transfer free_in in uf[i2] to uf[i1]
   \param unifier the unifier where the transfer occurs
   \param i1 the index of the valuation to which the transfer is done
   \param i2 the index of the valuation from which the transfer is done
   \remark destructive to free_in from uf[i2] */
static inline void
merge_free_in(Tunifier unifier, unsigned i1, unsigned i2)
{
	assert(unifier->val[i1].rep && unifier->val[i2].rep);
	if (!unifier->val[i2].free_in) return;
	if (!unifier->val[i1].free_in) stack_INIT(unifier->val[i1].free_in);
	stack_merge(unifier->val[i1].free_in, unifier->val[i2].free_in);
	stack_free(unifier->val[i2].free_in);
}

Tstack_DAG grounded_var_classes;

/**
    \brief
    \param unifier the unifier where the update occurs
    \param rep_var
    \param new_vars
    \param track workaround for avoiding grounded_var_classes stack when
    grounding
    \remark */
static void
update_free_vars(
	Tunifier unifier, unsigned rep_var, unsigned new_vars, bool track)
{
	unsigned i, rep_watched;
	if (unifier->val[rep_var].free_in)
		for (i = 0; i < stack_size(unifier->val[rep_var].free_in); ++i) {
			rep_watched =
				unify_find(unifier, stack_get(unifier->val[rep_var].free_in, i));
			/* previously free vars of [var] must be free in [watched] */
			assert(
				unifier->val[rep_var].free_vars & unifier->val[rep_watched].free_vars);
			/* new_vars of watched are those before minus that of [rep_var] plus
           the new ones of [rep_var] */
			update_free_vars(
				unifier, rep_watched,
				(unifier->val[rep_watched].free_vars ^
				 unifier->val[rep_var].free_vars) |
					new_vars,
				track);
		}
	unifier->val[rep_var].free_vars = new_vars;
	if (!unifier->val[rep_var].free_vars) {
		set_ground_var(unifier, rep_var);
		if (track) stack_push(grounded_var_classes, rep_var);
	}
}

#define UNIFY_VAR_INDEX_NOT_FOUND UINT_MAX

/**
   \brief get index of variable in unifier
   \param unfier a unifier
   \param var a variable
   \return the var index or UNIFY_VAR_INDEX_NOT_FOUND if not found */
unsigned
unify_var_index(Tunifier unifier, TDAG var)
{
	/* PF,HB: int is mandatory: we can have -1 either with size = 0 or
     imid = 0 and imax gets imid - 1 */
	int imid, imin = 0, imax = unifier->size - 1;
	while (imin <= imax) {
		imid = (imax + imin) / 2;
		if (var == unifier->val[imid].var) return imid;
		if (var < unifier->val[imid].var)
			imax = imid - 1;
		else if (var > unifier->val[imid].var)
			imin = imid + 1;
	}
	return UNIFY_VAR_INDEX_NOT_FOUND;
}

bool
unify_ground_var(Tunifier unifier, TDAG var)
{
	assert(
		!DAG_arity(var) && !ground_u(unifier, var) &&
		unify_var_index(unifier, var) != UNIFY_VAR_INDEX_NOT_FOUND &&
		unify_var_index(unifier, var) == var_pos(var));
	if ((unifier->ground_vars >> unify_find(unifier, var_pos(var))) & 1u) {
		/* Necessary because x=y=z, then z=t; x,y would not be marked as ground */
		set_ground_var(unifier, var_pos(var));
		return true;
	}
	return false;
}

/* TODO: oh, this might be wrong... */
/* TODO: this should be more efficient, I suppose */
unsigned
unify_nb_ground_vars(Tunifier unifier)
{
	unsigned i, total = 0;
	for (i = 0; i < unifier->size; ++i)
		if (check_var(unifier, i)) ++total;
	return total;
}

bool
unify_occurs(Tunifier unifier, TDAG var_DAG, TDAG UDAG)
{
	unsigned i, rep_var;
	assert(!ground_u(unifier, UDAG) && !stack_is_empty(get_fvars(UDAG)));
	for (i = 0; i < stack_size(get_fvars(UDAG)); ++i) {
		rep_var = unify_find(unifier, var_pos(stack_get(get_fvars(UDAG), i)));
		/* TODO: should check with the free vars of [var_DAG]? */
		if ((unifier->val[rep_var].free_vars >> var_pos(var_DAG)) & 1u) return true;
	}
	return false;
}

bool
unify_is_var(Tunifier unifier, TDAG var)
{
	unsigned pos_var, rep_var;
	assert(
		unify_var_index(unifier, var) != UNIFY_VAR_INDEX_NOT_FOUND &&
		unify_var_index(unifier, var) == var_pos(var));
	pos_var = var_pos(var);
	rep_var = unify_find(unifier, pos_var);
	return unifier->val[rep_var].free_vars & (1u << pos_var);
}

/*
  --------------------------------------------------------------
  Handling unifiers
  --------------------------------------------------------------
*/

Tunifier
unify_new(Tstack_DAG vars)
{
	unsigned i;
	Tunifier unifier;
	assert(stack_size(vars));
	/* HB Remember the memory alignment */
	MY_MALLOC(unifier, sizeof(Tunifier) + stack_size(vars) * sizeof(Tval));
	unifier->size = stack_size(vars);
	unifier->ground_vars = 0;
	for (i = 0; i < unifier->size; ++i) {
		assert(!i || stack_get(vars, i - 1) < stack_get(vars, i));
		unifier->val[i].var = stack_get(vars, i);
		unifier->val[i].rep = true;
		unifier->val[i].free_vars = 1u << i;
		unifier->val[i].free_in = NULL;
		unifier->val[i].term = DAG_NULL;
		unifier->val[i].diff = NULL;
	}
	return unifier;
}

void
unify_free(Tunifier unifier)
{
	unsigned i;
	assert(unifier);
	for (i = 0; i < unifier->size; ++i) {
		if (unifier->val[i].rep && unifier->val[i].diff)
			stack_free(unifier->val[i].diff);
		if (unifier->val[i].rep && unifier->val[i].free_in)
			stack_free(unifier->val[i].free_in);
	}
	free(unifier);
}

Tunifier
unify_copy(Tunifier unifier)
{
	unsigned i;
	Tunifier new_unifier;
	MY_MALLOC(new_unifier, sizeof(Tunifier) + unifier->size * sizeof(Tval));
	new_unifier->size = unifier->size;
	new_unifier->ground_vars = unifier->ground_vars;
	for (i = 0; i < unifier->size; ++i) {
		new_unifier->val[i].var = unifier->val[i].var;
		new_unifier->val[i].rep = unifier->val[i].rep;
		new_unifier->val[i].free_vars = unifier->val[i].free_vars;
		if (unifier->val[i].free_in)
			stack_COPY(new_unifier->val[i].free_in, unifier->val[i].free_in);
		else
			new_unifier->val[i].free_in = NULL;

		if (!unifier->val[i].rep) {
			new_unifier->val[i].equal_var = unifier->val[i].equal_var;
			continue;
		}
		new_unifier->val[i].term = unifier->val[i].term;
		if (unifier->val[i].diff)
			stack_COPY(new_unifier->val[i].diff, unifier->val[i].diff);
		else
			new_unifier->val[i].diff = NULL;
	}
	return new_unifier;
}

unsigned
unify_find(Tunifier unifier, unsigned orig)
{
	unsigned target, tmp;
	assert(0 <= orig && orig < unifier->size);
	if (unifier->val[orig].rep) return orig;
	target = unifier->val[orig].equal_var;
	while (!unifier->val[target].rep) target = unifier->val[target].equal_var;
	/* Path compression */
	while (unifier->val[orig].equal_var != target && !unifier->val[orig].rep) {
		tmp = unifier->val[orig].equal_var;
		unifier->val[orig].equal_var = target;
		orig = tmp;
	}
	return target;
}

TDAG
unify_find_DAG(Tunifier unifier, TDAG DAG)
{
	unsigned rep_var;
	if (ground(DAG) || DAG_arity(DAG)) return DAG;
	assert(
		unify_var_index(unifier, DAG) != UNIFY_VAR_INDEX_NOT_FOUND &&
		unify_var_index(unifier, DAG) == var_pos(DAG));
	rep_var = unify_find(unifier, var_pos(DAG));
	/* If DAG is a variable in unifier, i.e. it occurs among the free variables of
     its class, let itself be the result */
	assert(
		unifier->val[rep_var].term ||
		unifier->val[rep_var].free_vars & (1u << var_pos(DAG)));
	if (unifier->val[rep_var].free_vars & (1u << var_pos(DAG)))
		return unifier->val[rep_var].var;
	assert(unifier->val[rep_var].term);
	return unifier->val[rep_var].term;
}

/* TODO: Get rid of these calls with a spurious parameter to diff_breaks; have
   two different functions, probably */
bool
unify_union(Tunifier unifier, TDAG D0, TDAG D1)
{
	unsigned rep_D0, rep_D1, pos_D0, pos_D1;
#if DEBUG_UNIFY > 1
	my_DAG_message("union: in with <%D,%D>\n", D0, D1);
#endif
	/* D0 is a var */
	assert(!DAG_arity(D0) && !ground_u(unifier, D0));
	pos_D0 = var_pos(D0);
	assert(
		unify_var_index(unifier, D0) != UNIFY_VAR_INDEX_NOT_FOUND &&
		unify_var_index(unifier, D0) == pos_D0);
	rep_D0 = unify_find(unifier, pos_D0);
	/* if D1 is a ground term */
	if (ground_u(unifier, D1)) {
		if (
			(unifier->val[rep_D0].term &&
			 !congruent(unifier->val[rep_D0].term, D1)) ||
			diff_breaks(unifier, rep_D0, D1, false))
			return false;
		unifier->val[rep_D0].term = D1;
		update_free_vars(unifier, rep_D0, 0, true);
		return true;
	}
	/* if D1 is a non-ground function application */
	if (DAG_arity(D1)) {
		unsigned i, rep_var, new_vars;
		TDAG var;
		/* [D0] can only contain vars */
		assert(!unifier->val[rep_D0].term);
		unifier->val[rep_D0].term = D1;
		/* Compute new free vars of [D0] based on vars in D1 */
		assert(!ground(D1) && stack_size(get_fvars(D1)));
		new_vars = 0;
		for (i = 0; i < stack_size(get_fvars(D1)); ++i) {
			var = stack_get(get_fvars(D1), i);
			assert(
				unify_var_index(unifier, var) != UNIFY_VAR_INDEX_NOT_FOUND &&
				unify_var_index(unifier, var) == var_pos(var));
			rep_var = unify_find(unifier, var_pos(var));
			/* Grounded var classes should have no free vars */
			assert(
				!check_class(unifier, rep_var) || !unifier->val[rep_var].free_vars);
			if (!unifier->val[rep_var].free_vars) continue;
			new_vars |= unifier->val[rep_var].free_vars;
			/* Add [D0] to the watched classes of [var] */
			if (!unifier->val[rep_var].free_in)
				stack_INIT(unifier->val[rep_var].free_in);
			stack_push(unifier->val[rep_var].free_in, rep_D0);
		}
		/* no previously free var of [D0] occurs in [var] */
		assert(!(unifier->val[rep_D0].free_vars & new_vars));
		/* Update [D0] and all other classes in which [D0] appears */
		update_free_vars(unifier, rep_D0, new_vars, true);
		return true;
	}
	/* D1 is a var */
	pos_D1 = var_pos(D1);
	assert(
		unify_var_index(unifier, D1) != UNIFY_VAR_INDEX_NOT_FOUND &&
		unify_var_index(unifier, D1) == pos_D1);
	rep_D1 = unify_find(unifier, pos_D1);
	if (rep_D0 == rep_D1) return true;
	/* whether D1 in diff[D0] or D0 in diff[D1] */
	if (
		diff_breaks(unifier, rep_D0, unifier->val[rep_D1].var, false) ||
		diff_breaks(unifier, rep_D1, unifier->val[rep_D0].var, false))
		return false;
	/* there is s in [D0] */
	if (unifier->val[rep_D0].term) {
		/* if [D1] has a term, all its FVs have been grounded; moreover the term
         of [D0] must also be ground, otherwise would have led to E-uni outside;
         also, they should be ground terms with signatures, otherwise also
         handled outside... */
		assert(
			!unifier->val[rep_D1].term ||
			(!unifier->val[rep_D1].free_vars && !unifier->val[rep_D0].free_vars &&
			 CC_abstract(unifier->val[rep_D1].term) &&
			 CC_abstract(unifier->val[rep_D0].term)));
		/* Either s in diff[D1] or there is u in [D1] and either s != u
         or u in diff[D0] */
		if (
			diff_breaks(unifier, rep_D1, unifier->val[rep_D0].term, false) ||
			(unifier->val[rep_D1].term &&
			 (!congruent(unifier->val[rep_D0].term, unifier->val[rep_D1].term) ||
				diff_breaks(unifier, rep_D0, unifier->val[rep_D1].term, false))))
			return false;
		/* Update classes in which [D1] occurs */
		if (unifier->val[rep_D0].free_vars != unifier->val[rep_D1].free_vars)
			update_free_vars(unifier, rep_D1, unifier->val[rep_D0].free_vars, true);
		/* merges [D1] with [D0] */
		merge_free_in(unifier, rep_D0, rep_D1);
		merge_diff(unifier, rep_D0, rep_D1);
		unifier->val[rep_D1].rep = false;
		unifier->val[rep_D1].equal_var = rep_D0;
		return true;
	}
	/* [D0] is free and there is u in [D1] */
	if (unifier->val[rep_D1].term) {
		/* vars of [D0] cannot occur in [D1] */
		assert(!(unifier->val[rep_D0].free_vars & unifier->val[rep_D1].free_vars));
		/* Whether u in diff[D0] */
		if (diff_breaks(unifier, rep_D0, unifier->val[rep_D1].term, false))
			return false;
		/* There must be a difference in the free vars of the classes (either [D1]
         has gr or equal to a term whose FVs are different from those of
         [D0]) */
		assert(unifier->val[rep_D0].free_vars != unifier->val[rep_D1].free_vars);
		update_free_vars(unifier, rep_D0, unifier->val[rep_D1].free_vars, true);
		merge_diff(unifier, rep_D1, rep_D0);
		merge_free_in(unifier, rep_D1, rep_D0);
		unifier->val[rep_D0].rep = false;
		unifier->val[rep_D0].equal_var = rep_D1;
		return true;
	}
	/* Both are free */
	update_free_vars(
		unifier, rep_D1,
		unifier->val[rep_D0].free_vars |= unifier->val[rep_D1].free_vars, true);
	merge_diff(unifier, rep_D0, rep_D1);
	merge_free_in(unifier, rep_D0, rep_D1);
	unifier->val[rep_D1].rep = false;
	unifier->val[rep_D1].equal_var = rep_D0;
	/* get free vars from [D1] */
	unifier->val[rep_D0].free_vars |= unifier->val[rep_D1].free_vars;
	return true;
}

/* TODO: handle non-ground function applications eventually? Desirable...? */
bool
unify_union_diff(Tunifier unifier, TDAG D0, TDAG D1)
{
	unsigned rep_D0, rep_D1;
#if DEBUG_UNIFY > 1
	my_DAG_message("union_diff: in with <%D,%D>\n", D0, D1);
#endif
	/* D0 is a var; if D1 is non ground, it's a var */
	assert(
		(!DAG_arity(D0) && !ground_u(unifier, D0)) &&
		(ground_u(unifier, D1) || (!DAG_arity(D1) && !ground_u(unifier, D1))));
	rep_D0 = unify_find(unifier, var_pos(D0));
	assert(
		unify_var_index(unifier, D0) != UNIFY_VAR_INDEX_NOT_FOUND &&
		unify_var_index(unifier, D0) == var_pos(D0));
	/* Whether both D0,D1 are the same var or whether the DAG in
     theirs reps are congruent */
	if (
		D0 == D1 ||
		congruent(unify_find_DAG(unifier, D0), unify_find_DAG(unifier, D1)))
		return false;
	/* The disequalities are accumulated in the current rep of
     each class */
	if (unifier->val[rep_D0].var == D1)
		my_DAG_error("unify_union: how come this happened?\n");
	if (!unifier->val[rep_D0].diff) stack_INIT(unifier->val[rep_D0].diff);
	stack_push(unifier->val[rep_D0].diff, D1);
	stack_sort(unifier->val[rep_D0].diff, DAG_cmp_q);
	stack_uniq(unifier->val[rep_D0].diff);

	/* If D1 is also a var, have the disequality for both vars */
	if (!ground_u(unifier, D1)) {
		rep_D1 = unify_find(unifier, var_pos(D1));
		assert(
			unify_var_index(unifier, D1) != UNIFY_VAR_INDEX_NOT_FOUND &&
			unify_var_index(unifier, D1) == var_pos(D1));
		if (!unifier->val[rep_D1].diff) stack_INIT(unifier->val[rep_D1].diff);
		stack_push(unifier->val[rep_D1].diff, unifier->val[rep_D0].var);
		stack_sort(unifier->val[rep_D1].diff, DAG_cmp_q);
		stack_uniq(unifier->val[rep_D1].diff);
	}
	return true;
}

bool
unify_merge(Tunifier u1, Tunifier u2)
{
	unsigned i, j;
	assert(u1->size == u2->size);
	for (i = 0; i < u1->size; ++i) {
		TDAG var = u1->val[i].var;
		if (!u2->val[i].rep) {
			if (!unify_union(u1, var, u2->val[u2->val[i].equal_var].var))
				return false;
			continue;
		}
		if (u2->val[i].term && !unify_union(u1, var, u2->val[i].term)) return false;
		if (!u2->val[i].diff) continue;
		for (j = 0; j < stack_size(u2->val[i].diff); ++j)
			if (!unify_union_diff(u1, var, stack_get(u2->val[i].diff, j)))
				return false;
	}
	return true;
}

Tstack_unifier
unify_reset(Tstack_unifier unifiers, Tstack_DAG new_vars)
{
	unsigned i, j, k, index;
	TDAG var;
	Tunifier new_unifier, unifier;
	Tstack_DAG old_vars, common_vars;
	Tstack_unifier new_unifiers;
	assert(unifiers);
	stack_INIT(new_unifiers);
	if (stack_is_empty(unifiers)) return new_unifiers;
	unifier = stack_get(unifiers, 0);
	/* same vars, no need to adapt */
	if (unifier->size == stack_size(new_vars)) {
		for (i = 0; i < unifier->size; ++i)
			if (unifier->val[i].var != stack_get(new_vars, i)) break;
		if (i == unifier->size) {
			for (i = 0; i < stack_size(unifiers); ++i)
				stack_push(new_unifiers, unify_copy(stack_get(unifiers, i)));
			return new_unifiers;
		}
	}
	/* TODO: Rebasing the unifiers seems as a bad idea, as it is. Maybe should be
     ignored when different variables unless I'm indexing by fingerprints??? */
	stack_INIT(old_vars);
	for (i = 0; i < unifier->size; ++i) stack_push(old_vars, unifier->val[i].var);
	common_vars = stack_DAG_intersect(old_vars, new_vars);
	stack_free(old_vars);
	/* create new set of unifiers based on new_vars */
	for (i = 0; i < stack_size(unifiers); ++i) {
		new_unifier = unify_new(new_vars);
		unifier = stack_get(unifiers, i);
		for (j = 0; j < stack_size(common_vars); ++j) {
			var = stack_get(common_vars, j);
			assert(unify_var_index(unifier, var) != UNIFY_VAR_INDEX_NOT_FOUND);
			index = unify_var_index(unifier, var);
			if (!unifier->val[index].rep) {
				assert(stack_DAG_contains(
					new_vars, unifier->val[unifier->val[index].equal_var].var));
				unify_union(
					new_unifier, var, unifier->val[unifier->val[index].equal_var].var);
				continue;
			}
			if (unifier->val[index].term)
				unify_union(new_unifier, var, unifier->val[index].term);
			if (!unifier->val[index].diff) continue;
			for (k = 0; k < stack_size(unifier->val[index].diff); ++k)
				unify_union_diff(
					new_unifier, var, stack_get(unifier->val[index].diff, k));
		}
		stack_push(new_unifiers, new_unifier);
	}
	stack_free(common_vars);
	return new_unifiers;
}

bool
unify_union_ground(
	Tunifier unifier, TDAG rep_ind, Tstack_unsigned vars_inds, TDAG term,
	bool force_disequal)
{
	unsigned i;
	/* rep_ind is the index of a rep in unifier; term is ground */
	assert(
		0 <= rep_ind && rep_ind < unifier->size && unifier->val[rep_ind].rep &&
		ground_u(unifier, term));
#if DEBUG_UNIFY > 1
	my_DAG_message(
		"union_ground: in with <%D,%D>\n", unifier->val[rep_ind].var, term);
#endif
	if (
		(unifier->val[rep_ind].term &&
		 !congruent(unifier->val[rep_ind].term, term)) ||
		diff_breaks(unifier, rep_ind, term, force_disequal))
		return false;
	unifier->val[rep_ind].term = term;
	update_free_vars(unifier, rep_ind, 0, false);
	if (unifier->val[rep_ind].diff) stack_free(unifier->val[rep_ind].diff);
	/* Ground all variables in class */
	for (i = 0; i < stack_size(vars_inds); ++i)
		if (stack_get(vars_inds, i) != rep_ind) {
			unifier->val[stack_get(vars_inds, i)].rep = true;
			unifier->val[stack_get(vars_inds, i)].term = term;
			unifier->val[stack_get(vars_inds, i)].diff = NULL;
			update_free_vars(unifier, stack_get(vars_inds, i), 0, false);
		}
	return true;
}

typedef struct VarClass
{
	unsigned rep_ind;
	Tstack_unsigned vars_inds;
} VarClass;

TSstack(_VarClass, VarClass); /* typedefs Tstack_VarClass */

/* Associates a sort to a set of variables indices */
typedef struct SortVars
{
	Tsort sort;
	Tstack_VarClass var_classes;
} SortVars;

TSstack(_SortVars, SortVars); /* typedefs Tstack_SortVars */

/* Destructive */
Tstack_unifier
unify_ground(Tunifier unifier, unsigned cap, bool all_CIs)
{
	unsigned i, j, k, l, var_ind, rep_ind;
	Tstack_DAG terms;
	Tstack_unsigned free_vars_inds, fresh_vars_inds;
	Tstack_VarClass var_classes;
	Tstack_SortVars sort_vars;
	Tstack_unifier result, old_result;
	stack_INIT(free_vars_inds);
	stack_INIT(fresh_vars_inds);
	stack_INIT(result);
	for (i = 0; i < unifier->size; ++i) {
		rep_ind = unifier->val[i].rep ? i : unify_find(unifier, i);
		if (check_var(unifier, rep_ind) && ground(unifier->val[rep_ind].term)) {
			/* Guarantee that term var is equal to is disequal to all
             terms in diff; TODO: if ccfv_disequal is on this is
             already the case... */
			if (
				all_CIs &&
				diff_breaks(unifier, rep_ind, unifier->val[rep_ind].term, true)) {
				stack_free(result);
				return NULL;
			}
			if (unifier->val[rep_ind].diff) stack_free(unifier->val[rep_ind].diff);
			if (i != rep_ind) {
				unifier->val[i].rep = true;
				unifier->val[i].term = unifier->val[rep_ind].term;
				unifier->val[i].diff = NULL;
				update_free_vars(unifier, i, 0, false);
			}
			continue;
		}
		if (unifier->val[rep_ind].free_vars & (1u << rep_ind))
			stack_push(free_vars_inds, i);
		else
			stack_push(fresh_vars_inds, i);
	}
	stack_push(result, unifier);
#ifdef DEBUG
	if (stack_is_empty(free_vars_inds) && stack_is_empty(fresh_vars_inds))
		for (i = 0; i < unifier->size; ++i)
			assert(unifier->val[i].rep && ground(unifier->val[i].term));
#endif
	/* Unifier was grounded already */
	if (stack_is_empty(free_vars_inds) && stack_is_empty(fresh_vars_inds)) {
		assert(unify_grounded(unifier));
		stack_free(free_vars_inds);
		stack_free(fresh_vars_inds);
		return result;
	}
	if (!stack_is_empty(free_vars_inds)) {
		/* Partition free variables per sort/class */
		stack_INIT(sort_vars);
		while (!stack_is_empty(free_vars_inds)) {
			var_ind = stack_pop(free_vars_inds);
			rep_ind = unify_find(unifier, var_ind);
			for (i = 0; i < stack_size(sort_vars); ++i)
				if (DAG_sort(unifier->val[var_ind].var) == stack_get(sort_vars, i).sort)
					break;
			if (i != stack_size(sort_vars)) {
				for (j = 0; j < stack_size(stack_get(sort_vars, i).var_classes); ++j)
					if (
						stack_get(stack_get(sort_vars, i).var_classes, j).rep_ind ==
						rep_ind)
						break;
				if (j != stack_size(stack_get(sort_vars, i).var_classes))
					stack_push(
						stack_get(stack_get(sort_vars, i).var_classes, j).vars_inds,
						var_ind);
				else {
					VarClass var_class;
					var_class.rep_ind = rep_ind;
					stack_INIT(var_class.vars_inds);
					stack_push(var_class.vars_inds, var_ind);
					stack_push(stack_get(sort_vars, i).var_classes, var_class);
				}
				continue;
			}
			VarClass var_class;
			var_class.rep_ind = rep_ind;
			stack_INIT(var_class.vars_inds);
			stack_push(var_class.vars_inds, var_ind);
			stack_inc(sort_vars);
			stack_top(sort_vars).sort = DAG_sort(unifier->val[var_ind].var);
			stack_INIT(stack_top(sort_vars).var_classes);
			stack_push(stack_top(sort_vars).var_classes, var_class);
		}
		stack_free(free_vars_inds);
		assert(!stack_is_empty(sort_vars));
		/* Collect all ground terms from sort classes of free variables, then
         ground all variables with all terms (per sort), doing the proper
         combinations */
		stack_INIT(old_result);
		for (i = 0; i < stack_size(sort_vars); ++i) {
			terms = CC_get_sort_classes(stack_get(sort_vars, i).sort);
			if (stack_is_empty(terms)) {
				stack_free(terms);
				break;
			}
			/* For each free variable class, generate a set of unifiers by
             combining previous ones with each term in its sort class */
			var_classes = stack_get(sort_vars, i).var_classes;
			for (j = 0; j < stack_size(var_classes); ++j) {
				stack_merge(old_result, result);
				stack_reset(result);
				rep_ind = stack_get(var_classes, j).rep_ind;
				/* To avoid explosion; TODO: needs to be refined */
				for (k = 0; k < stack_size(terms) && k < cap; ++k)
					for (l = 0; l < stack_size(old_result); ++l) {
						unifier = unify_copy(stack_get(old_result, l));
						if (unify_union_ground(
									unifier, rep_ind, stack_get(var_classes, j).vars_inds,
									stack_get(terms, k), all_CIs))
							stack_push(result, unifier);
						else
							unify_free(unifier);
					}
				stack_apply(old_result, unify_free);
				stack_reset(old_result);
			}
			stack_free(terms);
		}
		stack_free(old_result);
		/* TODO: One sort class at least is empty. What should be done in such
         cases?  Giving up does not seem as the best option. How often does this
         happen in SMT-LIB? */
		if (i != stack_size(sort_vars)) {
			for (i = 0; i < stack_size(sort_vars); ++i) {
				for (j = 0; j < stack_size(stack_get(sort_vars, i).var_classes); ++j)
					stack_free(
						stack_get(stack_get(sort_vars, i).var_classes, j).vars_inds);
				stack_free(stack_get(sort_vars, i).var_classes);
			}
			stack_free(sort_vars);
			stack_apply(result, unify_free);
			stack_free(result);
			stack_free(fresh_vars_inds);
			return NULL;
		}
		for (i = 0; i < stack_size(sort_vars); ++i) {
			for (j = 0; j < stack_size(stack_get(sort_vars, i).var_classes); ++j)
				stack_free(stack_get(stack_get(sort_vars, i).var_classes, j).vars_inds);
			stack_free(stack_get(sort_vars, i).var_classes);
		}
		stack_free(sort_vars);
#ifdef DEBUG
		if (stack_is_empty(fresh_vars_inds)) {
			for (i = 0; i < stack_size(result); ++i)
				for (j = 0; j < stack_get(result, i)->size; ++j)
					assert(
						stack_get(result, i)->val[j].rep &&
						ground(stack_get(result, i)->val[j].term));
			for (i = 0; i < stack_size(result); ++i)
				assert(unify_grounded(stack_get(result, i)));
		}
#endif
	}
	if (!stack_is_empty(fresh_vars_inds)) {
		assert(!stack_is_empty(result));
		for (k = 0; k < stack_size(result); ++k) {
			i = 0;
			while (i < stack_size(fresh_vars_inds)) {
				var_ind = stack_get(fresh_vars_inds, i);
				rep_ind = unify_find(stack_get(result, k), var_ind);
				TDAG rep = stack_get(result, k)->val[rep_ind].term;
				if (var_ind == rep_ind && ground(rep)) {
					++i;
					continue;
				}
				if (var_ind != rep_ind && ground(rep)) {
					stack_get(result, k)->val[i].rep = true;
					stack_get(result, k)->val[i].term = rep;
					stack_get(result, k)->val[i].diff = NULL;
					++i;
					continue;
				}
				assert(!ground(rep));
				DAG_tmp_reserve();
				for (j = 0; j < stack_size(get_fvars(rep)); ++j) {
					if (!ground(unify_find_DAG(
								stack_get(result, k), stack_get(get_fvars(rep), j))))
						break;
					DAG_tmp_DAG[stack_get(get_fvars(rep), j)] =
						unify_find_DAG(stack_get(result, k), stack_get(get_fvars(rep), j));
				}
				/* All vars of rep must have been grounded */
				if (j != stack_size(get_fvars(rep))) {
					for (k = 0; k < j; ++k)
						DAG_tmp_DAG[stack_get(get_fvars(rep), k)] = DAG_NULL;
					DAG_tmp_release();
					++i;
					continue;
				}
				DAG_tmp_subst(rep);
				stack_get(result, k)->val[rep_ind].term = DAG_dup(DAG_tmp_DAG[rep]);
				DAG_tmp_reset_DAG(rep);
				DAG_tmp_release();
				/* Restart  */
				i = 0;
				continue;
			}
		}
#ifdef DEBUG
		for (i = 0; i < stack_size(result); ++i)
			for (j = 0; j < stack_get(result, i)->size; ++j)
				assert(
					stack_get(result, i)->val[j].rep &&
					ground(stack_get(result, i)->val[j].term));
		for (i = 0; i < stack_size(result); ++i)
			assert(unify_grounded(stack_get(result, i)));
#endif
	}
	stack_free(fresh_vars_inds);
	if (stack_is_empty(result)) stack_free(result);
	return result;
}

/**
   \brief Whether two variables' valuations are equivalent
   \param val1 first valuation
   \param val2 second valuation
   \return true if they are equivalent, false otherwise */
/* TODO: There appears to be a mistake here */
static inline bool
cmp_val(Tval val1, Tval val2)
{
	unsigned i, j;
	/* no terms nor diffs, both same val */
	if (!val1.term && !val2.term && !val1.diff && !val2.diff) return true;
	/* TODO: not so sure about this first test */
	if (!congruent(val1.term, val2.term)) return false;
	/* congruent terms and no diffs */
	if (!val1.diff && !val2.diff) return true;
	/* different size diffs */
	if (
		(!val1.diff && val2.diff) || (!val2.diff && val1.diff) ||
		stack_size(val1.diff) != stack_size(val2.diff))
		return false;
	/* different diffs */
	i = j = 0;
	while (i < stack_size(val1.diff) && j < stack_size(val2.diff))
		if (stack_get(val1.diff, i) < stack_get(val2.diff, j))
			return false;
		else if (stack_get(val1.diff, i) > stack_get(val2.diff, j))
			return false;
		else {
			i++;
			j++;
		}
	return true;
}

bool
unifier_redundant(Tstack_unifier unifiers, Tunifier unifier)
{
	unsigned i, j;
	assert(unifiers);
	for (i = 0; i < stack_size(unifiers); ++i) {
		Tunifier current = stack_get(unifiers, i);
		for (j = 0; j < unifier->size; ++j)
			if (!cmp_val(
						current->val[unify_find(current, j)],
						unifier->val[unify_find(unifier, j)]))
				break;
		if (j == unifiers->size) {
#if DEBUG_UNIFY
			my_DAG_message("Redundant unifiers:\n");
			unify_print(current);
			my_message_return();
			unify_print(unifier);
#endif
			return true;
		}
	}
	return false;
}

/*
  --------------------------------------------------------------
  Printing stuff
  --------------------------------------------------------------
*/

const char*
byte_to_binary(unsigned x, unsigned size)
{
	static char b[9];
	b[0] = '\0';
	unsigned z;
	for (z = 1 << (size - 1); z > 0; z >>= 1) {
		strcat(b, ((x & z) == z) ? "1" : "0");
	}
	return b;
}

void
unify_print(Tunifier unifier)
{
	unsigned i, j;
	char *inline_free_in, *inline_diff;
	for (i = 0; i < unifier->size; ++i) {
		if (!unifier->val[i].rep) {
			my_DAG_message(
				"\t%d: [%d/%D]: {--> %d/%D}\n", i, unifier->val[i].var,
				unifier->val[i].var, unifier->val[i].equal_var,
				unifier->val[unifier->val[i].equal_var].var);
			continue;
		}
		if (unifier->val[i].diff) {
			MY_MALLOC(
				inline_diff, stack_size(unifier->val[i].diff) * 100 * sizeof(char));
			/* inline_diff[depth*2] = '\0'; */
			sprintf(inline_diff, "; diff: [");
			DAG_sprint(
				inline_diff + strlen(inline_diff), stack_get(unifier->val[i].diff, 0));
			for (j = 1; j < stack_size(unifier->val[i].diff); ++j) {
				sprintf(inline_diff + strlen(inline_diff), ", ");
				DAG_sprint(
					inline_diff + strlen(inline_diff),
					stack_get(unifier->val[i].diff, j));
			}
			sprintf(inline_diff + strlen(inline_diff), "]");
		} else
			inline_diff = NULL;
		if (unifier->val[i].free_in) {
			MY_MALLOC(
				inline_free_in,
				stack_size(unifier->val[i].free_in) * 100 * sizeof(char));
			/* inline_free_in[depth*2] = '\0'; */
			sprintf(inline_free_in, "; free_in: [");
			sprintf(
				inline_free_in + strlen(inline_free_in), "%d",
				stack_get(unifier->val[i].free_in, 0));
			for (j = 1; j < stack_size(unifier->val[i].free_in); ++j) {
				sprintf(inline_free_in + strlen(inline_free_in), ", ");
				sprintf(
					inline_free_in + strlen(inline_free_in), "%d",
					stack_get(unifier->val[i].free_in, j));
			}
			sprintf(inline_free_in + strlen(inline_free_in), "]");
		} else
			inline_free_in = NULL;

		my_DAG_message(
			"\t%d: [%d/%D]: %s/{[%d]%D}%s%s\n", i, unifier->val[i].var,
			unifier->val[i].var,
			byte_to_binary(unifier->val[i].free_vars, unifier->size),
			CC_abstract(unifier->val[i].term), unifier->val[i].term,
			inline_free_in ? inline_free_in : "", inline_diff ? inline_diff : "");
		if (inline_free_in) free(inline_free_in);
		if (inline_diff) free(inline_diff);
	}
}

void
print_Tstack_unifier(Tstack_unifier stack)
{
	unsigned i;
	for (i = 0; i < stack_size(stack); ++i) {
		my_message("Unification [%d]:\n", i);
		unify_print(stack_get(stack, i));
	}
}
