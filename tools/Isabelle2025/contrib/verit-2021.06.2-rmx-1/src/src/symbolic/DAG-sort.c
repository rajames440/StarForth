/*
  --------------------------------------------------------------
  Sort utilities
  --------------------------------------------------------------
*/
#include "symbolic/DAG-sort.h"

#include "utils/general.h"

#include <stdio.h>
#include <string.h>

typedef struct Tname_sort
{
	char* name;
	Tsort sort;
} Tname_sort;

TSstack(_name_sort, Tname_sort);

static Tstack_name_sort name_sort_stack;

Tstack_Ssort DAG_sort_stack;

/** \brief associates sorts to sets of symbols; used for symbols masks */
Tstack_unsigned* sort_symbols;

#define v_arity(A) (((A) == DAG_SORT_NARY) ? 2 : (A))

/*
  --------------------------------------------------------------
  Debugging routines
  --------------------------------------------------------------
*/

#ifdef DEBUG
void
DAG_sort_table_print(void)
{
	unsigned i, j;
	fprintf(
		stderr, "%5s %15s %5s %4s %4s %4s %4s %4s\n", "index", "name", "arity",
		"para", "poly", "inst", "vari", "sub");
	for (i = 1; i < DAG_sort_stack->size; ++i) {
		fprintf(
			stderr, "%5i %15s %5i %4i %4i %4i %4i ", i,
			DAG_sort_name(i) ? DAG_sort_name(i) : "", DAG_sort_arity(i),
			DAG_sort_parametric(i), DAG_sort_polymorphic(i), DAG_sort_instance(i),
			DAG_sort_variable(i));
		if (DAG_sort_arity(i) == DAG_SORT_NARY)
			fprintf(stderr, "{%i+ %i", DAG_sort_sub(i, 0), DAG_sort_sub(i, 1));
		else if (DAG_sort_arity(i) == 0 || DAG_sort_parametric(i))
			fprintf(stderr, "{");
		else
			for (j = 0; j < DAG_sort_arity(i); ++j)
				fprintf(stderr, "%c%i", j == 0 ? '{' : ' ', DAG_sort_sub(i, j));
		fprintf(stderr, "}\n");
	}
}
#endif

/*
  --------------------------------------------------------------
  Invariant checking (only in DEBUG mode)
  --------------------------------------------------------------
*/

#ifdef DEBUG
/**
   \brief Invariant check for Tsort values

   \param sort a sort

   \return 1 if sort satisfies invariant properties, 0 otherwise.

   \note a value of type Tsort is either:

   - a variable sort: it is then polymorphic, has null arity and
   no sub-sort, is neither an instance or parametric.

   - an instance of a parametric sort: it has positive arity and
   sub-sorts; it is not a sort variable; its first sub-sort is a
   parametric sort constructor and the arity is one plus the arity of
   the constructor.

   - a parametric sort constructor has positive arity and the
   array of sub-sorts is NULL; it is not a sort variable.

   - a sort that is polymorphic is not a parametric sort constructor.

   - a sort of arity zero is polymorphic it is a sort variable.

   - a sort of positive arity is polymorphic if at least one sub-sort is
   polymorphic */
