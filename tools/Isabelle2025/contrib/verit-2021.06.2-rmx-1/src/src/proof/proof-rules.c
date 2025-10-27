#include "proof/proof-rules.h"

#include "proof/proof-checking.h"
#include "proof/proof-id.h"
#include "proof/proof-lemma-hash.h"
#include "proof/proof-output.h"
#include "proof/proof-rules-tautologies.h"
#include "proof/proof-step-table.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/DAG-symb.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "symbolic/context-recursion.h"
#include "symbolic/polarities.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"

/*
  --------------------------------------------------------------
  Proofs for [context dependent] transformations
  --------------------------------------------------------------
*/

Tproof
proof_step_conclusion(Tproof_type type, TDAG conclusion, Tstack_proof reasons)
{
	unsigned i;
	Tproof_step proof_step = proof_step_new();

	proof_step_add_DAG(proof_step, DAG_dup(conclusion));
	if (reasons)
		for (i = 0; i < stack_size(reasons); ++i)
			proof_step_add_reason(proof_step, stack_get(reasons, i));

#ifdef DEBUG_PROOF
	my_message("proof_rewriting: step:\n");
	print_proof_step(proof_step, top_steps, 0, stderr, false);
#endif

#ifdef DEBUG
	if (reasons)
		for (i = 0; i < stack_size(reasons); ++i)
			assert(stack_get(reasons, i) < stack_size(top_steps));
#endif

	proof_step->type = type;
	return steps_push(proof_step);
}

Tproof
proof_transformation(
	Tproof_type type, TDAG src, TDAG dest, Tstack_proof reasons)
{
	assert(DAG_sort(src) == DAG_sort(dest));
	TDAG equiv;
	Tproof proof_transf;

	equiv =
		DAG_sort(src) == SORT_BOOLEAN ? DAG_equiv(src, dest) : DAG_eq(src, dest);
	DAG_dup(equiv);
	proof_transf = proof_step_conclusion(type, equiv, reasons);

	DAG_free(equiv);
	return proof_transf;
}

/*
  --------------------------------------------------------------
  Choice handling
  --------------------------------------------------------------
*/

void
push_choices(TDAG src, Tstack_DAG DAGs)
{
	unsigned i, j;
	Tstack_DAG cache_old_sub;
	stack_INIT(cache_old_sub);
	/* Retrieve current sub \sigma = x1..xn -> e1..en and set DAG_symb_DAG */
	for (i = 0; i < stack_size(sko_var_to_choice); ++i) {
		TDAG var = stack_get(sko_var_to_choice, i).src;
		/* cache old sub for this var */
		stack_push(cache_old_sub, var);
		stack_push(cache_old_sub, DAG_symb_DAG[DAG_symb(var)]);
		/* map variable to its respective choice function */
		DAG_symb_DAG[DAG_symb(var)] = stack_get(sko_var_to_choice, i).dest;
	}
	/* mute substitution for given skolemized variables */
	for (i = 0; i < stack_size(DAGs) - 1u; i += 2) {
		TDAG var = stack_get(DAGs, i);
		stack_push(cache_old_sub, var);
		stack_push(cache_old_sub, DAG_symb_DAG[DAG_symb(var)]);
		DAG_symb_DAG[DAG_symb(var)] = DAG_NULL;
	}
	/* Introduce choice functions for given variables and sko terms */
	TDAG body = DAG_arg_last(src);
	for (i = 0; i < stack_size(DAGs) - 1u; i += 2) {
		/* var yi and its correspondent sko term sk_i */
		TDAG var = stack_get(DAGs, i), sko_term = stack_get(DAGs, i + 1);
		/* apply substitution \sigma on F[x1..xn, y1..yi-1, yi, yi+1..ym]
       *
       * An invariant is that domain(\sigma) = {x1..xn, y1..yi-1}
       */
		TDAG sub_form = DAG_tree_subst(body);
		/* build quantified formula exists y+1..ym. F\sigma */
		Tstack_DAG vars_and_body;
		stack_INIT(vars_and_body);
		for (j = i + 2; j < stack_size(DAGs) - 1u; j += 2) {
			stack_push(vars_and_body, stack_get(DAGs, j));
		}
		stack_push(vars_and_body, sub_form);
		TDAG choice_body = DAG_dup(
			stack_size(vars_and_body) == 1
				? sub_form
				: DAG_new_stack(DAG_symb(src), vars_and_body));
		DAG_free(sub_form);
		stack_free(vars_and_body);
		/* build choice epsilon yi . exists y+1..ym. F\sigma */
		TDAG choice = DAG_dup(DAG_new_binary(
			CHOICE_FUNCTION, var,
			DAG_symb(src) == QUANTIFIER_EXISTS ? choice_body : DAG_not(choice_body)));
		/* add yi -> (epsilon yi ...) to \sigma */
		DAG_symb_DAG[DAG_symb(var)] = choice;
		/* register yi -> (epsilon yi ...) for building choice functions downstream */
		stack_inc(sko_var_to_choice);
		stack_top(sko_var_to_choice).src = DAG_dup(var);
		stack_top(sko_var_to_choice).dest = DAG_dup(choice);
		/* register choice function for final printing instead of sko term */
		stack_inc(choices);
		stack_top(choices).src = DAG_dup(sko_term);
		stack_top(choices).dest = DAG_dup(choice);
	}
	/* Remove \sigma from DAG_symb_DAG and reset */
	for (i = 0; i < stack_size(cache_old_sub); i += 2) {
		DAG_symb_DAG[DAG_symb(stack_get(cache_old_sub, i))] =
			stack_get(cache_old_sub, i + 1);
	}
	stack_free(cache_old_sub);
}

