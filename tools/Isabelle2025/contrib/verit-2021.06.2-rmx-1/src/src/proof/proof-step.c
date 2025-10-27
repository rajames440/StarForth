#include "proof/proof-step.h"

#include "proof/proof-output.h"
#include "symbolic/DAG-symb.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "symbolic/polarities.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"

Tproof_step
proof_step_new(void)
{
	Tproof_step proof_step = NULL;
	MY_MALLOC(proof_step, sizeof(struct TSproof_step));

	proof_step->type = ps_type_none;
	stack_INIT(proof_step->DAGs);
	proof_step->misc = 0;
	proof_step->reasons = NULL;
	proof_step->args = NULL;
	proof_step->subproof_steps = NULL;

	return proof_step;
}

void
proof_step_free(Tproof_step* Pproof_step)
{
	unsigned i;
	Tstack_proof_step steps;

	if (!*Pproof_step) return;
	assert((*Pproof_step)->DAGs);

	stack_apply((*Pproof_step)->DAGs, DAG_free);
	stack_free((*Pproof_step)->DAGs);

	if ((*Pproof_step)->reasons) stack_free((*Pproof_step)->reasons);

	if ((*Pproof_step)->type >= ps_type_subproof) {
		if ((*Pproof_step)->args) {
			assert(!stack_is_empty((*Pproof_step)->args));
			/* Last element is always the number of bound variables, not a DAG */
			stack_dec((*Pproof_step)->args);
			stack_apply((*Pproof_step)->args, DAG_free);
			stack_free((*Pproof_step)->args);
		}

		steps = (*Pproof_step)->subproof_steps;
		if (steps) {
			for (i = 1; i < stack_size(steps); ++i)
				proof_step_free(&stack_get(steps, i));

			if (stack_get(steps, 0)) my_error("proof_step_free: internal error\n");
			stack_free(steps);
		}
	} else if ((*Pproof_step)->args) {
		stack_apply((*Pproof_step)->args, DAG_free);
		stack_free((*Pproof_step)->args);
	}

	free(*Pproof_step);
	*Pproof_step = NULL;
}

void
proof_step_add_DAG(Tproof_step proof_step, TDAG DAG)
{
	stack_push(proof_step->DAGs, DAG);
}

void
proof_step_add_arg(Tproof_step proof_step, TDAG DAG)
{
	assert(proof_step->type < ps_type_subproof);

	if (!proof_step->args) stack_INIT(proof_step->args);

	stack_push(proof_step->args, DAG);
}

void
proof_step_add_reason(Tproof_step proof_step, Tproof proof_id)
{
	if (!proof_step->reasons) stack_INIT(proof_step->reasons);

	stack_push(proof_step->reasons, proof_id);
}

void
proof_step_add_param(Tproof_step proof_step, unsigned param)
{
	if (!proof_step->reasons) stack_INIT(proof_step->reasons);

	stack_push(proof_step->reasons, param);
}