int
DAG_sort_invariant(Tsort sort)
{
	if (
		DAG_sort_parametric(sort) &&
		(DAG_sort_variable(sort) || DAG_sort_instance(sort)))
		return 0;
	if (
		DAG_sort_instance(sort) &&
		(DAG_sort_variable(sort) || DAG_sort_parametric(sort)))
		return 0;
	if (
		DAG_sort_variable(sort) &&
		(DAG_sort_instance(sort) || DAG_sort_parametric(sort)))
		return 0;
	if (DAG_sort_polymorphic(sort)) {
		if (DAG_sort_arity(sort) == 0) {
			if (!DAG_sort_variable(sort)) return 0;
		} else {
			unsigned i;
			for (i = 1; i < DAG_sort_arity(sort); ++i)
				if (DAG_sort_polymorphic(DAG_sort_sub(sort, i))) break;
			if (i == DAG_sort_arity(sort)) return 0;
		}
	}
	if (DAG_sort_parametric(sort))
		return DAG_sort_arity(sort) > 0 && DAG_sort_stack->data[sort].sub != NULL;
	if (DAG_sort_instance(sort))
		return DAG_sort_arity(sort) > 0 && DAG_sort_stack->data[sort].sub != NULL &&
			DAG_sort_parametric(DAG_sort_sub(sort, 0)) &&
			DAG_sort_arity(sort) == DAG_sort_arity(DAG_sort_sub(sort, 0)) + 1;
	if (DAG_sort_variable(sort))
		return DAG_sort_polymorphic(sort) && DAG_sort_arity(sort) == 0 &&
			DAG_sort_stack->data[sort].sub == NULL;
	return 1;
}
#endif

/*
  --------------------------------------------------------------
  Constructors, destructors, basic setters and getters
  --------------------------------------------------------------
*/

Tsort
DAG_sort_lookup(const char* const name)
{
	unsigned i;
	assert(name);
	for (i = 0; i < stack_size(name_sort_stack); ++i)
		if (!strcmp(stack_get(name_sort_stack, i).name, name))
			return stack_get(name_sort_stack, i).sort;
	return DAG_SORT_NULL;
}

static inline Tsort
DAG_sort_set_name(const Tsort s, const char* name)
{
	if (!name) return s;
	if (DAG_sort_lookup(name)) {
		if (DAG_sort_lookup(name) != s)
			my_error("Sort %d is defined twice\n", name);
#ifdef DEBUG
		my_warning("Sort %d is defined twice\n", name);
#endif
		return s;
	}
	stack_inc(name_sort_stack);
	stack_top(name_sort_stack).name = strmake(name);
	stack_top(name_sort_stack).sort = s;
	if (!stack_get(DAG_sort_stack, s).name)
		stack_get(DAG_sort_stack, s).name = stack_top(name_sort_stack).name;
	return s;
}

Tsort
DAG_sort_new(const char* const name, const unsigned arity, Tsort* const sub)
{
	unsigned i;
	/* Check if sort with same name already defined */
	Tsort tmp = name ? DAG_sort_lookup(name) : DAG_SORT_NULL;
	/* There is a sort with same name */
	if (tmp != DAG_SORT_NULL) {
		/* Check if the same, otherwise output error */
		if (DAG_sort_parametric(tmp))
			my_error("Sort %s defined as parametric and non-parametric", name);
		if (arity != DAG_sort_arity(tmp))
			my_error("Sort %s defined twice with different arities\n", name);
		for (i = 0; i < v_arity(arity); i++)
			if (sub[i] != DAG_sort_sub(tmp, i))
				my_error(
					"Sort %s defined twice with different component sorts\n", name);
		free(sub);
		return tmp;
	}
	/* Check if an alias */
	if (arity == 1) {
		DAG_sort_set_name(sub[0], name);
		free(sub);
	}
	/* Check if there exists a similar sort */
	if (arity)
		for (tmp = 1; tmp < DAG_sort_stack->size; tmp++) {
			if (DAG_sort_parametric(tmp) || DAG_sort_arity(tmp) != arity) continue;
			for (i = 0; i < v_arity(arity) && sub[i] == DAG_sort_sub(tmp, i); i++)
				;
			if (i < v_arity(arity)) continue;
			/* PF found identical sort, set alias */
			DAG_sort_set_name(tmp, name);
			free(sub);
			return tmp;
		}
	/* Create new sort */
	tmp = stack_size(DAG_sort_stack);
	stack_inc(DAG_sort_stack);
	stack_top(DAG_sort_stack).name = NULL;
	stack_top(DAG_sort_stack).arity = arity;
	stack_top(DAG_sort_stack).sub = sub;
	stack_top(DAG_sort_stack).mark = false;
	stack_top(DAG_sort_stack).binding = DAG_SORT_NULL;
	stack_top(DAG_sort_stack).variable = false;
	stack_top(DAG_sort_stack).instance =
		(arity > 0 && DAG_sort_parametric(sub[0]));
	stack_top(DAG_sort_stack).parametric = false;
	stack_top(DAG_sort_stack).polymorphic = false;
	for (i = 0; i < v_arity(arity); ++i)
		stack_top(DAG_sort_stack).polymorphic |= DAG_sort_polymorphic(sub[i]);
	stack_top(DAG_sort_stack).predefined = false;
	DAG_sort_set_name(tmp, name);

	/* For symbol mask */
	MY_REALLOC(sort_symbols, (tmp + 1) * sizeof(Tstack_unsigned));
	sort_symbols[tmp] = NULL;

	return tmp;
}