void
pop_choices(TDAG src)
{
	unsigned i, offset;
	assert(sko_var_to_choice);
	assert((DAG_arity(src) - 1u) <= stack_size(sko_var_to_choice));
	/* Remove var -> choice from current substitution for each bound variable */
	offset = stack_size(sko_var_to_choice) - (DAG_arity(src) - 1u);
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		DAG_free(stack_get(sko_var_to_choice, offset + i).src);
		DAG_free(stack_get(sko_var_to_choice, offset + i).dest);
	}
	stack_dec_n(sko_var_to_choice, DAG_arity(src) - 1u);
}

/*
  --------------------------------------------------------------
  ITE handling
  --------------------------------------------------------------
*/

void
notify_ites(Tstack_DAG_assoc ite_terms, unsigned n)
{
	unsigned i;
	for (i = n; i < stack_size(ite_terms); ++i) {
		stack_inc(ite_csts);
		stack_top(ite_csts).src = DAG_dup(stack_get(ite_terms, i).dest);
		stack_top(ite_csts).dest = DAG_dup(stack_get(ite_terms, i).src);
	}
}

/* A EQUIV A AND ITE_DEFS */
Tproof
proof_ite_defs(TDAG DAG)
{
	Tproof_step proof_step = proof_step_new();
	proof_step_add_DAG(proof_step, DAG_dup(DAG));
	proof_step->type = ps_type_ite_intro;
	return steps_push(proof_step);
}

Tproof
proof_ite_intro(TDAG src, TDAG dest, Tproof id)
{
	TDAG equiv;
	Tproof proof_transf;

	Tstack_proof reasons;
	stack_INIT(reasons);
	stack_push(reasons, id);

	equiv = DAG_dup(DAG_equiv(src, dest));
	stack_push(reasons, proof_ite_defs(equiv));
	stack_push(reasons, proof_equiv_pos2(equiv));
	DAG_free(equiv);

	proof_transf = proof_step_conclusion(ps_type_th_resolution, dest, reasons);

	stack_free(reasons);
	return proof_transf;
}

/*
  --------------------------------------------------------------
  Resolution
  --------------------------------------------------------------
*/

