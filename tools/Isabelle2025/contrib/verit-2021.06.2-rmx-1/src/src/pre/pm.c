/**
   \file pm.c

*/

#include "pre/pm.h"

#include "symbolic/DAG-flag.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG-sort-pm.h"
#include "symbolic/DAG.h"
#include "utils/general.h"
#include "utils/list.h"
#include "utils/table.h"

#include <assert.h>
#include <string.h>

/* #define DEBUG_PM */

/** \brief data structure for axiom schema */
typedef struct TSschema
{
	TDAG DAG; /**< the DAG for the axiom schema itself */
	Tlist vars; /**< sort variables occuring in schema */
	Tlist sorts; /**< generic instances of parametric sort in schema*/
	Ttable substs; /**< substitutions already used to instantiate */
} * Tschema;

/** \brief */
Tlist* sort_classes;
size_t sort_classes_size;

/*                   Axiom schemata filtering                   */

/**
   \brief Auxiliary routine responsible for recursing over DAG

   \sa filter_axiom_schemata
*/
static void
filter_axiom_schemata_rec(TDAG DAG, Tlist* Plist)
{
	unsigned i;
	bool changed = false;
	if (DAG_Pflag(DAG)) return;
	if (DAG_symb(DAG) == CONNECTOR_AND) {
		for (i = 0; i < DAG_arity(DAG); ++i) {
			filter_axiom_schemata_rec(DAG_arg(DAG, i), Plist);
			changed |= DAG_arg(DAG, i) != DAG_of_ptr(DAG_Pflag(DAG_arg(DAG, i)));
		}
		if (changed) {
			TDAG *PDAG, tmp;
			MY_MALLOC(PDAG, DAG_arity(DAG) * sizeof(TDAG));
			for (i = 0; i < DAG_arity(DAG); ++i)
				PDAG[i] = DAG_of_ptr(DAG_Pflag(DAG_arg(DAG, i)));
			tmp = DAG_new(CONNECTOR_AND, DAG_arity(DAG), PDAG);
			DAG_Pflag_set(DAG, DAG_ptr_of(tmp));
		} else
			DAG_Pflag_set(DAG, DAG_ptr_of(DAG));
	} else if (quantifier(DAG_symb(DAG))) {
		i = 0;
		while (i < DAG_arity(DAG) - 1u &&
					 !DAG_sort_polymorphic(DAG_symb_sort(DAG_symb(DAG_arg(DAG, i)))))
			++i;
		if (i == DAG_arity(DAG) - 1u) {
			DAG_Pflag_set(DAG, DAG_ptr_of(DAG));
		} else {
			*Plist = list_add(*Plist, DAG_ptr_of(DAG_dup(DAG)));
			DAG_Pflag_set(DAG, DAG_ptr_of(DAG_TRUE));
		}
	} else {
		DAG_Pflag_set(DAG, DAG_ptr_of(DAG));
	}
}

/**
   \brief filters axiom schemata from DAG

   \param DAG a DAG (conjunction is assumed)

   \param Plist pointer to a list

   \return DAG where axiom schemata have been replaced by TRUE

   \note Adds to the list pointed to by Plist the axiom schemata
   replaced in DAG.

   \remark caller is responsible for freeing returned list and
   its DAG elements.

   \remark uses Pflag attribute on DAG and sub-DAGs.
*/

static TDAG
filter_axiom_schemata(TDAG DAG, Tlist* Plist)
{
	TDAG result;
	filter_axiom_schemata_rec(DAG, Plist);
	result = DAG_of_ptr(DAG_Pflag(DAG));
	DAG_reset_Pflag(DAG);
	list_apply(*Plist, (TFapply)&DAG_reset_Pflag);
	return DAG_dup(result);
}

/*                         Sort selection                       */

static void
sort_reset_misc(Tsort sort)
{
	unsigned i = 0;
	if (DAG_sort_is_marked(sort) == 0) return;
	DAG_sort_unmark(sort);
	if (DAG_sort_parametric(sort) && !DAG_sort_instance(sort)) return;
	for (i = 0; i < DAG_sort_arity(sort); ++i)
		sort_reset_misc(DAG_sort_sub(sort, i));
}

/**
   \param Plist pointer to a list of Tsort values

   \param sort a sort

   \remark Accumulates in the list pointed by Plist the sub-sorts
   of sort that are sort variables

   - no duplicates in the list pointed by Plist

   - misc set for all sorts stored in the list pointed to by Plist
*/

