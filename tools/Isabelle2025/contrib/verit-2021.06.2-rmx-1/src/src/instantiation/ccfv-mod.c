#include "instantiation/ccfv-mod.h"

#include "congruence/congruence.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-sort.h"
#include "utils/general.h"

/**
   \brief associates all DAGs to their "value" according to a solution */
TDAG_modulo* DAGs_modulo;

static void
DAG_modulo_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	unsigned i;
	/* TODO: make sure this is correct */
	if (new_alloc < old_alloc)
		for (i = old_alloc; i > new_alloc; --i)
			if (DAGs_modulo[i - 1]) {
				if (DAGs_modulo[i - 1]->stack_pred)
					stack_free(DAGs_modulo[i - 1]->stack_pred);
				free(DAGs_modulo[i - 1]);
				DAGs_modulo[i - 1] = NULL;
			}
	MY_REALLOC(DAGs_modulo, new_alloc * sizeof(TDAG_modulo));
	for (i = old_alloc; i < new_alloc; ++i) DAGs_modulo[i] = NULL;
}

/*
  --------------------------------------------------------------
  Backtracking modulo
  --------------------------------------------------------------
*/

/** \brief undo_level when search started */
unsigned init_undo_lvl;

void unset_var_modulo(TDAG var);

void
backtrack_set_var(TDAG var)
{
	(*(TDAG*)undo_push(CCFV_UNDO_VAR)) = var;
}

static void
CCFV_hook_var(TDAG* var)
{
	unset_var_modulo(*var);
}

static void
backtrack_init(void)
{
	undo_set_hook(CCFV_UNDO_VAR, (Tundo_hook)CCFV_hook_var, sizeof(TDAG));
}

static void
backtrack_done(void)
{
	/* TODO: is this necessary? Appears in Simplex, not in CC */
	undo_unset_hook(CCFV_UNDO_VAR);
}

/*
  --------------------------------------------------------------
  Handling DAGs modulo current solution
  --------------------------------------------------------------
*/

/**
   \brief sets DAGs_modulo for non-ground DAG and all its non-ground
   sub-DAGs */
void
set_DAGs_modulo(TDAG DAG)
{
	unsigned i;
	if (DAGs_modulo[DAG] || ground(DAG)) return;
	MY_MALLOC(DAGs_modulo[DAG], 2 * sizeof(unsigned) + sizeof(Tstack_DAG));
	DAGs_modulo[DAG]->ground_args = 0;
	DAGs_modulo[DAG]->term = DAG_NULL;
	DAGs_modulo[DAG]->stack_pred = NULL;
	for (i = 0; i < DAG_arity(DAG); ++i)
		if (ground(DAG_arg(DAG, i)))
			set_arg_ground(DAG, i);
		else
			set_DAGs_modulo(DAG_arg(DAG, i));
	stack_INIT(DAGs_modulo[DAG]->stack_pred);
	for (i = 0; i < DAG_arity(DAG); ++i) {
		if (!DAGs_modulo[DAG_arg(DAG, i)]) continue;
		/* TODO: does it really need to be checked? Assertion failed for
         unsat-25, DAG that appeared in condition of ITE */
		if (!DAGs_modulo[DAG_arg(DAG, i)]->stack_pred)
			stack_INIT(DAGs_modulo[DAG_arg(DAG, i)]->stack_pred);
		/* assert(DAGs_modulo[DAG_arg(DAG, i)]->stack_pred); */
		stack_push(DAGs_modulo[DAG_arg(DAG, i)]->stack_pred, DAG);
		/* TODO: how to efficiently guarantee that no repeated predecessors? */
		stack_sort(DAGs_modulo[DAG_arg(DAG, i)]->stack_pred, DAG_cmp_q);
		stack_uniq(DAGs_modulo[DAG_arg(DAG, i)]->stack_pred);
	}
}

/**
   \brief frees DAGs_modulo for DAG and all its sub-DAGs */
void
unset_DAGs_modulo(TDAG DAG)
{
	unsigned i;
	if (!DAGs_modulo[DAG]) return;
	if (DAGs_modulo[DAG]->stack_pred) stack_free(DAGs_modulo[DAG]->stack_pred);
	free(DAGs_modulo[DAG]);
	DAGs_modulo[DAG] = NULL;
	for (i = 0; i < DAG_arity(DAG); ++i) unset_DAGs_modulo(DAG_arg(DAG, i));
}

