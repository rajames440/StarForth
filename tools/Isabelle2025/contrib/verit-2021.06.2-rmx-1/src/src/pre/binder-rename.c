/*
  --------------------------------------------------------------
  binder renaming in terms and formulas
  --------------------------------------------------------------
*/

#include "pre/binder-rename.h"

#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG.h"
#include "symbolic/recursion.h"
#include "utils/general.h"

#include <assert.h>
#include <stdlib.h>

#define symb_misc(s) DAG_symb_misc(s)
#define symb_set_misc(s, v) DAG_symb_set_misc(s, v)

/* #define DEBUG_BINDER_RENAME */

#define SORT_MAP_INIT_SIZE 32

static struct
{
	size_t size;
	Tsort* array;
} sort_map = { 0, NULL };

static void
sort_map_insert(Tsort key, Tsort value)
{
	size_t j;
	if (sort_map.size == 0) {
		MY_MALLOC(sort_map.array, SORT_MAP_INIT_SIZE * sizeof(Tsort));
		sort_map.size = SORT_MAP_INIT_SIZE;
		for (j = 0; j < sort_map.size; ++j) sort_map.array[j] = DAG_SORT_NULL;
	}
	if (sort_map.size <= (size_t)key) {
		size_t size = sort_map.size;
		size_t size2 = 2 * (key + 1);
		MY_REALLOC(sort_map.array, size2 * sizeof(Tsort));
		for (j = size; j < size2; ++j) sort_map.array[j] = DAG_SORT_NULL;
		sort_map.size = size2;
	}
	assert(sort_map.size > key);
	sort_map.array[key] = value;
}

static void
sort_map_reset_value(Tsort key)
{
	assert(sort_map.size > key);
	sort_map.array[key] = DAG_SORT_NULL;
}

static Tsort
sort_map_lookup(Tsort key)
{
	if (key > sort_map.size) return DAG_SORT_NULL;
	return sort_map.array[key];
}

static Tsort
binder_rename_sort2(Tsort sort, Tstack_sort sorts)
{
	Tsort* Psort;
	unsigned i, arity;

	if (!DAG_sort_polymorphic(sort)) return sort;

	if (DAG_sort_variable(sort)) {
		Tsort sort2 = sort_map_lookup(sort);
		if (sort2 == DAG_SORT_NULL) {
			sort2 = DAG_sort_new_var(NULL);
			sort_map_insert(sort, sort2);
			stack_push(sorts, sort);
		}
		return sort2;
	}
	arity = DAG_sort_arity(sort);
	MY_MALLOC(Psort, arity * sizeof(*Psort));
	for (i = 0; i < arity; ++i)
		Psort[i] = binder_rename_sort2(DAG_sort_sub(sort, i), sorts);
	return DAG_sort_new(DAG_sort_name(sort), arity, Psort);
}

/**
   \brief Recursively renames bound symbols (variables and sort
   variables)
   \param src a DAG */