static void
sorts_rec_sort(Tlist* Plist, Tsort sort)
{
	unsigned i;
	if (DAG_sort_is_marked(sort)) return;
	DAG_sort_mark(sort);
	if (DAG_sort_variable(sort)) *Plist = list_add(*Plist, DAG_ptr_of_sort(sort));
	if (DAG_sort_parametric(sort) && !DAG_sort_instance(sort)) return;
	for (i = 0; i < DAG_sort_arity(sort); ++i)
		sorts_rec_sort(Plist, DAG_sort_sub(sort, i));
}

/**
   \param Plist pointer to a list of Tsort values

   \param DAG a DAG

   \param cond a predicate on Tsort values

   \return none

   \remark Accumulates in the list pointed by Plist the sorts (or
   sub-sorts thereof) of symbols in DAG that satisfy cond. Caveat:
   sorts of pre-defined symbols are not visited.

   - no duplicates in the list pointed by Plist

   - flag attribute is set recursively on all DAG nodes

   - misc attribute is set recursively on all sorts and their sub-sorts.
*/

static void
sorts_rec_DAG(Tlist* Plist, TDAG DAG, int (*cond)(Tsort))
{
	unsigned i;
	if (DAG_flag(DAG)) return;
	DAG_flag_set(DAG, 1);
	for (i = 0; i < DAG_arity(DAG); ++i)
		sorts_rec_DAG(Plist, DAG_arg(DAG, i), cond);
}

static void
sorts_rec_DAG_reset(TDAG DAG)
{
	unsigned i;
	if (!DAG_flag(DAG)) return;
	DAG_flag_set(DAG, 0);
	if (!(DAG_symb_type(DAG_symb(DAG)) & SYMB_PREDEFINED))
		sort_reset_misc(DAG_symb_sort(DAG_symb(DAG)));
	for (i = 0; i < DAG_arity(DAG); ++i) sorts_rec_DAG_reset(DAG_arg(DAG, i));
}

/**
   \brief Gets a list of sorts in DAG satisfying some condition

   \param DAG a DAG

   \param cond a predicate on Tsort values

   \return A list of Tsort values v such that cond(v) is non-null and
   v appears as a symbol sort or sub-sort in DAG. */
static Tlist
sorts_cond(TDAG DAG, int (*cond)(Tsort))
{
	Tlist result = NULL;
	sorts_rec_DAG(&result, DAG, cond);
	sorts_rec_DAG_reset(DAG);
	return result;
}

static int
generic_inst_sort(Tsort sort)
{
	return DAG_sort_instance(sort) && DAG_sort_polymorphic(sort);
}

static int
ground_inst_sort(Tsort sort)
{
	return DAG_sort_instance(sort) && !DAG_sort_polymorphic(sort);
}

/*
  --------------------------------------------------------------
  Basic routines for axiom schemata
  --------------------------------------------------------------
*/

/**
   \brief Creates a new axiom schema data structure

   \param DAG DAG of an axiom schema

   \return The schema corresponding to DAG. The table substs
   is initially empty.
*/
static Tschema
schema_new(TDAG DAG)
{
	unsigned i;
	Tschema result;
	MY_MALLOC(result, sizeof(struct TSschema));
	result->DAG = DAG_dup(DAG);
	result->substs = table_new(5, 5);
	result->vars = NULL;
	for (i = 0; i < DAG_arity(DAG) - 1u; ++i)
		sorts_rec_sort(&result->vars, DAG_sort(DAG_arg(DAG, i)));
	/* restore misc attribute on sorts */
	for (i = 0; i < DAG_arity(DAG) - 1; ++i)
		sort_reset_misc(DAG_sort(DAG_arg(DAG, i)));
	result->sorts = sorts_cond(DAG, generic_inst_sort);

	return result;
}

static void
schema_delete(Tschema schema)
{
	list_free(&schema->sorts);
	list_free(&schema->vars);
	table_apply(schema->substs, (TFapply)free);
	table_free(&schema->substs);
	DAG_free(schema->DAG);
	free(schema);
}

