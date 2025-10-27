#include "instantiation/syntactic-unify.h"

#include "symbolic/DAG-print.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/veriT-DAG.h"

static inline bool
is_variable_not_in_set(TDAG t, Tstack_DAG strong_vars)
{
	if (!DAG_arity(t) && (DAG_symb_type(DAG_symb(t)) & SYMB_VARIABLE)) {
		if (!strong_vars || !stack_DAG_contains(strong_vars, t))
			return true;
	}
	return false;
}

static inline bool
is_variable(TDAG t)
{
	return (!DAG_arity(t) && (DAG_symb_type(DAG_symb(t)) & SYMB_VARIABLE));
}

/** \brief Resets the terms assigned to the subterms of `dag` except its
		       variables. Stops if a subterm has no mapping. */
static void
cleanup_substitution_short(TDAG dag)
{
	if (DAG_tmp_DAG[dag] == DAG_NULL)
		return;
	/* Do not touch the current substitution */
	if (is_variable(dag) && DAG_tmp_DAG[dag] != dag)
		return;

	DAG_free(DAG_tmp_DAG[dag]);
	DAG_tmp_DAG[dag] = DAG_NULL;
	for (unsigned i = 0; i < DAG_arity(dag); ++i)
		cleanup_substitution_short(DAG_arg(dag, i));
}

/** \remark We have to have our own reset routine, since some, but not all,
            subterms might have served for caching. */
static void
cleanup_substitution(TDAG dag)
{
	for (unsigned i = 0; i < DAG_arity(dag); ++i)
		cleanup_substitution(DAG_arg(dag, i));

	if (DAG_tmp_DAG[dag] == DAG_NULL)
		return;

	DAG_free(DAG_tmp_DAG[dag]);
	DAG_tmp_DAG[dag] = DAG_NULL;
}

/** \brief Performs the occurs check
    \remark This can probably be eliminated and rolled into
      `complete_substitution`, but doing it standalone is easier
      to understand. */
static bool
occurs_in(TDAG var, TDAG dag, Tstack_DAG weak_vars, Tstack_DAG strong_vars)
{
	if (is_free_in(var, dag))
		return true;
	if (ground(dag))
		return false;

	/* strong variables depend on _all_ variables in the strong_dag */
	bool has_strong = false;
	if (strong_vars != NULL && !stack_is_empty(strong_vars)) {
		assert (weak_vars != NULL);

		/* If at least one strong variable occurs */
		for (unsigned i = 0; i < stack_size(strong_vars); ++i) {
			if (is_free_in(stack_get(strong_vars, i), dag)) {
				has_strong = true;
				break;
			}
		}

		/* ... we consider this as an occurence of each weak variable, because
		       those would appear in the skolem term */
		if (has_strong) {
			if (stack_DAG_contains(weak_vars, var))
				return true;
			for (unsigned i = 0; i < stack_size(weak_vars); ++i) {
				TDAG weak_var = stack_get(weak_vars, i);
				TDAG n = DAG_tmp_DAG[weak_var];
				if (n != DAG_NULL) {
					if (occurs_in(var, n, weak_vars, strong_vars))
						return true;
				}
			}

		}
	}

	Tstack_DAG fvars = get_fvars(dag);
	for (unsigned i = 0; i < stack_size(fvars); ++i) {
		TDAG n = DAG_tmp_DAG[stack_get(fvars, i)];
		if (n != DAG_NULL) {
			/* Skip if we already checked */
			if (has_strong && stack_DAG_contains(weak_vars, n))
				continue;
			if (occurs_in(var, n, weak_vars, strong_vars))
				return true;
		}
	}
	return false;
}

/** \brief Recursively applies the calculated substitution to construct
       the unifier.
    \remark Caches results for subterms */
