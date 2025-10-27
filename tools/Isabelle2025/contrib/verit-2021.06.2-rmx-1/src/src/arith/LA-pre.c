#include "arith/LA-pre.h"

#include "arith/totality.h"
#include "proof/proof.h"
#include "symbolic/context-recursion-proof.h"
#include "symbolic/context-recursion.h"
#include "symbolic/recursion.h"

#define DAG_LEQ(a, b) DAG_new_binary(PREDICATE_LEQ, a, b)

extern bool LA_enable_lemmas_totality;

void
rewrite_eq_init(void)
{}

void
rewrite_eq_push(TDAG src, unsigned* pos)
{}

void
rewrite_eq_pop(TDAG src, unsigned pos)
{}

TDAG
rewrite_eq_reduce(TDAG src)
{
	TDAG dest;
	if (DAG_symb(src) != PREDICATE_EQ) return src;
	if (LA_enable_lemmas_totality) {
		TDAG DAG0 = DAG_LEQ(DAG_arg0(src), DAG_arg1(src));
		TDAG DAG1 = DAG_LEQ(DAG_arg1(src), DAG_arg0(src));
		totality_register(DAG_or2(DAG0, DAG1));
	}
	dest = DAG_dup(DAG_and2(
		DAG_LEQ(DAG_arg0(src), DAG_arg1(src)),
		DAG_LEQ(DAG_arg1(src), DAG_arg0(src))));
	DAG_free(src);
	return dest;
}

void
rewrite_eq_push_proof(TDAG src, unsigned* pos)
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
rewrite_eq_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
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
rewrite_eq_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	TDAG_proof dest;
	Tstack_proof reasons;
	Tproof proof_trans;
	dest.DAG = new_src;
	if (DAG_symb(new_src) != PREDICATE_EQ) {
		if (proof_build) {
			stack_INIT(dest.proof);
			stack_push(dest.proof, proof_build);
		} else
			dest.proof = NULL;
		return dest;
	}
	stack_INIT(reasons);
	if (proof_build) stack_push(reasons, proof_build);
	if (LA_enable_lemmas_totality) {
		TDAG DAG0 = DAG_LEQ(DAG_arg0(new_src), DAG_arg1(new_src));
		TDAG DAG1 = DAG_LEQ(DAG_arg1(new_src), DAG_arg0(new_src));
		totality_register(DAG_or2(DAG0, DAG1));
	}
	dest.DAG = DAG_dup(DAG_and2(
		DAG_LEQ(DAG_arg0(new_src), DAG_arg1(new_src)),
		DAG_LEQ(DAG_arg1(new_src), DAG_arg0(new_src))));
	stack_push(
		reasons, proof_transformation(ps_type_la_rw_eq, new_src, dest.DAG, NULL));
	if (stack_size(reasons) > 1) {
		proof_trans = proof_transformation(ps_type_trans, src, dest.DAG, reasons);
		stack_reset(reasons);
		stack_push(reasons, proof_trans);
	}
	dest.proof = reasons;
	DAG_free(src);
	return dest;
}