#ifdef DEBUG_PM
static void
schema_print(Tschema schema)
{
	Tlist list;
	int i, j;
	my_DAG_message("axiom schema: %D\n", schema->DAG);
	my_DAG_message("generic instances of parametric sorts:\n");
	list = schema->sorts;
	LIST_LOOP_BEGIN(list, Tsort, sort);
	my_DAG_message("\t%S\n", sort);
	LIST_LOOP_END(list);
	my_DAG_message("sort variables:\n");
	list = schema->vars;
	LIST_LOOP_BEGIN(list, Tsort, sort);
	my_DAG_message("\t%S\n", sort);
	LIST_LOOP_END(list);
	for (i = 0; i < table_length(schema->substs); ++i) {
		my_DAG_message("\tsubstitution %i\n", i + 1);
		for (j = 0; j < list_length(schema->vars); ++j)
			my_DAG_message("\t\t%S\n", ((Tsort*)table_get(schema->substs, i))[j]);
	}
}
#endif

/**
   \brief Stores the current binding of sort variables
   \param schema an axiom schema and related data
   \return 1 if the current binding is new for the current axiom
   schema, 0 if it was already stored.
*/
static int
schema_store_subst(Tschema schema)
{
	Tlist list;
	unsigned i, found, n;
	Tsort* Psort1;
	Tsort sort;
	list = schema->vars;
	n = list_length(list);
	MY_MALLOC(Psort1, n * sizeof(Tsort));
	i = 0;
	LIST_LOOP_BEGIN(list, void*, Psort);
	{
		sort = DAG_sort_of_ptr(Psort);
		assert(DAG_sort_binding(sort));
		Psort1[i++] = sort;
	}
	LIST_LOOP_END(list);
	for (i = found = 0; i < table_length(schema->substs) && !found; ++i) {
		Tsort* Psort2;
		Psort2 = (Tsort*)table_get(schema->substs, i);
		found = memcmp(Psort1, Psort2, n * sizeof(Tsort)) == 0;
	}
	if (!found)
		table_push(schema->substs, Psort1);
	else
		free(Psort1);
	return !found;
}

/**
   \brief auxiliary routine for sort_match_and_bind
   \sa sort_match_and_bind
*/
static int
sort_match_and_bind_rec(Tsort sort1, Tsort sort2, Tlist* Plist)
{
	unsigned i;
	if (DAG_sort_is_marked(sort1)) return 1;
	DAG_sort_mark(sort1);
	if (DAG_sort_variable(sort1)) {
		if (DAG_sort_binding(sort1) == DAG_SORT_NULL) {
			*Plist = list_add(*Plist, DAG_ptr_of_sort(sort1));
			DAG_sort_bind(sort1, sort2);
		}
		return DAG_sort_binding(sort1) == sort2;
	}
	if (
		DAG_sort_arity(sort1) == 0 ||
		(DAG_sort_parametric(sort1) && !DAG_sort_instance(sort1)))
		return sort1 == sort2;
	for (i = 0; i < DAG_sort_arity(sort1); ++i)
		if (!sort_match_and_bind_rec(
					DAG_sort_sub(sort1, i), DAG_sort_sub(sort2, i), Plist))
			return 0;
	return 1;
}

/**
   \brief Tries to match sort1 with sort2, recording bindings in
   the list pointed to by Plist.
   \param sort1 a generic instance of a parametric sort or its sub-sort
   \param sort2 a ground instance of a parametric sort or its sub-sort
   \param Plist pointer to a list of sort variables that have already
   been bound
   \return 1 if the substitution pointed by Plist could be extended when
   matching sort1 with sort2, 0 otherwise.
*/
static int
sort_match_and_bind(Tsort sort1, Tsort sort2, Tlist* Plist)
{
	int result;
	result = sort_match_and_bind_rec(sort1, sort2, Plist);
	sort_reset_misc(sort1);
	return result;
}

