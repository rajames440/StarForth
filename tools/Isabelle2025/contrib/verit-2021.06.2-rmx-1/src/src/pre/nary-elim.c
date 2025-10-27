/*
  --------------------------------------------------------------
  removing n-ary operators in terms and formulas
  --------------------------------------------------------------
*/

#include "pre/nary-elim.h"

#include "proof/proof.h"
#include "symbolic/context-recursion.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"

#include <assert.h>
#include <stdlib.h>

/* #define DEBUG_NARY_ELIM */

/* (f t1 · · · tn ) --> (f · · · (f (f t1 t2 ) t3 ) · · · tn ) */

static TDAG
left_assoc_elim(TDAG src)
{
	unsigned i;
	TDAG tmp;
	if (DAG_arity(src) <= 2) return src;
	tmp = DAG_new_binary(DAG_symb(src), DAG_arg(src, 0), DAG_arg(src, 1));
	for (i = 2; i < DAG_arity(src); i++)
		tmp = DAG_new_binary(DAG_symb(src), tmp, DAG_arg(src, i));
	tmp = DAG_dup(tmp);
	DAG_free(src);
	return tmp;
}

/* (f t1 · · · tn ) --> (f t1 (f t2 · · · (f tn−1 tn ) · · · ) */

static TDAG
right_assoc_elim(TDAG src)
{
	unsigned i;
	TDAG tmp;
	if (DAG_arity(src) <= 2) return src;
	tmp = DAG_new_binary(
		DAG_symb(src), DAG_arg(src, DAG_arity(src) - 2),
		DAG_arg(src, DAG_arity(src) - 1));
	for (i = DAG_arity(src) - 2; i-- != 0;)
		tmp = DAG_new_binary(DAG_symb(src), DAG_arg(src, i), tmp);
	tmp = DAG_dup(tmp);
	DAG_free(src);
	return tmp;
}

/* (f t1 · · · tn ) --> (and (f t1 t2 ) (f t2 t3 )· · · (f tn−1 tn )) */

static TDAG
chainable_elim(TDAG src)
{
	unsigned i;
	TDAG *tmp, dest;
	if (DAG_arity(src) <= 2) return src;
	MY_MALLOC(tmp, (DAG_arity(src) - 1u) * sizeof(TDAG));
	for (i = 0; i < DAG_arity(src) - 1u; i++)
		tmp[i] =
			DAG_new_binary(DAG_symb(src), DAG_arg(src, i), DAG_arg(src, i + 1));
	dest = DAG_dup(DAG_new(CONNECTOR_AND, DAG_arity(src) - 1u, tmp));
	DAG_free(src);
	return dest;
}

void
nary_elim_node_init(void)
{}

void
nary_elim_node_push(TDAG src, unsigned* pos)
{}

void
nary_elim_node_pop(TDAG src, unsigned pos)
{}

/* (f t1 · · · tn ) --> (and (f t1 t2 ) (f t2 t3 )· · · (f tn−1 tn )) */
TDAG
nary_elim_node_reduce(TDAG src)
{
	if (DAG_symb(src) == CONNECTOR_IMPLIES) return right_assoc_elim(src);
	if ((DAG_symb(src) == CONNECTOR_XOR) || (DAG_symb(src) == FUNCTION_MINUS))
		return left_assoc_elim(src);
	if (
		(DAG_symb(src) == PREDICATE_LESS) || (DAG_symb(src) == PREDICATE_LEQ) ||
		(DAG_symb(src) == PREDICATE_GREATER) ||
		(DAG_symb(src) == PREDICATE_GREATEREQ) ||
		(DAG_symb(src) == CONNECTOR_EQUIV) || (DAG_symb(src) == PREDICATE_EQ))
		return chainable_elim(src);
	return src;
}

TDAG
nary_elim_node(TDAG src)
{
	if (DAG_symb(src) == CONNECTOR_IMPLIES) return right_assoc_elim(src);
	if ((DAG_symb(src) == CONNECTOR_XOR) || (DAG_symb(src) == FUNCTION_MINUS))
		return left_assoc_elim(src);
	if (
		(DAG_symb(src) == PREDICATE_LESS) || (DAG_symb(src) == PREDICATE_LEQ) ||
		(DAG_symb(src) == PREDICATE_GREATER) ||
		(DAG_symb(src) == PREDICATE_GREATEREQ) ||
		(DAG_symb(src) == CONNECTOR_EQUIV) || (DAG_symb(src) == PREDICATE_EQ))
		return chainable_elim(src);
	return src;
}