static TDAG
complete_substitution_rec(TDAG dag)
{
	assert(!DAG_quant(dag));
	/* The variable case has multiple subcases here (t := DAG_tmp_DAG[var]):
		 - If t is DAG_NULL the variable is not involved in the unifier
		 - If t is a variable we have to solve that one first
		 - Otherwise look at DAG_tmp_DAG[t] if it is DAG_NULL this one has not been
		   solved yet
		 - If it's not DAG_NULL, then we have to solve it */
	if (is_variable(dag)) {
		TDAG t = DAG_tmp_DAG[dag];
		if (t != DAG_NULL) {
			TDAG new;
			if (!is_variable(t) && DAG_tmp_DAG[t] != DAG_NULL)
				new = DAG_tmp_DAG[t];
			else
				new = complete_substitution_rec(t);
			DAG_tmp_DAG[dag] = DAG_dup(new);
			DAG_free(t);
			return new;
		} else
			return dag;
	}

	if (DAG_tmp_DAG[dag])
		return DAG_tmp_DAG[dag];
	if (ground(dag))
		return dag;

	TDAG* PDAG;
	MY_MALLOC(PDAG, DAG_arity(dag) * sizeof(TDAG));
	/* TODO: this might do some unecessary work if all children are known */
	for (unsigned i = 0; i < DAG_arity(dag); ++i) {
		PDAG[i] = complete_substitution_rec(DAG_arg(dag, i));
	}
	TDAG t = DAG_dup(DAG_new(DAG_symb(dag), DAG_arity(dag), PDAG));
	DAG_tmp_DAG[dag] = t;
	assert(DAG_sort(dag) == DAG_sort(t));
	return t;
}

static inline void
complete_substitution(Tstack_DAG vars)
{
	for (unsigned i = 0; i < stack_size(vars); ++i)
		complete_substitution_rec(stack_get(vars, i));

	/* We only perform a bit of cleanup here, since the rest can be done when
	   cleaning up the substitution. We clean eventual assignments to subterms
	   of the computed subsitution terms. */
	for (unsigned i = 0; i < stack_size(vars); ++i) {
		TDAG mapping = DAG_tmp_DAG[stack_get(vars, i)];
		cleanup_substitution(mapping);
	}
}

bool
unify_syntactic_vars(TDAG dag1, TDAG dag2, Tstack_DAG strong_vars)
{
	Tstack_DAG constraints;
	stack_INIT(constraints);

	Tstack_DAG vars;
	stack_INIT(vars);

	stack_push(constraints, dag1);
	stack_push(constraints, dag2);

	/* TODO: this is a hack */
	Tstack_DAG weak_vars = NULL;
	if (strong_vars && !stack_is_empty(strong_vars)) {
		Tstack_DAG dag2_fvars = get_fvars(dag2);

		if (dag2_fvars)
			weak_vars = stack_DAG_difference(dag2_fvars, strong_vars);
	}

	bool fail = false;
	while (!stack_is_empty(constraints)) {
		TDAG d1 = stack_pop(constraints);
		TDAG d2 = stack_pop(constraints);
		if (d1 == d2)
			continue;
		/* We are lazy and don't support nested quantifiers for now */
		if (quantifier(DAG_symb(d1)) || quantifier(DAG_symb(d2))) {
			fail = true;
			goto CLEANUP;
		}
		if (DAG_sort(d1) != DAG_sort(d2)) {
			fail = true;
			goto CLEANUP;
		}
		if (
			!is_variable_not_in_set(d1, strong_vars) &&
			!is_variable_not_in_set(d2, strong_vars)) {
			if (DAG_symb(d1) != DAG_symb(d2)) {
				fail = true;
				goto CLEANUP;
			}
			if (DAG_arity(d1) != DAG_arity(d2)) {
				fail = true;
				goto CLEANUP;
			}
			for (unsigned i = 0; i < DAG_arity(d1); ++i) {
				stack_push(constraints, DAG_arg(d1, i));
				stack_push(constraints, DAG_arg(d2, i));
			}
			continue;
		}
		if (!is_variable_not_in_set(d1, strong_vars))
			SWAP(d1, d2);
		/* If the variable has already some substitution assigned */
		if (DAG_tmp_DAG[d1] != DAG_NULL) {
			stack_push(constraints, DAG_tmp_DAG[d1]);
			stack_push(constraints, d2);
			continue;
		}
		if (occurs_in(d1, d2, weak_vars, strong_vars)) {
			fail = true;
			goto CLEANUP;
		}
		stack_push(vars, d1);
		DAG_tmp_DAG[d1] = DAG_dup(d2);
	}

	complete_substitution(vars);
CLEANUP:
	stack_free(vars);
	stack_free(constraints);
	if (weak_vars)
		stack_free(weak_vars);
	return !fail;
}