static TDAG
binder_rename_aux(TDAG src)
{
	unsigned i;
	TDAG dest;
	TDAG* PDAG;

	if (DAG_symb(src) == LET) {
		TDAG* tmp;
		int* Pint;
		Tstack_sort sorts;
		stack_INIT(sorts);
		assert(DAG_arity(src) >= 3);
		/* Rename all let-terms */
		MY_MALLOC(tmp, DAG_arity(src) * sizeof(TDAG));
		MY_MALLOC(Pint, DAG_arity(src) * sizeof(*Pint));
		for (i = 1; i < DAG_arity(src) - 1u; i += 2)
			tmp[i] = binder_rename_aux(DAG_arg(src, i));
		for (i = 0; i < DAG_arity(src) - 1u; i += 2) {
			Tsymb symb;
			Tsort sort = DAG_sort(DAG_arg(src, i));
			Tsort sort2 = sort_map_lookup(sort);
			if (sort2 == DAG_SORT_NULL) sort2 = binder_rename_sort2(sort, sorts);
			symb = DAG_symb_variable(sort2);
			tmp[i] = DAG_dup(DAG_new(symb, 0, NULL));
			Pint[i] = symb_misc(DAG_symb(DAG_arg(src, i)));
			symb_set_misc(DAG_symb(DAG_arg(src, i)), (int)symb);
		}
		/* Rename main term according to let */
		tmp[DAG_arity(src) - 1u] = binder_rename_aux(DAG_arg_last(src));
		dest = DAG_new(LET, DAG_arity(src), tmp);
		/* i is unsigned: do not use decreasing loop */
		for (i = 0; i < DAG_arity(src) - 1u; i += 2) {
			symb_set_misc(DAG_symb(DAG_arg(src, i)), Pint[i]);
			DAG_free(DAG_arg(dest, i));
		}
		free(Pint);
		for (i = 0; i < sorts->size; i++) sort_map_reset_value(sorts->data[i]);
		stack_free(sorts);
	} else if (binder(DAG_symb(src)) && DAG_arity(src) >= 2) {
		TDAG* tmp;
		int* Pint;
		Tstack_sort sorts;
		stack_INIT(sorts);

		MY_MALLOC(tmp, DAG_arity(src) * sizeof(TDAG));
		MY_MALLOC(Pint, (DAG_arity(src) - 1u) * sizeof(*Pint));
		for (i = 0; i < DAG_arity(src) - 1u; ++i) {
			Tsymb symb;
			Tsort sort = DAG_sort(DAG_arg(src, i));
			Tsort sort2 = sort_map_lookup(sort);
			if (sort2 == DAG_SORT_NULL) sort2 = binder_rename_sort2(sort, sorts);
			symb = DAG_symb_variable(sort2);
			tmp[i] = DAG_dup(DAG_new(symb, 0, NULL));
			Pint[i] = symb_misc(DAG_symb(DAG_arg(src, i)));
			symb_set_misc(DAG_symb(DAG_arg(src, i)), (int)symb);
		}
		tmp[DAG_arity(src) - 1] = binder_rename_aux(DAG_arg_last(src));
		dest = DAG_new(DAG_symb(src), DAG_arity(src), tmp);
		if (quantifier(DAG_symb(src))) {
			Tstack_DAGstack* Pannot = DAG_prop_get(src, DAG_PROP_TRIGGER);
			if (Pannot) {
				Tstack_DAGstack annot2;
				unsigned i, j;
				stack_INIT_s(annot2, stack_size(*Pannot));
				for (i = 0; i < stack_size(*Pannot); ++i) {
					Tstack_DAG trigger = stack_get(*Pannot, i);
					Tstack_DAG trigger2;
					stack_INIT_s(trigger2, stack_size(trigger));
					for (j = 0; j < stack_size(trigger); ++j) {
						TDAG DAG = stack_get(trigger, j);
						TDAG DAG2 = binder_rename_aux(DAG);
						stack_push(trigger2, DAG_dup(DAG2));
					}
					stack_push(annot2, trigger2);
				}
				DAG_prop_set(dest, DAG_PROP_TRIGGER, &annot2);
			}
		}
		/* i is unsigned: do not use decreasing loop */
		for (i = 0; i < DAG_arity(src) - 1u; ++i) {
			symb_set_misc(DAG_symb(DAG_arg(src, i)), Pint[i]);
			DAG_free(DAG_arg(dest, i));
		}
		free(Pint);
		for (i = 0; i < sorts->size; i++) sort_map_reset_value(sorts->data[i]);
		stack_free(sorts);
	} else {
		Tsymb symb = DAG_symb(src);
		if (symb_misc(symb) != DAG_SYMB_NULL) symb = (Tsymb)symb_misc(symb);
		MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
		for (i = 0; i < DAG_arity(src); i++)
			PDAG[i] = binder_rename_aux(DAG_arg(src, i));
		dest = DAG_new(symb, DAG_arity(src), PDAG);
	}
	return dest;
}

TDAG
binder_rename(TDAG src)
{
	TDAG dest;
#ifdef DEBUG_BINDER_RENAME
	my_DAG_message("Before binder renaming\n%D\n", src);
#endif
	dest = DAG_dup(binder_rename_aux(src));
#ifdef DEBUG_BINDER_RENAME
	my_DAG_message("After binder renaming\n%D\n", dest);
#endif
	return dest;
}

void
binder_rename_array(unsigned n, TDAG* Psrc)
{
	unsigned i;
	for (i = 0; i < n; i++) {
		TDAG dest = DAG_dup(binder_rename_aux(Psrc[i]));
		DAG_free(Psrc[i]);
		Psrc[i] = dest;
	}
}
