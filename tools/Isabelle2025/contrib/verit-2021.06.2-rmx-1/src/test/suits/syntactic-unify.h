#include "instantiation/syntactic-unify.h"

#include "picotest.h"
#include "symbolic/DAG-sort.h"
#include "symbolic/DAG.h"
#include "symbolic/veriT-DAG.h"
#include "symbolic/DAG-tmp.h"

/* TODO: This tests are far from complete */
PICOTEST_SUITE(
	syntacticUnifySuite, simpleUnificationSU, varvarUnificationSU, generalUnifySU,
	occursCheckSU, explictVarsSU, unifEqSU)

PICOTEST_FIXTURE_CONTEXT(dagsSU)
{
	Tsort S; /* A sort */
	Tsymb x_symb;
	TDAG x; /* A variable */
	Tsymb y_symb;
	TDAG y; /* Another variable */
	Tsymb z_symb;
	TDAG z; /* Another variable */
	Tsymb a_symb;
	TDAG a; /* A constant */
	Tsymb b_symb;
	TDAG b; /* A constant */
	Tsymb g_symb; /* A unary function symbol */
	Tsymb f2_symb; /* A binary function symbol */
};


PICOTEST_FIXTURE_SETUP(dagsSU, context)
{
	standardEnv_init();

	DAG_set_hook_resize(fvars_hook_resize);
	DAG_set_hook_free(fvars_hook_free);

	context->S = DAG_sort_new("S", 0, NULL);
	context->x_symb = DAG_symb_new("x", SYMB_VARIABLE, context->S);
	context->y_symb = DAG_symb_new("y", SYMB_VARIABLE, context->S);
	context->z_symb = DAG_symb_new("z", SYMB_VARIABLE, context->S);
	context->a_symb = DAG_symb_new("a", 0, context->S);
	context->b_symb = DAG_symb_new("b", 0, context->S);

	Tsort* sort = all_sorts(2, context->S);
	context->g_symb = DAG_symb_new("g", 0, DAG_sort_new(NULL, 2, sort));
	sort = all_sorts(3, context->S);
	context->f2_symb = DAG_symb_new("f2", 0, DAG_sort_new(NULL, 3, sort));

	context->x = DAG_dup(DAG_new_nullary(context->x_symb));
	context->y = DAG_dup(DAG_new_nullary(context->y_symb));
	context->z = DAG_dup(DAG_new_nullary(context->z_symb));
	context->a = DAG_dup(DAG_new_nullary(context->a_symb));
	context->b = DAG_dup(DAG_new_nullary(context->b_symb));
}

PICOTEST_FIXTURE_TEARDOWN(dagsSU, context)
{
	DAG_free(context->x);
	DAG_free(context->y);
	DAG_free(context->z);
	DAG_free(context->a);
	DAG_free(context->b);
	standardEnv_done();
}

PICOTEST_CASE(simpleUnificationSU, dagsSU, context)
{
	TDAG g_a = DAG_dup(DAG_new_unary(context->g_symb, context->a));
	TDAG g_x = DAG_dup(DAG_new_unary(context->g_symb, context->x));
	set_fvars(g_x);

	PICOTEST_ASSERT(subsumes(g_a, g_a));
	PICOTEST_ASSERT(subsumes(g_x, g_a));

	DAG_tmp_reserve();

	PICOTEST_ASSERT(unify_syntactic(g_x, g_a));
	TDAG result = apply_substitution(g_x);
	PICOTEST_ASSERT(result == g_a);

	DAG_free(result);
	reset_substitution(g_x, g_a);
	DAG_tmp_release();

	DAG_free(g_a);
	DAG_free(g_x);
}

PICOTEST_CASE(varvarUnificationSU, dagsSU, context)
{
	TDAG g_x = DAG_dup(DAG_new_unary(context->g_symb, context->x));
	TDAG g_y = DAG_dup(DAG_new_unary(context->g_symb, context->y));
	set_fvars(g_x);
	set_fvars(g_y);

	PICOTEST_ASSERT(subsumes(g_x, g_y));
	PICOTEST_ASSERT(subsumes(g_y, g_x));

	DAG_tmp_reserve();

	PICOTEST_ASSERT(unify_syntactic(g_x, g_y));
	TDAG result1 = apply_substitution(g_x);
	TDAG result2 = apply_substitution(g_y);
	PICOTEST_ASSERT(result1 == result2);
	PICOTEST_ASSERT(result1 == g_x || result1 == g_y);

	DAG_free(result1);
	DAG_free(result2);
	reset_substitution(g_x, g_y);
	DAG_tmp_release();

	DAG_free(g_x);
	DAG_free(g_y);
}

static inline void
assert_unifiable(TDAG t1, TDAG t2)
{
	DAG_tmp_reserve();
	PICOTEST_ASSERT(unify_syntactic(t1, t2));
	TDAG result1 = apply_substitution(t1);
	TDAG result2 = apply_substitution(t2);
	PICOTEST_ASSERT(result1 == result2);
	DAG_free(result1);
	DAG_free(result2);
	reset_substitution(t1, t2);
	DAG_tmp_release();

	DAG_tmp_reserve();
	PICOTEST_ASSERT(unify_syntactic(t2, t1));
	result1 = apply_substitution(t2);
	result2 = apply_substitution(t1);
	PICOTEST_ASSERT(result1 == result2);
	DAG_free(result1);
	DAG_free(result2);
	reset_substitution(t1, t2);
	DAG_tmp_release();
}

