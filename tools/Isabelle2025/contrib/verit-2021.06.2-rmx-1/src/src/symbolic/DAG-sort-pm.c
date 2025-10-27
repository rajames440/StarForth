#include "symbolic/DAG-sort-pm.h"

#include "symbolic/DAG-flag.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG-sort.h"
#include "symbolic/DAG.h"
#include "utils/assoc.h"
#include "utils/general.h"
#include "utils/stack.h"

/* #define DEBUG_TYPE_VARIABLES */

static Tstack_sort sort_stack = NULL;

/*
  --------------------------------------------------------------
  Sort subsumption
  --------------------------------------------------------------
*/

static bool
subsumes_rec(Tsort sort1, Tsort sort2)
{
	unsigned i;
	bool res = true;

	if (DAG_sort_combine(sort1, sort2) != DAG_SORT_NULL) return true;
	/* PF No variable, should be = to sort 2 in previous test */
	if (DAG_sort_parametric(sort1)) return false;
	if (DAG_sort_binding(sort1)) return DAG_sort_binding(sort1) == sort2;
	if (DAG_sort_variable(sort1)) {
		assert(!DAG_sort_binding(sort1));
		DAG_sort_bind(sort1, sort2);
		return true;
	}
	if (DAG_sort_arity(sort1) == DAG_SORT_NARY) {
		unsigned n =
			(DAG_sort_arity(sort2) == DAG_SORT_NARY) ? 2 : DAG_sort_arity(sort2);
		res = subsumes_rec(DAG_sort_sub(sort1, 1), DAG_sort_sub(sort2, n - 1u));
		for (i = 0; i < n - 1u && res; ++i)
			res &= subsumes_rec(DAG_sort_sub(sort1, 0), DAG_sort_sub(sort2, i));
	} else if (DAG_sort_arity(sort2) == DAG_SORT_NARY) {
		/* PF2DD: why not require that sort1 be NARY to subsume sort2? */
		assert(DAG_sort_arity(sort1) != DAG_SORT_NARY);
		if (DAG_sort_arity(sort1) < 2) return false;
		res = subsumes_rec(
			DAG_sort_sub(sort1, DAG_sort_arity(sort1) - 1), DAG_sort_sub(sort2, 1));
		for (i = 0; i < DAG_sort_arity(sort1) - 1u && res; ++i)
			res &= subsumes_rec(DAG_sort_sub(sort1, i), DAG_sort_sub(sort2, 0));
	} else {
		if (
			DAG_sort_arity(sort1) != DAG_sort_arity(sort2) ||
			DAG_sort_arity(sort1) == 0)
			return false;
		/* PF2DD: why test only first element in
         DAG_sort_sub(sort1, 0) != DAG_sort_sub(sort2, 0) */
		if (DAG_sort_instance(sort1))
			if (
				!DAG_sort_instance(sort2) ||
				DAG_sort_sub(sort1, 0) != DAG_sort_sub(sort2, 0))
				return false;
		for (i = 0, res = 1; i < DAG_sort_arity(sort1) && res; ++i)
			res &= subsumes_rec(DAG_sort_sub(sort1, i), DAG_sort_sub(sort2, i));
	}
	if (res) {
		DAG_sort_bind(sort1, sort2);
		stack_push(sort_stack, sort1);
	}
	return res;
}

bool
DAG_sort_subsumes(Tsort sort1, Tsort sort2)
{
	bool result;
	stack_INIT(sort_stack);
	result = subsumes_rec(sort1, sort2);
	stack_apply(sort_stack, DAG_sort_unbind);
	stack_free(sort_stack);
	return result;
}

/*
  --------------------------------------------------------------
  Sort unification
  --------------------------------------------------------------
*/

static Tsort DAG_sort_subst_sort(Tlist constraints, Tsort sort);
static void DAG_sort_subst_sort2(Tsort sort, Tstack_sort* Psorts);

#ifdef DEBUG_TYPE_VARIABLES
static void
sort_assoc_list_print(Tlist list)
{
	LIST_LOOP_BEGIN(list, Tassoc, assoc);
	my_DAG_message("\t%S <=> %S\n", (Tsort)assoc->key, (Tsort)assoc->value);
	LIST_LOOP_END(list);
}
#endif

/**
   \brief checks if a sort variable appears free in a sort
   \return true if so, false otherwise
   \param sort1 a sort variable
   \param sort2 a sort
   \pre sort1 is a sort variable */
