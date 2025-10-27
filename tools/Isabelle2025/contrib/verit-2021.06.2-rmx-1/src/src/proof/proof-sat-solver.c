#include "proof/proof-sat-solver.h"

#include "bool/bool.h"
#include "proof/proof-rules.h"
#include "proof/proof-step-table.h"
#include "symbolic/DAG.h"

static unsigned clause_id_to_steps_size = 0;
static Tproof* clause_id_to_steps = NULL;
static Tproof last_steps_add = 0;

static inline void
proof_SAT_set_proof_id(SAT_Tclause clause_id, Tproof proof_id)
{
	unsigned i;
	if (clause_id_to_steps_size <= clause_id) {
		if (!clause_id_to_steps_size) {
			clause_id_to_steps_size = 1;
			MY_MALLOC(clause_id_to_steps, clause_id_to_steps_size * sizeof(Tproof));
			clause_id_to_steps[0] = 0;
		}
		while (clause_id_to_steps_size <= clause_id) {
			clause_id_to_steps_size *= 2;
			MY_REALLOC(clause_id_to_steps, clause_id_to_steps_size * sizeof(Tproof));
			for (i = clause_id_to_steps_size >> 1; i < clause_id_to_steps_size; i++)
				clause_id_to_steps[i] = 0;
		}
	}
	clause_id_to_steps[clause_id] = proof_id;
#ifdef DEBUG_PROOF
	my_message("SAT proof id (_%d_): %d\n", clause_id, proof_id);
#endif
}

static inline Tproof
proof_SAT_get_proof_id(SAT_Tclause clause_id)
{
	if (clause_id >= clause_id_to_steps_size) return 0;
	return clause_id_to_steps[clause_id];
}

static inline Tproof_step
proof_SAT_get_proof(SAT_Tclause clause_id)
{
	return stack_get(top_steps, proof_SAT_get_proof_id(clause_id));
}

/*
  --------------------------------------------------------------
  Interface with SAT solver
  --------------------------------------------------------------
*/

void
proof_SAT_reset(void)
{
	memset(clause_id_to_steps, 0, clause_id_to_steps_size * sizeof(int));
}

void
proof_SAT_insert(Tclause clause)
{
	assert(!last_steps_add);
	if (clause->proof_id == 0) {
		my_warning("Adding a clause without proof\n");
		return;
	}
	last_steps_add = clause->proof_id;
}

void
SAT_proof_set_id(SAT_Tclause clause_id)
{
	proof_SAT_set_proof_id(clause_id, last_steps_add);
	last_steps_add = 0;
}

void
SAT_proof_notify(SAT_Tclause clause)
{
	unsigned i;
	Tproof_step result;
	result = proof_step_resolve(
		proof_SAT_get_proof(SAT_proof_stack_clause[0]),
		proof_SAT_get_proof(SAT_proof_stack_clause[1]),
		var_to_DAG(lit_var(SAT_proof_stack_lit[0])));
	for (i = 2; i < SAT_proof_stack_n; ++i) {
		Tproof_step result_new = proof_step_resolve(
			result, proof_SAT_get_proof(SAT_proof_stack_clause[i]),
			var_to_DAG(lit_var(SAT_proof_stack_lit[i - 1])));
		proof_step_free(&result);
		result = result_new;
	}
	result->type = ps_type_SAT_resolution;
	for (i = 0; i < SAT_proof_stack_n; i++)
		proof_step_add_reason(
			result, proof_SAT_get_proof_id(SAT_proof_stack_clause[i]));
	proof_SAT_set_proof_id(clause, steps_push(result));
}

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

void
proof_SAT_init(void)
{
	clause_id_to_steps_size = 0;
	clause_id_to_steps = NULL;
	last_steps_add = 0;
}

void
proof_SAT_done(void)
{
	clause_id_to_steps_size = 0;
	free(clause_id_to_steps);
	last_steps_add = 0;
}