/* TODO: ugly workaround */
extern bool ccfv_depth_eager_discard;

void
update_DAGs_modulo(TDAG DAG)
{
	unsigned i;
	TDAG sig;
	Tstack_DAG params;
	assert(DAG_arity(DAG));
	/* DAG was already "instantiated"; this may occur when DAG appears in more
     than one predecessors list of freshly ground subterms; e.g. f(g(x), x)
     would have this called on twice when x is grounded */
	if (ground_mod(DAG)) return;
	for (i = 0; i < DAG_arity(DAG); ++i)
		if (
			!ground(DAG_arg(DAG, i)) && ground_mod(DAG_arg(DAG, i)) &&
			!check_arg_ground(DAG, i))
			set_arg_ground(DAG, i);
	/* Not all arguments are ground */
	if (!ground_mod(DAG)) return;
	stack_INIT(params);
	for (i = 0; i < DAG_arity(DAG); ++i)
		if (ground(DAG_arg(DAG, i)))
			stack_push(params, DAG_arg(DAG, i));
		else if (has_sig(DAG_arg(DAG, i)))
			stack_push(params, DAGs_modulo[DAG_arg(DAG, i)]->term);
		else
			break;
	/* All arguments have signatures */
	if (i == DAG_arity(DAG)) {
		/* Retrieve instance's signature from CC, if any */
		sig = sig_query_params(DAG_symb(DAG), params);
		if (sig) DAGs_modulo[DAG]->term = sig;
	}
	stack_free(params);
	if (DAGs_modulo[DAG]->stack_pred)
		stack_apply(DAGs_modulo[DAG]->stack_pred, update_DAGs_modulo);
}

void
set_var_modulo(TDAG var, TDAG term)
{
	if (DAGs_modulo[var]->term) return;
	undo_level_new();
	DAGs_modulo[var]->ground_args = 1;
	DAGs_modulo[var]->term = term;
	backtrack_set_var(var);
	if (DAGs_modulo[var]->stack_pred)
		stack_apply(DAGs_modulo[var]->stack_pred, update_DAGs_modulo);
}

void
revert_DAGs_modulo(TDAG DAG, TDAG arg_DAG)
{
	unsigned i;
	assert(DAG_arity(DAG));
	if (ground_mod(DAG)) {
		if (DAGs_modulo[DAG]->term) DAGs_modulo[DAG]->term = DAG_NULL;
		/* TODO: make sure OK to do it before updating groundness of this guy */
		if (DAGs_modulo[DAG]->stack_pred)
			for (i = 0; i < stack_size(DAGs_modulo[DAG]->stack_pred); ++i)
				revert_DAGs_modulo(stack_get(DAGs_modulo[DAG]->stack_pred, i), DAG);
	}
	for (i = 0; i < DAG_arity(DAG); ++i)
		if (DAG_arg(DAG, i) == arg_DAG) {
			assert(check_arg_ground(DAG, i));
			unset_arg_ground(DAG, i);
		}
}

void
unset_var_modulo(TDAG var)
{
	unsigned i;
	assert(DAGs_modulo[var]->ground_args);
	DAGs_modulo[var]->ground_args = 0;
	DAGs_modulo[var]->term = DAG_NULL;
	if (DAGs_modulo[var]->stack_pred)
		for (i = 0; i < stack_size(DAGs_modulo[var]->stack_pred); ++i)
			revert_DAGs_modulo(stack_get(DAGs_modulo[var]->stack_pred, i), var);
}

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

void
CCFV_mod_init(void)
{
	DAGs_modulo = NULL;
	DAG_set_hook_resize(DAG_modulo_hook_resize);
	backtrack_init();
}

void
CCFV_mod_done(void)
{
	backtrack_done();
}

/*
  --------------------------------------------------------------
  Debugging
  --------------------------------------------------------------
*/

void
print_DAG_modulo(TDAG DAG)
{
	if (ground(DAG)) {
		my_DAG_message("%D is ground\n", DAG);
		return;
	}
	my_DAG_message(
		"{%d}%D -> %s/%D\n", DAG, DAG,
		byte_to_binary(
			DAGs_modulo[DAG]->ground_args, DAG_arity(DAG) ? DAG_arity(DAG) : 1),
		DAGs_modulo[DAG]->term);
}