static bool
DAG_sort_is_free(const Tsort sort1, const Tsort sort2)
{
	unsigned i;
	assert(DAG_sort_variable(sort1));
	if (sort1 == sort2) return true;
	if (!DAG_sort_polymorphic(sort2)) return false;
	assert(DAG_sort_arity(sort2) != DAG_SORT_NARY);
	for (i = 0; i < DAG_sort_arity(sort2); ++i)
		if (DAG_sort_is_free(sort1, DAG_sort_sub(sort2, i))) return true;
	return false;
}

/* TODO: when constraint is unsatisfiable propagate to caller instead
  of stopping execution. The caller will be able to produce a message
  with a more meaningful context for the user. */
/**
   \brief adds a sort unification constraint
   \param Plist a pointer to a list of sort constraints
   \param sort1 a sort
   \param sort2 a sort
   \note The list pointed to by Plist is added the constraint formed
   by sort1 and sort2, or left unchanged.
   - A constraint is represented by an association of sort1 with sort2
   (Tassoc). If either one is polymorphic while the other is not, 
   then it is set as the key of the association.
   - If sort1 == sort2, the list pointed to by Plist is left unchanged
   - If the constraint formed by sort1 and sort2 is trivially
   not satisfiable, the program halts and an error message is printed.
   \remark Ref: "Types and Programming Languages", by Benjamin C. Pierce */
static void
DAG_sort_unif_constrain(Tlist* Plist, Tsort sort1, Tsort sort2)
{
	Tassoc assoc;

#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_sort_unif_constrain: %S <=> %S\n", sort1, sort2);
#endif
	if (DAG_sort_combine(sort1, sort2)) return;

	if (DAG_sort_polymorphic(sort2) && !DAG_sort_polymorphic(sort1))
		SWAP(sort1, sort2);

	if (DAG_sort_variable(sort2) && !DAG_sort_variable(sort1)) SWAP(sort1, sort2);

	if (DAG_sort_variable(sort1)) {
		if (DAG_sort_is_free(sort1, sort2))
			my_DAG_error("Sort %S cannot be unified with sort %S.\n", sort1, sort2);

	} else if (DAG_sort_arity(sort1) != DAG_sort_arity(sort2))
		my_DAG_error("Sort %S and %S mismatch.\n", sort1, sort2);

	MY_MALLOC(assoc, sizeof(struct TSassoc));
	assoc->key = DAG_ptr_of_sort(sort1);
	assoc->value = DAG_ptr_of_sort(sort2);
	/* the solver is more efficient when constraints between a sort
     variable and another sort appear first, so they are inserted
     at the head of the list of constraints. */
	if (DAG_sort_variable(sort1))
		*Plist = list_cons(assoc, *Plist);
	else
		*Plist = list_add(*Plist, assoc);
}

/**
   \brief computes the most general unifier (mgu) for sort unification
   constraints
   \param Plist a pointer to the list of sort unification 
   constraints
   \remark When the function returns Plist points to a list of assoc
   pairs that store the mgu sort substitution.
   \sa DAG_sort_unif_constrain */
static void
DAG_sort_unif_solve(Tlist* Plist)
{
	Tlist result;
#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_sort_unif_solve\n");
	sort_assoc_list_print(*Plist);
#endif
	result = NULL;
	while (*Plist) {
		Tassoc assoc = (Tassoc)list_car(*Plist);
		Tsort sort1 = DAG_sort_of_ptr(assoc->key);
		Tsort sort2 = DAG_sort_of_ptr(assoc->value);
		*Plist = list_remove(*Plist);
		if (DAG_sort_variable(sort1)) {
			Tlist tmp = NULL;
			Tstack_sort sorts;
			Tsort sort22;
			unsigned i;
			stack_INIT(sorts);
			DAG_sort_bind(sort1, sort2);
			stack_push(sorts, sort1);
			result = list_cons(assoc, result);
			LIST_LOOP_BEGIN(result, Tassoc, assoc2);
			sort22 = DAG_sort_of_ptr(assoc2->value);
			DAG_sort_subst_sort2(sort22, &sorts);
			assoc2->value = DAG_ptr_of_sort(DAG_sort_binding(sort22));
			LIST_LOOP_END(result);
			while (*Plist) {
				Tassoc assoc2 = NULL;
				Tsort sort12, sort22;
				assoc2 = (Tassoc)list_car(*Plist);
				*Plist = list_remove(*Plist);
				DAG_sort_subst_sort2(DAG_sort_of_ptr(assoc2->key), &sorts);
				DAG_sort_subst_sort2(DAG_sort_of_ptr(assoc2->value), &sorts);
				sort12 = DAG_sort_binding(DAG_sort_of_ptr(assoc2->key));
				sort22 = DAG_sort_binding(DAG_sort_of_ptr(assoc2->value));
				free(assoc2);
				DAG_sort_unif_constrain(&tmp, sort12, sort22);
			}
			for (i = 0; i < stack_size(sorts); ++i)
				DAG_sort_unbind(stack_get(sorts, i));
			stack_free(sorts);
			*Plist = tmp;
		} else if (!DAG_sort_parametric(sort1)) {
			unsigned i;
			assert(DAG_sort_arity(sort1) != DAG_SORT_NARY);
			assert(DAG_sort_arity(sort1) > 0);
			assert(DAG_sort_arity(sort1) == DAG_sort_arity(sort2));
			for (i = 0; i < DAG_sort_arity(sort1); ++i)
				DAG_sort_unif_constrain(
					Plist, DAG_sort_sub(sort1, i), DAG_sort_sub(sort2, i));
			free(assoc);
		}
	}
#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_sort_unif_solve output:\n");
	sort_assoc_list_print(result);
#endif
	*Plist = result;
}