#define FAIL_RESOLUTION \
	do { \
		my_message("Clause 1:\n"); \
		print_proof_step(proof_step1, top_steps, 0, stderr, false); \
		my_message("Clause 2:\n"); \
		print_proof_step(proof_step2, top_steps, 0, stderr, false); \
		my_DAG_message("Resolvent: %D\n", DAG); \
		my_error("proof_step_resolve: error\n"); \
		return NULL; \
	} while (0)

Tproof_step
proof_step_resolve(Tproof_step proof_step1, Tproof_step proof_step2, TDAG DAG)
{
	unsigned i;
	Tpol pol1 = POL_NONE, pol2 = POL_NONE;
	Tproof_step result = proof_step_new();

	for (i = 0; i < stack_size(proof_step1->DAGs); ++i)
		if (DAG_atom(stack_get(proof_step1->DAGs, i)) == DAG) {
			if (pol1 != POL_NONE) FAIL_RESOLUTION;
			pol1 = DAG_polarity(stack_get(proof_step1->DAGs, i));
		} else if (!DAG_misc(stack_get(proof_step1->DAGs, i))) {
			proof_step_add_DAG(result, DAG_dup(stack_get(proof_step1->DAGs, i)));
			DAG_misc_set(stack_get(proof_step1->DAGs, i), 1);
		}

	for (i = 0; i < stack_size(proof_step2->DAGs); ++i)
		if (DAG_atom(stack_get(proof_step2->DAGs, i)) == DAG) {
			if (pol2 != POL_NONE) FAIL_RESOLUTION;
			pol2 = DAG_polarity(stack_get(proof_step2->DAGs, i));
		} else if (!DAG_misc(stack_get(proof_step2->DAGs, i))) {
			proof_step_add_DAG(result, DAG_dup(stack_get(proof_step2->DAGs, i)));
			DAG_misc_set(stack_get(proof_step2->DAGs, i), 1);
		}

	if (pol1 == POL_NONE || pol2 == POL_NONE || pol1 == pol2) FAIL_RESOLUTION;
	for (i = 0; i < stack_size(result->DAGs); ++i)
		DAG_misc_set(stack_get(result->DAGs, i), 0);

#ifdef DEBUG_PROOF
	my_message("Resolving:\n");
	my_message("Clause 1:\n");
	print_proof_step(proof_step1, top_steps, 0, stderr, false);
	my_message("Clause 2:\n");
	print_proof_step(proof_step2, top_steps, 0, stderr, false);
	my_DAG_message("Resolvent: %D\n", DAG);
#endif

	return result;
}

/* TODO: I really dislike the next three functions. Do something about
   them. For one, instead of DAG_misc use DAG_tmp_unsigned */

Tproof_step* Pproof_step_tmp = NULL;
unsigned Pproof_step_tmp_nb = 0;

/* PF
   It would be nice to change n-resolution in 2-resolution.
   1. Identify atoms that are explained
   In fact those atoms that appear positively and negatively
   2. Use flag to assign a number to each atom
   3. For each explained atom, associate a step, explanation[atom],
   that explains it
   4. For each explained atom A, associate a list of steps A.steps
   requiring explanation
   5. Associate to each step C a number C.depth initialised to 0
   and the explained atom C.expl
   6. The first step does not require an explanation (conflict)
   7. For the first step C, call N(C)

   N(C)
   {
   if (C.depth > 0) return;
   For each C' in (C.expl).steps
   {
   N(C');
   C.depth = max(C.depth, C'.depth);
   }
   }
   8. Sort steps according to depth
   9. Binary resolve the first step with steps of increasing depth
*/

