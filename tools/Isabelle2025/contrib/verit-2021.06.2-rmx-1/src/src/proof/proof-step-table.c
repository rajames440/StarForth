#include "proof/proof-step-table.h"

#include "proof/proof-output.h"
#include "proof/proof-rules-tautologies.h"
#include "proof/proof-rules.h"
#include "proof/proof-step-hash.h"
#include "symbolic/veriT-DAG.h"

Tstack_proof_step_stack steps_stack = NULL;

Tstack_DAG_assoc sko_var_to_choice;
Tstack_DAG_assoc choices;
Tstack_DAG_assoc ite_csts;

/*
  --------------------------------------------------------------
  Step stacks handling
  --------------------------------------------------------------
*/

void
steps_init(void)
{
	stack_INIT(sko_var_to_choice);
	stack_INIT(choices);
	stack_INIT(ite_csts);

	stack_INIT(steps_stack);
	stack_inc(steps_stack);
	stack_INIT(top_steps);
	stack_push(top_steps, NULL);
	steps_hash = hash_new(
		100, (TFhash)steps_hash_function, (TFequal)steps_hash_equal,
		(TFfree)steps_hash_free);
}

/** \brief
   Simplifies clause. And produce the appropriate proof.
   - if clause contains X = X literals, returns TRUE clause
   - if clause contains complementary literals, returns TRUE clause
   - otherwise eliminates NOT(X = X) literals and repeated literals.
   - NOT(NOT P) is recursively rewritten as P */
Tproof
proof_step_clean(void)
{
	/* TODO: how to not use DAG_misc given that DAG_tmp may be in use by the
	   modules calling this function? */
	Tproof proof = stack_size(top_steps) - 1;
	Tproof_step proof_step = stack_get(top_steps, proof);
#ifdef DEBUG_PROOF
	my_message("proof_step_clean: proof_step:\n");
	print_proof_step(proof_step, top_steps, 0, stderr, false);
#endif

	/* Double negation elimination */
	for (unsigned i = 0; i < stack_size(proof_step->DAGs); ++i) {
		while (DAG_symb(stack_get(proof_step->DAGs, i)) == CONNECTOR_NOT &&
					 DAG_symb(DAG_arg0(stack_get(proof_step->DAGs, i))) ==
						 CONNECTOR_NOT) {
			TDAG DAG = stack_get(proof_step->DAGs, i);
			Tproof not_not = proof_not_not(DAG); /* Resolvent */

			Tproof_step not_not_resolve_step = proof_step_new();
			not_not_resolve_step->type = ps_type_th_resolution;
			proof_step_add_reason(not_not_resolve_step, not_not);
			proof_step_add_reason(not_not_resolve_step, proof);

			stack_COPY(not_not_resolve_step->DAGs, proof_step->DAGs);
			stack_set(not_not_resolve_step->DAGs, i, DAG_arg0(DAG_arg0(DAG)));
			stack_apply(not_not_resolve_step->DAGs, DAG_dup);

			stack_push(top_steps, not_not_resolve_step);
			proof = stack_size(top_steps) - 1;
			proof_step = not_not_resolve_step;
		}
	}
	proof = stack_size(top_steps) - 1;

	unsigned tmp = 0;
	/* check for valid clauses, and use loop to detect
	   if literals to eliminate (NOT X=X or repeated) */
	for (unsigned i = 0; i < stack_size(proof_step->DAGs); ++i) {
		TDAG DAG = stack_get(proof_step->DAGs, i);
		Tpol pol = DAG_polarity(stack_get(proof_step->DAGs, i));

		/* search for appearences of literals in both polarities */
		DAG = DAG_atom(DAG);
		if (DAG_misc(DAG)) {
			DAG_misc_set(DAG, DAG_misc(DAG) | pol);
			if (DAG_misc(DAG) == POL_BOTH) {
				tmp = 2; /* Valid clause */
				break;
			}
			tmp = 1; /* Literal to eliminate */
		} else
			DAG_misc_set(DAG, DAG_misc(DAG) | pol);
	}

	if (tmp == 0) {
		/* No transformation has to occur */
		for (unsigned i = 0; i < stack_size(proof_step->DAGs); ++i)
			DAG_misc_set(DAG_atom(stack_get(proof_step->DAGs, i)), 0);

#ifdef DEBUG_PROOF
		my_message("result:\n");
		print_proof_step(proof_step, top_steps, 0, stderr, false);
#endif
		return proof;
	}

	if (tmp == 2) {
		/* Valid clause */
		for (unsigned i = 0; i < stack_size(proof_step->DAGs); ++i)
			DAG_misc_set(DAG_atom(stack_get(proof_step->DAGs, i)), 0);

		Tproof tautology = proof_tautology(proof);
#ifdef DEBUG_PROOF
		my_message("result:\n");
		print_proof_step(
			stack_get(top_steps, tautology), top_steps, 0, stderr, false);
#endif
		return tautology;
	}

	/* Literal to eliminate. */
	assert(tmp == 1);
	if (tmp == 1) {
		Tproof_step result = proof_step_new();
		result->type = ps_type_contraction;
		proof_step_add_reason(result, proof);

		for (unsigned i = 0; i < stack_size(proof_step->DAGs); ++i)
			if (DAG_misc(DAG_atom(stack_get(proof_step->DAGs, i)))) {
				proof_step_add_DAG(result, DAG_dup(stack_get(proof_step->DAGs, i)));
				DAG_misc_set(DAG_atom(stack_get(proof_step->DAGs, i)), 0);
			}
		stack_push(top_steps, result);
		proof = stack_size(top_steps) - 1;
	}

#ifdef DEBUG_PROOF
	my_message("result:\n");
	print_proof_step(proof_step, top_steps, 0, stderr, false);
#endif

	return proof;
}