Tsort
DAG_sort_new_args(const char* const name, const unsigned arity, ...)
{
	va_list adpar;
	Tsort sort;
	unsigned arity2 = 0;
	Tsort* subs = NULL;
	va_start(adpar, arity);
	while ((sort = va_arg(adpar, Tsort)) != DAG_SORT_NULL) {
		MY_REALLOC(subs, (arity2 + 1) * sizeof(Tsort));
		subs[arity2++] = sort;
	}
	if ((arity == DAG_SORT_NARY ? 2 : arity) != arity2)
		my_error("DAG_sort_new_args: incompatible number of arguments\n");
	va_end(adpar);
	return DAG_sort_new(name, arity, subs);
}

Tsort
DAG_sort_new_var(const char* const name)
{
	static unsigned long counter = 0;
	Tsort tmp = DAG_SORT_NULL;
	if (name == NULL) {
		char* name2;
		unsigned size = ul_str_size(counter);
		MY_MALLOC(name2, sizeof(char) * (size + 3));
		sprintf(name2, "'_%lu", counter);
		counter++;
		tmp = DAG_sort_new(name2, 0, NULL);
		free(name2);
	} else
		tmp = DAG_sort_new(name, 0, NULL);
	DAG_sort_stack->data[tmp].variable = true;
	DAG_sort_stack->data[tmp].polymorphic = true;
	return tmp;
}

Tsort
DAG_sort_new_param(const char* const name, const unsigned arity)
{
	Tsort tmp = DAG_SORT_NULL;
	if (arity == 0 || arity == DAG_SORT_NARY)
		my_error("Parametric sort must have positive arity\n");
	if (name == NULL) my_error("Parametric sort must be named\n");
	tmp = DAG_sort_lookup(name);
	if (tmp) {
		if (!DAG_sort_parametric(tmp))
			my_error("Sort %s defined as parametric and non-parametric\n", name);
		if (arity != DAG_sort_arity(tmp))
			my_error("Sort %s defined twice with different arities\n", name);
		my_warning("Sort %s defined twice\n", name);
		return tmp;
	}
	tmp = stack_size(DAG_sort_stack);
	stack_inc(DAG_sort_stack);
	stack_top(DAG_sort_stack).name = NULL;
	stack_top(DAG_sort_stack).arity = arity;
	stack_top(DAG_sort_stack).sub = NULL;
	stack_top(DAG_sort_stack).mark = false;
	stack_top(DAG_sort_stack).binding = DAG_SORT_NULL;
	stack_top(DAG_sort_stack).polymorphic = false;
	stack_top(DAG_sort_stack).instance = false;
	stack_top(DAG_sort_stack).variable = false;
	stack_top(DAG_sort_stack).parametric = true;
	stack_top(DAG_sort_stack).predefined = false;
	DAG_sort_set_name(tmp, name);

	/* For symbol mask */
	MY_REALLOC(sort_symbols, (tmp + 1) * sizeof(Tstack_unsigned));
	sort_symbols[tmp] = NULL;

	return tmp;
}