Tproof
proof_resolve_array(unsigned nb_clauses, Tproof* clauses)
{
	unsigned i, j;
	Tproof_step proof_step;

	if (nb_clauses < 1) proof_error("proof_resolve_array", NULL);
	if (nb_clauses == 1) {
		if (clauses[0] == 0) proof_error("proof_resolve_array", NULL);
		return clauses[0];
	}
	proof_step = proof_step_new();

#ifdef DEBUG_PROOF
	my_message("Resolving from\n");
	for (i = 0; i < nb_clauses; ++i) my_message("  step %d\n", clauses[i]);
#endif

	if (nb_clauses > Pproof_step_tmp_nb) {
		MY_REALLOC(Pproof_step_tmp, nb_clauses * sizeof(Tproof_step));
		Pproof_step_tmp_nb = nb_clauses;
	}

	for (i = 0; i < nb_clauses; ++i)
		if (clauses[i] == 0)
			proof_error("proof_resolve_array", NULL);
		else
			Pproof_step_tmp[i] = stack_get(top_steps, clauses[i]);

	for (i = 0; i < nb_clauses; ++i)
		for (j = 0; j < stack_size(Pproof_step_tmp[i]->DAGs); ++j) {
			TDAG DAG = stack_get((Pproof_step_tmp[i])->DAGs, j);
			TDAG DAG2 = DAG_atom(DAG);

			DAG_misc_set(DAG2, DAG_misc(DAG2) | DAG_polarity(DAG));
		}

	for (i = 0; i < nb_clauses; ++i)
		for (j = 0; j < stack_size(Pproof_step_tmp[i]->DAGs); j++) {
			TDAG DAG = stack_get((Pproof_step_tmp[i])->DAGs, j);
			TDAG DAG2 = DAG_atom(DAG);

			if (DAG_misc(DAG2) == POL_POS || DAG_misc(DAG2) == POL_NEG)
				proof_step_add_DAG(proof_step, DAG_dup(DAG));

			DAG_misc_set(DAG2, 0);
		}

	proof_step->type = ps_type_th_resolution;
	for (i = 0; i < nb_clauses; ++i)
		proof_step_add_reason(proof_step, clauses[i]);
	return steps_push(proof_step);
}

Tproof
proof_resolve(unsigned nb_steps, ...)
{
	unsigned i;
	va_list adpar;
	Tproof* steps;
	MY_MALLOC(steps, nb_steps * sizeof(Tproof));
	if (nb_steps < 1) proof_error("proof_resolve_array", NULL);
	va_start(adpar, nb_steps);
	for (i = 0; i < nb_steps; ++i) steps[i] = va_arg(adpar, Tproof);
	va_end(adpar);
	i = proof_resolve_array(nb_steps, steps);
	free(steps);
	return i;
}