/**
   \brief destructor for a sort substitution
   \param Plist pointer to the list of sort substitutions */
static void
DAG_sort_unif_delete(Tlist* Plist)
{
	Tlist list = *Plist;
	*Plist = NULL;
	while (list) {
		Tassoc assoc = (Tassoc)list_car(list);
		free(assoc);
		list = list_remove(list);
	}
}

Tsort
DAG_sort_unif_pair(Tsort sort1, Tsort sort2)
{
	Tsort result;
#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_sort_unif_pair(%S, %S)\n", sort1, sort2);
#endif
	if (sort1 == sort2)
		result = sort1;
	else if (!DAG_sort_polymorphic(sort1) && !DAG_sort_polymorphic(sort2))
		result = DAG_sort_combine(sort1, sort2);
	else {
		Tlist constraints = NULL;
		DAG_sort_unif_constrain(&constraints, sort1, sort2);
		DAG_sort_unif_solve(&constraints);
		result = DAG_sort_subst_sort(constraints, sort1);
		DAG_sort_unif_delete(&constraints);
	}
#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_sort_unif_pair(%S, %S) -> %S\n", sort1, sort2, result);
#endif
	return result;
}

Tsort
DAG_sort_unif_apply(
	const Tsort* Psort1, const Tsort* Psort2, const unsigned n, const Tsort sort)
{
	unsigned i;
	Tsort result;
#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_sort_unif_apply(...%i, %S)\n", n, sort);
#endif
	if (!DAG_sort_polymorphic(sort)) {
		result = sort;
		for (i = 0; i < n && result != DAG_SORT_NULL; ++i) {
			if (
				!DAG_sort_polymorphic(Psort1[i]) && !DAG_sort_polymorphic(Psort2[i]) &&
				!DAG_sort_combine(Psort1[i], Psort2[i]))
				result = DAG_SORT_NULL;
		}
	} else {
		Tlist unif = NULL;
		/* IMPROVE This code takes silently advantage of an asymetry in
         DAG_sort_unif_constrain to replace Psort2[i] with Psort1[i] */
		for (i = 0; i < n; ++i)
			DAG_sort_unif_constrain(&unif, Psort2[i], Psort1[i]);
		DAG_sort_unif_solve(&unif);
		result = DAG_sort_subst_sort(unif, sort);
		DAG_sort_unif_delete(&unif);
	}
#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_sort_unif_apply(...%i, %S) -> %S\n", n, sort, result);
#endif
	return result;
}

Tsort
DAG_sort_unif_apply_polyadic(
	const Tsort* Psort, const Tsort sort1, const unsigned n, const Tsort sort2)
{
	unsigned i;
	Tsort result;

#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message(
		"DAG_sort_unif_apply_polyadic(...%S, %i, %S)\n", sort1, n, sort2);
#endif
	if (DAG_sort_polymorphic(sort1)) {
		result = sort2;
		for (i = 0; i < n && result != DAG_SORT_NULL; ++i) {
			if (!DAG_sort_combine(Psort[i], sort1)) result = DAG_SORT_NULL;
		}
	} else {
		Tlist unif = NULL;
		/* IMPROVE This code takes silently advantage of an asymetry in
         DAG_sort_unif_constrain to replace sort1 with Psort[i] */
		for (i = 0; i < n; ++i) DAG_sort_unif_constrain(&unif, sort1, Psort[i]);
		DAG_sort_unif_solve(&unif);
		result = DAG_sort_subst_sort(unif, sort2);
		DAG_sort_unif_delete(&unif);
	}