bool
unify_syntactic(TDAG dag1, TDAG dag2)
{
	return unify_syntactic_vars(dag1, dag2, NULL);
}

/* \brief This generates the term with the subsitution applied in DAG_tmp_DAG
   \remark this is very similar to DAG_tmp_inst, but for some reason we have to
           do the correct `DAG_dup`s here. */
static bool
update_tmp_for_substitution(TDAG dag)
{
	if (is_variable(dag)) {
		if (DAG_tmp_DAG[dag] == DAG_NULL)
			DAG_tmp_DAG[dag] = DAG_dup(dag);
		if (DAG_tmp_DAG[dag] == dag)
			return false;
		return true;
	}

	if (DAG_tmp_DAG[dag])
		return DAG_tmp_DAG[dag] != dag;

	bool res = false;
	for (unsigned i = 0; i < DAG_arity(dag); i++)
		res |= update_tmp_for_substitution(DAG_arg(dag, i));

	if (res) {
		if (DAG_symb(dag) == QUANTIFIER_FORALL) {
			Tstack_DAG DAGs;
			stack_INIT(DAGs);
			for (unsigned i = 0; i < DAG_arity(dag) - 1; i++)
				if (DAG_tmp_DAG[DAG_arg(dag, i)] == DAG_arg(dag, i))
					stack_push(DAGs, DAG_arg(dag, i));

			TDAG tmp_DAG;
			if (!stack_is_empty(DAGs)) {
				stack_push(DAGs, DAG_tmp_DAG[DAG_arg_last(dag)]);
				tmp_DAG = DAG_new_stack(QUANTIFIER_FORALL, DAGs);
			} else
				tmp_DAG = DAG_tmp_DAG[DAG_arg_last(dag)];
			stack_free(DAGs);
			DAG_tmp_DAG[dag] = DAG_dup(tmp_DAG);
			return true;
		}
		TDAG* PDAG;
		MY_MALLOC(PDAG, DAG_arity(dag) * sizeof(TDAG));
		for (unsigned i = 0; i < DAG_arity(dag); i++)
			PDAG[i] = DAG_dup(DAG_tmp_DAG[DAG_arg(dag, i)]);
		TDAG t = DAG_dup(DAG_new(DAG_symb(dag), DAG_arity(dag), PDAG));
		DAG_tmp_DAG[dag] = t;
		assert(DAG_sort(dag) == DAG_sort(t));
		return true;
	}

	DAG_tmp_DAG[dag] = DAG_dup(dag);
	return false;
}

TDAG
apply_substitution(TDAG dag)
{
	update_tmp_for_substitution(dag);
	TDAG new_dag = DAG_dup(DAG_tmp_DAG[dag]);
	cleanup_substitution_short(dag);
	return new_dag;
}

void
reset_substitution(TDAG dag1, TDAG dag2)
{
	cleanup_substitution(dag1);
	cleanup_substitution(dag2);
}

static bool
subsumes_rec(TDAG dag1, TDAG dag2)
{
	assert(!quantifier(DAG_symb(dag1)));
	assert(!quantifier(DAG_symb(dag2)));
	if (dag1 == dag2)
		return true;
	if (ground(dag1) && ground(dag2) && dag1 != dag2)
		return false;

	if (is_variable(dag1)) {
		if (DAG_tmp_DAG[dag1] == DAG_NULL) {
			if (is_free_in(dag1, dag2))
				return false;
			DAG_tmp_DAG[dag1] = dag2;
			return true;
		} else
			return DAG_tmp_DAG[dag1] == dag2 ? true : false;
	}

	if (DAG_symb(dag1) != DAG_symb(dag2) || DAG_arity(dag1) != DAG_arity(dag2))
		return false;

	for (unsigned i = 0; i < DAG_arity(dag1); ++i) {
		if (!subsumes_rec(DAG_arg(dag1, i), DAG_arg(dag2, i)))
			return false;
	}
	return true;
}

bool
subsumes(TDAG dag1, TDAG dag2)
{
	DAG_tmp_reserve();
	bool res = subsumes_rec(dag1, dag2);
	Tstack_DAG fvars = get_fvars(dag1);
	if (fvars) {
		for (unsigned i = 0; i < stack_size(fvars); ++i)
			DAG_tmp_DAG[stack_get(fvars, i)] = DAG_NULL;
	}
	DAG_tmp_release();
	return res;
}

