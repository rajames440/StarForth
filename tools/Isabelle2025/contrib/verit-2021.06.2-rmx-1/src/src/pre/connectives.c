#include "pre/connectives.h"

#include "symbolic/DAG-prop.h"
#include "symbolic/DAG.h"
#include "symbolic/context-recursion.h"
#include "symbolic/qnt-utils.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"

/*
  --------------------------------------------------------------
  Leave quantifiers under connectives with single polarity
  --------------------------------------------------------------
*/

void
single_pol_qnt_init(void)
{}

void
single_pol_qnt_push(TDAG src, unsigned* pos)
{}

void
single_pol_qnt_pop(TDAG src, unsigned pos)
{}

TDAG
single_pol_qnt_reduce(TDAG src)
{
	TDAG dest;
	if (DAG_symb(src) == CONNECTOR_XOR) {
		dest = DAG_dup(DAG_or2(
			DAG_and2(DAG_not(DAG_arg0(src)), DAG_arg1(src)),
			DAG_and2(DAG_not(DAG_arg1(src)), DAG_arg0(src))));
		DAG_free(src);
		return dest;
	}
	if (DAG_symb(src) == CONNECTOR_EQUIV) {
		dest = DAG_dup(DAG_and2(
			DAG_implies(DAG_arg0(src), DAG_arg1(src)),
			DAG_implies(DAG_arg1(src), DAG_arg0(src))));
		DAG_free(src);
		return dest;
	}
	if (DAG_symb(src) == CONNECTOR_ITE && DAG_quant(DAG_arg(src, 0))) {
		dest = DAG_dup(DAG_and2(
			DAG_implies(DAG_arg(src, 0), DAG_arg(src, 1)),
			DAG_implies(DAG_not(DAG_arg(src, 0)), DAG_arg(src, 2))));
		DAG_free(src);
		return dest;
	}
	return src;
}

void
single_pol_qnt_push_proof(TDAG src, unsigned* pos)
{
	unsigned i;
	Tstack_DAG DAGs;
	if (!quantifier(DAG_symb(src))) return;
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(src) - 1u; ++i) stack_push(DAGs, DAG_arg(src, i));
	stack_push(DAGs, DAG_arity(src) - 1u);
	/* Start transformation proof */
	proof_subproof_begin_context(ps_type_bind, DAGs, NULL);
	stack_free(DAGs);
}

Tproof
single_pol_qnt_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
{
	if (new_src == src) {
		if (quantifier(DAG_symb(new_src))) proof_subproof_remove();
		return 0;
	}
	assert(!stack_is_empty(reasons));
	/* Build proof from src to new_src */
	return quantifier(DAG_symb(new_src))
		? proof_subproof_end_transformation(src, new_src)
		: proof_transformation(ps_type_cong, src, new_src, reasons);
}

TDAG_proof
single_pol_qnt_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	TDAG_proof dest;
	Tstack_proof reasons;
	Tproof proof_trans;
	dest.DAG = new_src;
	stack_INIT(reasons);
	if (proof_build) stack_push(reasons, proof_build);
	if (DAG_symb(new_src) == CONNECTOR_XOR)
		dest.DAG = DAG_dup(DAG_or2(
			DAG_and2(DAG_not(DAG_arg0(new_src)), DAG_arg1(new_src)),
			DAG_and2(DAG_not(DAG_arg0(new_src)), DAG_arg1(new_src))));
	else if (DAG_symb(new_src) == CONNECTOR_EQUIV)
		dest.DAG = DAG_dup(DAG_and2(
			DAG_implies(DAG_arg0(new_src), DAG_arg1(new_src)),
			DAG_implies(DAG_arg1(new_src), DAG_arg0(new_src))));
	else if (DAG_symb(new_src) == CONNECTOR_ITE && DAG_quant(DAG_arg(new_src, 0)))
		dest.DAG = DAG_dup(DAG_and2(
			DAG_implies(DAG_arg(new_src, 0), DAG_arg(new_src, 1)),
			DAG_implies(DAG_not(DAG_arg(new_src, 0)), DAG_arg(new_src, 2))));
	/* Prove transformation */
	if (dest.DAG != new_src) {
		stack_push(
			reasons,
			proof_transformation(ps_type_connective_def, new_src, dest.DAG, NULL));
		DAG_free(new_src);
		if (stack_size(reasons) > 1) {
			proof_trans = proof_transformation(ps_type_trans, src, dest.DAG, reasons);
			stack_reset(reasons);
			stack_push(reasons, proof_trans);
		}
	}
	if (stack_is_empty(reasons)) stack_free(reasons);
	dest.proof = reasons;
	return dest;
}

