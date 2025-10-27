#include "instantiation/discrimination-tree.h"

#include "picotest.h"
#include "symbolic/DAG-sort.h"
#include "symbolic/DAG.h"
#include "symbolic/veriT-DAG.h"
#include "utils/options.h"
#include "utils/statistics.h"

/* TODO: we don't have a test for adding meta information yet */
PICOTEST_SUITE(discriminationTreeSuite, createDT, insertGroundDT,
insertOverlapGroundDT, variableInIndexDT, variableInQueryDT, variableOverlappDT,
narySymbolsDT, ignoredVariablesDT)

PICOTEST_FIXTURE_CONTEXT(dags)
{
	Tsort S; /* A sort */
	Tsymb x_symb;
	TDAG x; /* A variable */
	Tsymb y_symb;
	TDAG y; /* Another variable */
	Tsymb a_symb;
	TDAG a; /* A constant */
	Tsymb b_symb;
	TDAG b; /* Another constant */
	Tsymb g_symb; /* A unary function symbol */
	Tsymb f1_symb; /* Another unary function symbol */
	Tsymb f2_symb; /* A binary function symbol */
	Tsymb f3_symb; /* A trenary function symbol */
	Tsymb P_symb; /* A predicate symbol */
	Tsymb z_symb; /* A boolen variable symbol */
	Tsymb z; /* A boolen variable */
};

static inline bool
in_DAG_stack(Tstack_DAG s, TDAG t)
{
	for (unsigned i = 0; i < stack_size(s); ++i) {
		if (stack_get(s, i) == t)
			return true;
	}
	return false;
}

static inline Tsort*
all_sorts(unsigned n, Tsort S)
{
	Tsort* list;
	MY_MALLOC(list, n * sizeof(Tsort));
	for (unsigned i = 0; i < n; ++i) list[i] = S;
	return list;
}

PICOTEST_FIXTURE_SETUP(dags, context)
{
	standardEnv_init();
	context->S = DAG_sort_new("S", 0, NULL);
	context->x_symb = DAG_symb_new("x", SYMB_VARIABLE, context->S);
	context->y_symb = DAG_symb_new("y", SYMB_VARIABLE, context->S);
	context->a_symb = DAG_symb_new("a", 0, context->S);
	context->b_symb = DAG_symb_new("b", 0, context->S);

	Tsort* sort = all_sorts(2, context->S);
	context->g_symb = DAG_symb_new("g", 0, DAG_sort_new(NULL, 2, sort));
	sort = all_sorts(2, context->S);
	context->f1_symb = DAG_symb_new("f1", 0, DAG_sort_new(NULL, 2, sort));
	sort = all_sorts(3, context->S);
	context->f2_symb = DAG_symb_new("f2", 0, DAG_sort_new(NULL, 3, sort));
	sort = all_sorts(4, context->S);
	context->f3_symb = DAG_symb_new("f3", 0, DAG_sort_new(NULL, 4, sort));

	Tsort* list;
	MY_MALLOC(list, 2 * sizeof(Tsort));
	list[0] = context->S;
	list[1] = SORT_BOOLEAN;
	context->P_symb =
		DAG_symb_new("P", SYMB_PREDICATE, DAG_sort_new(NULL, 2, list));

	context->z_symb = DAG_symb_new("z", SYMB_VARIABLE, SORT_BOOLEAN);

	context->x = DAG_dup(DAG_new_nullary(context->x_symb));
	context->y = DAG_dup(DAG_new_nullary(context->y_symb));
	context->z = DAG_dup(DAG_new_nullary(context->z_symb));
	context->a = DAG_dup(DAG_new_nullary(context->a_symb));
	context->b = DAG_dup(DAG_new_nullary(context->b_symb));
}

PICOTEST_FIXTURE_TEARDOWN(dags, context)
{
	DAG_free(context->x);
	DAG_free(context->y);
	DAG_free(context->z);
	DAG_free(context->a);
	DAG_free(context->b);
	standardEnv_done();
}

PICOTEST_CASE(createDT, dags, context)
{
	Tdisc_tree* dt = disc_tree_INIT();
	PICOTEST_ASSERT(dt != NULL);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 0);
	disc_tree_free(dt);
}