Tproof
proof_bin_resolve(Tproof id1, Tproof id2)
{
	unsigned i;
	int tmp;
	Tproof_step proof_step = proof_step_new();
	Tproof_step step1, step2;
	if (!id1 || !id2) proof_error("proof_bin_resolve", NULL);
#ifdef DEBUG_PROOF
	my_message("Binary resolving from\n\t step %d\n\t step %d\n", id1, id2);
#endif
	step1 = stack_get(top_steps, id1);
	step2 = stack_get(top_steps, id2);
	/* PF count polarities.  The misc field of DAGs is used
     876543210
     bit 0 : DAG is used with positive polarity in step1
     bit 1 : DAG is used with negative polarity in step1
     bit 2 : DAG is used with positive polarity in step2
     bit 3 : DAG is used with negative polarity in step2
     bit 4 : DAG is resolvent
  */

	/* Misc setting to express DAG is resolvent */
	int32_t POL_RES = (1 << 4);

	for (i = 0; i < stack_size(step1->DAGs); ++i) {
		TDAG DAG = DAG_atom(stack_get(step1->DAGs, i));
		DAG_misc_set(DAG, DAG_misc(DAG) | DAG_polarity(stack_get(step1->DAGs, i)));
	}
	for (i = 0; i < stack_size(step2->DAGs); ++i) {
		TDAG DAG = DAG_atom(stack_get(step2->DAGs, i));
		DAG_misc_set(
			DAG, DAG_misc(DAG) | (DAG_polarity(stack_get(step2->DAGs, i)) << 2));
	}
	/* PF find resolvent
     Not merged with previous step for clarity
     Optimize if required */
	for (i = 0; i < stack_size(step1->DAGs); ++i) {
		tmp = DAG_misc(DAG_atom(stack_get(step1->DAGs, i)));
		if ((tmp & POL_POS) && (tmp & (POL_NEG << 2))) {
			DAG_misc_set(
				DAG_atom(stack_get(step1->DAGs, i)),
				POL_RES | POL_POS | (POL_NEG << 2));
			break;
		} else if ((tmp & (POL_POS << 2)) && (tmp & POL_NEG)) {
			DAG_misc_set(
				DAG_atom(stack_get(step1->DAGs, i)),
				POL_RES | (POL_POS << 2) | POL_NEG);
			break;
		}
	}
	if (i == stack_size(step1->DAGs)) {
		my_message("Binary resolving from\n");
		my_message("  step %d\n", id1);
		my_message("  step %d\n", id2);
		my_error("Nothing to resolve\n");
	}
	/* PF now build new step */
	tmp = 0;
	for (i = 0; i < stack_size(step1->DAGs); ++i) {
		TDAG dag = stack_get(step1->DAGs, i);
		if (
			!tmp && (DAG_misc(DAG_atom(dag)) & POL_RES) &&
			(DAG_misc(DAG_atom(dag)) & DAG_polarity(dag)))
			tmp = 1;
		else
			proof_step_add_DAG(proof_step, DAG_dup(dag));
	}
	if (!tmp) my_error("proof_bin_resolve: internal error\n");

	tmp = 0;
	for (i = 0; i < stack_size(step2->DAGs); ++i) {
		TDAG dag = stack_get(step2->DAGs, i);
		if (
			!tmp && (DAG_misc(DAG_atom(dag)) & POL_RES) &&
			(DAG_misc(DAG_atom(dag)) & (DAG_polarity(dag) << 2)))
			tmp = 1;
		else
			proof_step_add_DAG(proof_step, DAG_dup(stack_get(step2->DAGs, i)));
	}
	if (!tmp) my_error("proof_bin_resolve: internal error\n");

	/* PF reinitialise misc */
	for (i = 0; i < stack_size(step1->DAGs); ++i)
		DAG_misc_set(DAG_atom(stack_get(step1->DAGs, i)), 0);
	for (i = 0; i < stack_size(step2->DAGs); ++i)
		DAG_misc_set(DAG_atom(stack_get(step2->DAGs, i)), 0);

	proof_step->type = ps_type_th_resolution;
	proof_step_add_reason(proof_step, id1);
	proof_step_add_reason(proof_step, id2);
	return steps_push(proof_step);
}

/*
  --------------------------------------------------------------
  Input, SAT, UNSAT and Lemmas
  --------------------------------------------------------------
*/

void
proof_satisfiable(void)
{
	if (empty_clause) my_warning("proof_satisfiable: empty clause derived\n");
	if (proof_status != OPEN) my_warning("proof_satisfiable: status not open\n");
	proof_status = SAT;
}

void
proof_unsatisfiable(void)
{
	if (!empty_clause) my_error("proof_unsatifiable: no empty clause derived\n");
	if (proof_status != OPEN) my_warning("proof_unsatifiable: status not open\n");
	proof_status = UNSAT;
}

Tproof
proof_add_input(TDAG DAG)
{
	TDAG tmp = DAG;
	Tproof_step proof_step = proof_step_new();
	while (DAG_symb(tmp) == CONNECTOR_NOT &&
				 DAG_symb(DAG_arg0(tmp)) == CONNECTOR_NOT) {
		char** Pname = DAG_prop_get(tmp, DAG_PROP_NAMED);
		if (Pname) {
			char* name = strmake(*Pname);
			DAG_prop_set(DAG_arg0(DAG_arg0(tmp)), DAG_PROP_NAMED, &name);
		}
		tmp = DAG_arg0(DAG_arg0(tmp));
	}
	proof_step_add_DAG(proof_step, DAG_dup(DAG));
	proof_step->type = ps_type_assume;
	return steps_push(proof_step);
}