Tproof
steps_push(Tproof_step proof_step)
{
	if (!proof_step->type) my_error("steps_push: proof_step without type\n");
	stack_push(top_steps, proof_step);
#ifdef DEBUG_PROOF
	my_message("Adding step (%d)\n", stack_size(top_steps) - 1);
	print_proof_step(
		proof_step, top_steps, stack_size(top_steps) - 1, stderr, false);
#endif
	if (stack_is_empty(proof_step->DAGs)) {
		empty_clause = stack_size(top_steps) - 1;
		return stack_size(top_steps) - 1;
	}
	/* This will never create an empty clause */
	return proof_step_clean();
}

static void
steps_prune_stack(Tstack_proof_step step_stack)
{
	assert(stack_size(step_stack) > 0);
	/* PF first mark all used clauses (from the end) with misc */
	stack_top(step_stack)->misc = 1;
	for (unsigned i = stack_size(step_stack); --i > 0;) {
		Tproof_step proof_step = stack_get(step_stack, i);
		Tproof_type type = proof_step->type;
		if (!proof_step->misc) continue;
		/* get number of proof steps as reasons */
		assert(
			ps_type_desc[type].nb_reasons != -1 || ps_type_desc[type].nb_params == 0);
		unsigned nb_reasons = ps_type_desc[type].nb_reasons != -1
			? ps_type_desc[type].nb_reasons
			: (proof_step->reasons ? stack_size(proof_step->reasons) : 0);
		assert(
			nb_reasons == 0 ||
			(proof_step->reasons && nb_reasons <= stack_size(proof_step->reasons)));
		for (unsigned j = 0; j < nb_reasons; ++j) {
			Tproof r = stack_get(proof_step->reasons, j);
			Tproof_step s = stack_get(step_stack, r);
			/* Skip reasons of resolutin steps that end with TRUE */
			if (
				(type == ps_type_th_resolution || type == ps_type_SAT_resolution) &&
				stack_size(s->DAGs) == 1 && stack_get(s->DAGs, 0) == DAG_TRUE)
				continue;
			s->misc = 1;
		}
		if (proof_step->subproof_steps) {
			assert(proof_step->type >= ps_type_subproof);
			steps_prune_stack(proof_step->subproof_steps);
		}
	}
	/* PF number all used clauses in a dense manner */
	for (unsigned i = 1, j = 1; i < stack_size(step_stack); ++i)
		if (stack_get(step_stack, i)->misc) stack_get(step_stack, i)->misc = j++;
	/* PF renumber clause_ids, and eliminate all unused clauses */
	for (unsigned i = 1; i < stack_size(step_stack); i++) {
		Tproof_step proof_step = stack_get(step_stack, i);
		Tproof_type type = proof_step->type;
		if (!proof_step->misc) {
			proof_step_free(&proof_step);
			stack_set(step_stack, i, NULL);
			continue;
		}
		/* get number of proof steps as reasons */
		assert(
			ps_type_desc[type].nb_reasons != -1 || ps_type_desc[type].nb_params == 0);
		unsigned nb_reasons = ps_type_desc[type].nb_reasons != -1
			? ps_type_desc[type].nb_reasons
			: (proof_step->reasons ? stack_size(proof_step->reasons) : 0);
		assert(
			nb_reasons == 0 ||
			(proof_step->reasons && nb_reasons <= stack_size(proof_step->reasons)));

		if (
			proof_step->type == ps_type_th_resolution ||
			proof_step->type == ps_type_SAT_resolution) {
			unsigned w = 0;
			for (unsigned j = 0; j < nb_reasons; ++j) {
				/* Reason has been pruned */
				if (!stack_get(step_stack, stack_get(proof_step->reasons, j))) continue;
				stack_set(
					proof_step->reasons, w,
					stack_get(step_stack, stack_get(proof_step->reasons, j))->misc);
				w++;
			}
			stack_resize(proof_step->reasons, w);
		} else
			for (unsigned j = 0; j < nb_reasons; ++j)
				stack_set(
					proof_step->reasons, j,
					stack_get(step_stack, stack_get(proof_step->reasons, j))->misc);
	}
	/* PF eliminate all the garbage */
	unsigned i = 1;
	unsigned j = 1;
	while (i < stack_size(step_stack)) {
		Tproof_step proof_step = stack_get(step_stack, i);
		if (proof_step) {
			stack_set(step_stack, j++, proof_step);
			proof_step->misc = 0;
		}
		i++;
	}
	stack_resize(step_stack, j);
}