PICOTEST_CASE(insertGroundDT, dags, context)
{
	Tdisc_tree* dt = disc_tree_INIT();
	PICOTEST_ASSERT(dt != NULL);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 0);
	TDAG g_a = DAG_new_unary(context->g_symb, context->a);
	TDAG term = DAG_dup(DAG_new_binary(context->f2_symb, g_a, context->b));

	disc_tree_insert(dt, term);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 1);

	Tstack_DAG result = disc_tree_lookup(dt, term);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(stack_get(result, 0) == term);

	disc_tree_insert(dt, term);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 2);
	stack_free(result);

	result = disc_tree_lookup(dt, term);
	PICOTEST_ASSERT(stack_size(result) == 2);
	PICOTEST_ASSERT(stack_get(result, 0) == term);
	PICOTEST_ASSERT(stack_get(result, 1) == term);

	stack_free(result);
	DAG_free(term);
	disc_tree_free(dt);
}

PICOTEST_CASE(insertOverlapGroundDT, dags, context)
{
	/* Create a bunch of terms */
	TDAG g_a = DAG_dup(DAG_new_unary(context->g_symb, context->a));
	TDAG g_b = DAG_dup(DAG_new_unary(context->g_symb, context->b));
	TDAG g_g_a = DAG_dup(DAG_new_unary(context->g_symb, g_a));
	TDAG g_g_b = DAG_dup(DAG_new_unary(context->g_symb, g_b));
	TDAG f2_g_a_b = DAG_dup(DAG_new_binary(context->f2_symb, g_a, context->b));
	TDAG f2_g_a_g_a = DAG_dup(DAG_new_binary(context->f2_symb, g_a, g_a));
	TDAG f2_g_b_g_a = DAG_dup(DAG_new_binary(context->f2_symb, g_b, g_a));
	TDAG f2_g_a_g_b = DAG_dup(DAG_new_binary(context->f2_symb, g_a, g_b));
	TDAG f2_g_b_g_b = DAG_dup(DAG_new_binary(context->f2_symb, g_b, g_b));
	TDAG f3_1 =
		DAG_dup(DAG_new_args(context->f3_symb, f2_g_b_g_b, g_b, g_a, DAG_NULL));
	TDAG f3_2 = DAG_dup(
		DAG_new_args(context->f3_symb, f2_g_b_g_a, g_b, context->a, DAG_NULL));

	const TDAG ds[] = { DAG_dup(context->a),
											DAG_dup(context->b),
											g_a,
											g_b,
											g_g_a,
											g_g_b,
											f2_g_a_b,
											f2_g_a_g_a,
											f2_g_b_g_a,
											f2_g_a_g_b,
											f2_g_b_g_b,
											f3_1,
											f3_2 };
	unsigned n = sizeof(ds) / sizeof(TDAG);

	Tdisc_tree* dt = disc_tree_INIT();
	PICOTEST_ASSERT(dt != NULL);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 0);

	for (unsigned i = 0; i < n; ++i) disc_tree_insert(dt, ds[i]);

	PICOTEST_ASSERT(disc_tree_num_terms(dt) == n);

	for (unsigned i = 0; i < n; ++i) {
		Tstack_DAG result = disc_tree_lookup(dt, ds[i]);
		PICOTEST_ASSERT(stack_size(result) == 1);
		PICOTEST_ASSERT(stack_get(result, 0) == ds[i]);
		stack_free(result);
	}

	disc_tree_free(dt);
	dt = disc_tree_INIT();

	/* Let's do the same again, but insert the other way arround */
	for (unsigned i = 1; i <= n; ++i) disc_tree_insert(dt, ds[n - i]);

	PICOTEST_ASSERT(disc_tree_num_terms(dt) == n);

	for (unsigned i = 0; i < n; ++i) {
		Tstack_DAG result = disc_tree_lookup(dt, ds[i]);
		PICOTEST_ASSERT(stack_size(result) == 1);
		PICOTEST_ASSERT(stack_get(result, 0) == ds[i]);
		stack_free(result);
	}

	disc_tree_free(dt);

	for (unsigned i = 0; i < n; ++i) DAG_free(ds[i]);
}

