#include "proof/proof-unsat-core.h"

#include "proof/proof-step-table.h"
#include "symbolic/DAG-prop.h"

static Tstack_DAG hypotheses = NULL; /**< already printed hypotheses */
static bool hypothesis_first; /**< first hypothesis is to be printed */

/**
   \brief handler for clauses of type `ps_type_assume` in
   `proof_step_hyp`
   \param proof_step the proof step to handle
   \param file the file to write to
   \seealso proof_step_hyp proof_hyp */
static void
proof_step_hyp_input(Tproof_step proof_step, FILE* file)
{
	char** Pname;
	assert(proof_step->type == ps_type_assume);
	if (stack_size(proof_step->DAGs) != 1)
		my_error("proof_step_hyp: internal error\n");
	if (DAG_tmp_bool[stack_top(proof_step->DAGs)]) return;
	Pname = DAG_prop_get(stack_top(proof_step->DAGs), DAG_PROP_NAMED);
	if (!Pname) return;
	stack_push(hypotheses, stack_top(proof_step->DAGs));
	DAG_tmp_bool[stack_top(proof_step->DAGs)] = true;
	fprintf(file, hypothesis_first ? "%s" : " %s", *Pname);
	hypothesis_first = false;
}

/**
   \brief prints hypothesis label found in proof clause
   (in response to SMT command get-unsat-core)
   \param proof_step the proof clause
   \param steps the stack of all proof steps
   \param file the file to write to
   \seealso proof_step proof_hyp  */
static void
proof_step_hyp(Tproof_step proof_step, Tstack_proof_step steps, FILE* file)
{
	unsigned i, nb_reasons;
	Tproof_type type = proof_step->type;
	if (type == ps_type_assume) {
		proof_step_hyp_input(proof_step, file);
		return;
	}
	if (type >= ps_type_subproof) {
		for (i = 1; i < stack_size(proof_step->subproof_steps); ++i)
			proof_step_hyp(
				stack_get(proof_step->subproof_steps, i), proof_step->subproof_steps,
				file);
		return;
	}
	nb_reasons = proof_step->reasons
		? (ps_type_desc[type].nb_reasons == -1 ? stack_size(proof_step->reasons)
																					 : ps_type_desc[type].nb_reasons)
		: 0;
	assert(nb_reasons <= stack_size(proof_step->reasons));
	for (i = 0; i < nb_reasons; ++i) {
		assert(stack_get(proof_step->reasons, i) <= stack_size(steps));
		Tproof_step proof_step2 =
			stack_get(steps, stack_get(proof_step->reasons, i));
		if (proof_step2->type == ps_type_assume)
			proof_step_hyp_input(proof_step2, file);
	}
}

void
proof_hyp(FILE* file)
{
	unsigned i;
	/* the proof tree is first reduced (this is optional) */
	steps_merge();
	steps_prune();
	/* initialization of variables used to avoid duplicates */
	DAG_tmp_reserve();
	stack_INIT(hypotheses);
	/* print hypotheses found in each proof clause */
	fprintf(file, "(");
	hypothesis_first = true;
	for (i = 1; i < stack_size(top_steps); ++i)
		proof_step_hyp(stack_get(top_steps, i), top_steps, file);
	fprintf(file, ")\n");
	/* finalization of variables used to avoid duplicates */
	for (i = 0; i < stack_size(hypotheses); ++i)
		DAG_tmp_bool[stack_get(hypotheses, i)] = false;
	stack_free(hypotheses);
	DAG_tmp_release();
}
