#include "proof/proof-checking.h"

#include "proof/proof-output.h"
#include "symbolic/polarities.h"

void
proof_error(char* str, Tproof_step proof_step)
{
	/* PF There is a conception problem that force to put 0 here */
	DAG_tmp_reserve();
	if (proof_step) print_proof_step(proof_step, top_steps, 0, stderr, false);
	DAG_tmp_release();
	my_error("%s : proof error\n", str);
}

/*
  --------------------------------------------------------------
  Congruence checking
  --------------------------------------------------------------
*/

void
proof_check_eq_congruent_pred(Tproof_step proof_step)
{
	TDAG pred1, pred2;
	unsigned i, num_eqs;

	if (
		DAG_polarity(stack_top_n(proof_step->DAGs, 2)) ==
		DAG_polarity(stack_top(proof_step->DAGs)))
		proof_error("eq_congruent_pred", proof_step);

	pred1 = DAG_atom(stack_top_n(proof_step->DAGs, 2));
	pred2 = DAG_atom(stack_top(proof_step->DAGs));
	num_eqs = stack_size(proof_step->DAGs) - 2u;

	if (
		DAG_symb(pred1) != DAG_symb(pred2) ||
		DAG_arity(pred1) != DAG_arity(pred2) || DAG_arity(pred1) < num_eqs)
		proof_error("eq_congruent_pred", proof_step);

	for (i = 0; i < num_eqs; ++i) {
		TDAG eq = DAG_atom(stack_get(proof_step->DAGs, i));

		if (DAG_polarity(stack_get(proof_step->DAGs, i)) != POL_NEG)
			proof_error("eq_congruent_pred", proof_step);

		if (
			DAG_symb(eq) != PREDICATE_EQ ||
			((DAG_arg0(eq) != DAG_arg(pred1, i) ||
				DAG_arg1(eq) != DAG_arg(pred2, i)) &&
			 (DAG_arg1(eq) != DAG_arg(pred1, i) ||
				DAG_arg0(eq) != DAG_arg(pred2, i))))
			proof_error("eq_congruent_pred", proof_step);
	}
}

void
proof_check_eq_congruent(Tproof_step proof_step)
{
	unsigned i, num_eqs;
	TDAG concl = stack_top(proof_step->DAGs);
	num_eqs = stack_size(proof_step->DAGs) - 1u;
	if (DAG_polarity(concl) != POL_POS) proof_error("eq_congruent", proof_step);
	concl = DAG_atom(concl);
	if (
		DAG_symb(concl) != PREDICATE_EQ ||
		DAG_symb(DAG_arg0(concl)) != DAG_symb(DAG_arg1(concl)) ||
		DAG_arity(DAG_arg0(concl)) != DAG_arity(DAG_arg1(concl)) ||
		DAG_arity(DAG_arg0(concl)) < num_eqs)
		proof_error("eq_congruent", proof_step);
	for (i = 0; i < num_eqs; ++i) {
		TDAG eq = DAG_atom(stack_get(proof_step->DAGs, i));
		if (DAG_polarity(stack_get(proof_step->DAGs, i)) != POL_NEG)
			proof_error("eq_congruent", proof_step);
		if (
			DAG_symb(eq) != PREDICATE_EQ ||
			((DAG_arg0(eq) != DAG_arg(DAG_arg0(concl), i) ||
				DAG_arg1(eq) != DAG_arg(DAG_arg1(concl), i)) &&
			 (DAG_arg1(eq) != DAG_arg(DAG_arg0(concl), i) ||
				DAG_arg0(eq) != DAG_arg(DAG_arg1(concl), i))))
			proof_error("eq_congruent", proof_step);
	}
}