PICOTEST_CASE(variableInIndexDT, dags, context)
{
	TDAG g_a = DAG_dup(DAG_new_unary(context->g_symb, context->a));
	TDAG g_b = DAG_dup(DAG_new_unary(context->g_symb, context->b));
	TDAG g_g_a = DAG_dup(DAG_new_unary(context->g_symb, g_a));
	TDAG g_g_b = DAG_dup(DAG_new_unary(context->g_symb, g_b));
	TDAG f2_ground_match = DAG_dup(DAG_new_binary(context->f2_symb, g_a, g_g_b));
	TDAG f2_ground_no_match =
		DAG_dup(DAG_new_binary(context->f2_symb, g_a, g_g_a));
	TDAG f2_v1 = DAG_dup(DAG_new_binary(context->f2_symb, context->x, g_g_b));
	TDAG f2_v2 = DAG_dup(DAG_new_binary(context->f2_symb, g_a, context->y));

	Tdisc_tree* dt = disc_tree_INIT();
	PICOTEST_ASSERT(dt != NULL);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 0);

	disc_tree_insert(dt, f2_v1);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 1);

	/* We don't test if we can retrieve f2_v1, because this is a query with a
	   variable in the query test which is another test. */

	disc_tree_insert(dt, f2_ground_match);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 2);

	Tstack_DAG result = disc_tree_lookup(dt, f2_ground_match);
	PICOTEST_ASSERT(stack_size(result) == 2);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v1));
	PICOTEST_ASSERT(in_DAG_stack(result, f2_ground_match));
	stack_free(result);

	result = disc_tree_lookup(dt, f2_ground_no_match);
	PICOTEST_ASSERT(stack_is_empty(result));
	stack_free(result);

	disc_tree_insert(dt, f2_v2);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 3);

	result = disc_tree_lookup(dt, f2_ground_match);
	PICOTEST_ASSERT(stack_size(result) == 3);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v1));
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v2));
	PICOTEST_ASSERT(in_DAG_stack(result, f2_ground_match));
	stack_free(result);

	result = disc_tree_lookup(dt, f2_ground_no_match);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v2));
	stack_free(result);

	disc_tree_free(dt);

	DAG_free(g_a);
	DAG_free(g_b);
	DAG_free(g_g_a);
	DAG_free(g_g_b);
	DAG_free(f2_ground_match);
	DAG_free(f2_ground_no_match);
	DAG_free(f2_v1);
	DAG_free(f2_v2);
}

PICOTEST_CASE(variableInQueryDT, dags, context)
{
	TDAG g_a = DAG_dup(DAG_new_unary(context->g_symb, context->a));
	TDAG g_x = DAG_dup(DAG_new_unary(context->g_symb, context->x));
	TDAG g_b = DAG_dup(DAG_new_unary(context->g_symb, context->b));
	TDAG g_g_a = DAG_dup(DAG_new_unary(context->g_symb, g_a));
	TDAG g_g_b = DAG_dup(DAG_new_unary(context->g_symb, g_b));
	TDAG f2_m1 = DAG_dup(DAG_new_binary(context->f2_symb, g_g_a, g_a));
	TDAG f2_m2 = DAG_dup(DAG_new_binary(context->f2_symb, g_g_b, g_a));
	TDAG f2_nm = DAG_dup(DAG_new_binary(context->f2_symb, g_g_b, g_b));
	TDAG f2_v1 = DAG_dup(DAG_new_binary(context->f2_symb, context->x, g_a));
	TDAG f2_v2 = DAG_dup(DAG_new_binary(context->f2_symb, g_g_b, context->y));

	Tdisc_tree* dt = disc_tree_INIT();
	PICOTEST_ASSERT(dt != NULL);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 0);

	disc_tree_insert(dt, g_a);
	disc_tree_insert(dt, f2_m1);
	disc_tree_insert(dt, f2_m2);
	disc_tree_insert(dt, f2_nm);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 4);

	Tstack_DAG result = disc_tree_lookup(dt, g_x);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, g_a));
	stack_free(result);

	result = disc_tree_lookup(dt, f2_v1);
	PICOTEST_ASSERT(stack_size(result) == 2);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_m1));
	PICOTEST_ASSERT(in_DAG_stack(result, f2_m2));
	PICOTEST_ASSERT(!in_DAG_stack(result, f2_nm));
	stack_free(result);

	result = disc_tree_lookup(dt, context->x);
	PICOTEST_ASSERT(stack_size(result) == 4);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_m1));
	PICOTEST_ASSERT(in_DAG_stack(result, f2_m2));
	PICOTEST_ASSERT(in_DAG_stack(result, f2_nm));
	PICOTEST_ASSERT(in_DAG_stack(result, g_a));
	stack_free(result);

	result = disc_tree_lookup(dt, f2_v2);
	PICOTEST_ASSERT(stack_size(result) == 2);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_m2));
	PICOTEST_ASSERT(in_DAG_stack(result, f2_nm));
	stack_free(result);

	disc_tree_free(dt);

	DAG_free(g_a);
	DAG_free(g_x);
	DAG_free(g_b);
	DAG_free(g_g_a);
	DAG_free(g_g_b);
	DAG_free(f2_m1);
	DAG_free(f2_m2);
	DAG_free(f2_nm);
	DAG_free(f2_v1);
	DAG_free(f2_v2);
}