Tsort
DAG_sort_new_inst(
	const char* const name, const Tsort sort, const unsigned arity,
	Tsort* const sub)
{
	unsigned i, j;
	Tsort tmp, *sub2;
	bool same;
	assert(DAG_sort_arity(sort) == arity);
	if (!arity) {
		assert(!DAG_sort_parametric(sort));
		DAG_sort_set_name(sort, name);
		return sort;
	}
	assert(DAG_sort_parametric(sort));
	assert(arity > 0);
	for (i = 1; i < DAG_sort_stack->size; ++i) {
		tmp = i;
		if (!DAG_sort_instance(tmp)) continue;
		if (DAG_sort_arity(tmp) != DAG_sort_arity(sort) + 1) continue;
		if (DAG_sort_sub(tmp, 0) != sort) continue;
		for (j = 1, same = true; j <= arity && same; ++j)
			same = sub[j - 1] == DAG_sort_sub(tmp, j);
		if (!same) continue;
		if (name && DAG_sort_name(tmp) && strcmp(name, DAG_sort_name(tmp)) == 0)
			my_error(
				"Duplicate sort definitions (%s, %s)\n", name, DAG_sort_name(tmp));
		if (!DAG_sort_name(tmp) && name) DAG_sort_set_name(tmp, name);
		free(sub);
		return tmp;
	}
	tmp = stack_size(DAG_sort_stack);
	stack_inc(DAG_sort_stack);
	stack_top(DAG_sort_stack).name = NULL;
	stack_top(DAG_sort_stack).arity = arity + 1;
	MY_MALLOC(sub2, (arity + 1) * sizeof(Tsort));
	sub2[0] = sort;
	memcpy(sub2 + 1, sub, arity * sizeof(Tsort));
	stack_top(DAG_sort_stack).sub = sub2;
	stack_top(DAG_sort_stack).mark = false;
	stack_top(DAG_sort_stack).binding = DAG_SORT_NULL;
	stack_top(DAG_sort_stack).parametric = false;
	stack_top(DAG_sort_stack).variable = false;
	stack_top(DAG_sort_stack).instance = true;
	stack_top(DAG_sort_stack).polymorphic = false;
	for (i = 0; i < arity; ++i)
		stack_top(DAG_sort_stack).polymorphic |= DAG_sort_polymorphic(sub[i]);
	stack_top(DAG_sort_stack).predefined = false;
	DAG_sort_set_name(tmp, name);
	free(sub);

	/* For symbol mask */
	MY_REALLOC(sort_symbols, (tmp + 1) * sizeof(Tstack_unsigned));
	sort_symbols[tmp] = NULL;

	return tmp;
}

void
DAG_sort_unbind_rec(const Tsort sort)
{
	unsigned i;
	if (sort == DAG_SORT_NULL || !DAG_sort_binding(sort)) return;
	DAG_sort_unbind(sort);
	if (DAG_sort_parametric(sort)) return;
	for (i = 0; i < v_arity(DAG_sort_arity(sort)); ++i)
		DAG_sort_unbind_rec(DAG_sort_sub(sort, i));
}

/*
  --------------------------------------------------------------
  Module initialization and release
  --------------------------------------------------------------
*/

void
DAG_sort_init(void)
{
	stack_INIT(name_sort_stack);
	stack_INIT(DAG_sort_stack);
	/* Reserve ((Tsort) 0) */
	stack_inc(DAG_sort_stack);

	/* For symbol mask */
	MY_MALLOC(sort_symbols, sizeof(Tstack_unsigned));
	sort_symbols[0] = NULL;
}

void
DAG_sort_done(void)
{
	unsigned i;
	Tsort tmp;
	for (tmp = 1; tmp < stack_size(DAG_sort_stack); ++tmp) {
		free(stack_get(DAG_sort_stack, tmp).sub);
		/* For symbol mask */
		if (sort_symbols[tmp]) stack_free(sort_symbols[tmp]);
	}
	free(sort_symbols);
	stack_free(DAG_sort_stack);
	for (i = 0; i < stack_size(name_sort_stack); i++)
		free(stack_get(name_sort_stack, i).name);
	stack_free(name_sort_stack);
}