/*
  --------------------------------------------------------------
  Proof production
  --------------------------------------------------------------
*/

static TDAG_proof
left_assoc_elim_proof(TDAG src)
{
	unsigned i;
	TDAG tmp;
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	if (DAG_arity(src) <= 2) return dest;
	tmp = DAG_new_binary(DAG_symb(src), DAG_arg(src, 0), DAG_arg(src, 1));
	for (i = 2; i < DAG_arity(src); i++)
		tmp = DAG_new_binary(DAG_symb(src), tmp, DAG_arg(src, i));
	dest.DAG = DAG_dup(tmp);
	stack_INIT(dest.proof);
	stack_push(
		dest.proof,
		proof_transformation(ps_type_nary_elim, src, dest.DAG, NULL));
	DAG_free(src);
	return dest;
}

static TDAG_proof
right_assoc_elim_proof(TDAG src)
{
	unsigned i;
	TDAG tmp;
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	if (DAG_arity(src) <= 2) return dest;
	tmp = DAG_new_binary(
		DAG_symb(src), DAG_arg(src, DAG_arity(src) - 2),
		DAG_arg(src, DAG_arity(src) - 1));
	for (i = DAG_arity(src) - 2; i-- != 0;)
		tmp = DAG_new_binary(DAG_symb(src), DAG_arg(src, i), tmp);
	dest.DAG = DAG_dup(tmp);
	stack_INIT(dest.proof);
	stack_push(
		dest.proof,
		proof_transformation(ps_type_nary_elim, src, dest.DAG, NULL));
	DAG_free(src);
	return dest;
}

static TDAG_proof
chainable_elim_proof(TDAG src)
{
	unsigned i;
	TDAG* PDAG;
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	if (DAG_arity(src) <= 2) return dest;
	MY_MALLOC(PDAG, (DAG_arity(src) - 1) * sizeof(TDAG));
	for (i = 0; i < DAG_arity(src) - 1; i++)
		PDAG[i] =
			DAG_new_binary(DAG_symb(src), DAG_arg(src, i), DAG_arg(src, i + 1));
	dest.DAG = DAG_dup(DAG_new(CONNECTOR_AND, DAG_arity(src) - 1, PDAG));
	stack_INIT(dest.proof);
	stack_push(
		dest.proof,
		proof_transformation(ps_type_nary_elim, src, dest.DAG, NULL));
	DAG_free(src);
	return dest;
}

void
nary_elim_node_push_proof(TDAG src, unsigned* pos)
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
nary_elim_node_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
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
nary_elim_node_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	TDAG_proof dest, tmp;
	Tstack_proof reasons;
	Tproof proof_trans;
	stack_INIT(reasons);
	if (proof_build) stack_push(reasons, proof_build);
	tmp.DAG = new_src;
	tmp.proof = NULL;
	if (DAG_symb(new_src) == CONNECTOR_IMPLIES)
		tmp = right_assoc_elim_proof(new_src);
	else if (
		(DAG_symb(new_src) == CONNECTOR_XOR) ||
		(DAG_symb(new_src) == FUNCTION_MINUS))
		tmp = left_assoc_elim_proof(new_src);
	else if (
		(DAG_symb(new_src) == PREDICATE_LESS) ||
		(DAG_symb(new_src) == PREDICATE_LEQ) ||
		(DAG_symb(new_src) == PREDICATE_GREATER) ||
		(DAG_symb(new_src) == PREDICATE_GREATEREQ) ||
		(DAG_symb(new_src) == CONNECTOR_EQUIV) ||
		(DAG_symb(new_src) == PREDICATE_EQ))
		tmp = chainable_elim_proof(new_src);
	dest.DAG = tmp.DAG;
	if (tmp.proof) {
		stack_merge(reasons, tmp.proof);
		stack_free(tmp.proof);
	}
	if (stack_is_empty(reasons))
		stack_free(reasons);
	else if (stack_size(reasons) > 1) {
		proof_trans = proof_transformation(ps_type_trans, src, dest.DAG, reasons);
		stack_reset(reasons);
		stack_push(reasons, proof_trans);
	}
	dest.proof = reasons;
	return dest;
}