/**
   \brief instantiates an axiom schema
   \param schema the axiom schema data structure
   \param generic a list of generic instances of parametric sorts
   \param Pinst pointer to a list where new instances are stored
   \param count number of sort variables left to be bound
   \pre The attribute classes of parametric sort constructors is
   equal to the list of ground instances of parametric sorts to be
   used to find the instances.
*/
static void
instantiate(Tschema schema, Tlist generic, Tlist* Pinst, unsigned count)
{
	Tsort sort1 = DAG_sort_of_ptr(list_car(generic));
	Tlist ground = sort_classes[DAG_sort_sub(sort1, 0)];
	LIST_LOOP_BEGIN(ground, void*, Psort);
	{
		Tsort sort2 = DAG_sort_of_ptr(Psort);
		Tlist bindings = NULL;
		assert(DAG_sort_sub(sort1, 0) == DAG_sort_sub(sort2, 0));
		if (sort_match_and_bind(sort1, sort2, &bindings)) {
			unsigned count2;
			assert(count >= list_length(bindings));
			count2 = list_length(bindings);
			if (count == count2) {
				if (schema_store_subst(schema))
					*Pinst =
						list_add(*Pinst, DAG_ptr_of(DAG_sort_subst_DAG(NULL, schema->DAG)));
				else if (generic != list_prev(schema->sorts))
					instantiate(schema, list_cdr(generic), Pinst, count - count2);
			}
		}
		list_apply(bindings, (TFapply)&DAG_sort_unbind_rec);
		list_free(&bindings);
	}
	LIST_LOOP_END(ground);
	if (generic != list_prev(schema->sorts))
		instantiate(schema, list_cdr(generic), Pinst, count);
}