PICOTEST_CASE(variableOverlappDT, dags, context)
{
	TDAG g_a = DAG_dup(DAG_new_unary(context->g_symb, context->a));
	TDAG g_b = DAG_dup(DAG_new_unary(context->g_symb, context->b));
	TDAG g_g_a = DAG_dup(DAG_new_unary(context->g_symb, g_a));
	TDAG g_g_b = DAG_dup(DAG_new_unary(context->g_symb, g_b));
	TDAG f2_v1 = DAG_dup(DAG_new_binary(context->f2_symb, context->x, g_g_a));
	TDAG f2_m1 = DAG_dup(DAG_new_binary(context->f2_symb, g_g_b, g_a));
	TDAG f2_v2 = DAG_dup(DAG_new_binary(context->f2_symb, g_g_b, context->y));

	Tdisc_tree* dt = disc_tree_INIT();
	PICOTEST_ASSERT(dt != NULL);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 0);

	disc_tree_insert(dt, f2_v1);
	disc_tree_insert(dt, f2_m1);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 2);

	Tstack_DAG result = disc_tree_lookup(dt, f2_v2);
	PICOTEST_ASSERT(stack_size(result) == 2);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v1));
	PICOTEST_ASSERT(in_DAG_stack(result, f2_m1));
	stack_free(result);

	disc_tree_free(dt);

	DAG_free(g_a);
	DAG_free(g_b);
	DAG_free(g_g_a);
	DAG_free(g_g_b);
	DAG_free(f2_m1);
	DAG_free(f2_v1);
	DAG_free(f2_v2);
}

PICOTEST_CASE(narySymbolsDT, dags, context)
{
	TDAG g_a = DAG_new_unary(context->g_symb, context->a);
	TDAG P_a = DAG_dup(DAG_new_unary(context->P_symb, context->a));
	TDAG P_b = DAG_dup(DAG_new_unary(context->P_symb, context->b));
	TDAG P_g_a = DAG_dup(DAG_new_unary(context->P_symb, g_a));

	TDAG bool_1 = DAG_new_args(CONNECTOR_OR, P_a, P_b, P_g_a, DAG_NULL);
	TDAG bool_2 =
		DAG_new_binary(CONNECTOR_OR, DAG_new_binary(CONNECTOR_OR, P_a, P_b), P_g_a);

	TDAG t1 = DAG_dup(DAG_new_binary(CONNECTOR_AND, P_a, bool_1));
	TDAG t2 = DAG_dup(DAG_new_binary(CONNECTOR_AND, P_a, bool_2));
	TDAG t3 = DAG_dup(DAG_new_binary(CONNECTOR_AND, P_a, context->z));

	Tdisc_tree* dt = disc_tree_INIT();
	PICOTEST_ASSERT(dt != NULL);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 0);

	disc_tree_insert(dt, t1);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 1);

	Tstack_DAG result = disc_tree_lookup(dt, t1);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, t1));
	stack_free(result);

	result = disc_tree_lookup(dt, t2);
	PICOTEST_ASSERT(stack_size(result) == 0);
	PICOTEST_ASSERT(!in_DAG_stack(result, t1));
	stack_free(result);

	result = disc_tree_lookup(dt, t3);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, t1));
	stack_free(result);
	disc_tree_free(dt);

	dt = disc_tree_INIT();
	disc_tree_insert(dt, t2);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 1);

	result = disc_tree_lookup(dt, t2);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, t2));
	stack_free(result);

	result = disc_tree_lookup(dt, t1);
	PICOTEST_ASSERT(stack_size(result) == 0);
	PICOTEST_ASSERT(!in_DAG_stack(result, t2));

	result = disc_tree_lookup(dt, t3);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, t2));

	disc_tree_insert(dt, t1);
	result = disc_tree_lookup(dt, t3);
	PICOTEST_ASSERT(stack_size(result) == 2);
	PICOTEST_ASSERT(in_DAG_stack(result, t1));
	PICOTEST_ASSERT(in_DAG_stack(result, t2));
	stack_free(result);
	disc_tree_free(dt);

	dt = disc_tree_INIT();
	disc_tree_insert(dt, t3);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 1);

	result = disc_tree_lookup(dt, t1);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, t3));
	stack_free(result);
	result = disc_tree_lookup(dt, t2);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, t3));

	stack_free(result);
	disc_tree_free(dt);

	DAG_free(t1);
	DAG_free(t2);
	DAG_free(t3);
	DAG_free(P_a);
	DAG_free(P_b);
	DAG_free(P_g_a);
}