#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message(
		"DAG_sort_unif_apply_polyadic(...%S, %i, %S) -> %S\n", sort1, n, sort2,
		result);
#endif
	return result;
}

/*
  --------------------------------------------------------------
  Sort substitution
  --------------------------------------------------------------
*/

/**
   \brief applies a sort substitution \f$\sigma\f$ to a sort
   \param sort a sort
   \param Psorts a pointer to a stack to store the sorts that are bound in the
   substitution (and need later to be unbound)
   \post DAG_sort_binding(sort) the result of the substitution of sort
   (idem for its subsorts)
   \pre \f$\sigma\f$ is represented in the binding field of sort variables
   \sa DAG_sort_unbind_rec, DAG_sort_subst_sort */
static void
DAG_sort_subst_sort2(Tsort sort, Tstack_sort* Psorts)
{
	unsigned arity = DAG_sort_arity(sort);
	unsigned i;
	int changed;
	Tsort* Psort;

	assert(arity != DAG_SORT_NARY);
	if (DAG_sort_binding(sort)) return;
	stack_push(*Psorts, sort);
	if (arity == 0 || !DAG_sort_polymorphic(sort) || DAG_sort_parametric(sort)) {
		DAG_sort_bind(sort, sort);
		return;
	}
	for (i = 0, changed = 0; i < arity; ++i) {
		DAG_sort_subst_sort2(DAG_sort_sub(sort, i), Psorts);
		changed |=
			(DAG_sort_sub(sort, i) != DAG_sort_binding(DAG_sort_sub(sort, i)));
	}
	if (!changed) {
		DAG_sort_bind(sort, sort);
		return;
	}
	if (DAG_sort_instance(sort)) {
		MY_MALLOC(Psort, (arity - 1) * sizeof(Tsort));
		for (i = 1; i < arity; ++i)
			Psort[i - 1u] = DAG_sort_binding(DAG_sort_sub(sort, i));
		DAG_sort_bind(
			sort, DAG_sort_new_inst(NULL, DAG_sort_sub(sort, 0), arity - 1u, Psort));
	} else {
		MY_MALLOC(Psort, arity * sizeof(Tsort));
		for (i = 0; i < arity; ++i)
			Psort[i] = DAG_sort_binding(DAG_sort_sub(sort, i));
		DAG_sort_bind(sort, DAG_sort_new(NULL, arity, Psort));
	}
}

/**
   \param list list of sort constraints (pairs of sorts)
   \param sort Sort to be unified under the list of sort constraints
   \return returns the result of the unification of sort by the most
   general unifier satisfying list.

   \return sort if there is no most general unifier.
*/
static Tsort
DAG_sort_subst_sort(Tlist list, Tsort sort)
{
	Tsort result;
	Tstack_sort sorts; /* records all sorts bound within this call */
	Tsort sort1, sort2;
	unsigned i;
	stack_INIT(sorts);
	LIST_LOOP_BEGIN(list, Tassoc, assoc);
	sort1 = DAG_sort_of_ptr(assoc->key);
	sort2 = DAG_sort_of_ptr(assoc->value);
	DAG_sort_bind(sort1, sort2);
	stack_push(sorts, sort1);
	LIST_LOOP_END(list);

	DAG_sort_subst_sort2(sort, &sorts);
	result = DAG_sort_binding(sort);

	for (i = 0; i < stack_size(sorts); ++i) DAG_sort_unbind(stack_get(sorts, i));
	stack_free(sorts);

#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("given sort: %S\n", sort);
	my_DAG_message("result sort: %S\n", result);
#endif
	return result;
}

#define SET_SUBST_DAG(DAG1, DAG2) \
	{ \
		TDAG tmp; \
		tmp = DAG2; \
		((Tassoc)(DAG_Pflag(DAG1)))->key = DAG_ptr_of(tmp); \
	}

#define GET_SUBST_DAG(DAG1) (DAG_of_ptr(((Tassoc)(DAG_Pflag(DAG1)))->key))