void
proof_check_eq_reflexive(Tproof_step proof_step)
{
	if (
		stack_size(proof_step->DAGs) != 1 ||
		DAG_symb(stack_get(proof_step->DAGs, 0)) != PREDICATE_EQ ||
		DAG_arg0(stack_get(proof_step->DAGs, 0)) !=
			DAG_arg1(stack_get(proof_step->DAGs, 0)))
		proof_error("eq_reflexive", proof_step);
}

void
proof_check_eq_transitive(Tproof_step proof_step)
{
	TDAG eq1, eq2;
	unsigned i, start, orient;

	if (stack_is_empty(proof_step->DAGs))
		proof_error("eq_transitive", proof_step);

	for (i = 0; i < stack_size(proof_step->DAGs) - 1u; ++i)
		if (
			DAG_polarity(stack_get(proof_step->DAGs, i)) != POL_NEG ||
			DAG_symb(DAG_atom(stack_get(proof_step->DAGs, i))) != PREDICATE_EQ ||
			DAG_arity(DAG_atom(stack_get(proof_step->DAGs, i))) != 2)
			proof_error("eq_transitive", proof_step);

	i = stack_size(proof_step->DAGs) - 1u;

	if (
		DAG_polarity(stack_get(proof_step->DAGs, i)) != POL_POS ||
		DAG_symb(DAG_atom(stack_get(proof_step->DAGs, i))) != PREDICATE_EQ ||
		DAG_arity(DAG_atom(stack_get(proof_step->DAGs, i))) != 2)
		proof_error("eq_transitive", proof_step);

	/* PF first detect where the chain start is */
	eq1 = DAG_atom(stack_get(proof_step->DAGs, 0));
	eq2 = DAG_atom(stack_get(proof_step->DAGs, 1));
	DAG_misc_set(DAG_arg0(eq2), 1);
	DAG_misc_set(DAG_arg1(eq2), 1);
	start = (DAG_misc(DAG_arg0(eq1)) == 1);
	DAG_misc_set(DAG_arg0(eq2), 0);
	DAG_misc_set(DAG_arg1(eq2), 0);
	orient = 1 - start;

	for (i = 0; i < stack_size(proof_step->DAGs) - 2u; ++i) {
		eq1 = DAG_atom(stack_get(proof_step->DAGs, i));
		eq2 = DAG_atom(stack_get(proof_step->DAGs, i + 1));
		if (DAG_arg(eq1, orient) == DAG_arg0(eq2)) {
			orient = 1;
			continue;
		}
		if (DAG_arg(eq1, orient) == DAG_arg1(eq2)) {
			orient = 0;
			continue;
		}
		proof_error("eq_transitive", proof_step);
	}

	eq1 = DAG_atom(stack_top(proof_step->DAGs));
	eq2 = DAG_atom(stack_get(proof_step->DAGs, 0));
	if (DAG_arg(eq2, start) == DAG_arg0(eq1)) {
		eq2 = DAG_atom(stack_top_n(proof_step->DAGs, 2));
		if (DAG_arg(eq2, orient) != DAG_arg1(eq1))
			proof_error("eq_transitive", proof_step);
	} else if (DAG_arg(eq2, start) == DAG_arg1(eq1)) {
		eq2 = DAG_atom(stack_top_n(proof_step->DAGs, 2));
		if (DAG_arg(eq2, orient) != DAG_arg0(eq1))
			proof_error("eq_transitive", proof_step);
	}
}

void
check_proof()
{
	unsigned i;
	Tproof_step proof_step;

	/* TODO: add proof checking for other types */
	for (i = 1; i < stack_size(top_steps); ++i) {
		proof_step = stack_get(top_steps, i);
		switch (proof_step->type) {
			case ps_type_eq_congruent_pred:
				proof_check_eq_congruent_pred(proof_step);
				break;
			case ps_type_eq_congruent: proof_check_eq_congruent(proof_step); break;
			case ps_type_eq_transitive: proof_check_eq_transitive(proof_step); break;
			case ps_type_eq_reflexive: proof_check_eq_reflexive(proof_step);
			default:;
		}
	}
}