void
steps_prune(void)
{
	assert(!stack_is_empty(top_steps));
	assert(stack_size(steps_stack) == 1);
	/* Search the empty clause representing the end of the proof. Should the
	   popped steps be freed? */
	while (stack_size(top_steps) > 0 &&
				 stack_size(stack_top(top_steps)->DAGs) != 0)
		stack_dec(top_steps);
	assert(stack_size(stack_top(top_steps)->DAGs) == 0);
	steps_prune_stack(top_steps);
}

void
steps_merge(void)
{
	unsigned i, j, nb_reasons;
	Tproof_step proof_step;
	Tproof proof_id;
	Tproof_type type;

	assert(stack_size(steps_stack) == 1);
	/* PF first enter every clause into a hash table, and mark all
     repeated clauses with the number of the original one */
	for (i = 1; i < stack_size(top_steps); ++i) {
		proof_id = steps_hash_get(stack_get(top_steps, i));
		if (proof_id)
			stack_get(top_steps, i)->misc = proof_id;
		else
			steps_hash_push(stack_get(top_steps, i), i);
	}
	/* PF renumber clause_ids */
	for (i = 1; i < stack_size(top_steps); ++i) {
		proof_step = stack_get(top_steps, i);
		type = proof_step->type;
		if (type >= ps_type_subproof) continue;
		/* get number of proof steps as reasons */
		assert(
			ps_type_desc[type].nb_reasons != -1 || ps_type_desc[type].nb_params == 0);
		nb_reasons = ps_type_desc[type].nb_reasons != -1
			? ps_type_desc[type].nb_reasons
			: (proof_step->reasons ? stack_size(proof_step->reasons) : 0);
		assert(
			nb_reasons == 0 ||
			(proof_step->reasons && nb_reasons <= stack_size(proof_step->reasons)));
		for (j = 0; j < nb_reasons; ++j)
			if (stack_get(top_steps, stack_get(proof_step->reasons, j))->misc) {
				assert(stack_get(proof_step->reasons, j) < stack_size(top_steps));
				stack_set(
					proof_step->reasons, j,
					stack_get(top_steps, stack_get(proof_step->reasons, j))->misc);
			}
	}
	/* PF remove from hash table and tidy */
	for (i = 1; i < stack_size(top_steps); ++i) {
		steps_hash_remove(stack_get(top_steps, i));
		stack_get(top_steps, i)->misc = 0;
	}
}

void
steps_done(void)
{
	unsigned i;
	assert(stack_size(steps_stack) == 1);
	if (!top_steps) my_error("steps_done: no steps_init\n");
	for (i = 1; i < stack_size(top_steps); ++i) {
		Tproof_step proof_step = stack_get(top_steps, i);
		proof_step_free(&proof_step);
	}
	if (stack_get(top_steps, 0)) my_error("proof_steps_done: internal error\n");
	stack_free(sko_var_to_choice);
	for (i = 0; i < stack_size(choices); ++i) {
		DAG_free(stack_get(choices, i).src);
		DAG_free(stack_get(choices, i).dest);
	}
	stack_free(choices);
	for (i = 0; i < stack_size(ite_csts); ++i) {
		DAG_free(stack_get(ite_csts, i).src);
		DAG_free(stack_get(ite_csts, i).dest);
	}
	stack_free(ite_csts);
	stack_free(top_steps);
	stack_free(steps_stack);
	hash_free(&steps_hash);
}