PICOTEST_CASE(generalUnifySU, dagsSU, context)
{
	TDAG g_x = DAG_dup(DAG_new_unary(context->g_symb, context->x));
	set_fvars(g_x);
	TDAG g_y = DAG_dup(DAG_new_unary(context->g_symb, context->y));
	set_fvars(g_y);

	TDAG g_a = DAG_new_unary(context->g_symb, context->a);
	TDAG g_g_a = DAG_new_unary(context->g_symb, g_a);
	
	TDAG f2_g_g_a_g_a = DAG_dup(DAG_new_binary(context->f2_symb, g_g_a, g_a));
	set_fvars(f2_g_g_a_g_a);
	TDAG f2_g_x_x = DAG_dup(DAG_new_binary(context->f2_symb, g_x, context->x));
	set_fvars(f2_g_x_x);

	TDAG f2_g_g_a_y = DAG_dup(DAG_new_binary(context->f2_symb, g_g_a, context->y));
	set_fvars(f2_g_g_a_y);
	TDAG f2_g_x_g_a = DAG_dup(DAG_new_binary(context->f2_symb, g_x, g_a));
	set_fvars(f2_g_x_g_a);

	PICOTEST_ASSERT(subsumes(f2_g_x_x, f2_g_g_a_g_a));
	PICOTEST_ASSERT(!subsumes(f2_g_g_a_g_a, f2_g_x_x));
	PICOTEST_ASSERT(subsumes(f2_g_x_x, f2_g_x_g_a));

	assert_unifiable(f2_g_g_a_g_a, f2_g_x_x);
	assert_unifiable(f2_g_g_a_g_a, f2_g_g_a_y);
	assert_unifiable(f2_g_x_x, f2_g_g_a_y);
	assert_unifiable(f2_g_x_g_a, f2_g_g_a_y);

	DAG_free(f2_g_g_a_g_a);
	DAG_free(f2_g_x_x);
	DAG_free(f2_g_g_a_y);
	DAG_free(f2_g_x_g_a);
	DAG_free(g_x);
	DAG_free(g_y);
}

PICOTEST_CASE(occursCheckSU, dagsSU, context)
{
	TDAG g_x = DAG_dup(DAG_new_unary(context->g_symb, context->x));
	TDAG g_g_x = DAG_dup(DAG_new_unary(context->g_symb, g_x));
	set_fvars(g_x);

	PICOTEST_ASSERT(!subsumes(g_x, g_g_x));

	DAG_tmp_reserve();

	PICOTEST_ASSERT(!unify_syntactic(g_g_x, g_x));
	reset_substitution(g_x, g_g_x);
	PICOTEST_ASSERT(!unify_syntactic(g_x, g_g_x));
	reset_substitution(g_x, g_g_x);

	DAG_tmp_release();

	DAG_free(g_x);
	DAG_free(g_g_x);
}

PICOTEST_CASE(explictVarsSU, dagsSU, context)
{
	TDAG g_x = DAG_dup(DAG_new_unary(context->g_symb, context->x));
	TDAG g_a = DAG_dup(DAG_new_unary(context->g_symb, context->a));
	set_fvars(g_x);
	set_fvars(g_a);

	PICOTEST_ASSERT(subsumes(g_x, g_a));
	PICOTEST_ASSERT(!subsumes(g_a, g_x));

	DAG_tmp_reserve();
	PICOTEST_ASSERT(unify_syntactic(g_x, g_a));
	TDAG result1 = apply_substitution(g_x);
	TDAG result2 = apply_substitution(g_a);
	PICOTEST_ASSERT(result1 == result2);
	PICOTEST_ASSERT(result1 == g_a);

	DAG_free(result1);
	DAG_free(result2);
	reset_substitution(g_x, g_a);

	Tstack_DAG vars;
	stack_INIT(vars);
	stack_push(vars, context->y);
	PICOTEST_ASSERT(unify_syntactic_vars(g_x, g_a, vars));
	result1 = apply_substitution(g_x);
	result2 = apply_substitution(g_a);
	PICOTEST_ASSERT(result1 == result2);
	PICOTEST_ASSERT(result1 == g_a);
	DAG_free(result1);
	DAG_free(result2);
	reset_substitution(g_x, g_a);

	stack_push(vars, context->x);
	stack_sort(vars, DAG_cmp_q);
	PICOTEST_ASSERT(!unify_syntactic_vars(g_x, g_a, vars));

	stack_free(vars);
	reset_substitution(g_x, g_a);

	DAG_tmp_release();

	DAG_free(g_x);
	DAG_free(g_a);
}

PICOTEST_CASE(unifEqSU, dagsSU, context)
{
	/* f2(x, x) */
	TDAG t1 = DAG_dup(DAG_new_binary(context->f2_symb, context->x, context->x));
	TDAG h = DAG_new_unary(context->g_symb, context->z);
	/* f2(g(z), y) */
	TDAG t2 = DAG_dup(DAG_new_binary(context->f2_symb, h, context->y));
	set_fvars(t1);
	set_fvars(t2);

	DAG_tmp_reserve();
	PICOTEST_ASSERT(unify_syntactic(t1, t2));
	TDAG result1 = apply_substitution(t1);
	TDAG result2 = apply_substitution(t2);
	PICOTEST_ASSERT(result1 == result2);

	DAG_free(result1);
	DAG_free(result2);
	reset_substitution(t1, t2);

	Tstack_DAG vars;
	stack_INIT(vars);
	stack_push(vars, context->z);
	/* Occurs check should fail, because z is skolemized to s(y) */
	PICOTEST_ASSERT(!unify_syntactic_vars(t1, t2, vars));
	reset_substitution(t1, t2);
	DAG_tmp_release();

	DAG_free(t1);
	DAG_free(t2);
}