Tproof
proof_add_disequality_lemma(TDAG DAG)
{
	Tproof_step proof_step;
	Tproof proof_id = proof_lemma_get(DAG);
	if (proof_id) return proof_id;
	proof_step = proof_step_new();
	proof_step_add_DAG(proof_step, DAG);
	proof_step->type = ps_type_disequality_lemma;
	proof_id = steps_push(proof_step);
	proof_lemma_push(DAG, proof_id);
	return proof_id;
}

Tproof
proof_add_totality_lemma(TDAG DAG)
{
	Tproof_step proof_step;
	Tproof proof_id = proof_lemma_get(DAG);
	if (proof_id) return proof_id;
	proof_step = proof_step_new();
	proof_step_add_DAG(proof_step, DAG);
	proof_step->type = ps_type_totality_lemma;
	proof_id = steps_push(proof_step);
	proof_lemma_push(DAG, proof_id);
	return proof_id;
}

Tproof
proof_add_la_tautology(TDAG DAG)
{
	Tproof_step proof_step;
	Tproof proof_id = proof_lemma_get(DAG);
	if (proof_id) {
		DAG_free(DAG);
		return proof_id;
	}
	proof_step = proof_step_new();
	proof_step_add_DAG(proof_step, DAG);
	proof_step->type = ps_type_la_tautology;
	proof_id = steps_push(proof_step);
	proof_lemma_push(DAG, proof_id);
	return proof_id;
}

Tproof
proof_add_forall_inst_lemma(TDAG DAG, unsigned n, TDAG* PDAG)
{
	unsigned i;
	Tproof_step proof_step;
	Tproof proof_id;
	proof_id = proof_lemma_get(DAG);
	if (proof_id) return proof_id;
	proof_step = proof_step_new();
	proof_step_add_DAG(proof_step, DAG_dup(DAG));
	proof_step->type = ps_type_forall_inst_lemma;
	for (i = 0; i < n; i++) {
		assert(PDAG[i]);
		proof_step_add_arg(proof_step, DAG_dup(PDAG[i]));
	}
	proof_id = steps_push(proof_step);
	proof_lemma_push(DAG, proof_id);
	return proof_id;
}

Tproof
proof_get_lemma_id(TDAG DAG)
{
	Tproof proof_id = proof_lemma_get(DAG);
	if (!proof_id) my_error("proof_get_lemma_id: no lemma\n");
	return proof_id;
}

void
proof_set_lemma_id(TDAG DAG, Tproof id)
{
	if (!top_steps) my_error("no steps\n");
	if (stack_size(steps_stack) != 1) my_error("not available in subproof\n");
	if (
			stack_size(stack_get(top_steps, id)->DAGs) != 1 ||
			stack_get(stack_get(top_steps, id)->DAGs, 0) != DAG)
		my_error("proof_set_lemma_id: proof error\n");
	proof_lemma_push(DAG, id);
}

Tproof
proof_bfun_elim(Tproof id, TDAG DAG)
{
	Tproof_step new_proof_step = proof_step_new();
	if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
		proof_error("proof_bfun_elim", stack_get(top_steps, id));
	proof_step_add_DAG(new_proof_step, DAG_dup(DAG));
	new_proof_step->type = ps_type_bfun_elim;
	proof_step_add_reason(new_proof_step, id);
	return steps_push(new_proof_step);
}

Tproof
proof_deep_skolemize(Tproof id, TDAG DAG)
{
	Tproof_step new_proof_step = proof_step_new();
	if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
		proof_error("proof_deep_skolemize", stack_get(top_steps, id));
	proof_step_add_DAG(new_proof_step, DAG_dup(DAG));
	new_proof_step->type = ps_type_deep_skolemize;
	proof_step_add_reason(new_proof_step, id);
	return steps_push(new_proof_step);
}

