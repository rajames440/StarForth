#include "proof/proof-subproof.h"

#include "proof/proof-rules.h"

/*
  --------------------------------------------------------------
  Subproofs starting/ending
  --------------------------------------------------------------
*/

void
proof_subproof_begin(Tproof_type type)
{
	assert(type >= ps_type_subproof && type < ps_type_bind);

	Tproof_step proof_step = proof_step_new();
	proof_step->type = type;
	proof_step->args = NULL;
	/* Initiate a new stack of steps */
	stack_inc(steps_stack);
	stack_INIT(top_steps);
	stack_push(top_steps, proof_step);
}

void
proof_subproof_begin_context(
	Tproof_type type, Tstack_DAG context, Tstack_proof reasons)
{
	assert(type >= ps_type_bind);

	unsigned i;
	Tproof_step proof_step = proof_step_new();
	proof_step->type = type;
	/* Adding reasons */
	if (reasons)
		for (i = 0; i < stack_size(reasons); ++i)
			proof_step_add_reason(proof_step, stack_get(reasons, i));
	/* Add context */
	stack_INIT(proof_step->args);
	assert(context && !stack_is_empty(context));
	for (i = 0; i < stack_size(context) - 1; ++i) DAG_dup(stack_get(context, i));
	stack_merge(proof_step->args, context);
	/* Initiate a new stack of steps */
	stack_inc(steps_stack);
	stack_INIT(top_steps);
	stack_push(top_steps, proof_step);
}

void
proof_subproof_begin_sko(
	Tproof_type type, Tstack_DAG context, Tstack_proof reasons, TDAG src)
{
	assert(type >= ps_type_sko_ex);
	/* Set up choice functions */
	push_choices(src, context);
	/* bulid the proof step */
	proof_subproof_begin_context(type, context, reasons);
}

void
proof_subproof_remove(void)
{
	unsigned i;
	Tproof_step ps;
	assert(
		steps_stack && stack_size(steps_stack) > 1 && stack_get(top_steps, 0) &&
		stack_get(top_steps, 0)->type >= ps_type_subproof &&
		stack_get(top_steps, 0)->type < ps_type_sko_ex);
	for (i = 0; i < stack_size(top_steps); ++i) {
		ps = stack_get(top_steps, i);
		proof_step_free(&ps);
	}
	stack_free(top_steps);
	stack_dec(steps_stack);
}

#define TRANSFER_STEPS(ps) \
	do { \
		ps = stack_get(top_steps, 0); \
		stack_INIT(ps->subproof_steps); \
		stack_merge(ps->subproof_steps, top_steps); \
		stack_set(ps->subproof_steps, 0, NULL); \
		stack_free(top_steps); \
		stack_dec(steps_stack); \
	} while (0)

Tproof
proof_subproof_end_input(void)
{
	unsigned i, j;
	Tproof_step ps_subproof, ps;
	assert(
		steps_stack && stack_size(steps_stack) > 1 && stack_get(top_steps, 0) &&
		stack_get(top_steps, 0)->type >= ps_type_subproof);
	TRANSFER_STEPS(ps_subproof);
	/* Collect inputs */
	for (i = 1; i < stack_size(ps_subproof->subproof_steps) - 1; ++i)
		if ((ps = stack_get(ps_subproof->subproof_steps, i))->type == ps_type_assume)
			for (j = 0; j < stack_size(ps->DAGs); ++j)
				proof_step_add_DAG(
					ps_subproof, DAG_dup(DAG_not(stack_get(ps->DAGs, j))));
	/* Retrieve last conclusion */
	ps = stack_get(ps_subproof->subproof_steps, i);
	for (j = 0; j < stack_size(ps->DAGs); ++j)
		proof_step_add_DAG(ps_subproof, DAG_dup(stack_get(ps->DAGs, j)));
	return steps_push(ps_subproof);
}

Tproof
proof_subproof_end_transformation(TDAG src, TDAG dest)
{
	Tproof_step ps_subproof;
	assert(
		steps_stack && stack_size(steps_stack) > 1 && stack_get(top_steps, 0) &&
		stack_get(top_steps, 0)->type >= ps_type_subproof);
	TRANSFER_STEPS(ps_subproof);
	proof_step_add_DAG(
		ps_subproof,
		DAG_dup(
			DAG_sort(src) == SORT_BOOLEAN ? DAG_equiv(src, dest)
																		: DAG_eq(src, dest)));
	/* Pop inserted choice functions */
	if (ps_subproof->type >= ps_type_sko_ex) pop_choices(src);
	return steps_push(ps_subproof);
}
