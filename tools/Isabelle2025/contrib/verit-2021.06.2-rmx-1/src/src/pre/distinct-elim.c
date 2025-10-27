/*
  --------------------------------------------------------------
  distinct operator removing in terms and formulas
  --------------------------------------------------------------
*/

#include "pre/distinct-elim.h"

#include "pre/simp-sym.h"
#include "proof/proof.h"
#include "symbolic/context-recursion-proof.h"
#include "symbolic/context-recursion.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void
distinct_elim_init(void)
{}

void
distinct_elim_push(TDAG src, unsigned* pos)
{}

void
distinct_elim_pop(TDAG src, unsigned pos)
{}

TDAG
distinct_elim_reduce(TDAG src)
{
	unsigned i, j, k = 0;
	TDAG dest, *PDAG;
	if (DAG_symb(src) != PREDICATE_DISTINCT) return src;
	if (DAG_arity(src) <= 1) {
		DAG_free(src);
		return DAG_dup(DAG_TRUE);
	}
	if (DAG_sort(DAG_arg(src, 0)) == SORT_BOOLEAN) {
		dest = DAG_dup(
			DAG_arity(src) > 2 ? DAG_FALSE
												 : DAG_not(DAG_equiv(DAG_arg0(src), DAG_arg1(src))));
		DAG_free(src);
		return dest;
	}
	if (DAG_arity(src) == 2) {
		dest = DAG_dup(DAG_neq(DAG_arg0(src), DAG_arg1(src)));
		DAG_free(src);
		return dest;
	}
	MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
	memcpy(PDAG, DAG_args(src), DAG_arity(src) * sizeof(TDAG));
	simp_sym_notify(DAG_arity(src), PDAG);
	MY_MALLOC(PDAG, DAG_arity(src) * (DAG_arity(src) - 1u) * sizeof(TDAG) / 2);
	for (i = 0; i < DAG_arity(src); ++i)
		for (j = i + 1; j < DAG_arity(src); ++j)
			PDAG[k++] = DAG_neq(DAG_arg(src, i), DAG_arg(src, j));
	dest = DAG_dup(
		DAG_new(CONNECTOR_AND, DAG_arity(src) * (DAG_arity(src) - 1u) / 2, PDAG));
	DAG_free(src);
	return dest;
}

/*
  --------------------------------------------------------------
  Proof production
  --------------------------------------------------------------
*/

void
distinct_elim_push_proof(TDAG src, unsigned* pos)
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
distinct_elim_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
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
distinct_elim_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	unsigned i, j, k = 0;
	TDAG_proof dest;
	TDAG* PDAG;
	Tstack_proof reasons;
	Tproof proof_trans;
	dest.DAG = new_src;
	if (DAG_symb(new_src) != PREDICATE_DISTINCT) {
		if (proof_build) {
			stack_INIT(dest.proof);
			stack_push(dest.proof, proof_build);
		} else
			dest.proof = NULL;
		return dest;
	}
	stack_INIT(reasons);
	if (proof_build) stack_push(reasons, proof_build);
	if (DAG_arity(new_src) <= 1)
		dest.DAG = DAG_dup(DAG_TRUE);
	else if (DAG_sort(DAG_arg(new_src, 0)) == SORT_BOOLEAN)
		dest.DAG = DAG_dup(
			DAG_arity(new_src) > 2
				? DAG_FALSE
				: DAG_not(DAG_equiv(DAG_arg0(new_src), DAG_arg1(new_src))));
	else if (DAG_arity(new_src) == 2)
		dest.DAG = DAG_dup(DAG_neq(DAG_arg0(new_src), DAG_arg1(new_src)));
	else {
		MY_MALLOC(PDAG, DAG_arity(new_src) * sizeof(TDAG));
		memcpy(PDAG, DAG_args(new_src), DAG_arity(new_src) * sizeof(TDAG));
		simp_sym_notify(DAG_arity(new_src), PDAG);
		MY_MALLOC(
			PDAG, DAG_arity(new_src) * (DAG_arity(new_src) - 1) * sizeof(TDAG) / 2);
		for (i = 0; i < DAG_arity(new_src); i++)
			for (j = i + 1; j < DAG_arity(new_src); j++)
				PDAG[k++] = DAG_neq(DAG_arg(new_src, i), DAG_arg(new_src, j));
		dest.DAG = DAG_dup(DAG_new(
			CONNECTOR_AND, DAG_arity(new_src) * (DAG_arity(new_src) - 1) / 2, PDAG));
	}
	stack_push(
		reasons,
		proof_transformation(ps_type_distinct_elim, new_src, dest.DAG, NULL));
	DAG_free(new_src);
	if (stack_size(reasons) > 1) {
		proof_trans = proof_transformation(ps_type_trans, src, dest.DAG, reasons);
		stack_reset(reasons);
		stack_push(reasons, proof_trans);
	}
	dest.proof = reasons;
	return dest;
}