/**
   \remark Internal use of attribute misc on symbols.

   \note The routine assumes that src is a conjunction of axioms,
   additional assumptions, and the (negated) goal. Shall such
   assumption be relaxed, routine pm_qnt_formulas would have to be
   rewritten.

   \note This routine processes values of type Tsymb. There are three
   kinds of values:

   - Polymorphic symbol declarations, e.g. pair: these are declarations
   and do not occur in src. These are named P-declarations henceforth.

   - Polymorphic instances of P-declarations, e.g. pair: 's * 't -> ('s
   't Pair): such symbols are used in axioms that define the semantics
   of P-declarations: universally quantified formulas over variables of
   a some polymorphic sort. These are named P-symbols henceforth.

   - Non-polymorphic instances of P-declarations, e.g.  pair: int * int
   -> (Int Int Pair): these occur in the goal formula. These are named
   g-symbols henceforth.
*/
TDAG
pm_process(TDAG src)
{
	Tlist schemata; /*< list of axiom schemata found in src */
	Tlist DAGs; /*< list of axiom schemata found in src */
	Tlist decls; /*< declarations of parametric sorts appearing in schemata */
	Tlist ground; /*< ground instances of parametric sorts */
	Tstack_DAG instances; /*< DAGs of schemata instances */
	TDAG result;
	Tsort sort_max;

	DAGs = NULL;
	result = filter_axiom_schemata(src, &DAGs);

	if (DAGs == NULL) return result;

#ifdef DEBUG_PM
	my_DAG_message("DAGs for axiom schemata:\n");
	list_apply(DAGs, (TFapply)DAG_print);
	my_DAG_message("\n");
#endif

	schemata = NULL;
	LIST_LOOP_BEGIN_DAG(DAGs, DAG);
	schemata = list_add(schemata, schema_new(DAG));
	LIST_LOOP_END_DAG(DAGs);
	list_apply(DAGs, (TFapply)DAG_free);
	list_free(&DAGs);

#ifdef DEBUG_PM
	list_apply(schemata, (TFapply)schema_print);
#endif

	/* assign decls with the parametric sorts of interest */
	decls = NULL;
	sort_max = 0;
	LIST_LOOP_BEGIN(schemata, Tschema, schema);
	{
		Tlist list = schema->sorts;
		LIST_LOOP_BEGIN(list, void*, P);
		{
			Tsort sort = DAG_sort_of_ptr(P);
			Tsort decl = DAG_sort_sub(sort, 0);
			assert(DAG_sort_parametric(decl));
			if (DAG_sort_is_marked(decl)) {
				DAG_sort_mark(decl);
				decls = list_cons(DAG_ptr_of_sort(decl), decls);
				if (sort > sort_max) sort_max = sort;
			}
		}
		LIST_LOOP_END(list);
	}
	LIST_LOOP_END(schemata);
	LIST_LOOP_BEGIN(decls, void*, P);
	{
		Tsort sort = DAG_sort_of_ptr(P);
		assert(DAG_sort_parametric(sort) && DAG_sort_is_marked(sort) == 1);
		DAG_sort_unmark(sort);
	}
	LIST_LOOP_END(decls);
	sort_max = 0;
	LIST_LOOP_BEGIN(decls, void*, P);
	{
		Tsort sort = DAG_sort_of_ptr(P);
		if (sort > sort_max) sort_max = sort;
	}
	LIST_LOOP_END(decls);
	sort_classes_size = (size_t)(sort_max + 1);
	MY_MALLOC(sort_classes, sort_classes_size * sizeof(Tlist));
	memset(sort_classes, '\0', sort_classes_size * sizeof(Tlist));

	/* Parametric sorts have misc attribute set */
#ifdef DEBUG_PM
	my_DAG_message("Parametric sorts appearing in schemata:\n");
	LIST_LOOP_BEGIN(decls, Tsort, sort);
	my_DAG_message("\t%S\n", sort);
	LIST_LOOP_END(decls);
#endif

	ground = sorts_cond(result, ground_inst_sort);
#ifdef DEBUG_PM
	my_DAG_message("Ground instances of parametric sorts found in formula:\n");
	LIST_LOOP_BEGIN(ground, Tsort, sort);
	my_DAG_message("\t%S\n", sort);
	LIST_LOOP_END(ground);
#endif

	/* The instantiation process is iterative. Indeed an instance of an
     axiom schema may contain an instance of a parametric sort that do
     not appear in the original formula.

     TODO: design a static analysis to check that this ends, e.g.
     (forall (l (List 's)) (= 1 (length (cons l nil)))) should be
     forbidden!
  */
	stack_INIT(instances);
	while (ground) {
		Tlist instances2; /* new instances found in iteration */
		Tlist ground2; /* new ground sorts found in iteration */
		Tsort decl;
		LIST_LOOP_BEGIN(ground, void*, P);
		{
			decl = DAG_sort_sub(DAG_sort_of_ptr(P), 0);
			sort_classes[decl] = list_add(sort_classes[decl], P);
		}
		LIST_LOOP_END(ground);

#ifdef DEBUG_PM
		LIST_LOOP_BEGIN(decls, Tsort, sort);
		{
			Tlist ground = sort_classes[sort];
			my_DAG_message("Ground instances for parametric sort %S:\n", sort);
			LIST_LOOP_BEGIN(ground, Tsort, sort2);
			my_DAG_message("\t%S\n", sort2);
			LIST_LOOP_END(ground);
		}
		LIST_LOOP_END(decls);
#endif

		instances2 = NULL;
		LIST_LOOP_BEGIN(schemata, Tschema, schema);
		instantiate(schema, schema->sorts, &instances2, list_length(schema->vars));
		LIST_LOOP_END(schemata);
		ground2 = NULL;
		LIST_LOOP_BEGIN_DAG(instances2, DAG);
		ground2 = list_merge(ground2, sorts_cond(DAG, ground_inst_sort));
		LIST_LOOP_END_DAG(instances2);
		LIST_LOOP_BEGIN_DAG(instances2, DAG);
		stack_push(instances, DAG);
		LIST_LOOP_END_DAG(instances2);
		list_free(&instances2);
		while (ground) {
			Tsort sort = DAG_sort_of_ptr(list_car(ground));
			DAG_sort_mark(sort);
			ground = list_remove(ground);
		}
		while (ground2) {
			void* P = list_car(ground2);
			Tsort sort = DAG_sort_of_ptr(P);
			if (DAG_sort_is_marked(sort))
				DAG_sort_unmark(sort);
			else
				ground = list_add(ground, P);
			ground2 = list_remove(ground2);
		}
		LIST_LOOP_BEGIN(decls, void*, P);
		decl = DAG_sort_of_ptr(P);
		list_free(&sort_classes[decl]);
		LIST_LOOP_END(decls);
	}
	free(sort_classes);
	if (stack_size(instances) > 0) {
		TDAG tmp;
		stack_push(instances, result);
		tmp = DAG_new_stack(CONNECTOR_AND, instances);
		DAG_free(result);
		result = DAG_dup(tmp);
	}
	stack_free(instances);
#ifdef DEBUG_PM
	my_DAG_message("pm input: %D\n", src);
	my_DAG_message("pm output: %D\n", result);
#endif
	list_apply(schemata, (TFfree)schema_delete);
	list_free(&schemata);
	list_free(&decls);
	return result;
}

void
pm_process_array(unsigned n, TDAG* Psrc)
{
	unsigned i;
	for (i = 0; i < n; i++) {
		TDAG dest = pm_process(Psrc[i]);
		DAG_free(Psrc[i]);
		Psrc[i] = dest;
	}
}