static void
subst_DAG_rec(TDAG src, Tstack_sort* Psorts)
{
	Tsymb symb;
	TDAG* PDAG = NULL;
	unsigned i;
	bool changed = false;
	Tassoc assoc;

	if (DAG_flag(src)) return;
	DAG_flag_set(src, 1);

	MY_MALLOC(assoc, sizeof(struct TSassoc));
	assoc->value = DAG_Pflag(src);
	DAG_Pflag_set(src, assoc);

	/* find head symbol of result */
	if (DAG_symb_type(DAG_symb(src)) & SYMB_PREDEFINED)
		symb = DAG_symb(src);
	else if (DAG_symb_misc(
						 DAG_symb(src))) /* if head symbol is to be substituted */
	{
		symb = (Tsymb)DAG_symb_misc(DAG_symb(src));
		changed = true;
	} else {
		Tsort sort;
		DAG_sort_subst_sort2(DAG_sort(src), Psorts);
		sort = DAG_sort_binding(DAG_sort(src));
		if (
			DAG_sort(src) != sort && (DAG_symb_type(DAG_symb(src)) & SYMB_VARIABLE)) {
			symb = DAG_symb_variable(sort);
			DAG_symb_set_misc(DAG_symb(src), (int)symb);
			changed = true;
		} else
			symb = DAG_symb(src);
	}
	/* apply recursively to arguments */
	for (i = 0; i < DAG_arity(src); ++i) {
		subst_DAG_rec(DAG_arg(src, i), Psorts);
		changed |= (GET_SUBST_DAG(DAG_arg(src, i)) != DAG_arg(src, i));
	}

	if (!changed) {
		SET_SUBST_DAG(src, DAG_dup(src));
		return;
	}

	MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
	for (i = 0; i < DAG_arity(src); ++i) PDAG[i] = GET_SUBST_DAG(DAG_arg(src, i));
	SET_SUBST_DAG(src, DAG_dup(DAG_new(symb, DAG_arity(src), PDAG)));

	if (quantifier(DAG_symb(src))) {
		Tstack_DAGstack* Pannot = DAG_prop_get(src, DAG_PROP_TRIGGER);
		if (Pannot) {
			unsigned i, j;
			Tstack_DAGstack annot2;
			stack_INIT_s(annot2, stack_size(*Pannot));
			for (i = 0; i < stack_size(*Pannot); ++i) {
				Tstack_DAG trigger = stack_get(*Pannot, i);
				Tstack_DAG trigger2;
				stack_INIT_s(trigger2, stack_size(trigger));
				for (j = 0; j < stack_size(trigger); ++j) {
					TDAG DAG = stack_get(trigger, j);
					subst_DAG_rec(DAG, Psorts);
					stack_push(trigger2, DAG_dup(GET_SUBST_DAG(DAG)));
				}
				stack_push(annot2, trigger2);
			}
			DAG_prop_set(GET_SUBST_DAG(src), DAG_PROP_TRIGGER, &annot2);
		}
	}
}

static void
subst_DAG_restore(TDAG src)
{
	unsigned i;
	Tassoc assoc;

	if (DAG_flag(src) == 0) return;
	DAG_flag_set(src, 0);

	if (quantifier(DAG_symb(src))) {
		Tstack_DAGstack annot = DAG_prop_get(src, DAG_PROP_TRIGGER);
		if (annot) {
			unsigned i;
			for (i = 0; i < stack_size(annot); ++i) {
				Tstack_DAG trigger = stack_get(annot, i);
				unsigned j;
				for (j = 0; j < stack_size(trigger); ++j)
					subst_DAG_restore(stack_get(trigger, j));
			}
		}
	}

	for (i = 0; i < DAG_arity(src); ++i) subst_DAG_restore(DAG_arg(src, i));

	DAG_free(GET_SUBST_DAG(src));
	assoc = (Tassoc)DAG_Pflag(src);
	DAG_Pflag_set(src, assoc->value);
	free(assoc);
}

TDAG
DAG_sort_subst_DAG(Tlist list, TDAG src)
{
	TDAG result;
	Tstack_sort sorts; /* records all sorts bound within this call */
	Tsort sort1, sort2;
	unsigned i;

	stack_INIT(sorts);
	/* for each pair (s1, s2) in list, bind s1 with s2 */
	LIST_LOOP_BEGIN(list, Tassoc, assoc);
	sort1 = DAG_sort_of_ptr(assoc->key);
	sort2 = DAG_sort_of_ptr(assoc->value);
	DAG_sort_bind(sort1, sort2);
	stack_push(sorts, sort1);
	LIST_LOOP_END(list);

	subst_DAG_rec(src, &sorts);
	result = DAG_dup(GET_SUBST_DAG(src));

	subst_DAG_restore(src);
	for (i = 0; i < stack_size(sorts); ++i) DAG_sort_unbind(stack_get(sorts, i));
	stack_free(sorts);

	return result;
}