/*
  --------------------------------------------------------------
  Rewrite weak existentials
  --------------------------------------------------------------
*/

#define previous_pol (stack_top_n(context.pols, 2))

bool
rewrite_w_exist_cont(TDAG DAG)
{
	return DAG_quant(DAG) && ctx_pol != POL_BOTH;
}

void
rewrite_w_exist_init(void)
{
	stack_INIT(context.pols);
	stack_push(context.pols, POL_POS);
}

void
rewrite_w_exist_push(TDAG src, unsigned* pos)
{
	Tpol pol;
	assert(!stack_is_empty(context.pols));
	pol = ctx_pol;
	if (
		DAG_symb(src) == CONNECTOR_NOT ||
		(DAG_symb(src) == CONNECTOR_IMPLIES && *pos == 0)) {
		assert(DAG_symb(src) == CONNECTOR_NOT || DAG_arity(src) == 2);
		pol = INV_POL(pol);
		stack_push(context.pols, pol);
		return;
	}
	/* Last case is for atoms and terms */
	if (
		(DAG_symb(src) == CONNECTOR_ITE && *pos == 0) ||
		DAG_symb(src) == CONNECTOR_EQUIV || DAG_symb(src) == CONNECTOR_XOR ||
		(!boolean_connector(DAG_symb(src)) && !binder(DAG_symb(src)))) {
		assert(
			DAG_symb(src) == CONNECTOR_ITE || DAG_arity(src) == 2 ||
			DAG_literal(src));
		stack_push(context.pols, POL_BOTH);
		return;
	}
}

void
rewrite_w_exist_pop(TDAG src, unsigned pos)
{
	if (
		DAG_symb(src) == CONNECTOR_NOT ||
		(DAG_symb(src) == CONNECTOR_IMPLIES && pos == 0) ||
		((DAG_symb(src) == CONNECTOR_ITE && pos == 0) ||
		 DAG_symb(src) == CONNECTOR_EQUIV || DAG_symb(src) == CONNECTOR_XOR ||
		 (!boolean_connector(DAG_symb(src)) && !binder(DAG_symb(src)))))
		stack_dec(context.pols);
}

TDAG
rewrite_w_exist_reduce(TDAG src)
{
	unsigned i;
	TDAG *PDAG, dest;
	Tstack_DAGstack *Ptriggers, triggers;
	if (ctx_pol != POL_NEG || DAG_symb(src) != QUANTIFIER_EXISTS) return src;
	MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
	for (i = 0; i < DAG_arity(src) - 1u; ++i) PDAG[i] = DAG_arg(src, i);
	PDAG[i] = DAG_not(DAG_arg_last(src));
	dest = DAG_dup(DAG_not(DAG_new(QUANTIFIER_FORALL, DAG_arity(src), PDAG)));
	/* Copy triggers */
	{
		Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
		if (Ptriggers) {
			triggers = copy_triggers(*Ptriggers);
			DAG_prop_set(dest, DAG_PROP_TRIGGER, &triggers);
		}
	}
	DAG_free(src);
	return dest;
}