PICOTEST_CASE(ignoredVariablesDT, dags, context)
{
	TDAG g_a = DAG_dup(DAG_new_unary(context->g_symb, context->a));
	TDAG g_b = DAG_dup(DAG_new_unary(context->g_symb, context->b));
	TDAG g_g_a = DAG_dup(DAG_new_unary(context->g_symb, g_a));
	TDAG g_g_b = DAG_dup(DAG_new_unary(context->g_symb, g_b));

	TDAG f2_v1 = DAG_dup(DAG_new_binary(context->f2_symb, context->x, g_g_a));
	TDAG f2_m1 = DAG_dup(DAG_new_binary(context->f2_symb, g_g_b, g_g_a));

	TDAG f2_v2 = DAG_dup(DAG_new_binary(context->f2_symb, context->y, g_g_a));

	Tstack_DAG ignore_x;
	stack_INIT(ignore_x);
	Tstack_DAG ignore_y;
	stack_INIT(ignore_y);
	stack_push(ignore_x, context->x);
	stack_push(ignore_y, context->y);

	Tdisc_tree* dt = disc_tree_INIT();
	PICOTEST_ASSERT(dt != NULL);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 0);

	disc_tree_insert_vars(dt, f2_v1, ignore_x);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 1);

	/* x is a variable here */
	Tstack_DAG result = disc_tree_lookup(dt, f2_v1);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v1));
	stack_free(result);

	/* x is a constant here */
	result = disc_tree_lookup_vars(dt, f2_v1, ignore_x);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v1));
	stack_free(result);

	result = disc_tree_lookup_vars(dt, f2_v2, ignore_y);
	PICOTEST_ASSERT(stack_size(result) == 0);
	stack_free(result);

	result = disc_tree_lookup(dt, f2_m1);
	PICOTEST_ASSERT(stack_size(result) == 0);
	stack_free(result);

	result = disc_tree_lookup(dt, f2_v2);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v1));
	stack_free(result);

	/* Add x again, but this time consider it a variable */
	disc_tree_insert(dt, f2_v1);
	PICOTEST_ASSERT(disc_tree_num_terms(dt) == 2);

	/* x is a variable here */
	result = disc_tree_lookup(dt, f2_v1);
	PICOTEST_ASSERT(stack_size(result) == 2);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v1));
	stack_free(result);

	/* x is a constant here */
	result = disc_tree_lookup_vars(dt, f2_v1, ignore_x);
	PICOTEST_ASSERT(stack_size(result) == 2);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v1));
	stack_free(result);

	/* This only matches the variable version, not the one added as a constant */
	result = disc_tree_lookup_vars(dt, f2_v2, ignore_y);
	PICOTEST_ASSERT(stack_size(result) == 1);
	PICOTEST_ASSERT(in_DAG_stack(result, f2_v1));
	stack_free(result);

	disc_tree_free(dt);
	stack_free(ignore_x);
	stack_free(ignore_y);

	DAG_free(g_a);
	DAG_free(g_b);
	DAG_free(g_g_a);
	DAG_free(g_g_b);
	DAG_free(f2_v1);
	DAG_free(f2_m1);
	DAG_free(f2_v2);
}