void
rewrite_w_exist_push_proof(TDAG src, unsigned* pos)
{
	unsigned i;
	Tstack_DAG DAGs;
	Tpol pol;
	if (quantifier(DAG_symb(src))) {
		stack_INIT(DAGs);
		for (i = 0; i < DAG_arity(src) - 1u; ++i) stack_push(DAGs, DAG_arg(src, i));
		stack_push(DAGs, DAG_arity(src) - 1u);
		/* Start transformation proof */
		proof_subproof_begin_context(ps_type_bind, DAGs, NULL);
		stack_free(DAGs);
		return;
	}
	assert(!stack_is_empty(context.pols));
	pol = ctx_pol;
	/* Update polarity */
	if (
		DAG_symb(src) == CONNECTOR_NOT ||
		(DAG_symb(src) == CONNECTOR_IMPLIES && *pos == 0)) {
		assert(DAG_symb(src) == CONNECTOR_NOT || DAG_arity(src) == 2);
		pol = INV_POL(pol);
		stack_push(context.pols, pol);
		return;
	}
	/* Last case is for atoms and terms */
	if (
		(DAG_symb(src) == CONNECTOR_ITE && *pos == 0) ||
		DAG_symb(src) == CONNECTOR_EQUIV || DAG_symb(src) == CONNECTOR_XOR ||
		(!boolean_connector(DAG_symb(src)) && !binder(DAG_symb(src)))) {
		assert(
			DAG_symb(src) == CONNECTOR_ITE || DAG_arity(src) == 2 ||
			DAG_literal(src));
		stack_push(context.pols, POL_BOTH);
		return;
	}
}

Tproof
rewrite_w_exist_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
{
	if (new_src == src) {
		if (quantifier(DAG_symb(new_src))) proof_subproof_remove();
		return 0;
	}
	assert(!stack_is_empty(reasons));
	return (quantifier(DAG_symb(new_src)))
		? proof_subproof_end_transformation(src, new_src)
		: proof_transformation(ps_type_cong, src, new_src, reasons);
}

TDAG_proof
rewrite_w_exist_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	unsigned i;
	TDAG_proof dest;
	TDAG* PDAG;
	Tstack_DAGstack *Ptriggers, triggers;
	Tstack_proof reasons;
	Tproof proof_trans;
	dest.DAG = new_src;
	if (ctx_pol != POL_NEG || DAG_symb(new_src) != QUANTIFIER_EXISTS) {
		if (proof_build) {
			stack_INIT(dest.proof);
			stack_push(dest.proof, proof_build);
		} else
			dest.proof = NULL;
		return dest;
	}
	stack_INIT(reasons);
	if (proof_build) stack_push(reasons, proof_build);
	MY_MALLOC(PDAG, DAG_arity(new_src) * sizeof(TDAG));
	for (i = 0; i < DAG_arity(new_src) - 1u; ++i) PDAG[i] = DAG_arg(new_src, i);
	PDAG[i] = DAG_not(DAG_arg_last(new_src));
	dest.DAG =
		DAG_dup(DAG_not(DAG_new(QUANTIFIER_FORALL, DAG_arity(new_src), PDAG)));
	/* Copy triggers */
	{
		Ptriggers = DAG_prop_get(new_src, DAG_PROP_TRIGGER);
		if (Ptriggers) {
			triggers = copy_triggers(*Ptriggers);
			DAG_prop_set(dest.DAG, DAG_PROP_TRIGGER, &triggers);
		}
	}
	/* Prove transformation */
	stack_push(
		reasons,
		proof_transformation(ps_type_connective_def, new_src, dest.DAG, NULL));
	DAG_free(new_src);
	if (stack_size(reasons) > 1) {
		proof_trans = proof_transformation(ps_type_trans, src, dest.DAG, reasons);
		stack_reset(reasons);
		stack_push(reasons, proof_trans);
	}
	dest.proof = reasons;
	return dest;
}
