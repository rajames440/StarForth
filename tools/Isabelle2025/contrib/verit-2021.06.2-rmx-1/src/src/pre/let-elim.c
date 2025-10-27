/*
  --------------------------------------------------------------
  let operator removing in terms and formulas
  --------------------------------------------------------------
*/

#include "pre/let-elim.h"

#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/DAG-symb.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"
#include "utils/stack.h"

#include <assert.h>
#include <stdlib.h>

/*
  --------------------------------------------------------------
  Context recursion - let elimination
  --------------------------------------------------------------
*/

void
let_elim_init(void)
{
	stack_INIT(context.DAGs);
}

void
let_elim_push(TDAG src, unsigned* pos)
{
	unsigned i;
	TDAG *PDAG, tmp;
	if (quantifier(DAG_symb(src))) {
		for (i = 0; i < DAG_arity(src) - 1u; ++i) {
			/* Account for shadowed variable */
			stack_push(context.DAGs, DAG_arg(src, i));
			stack_push(context.DAGs, DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
			/* Variable already bound, rename it */
			if (DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]) {
				tmp = DAG_new_nullary(DAG_symb_variable(DAG_sort(DAG_arg(src, i))));
				DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_dup(tmp);
				continue;
			}
			/* Mark that variables is bound */
			DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_arg(src, i);
		}
		return;
	}
	if (DAG_symb(src) != LET) return;
	MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
	/* The values assigned */
	for (i = 1; i < DAG_arity(src); i += 2)
		PDAG[i] = context_tree_rec(DAG_arg(src, i));
	/* The variables */
	for (i = 0; i < DAG_arity(src) - 1u; i += 2) {
		/* Necessary because shadowing is allowed */
		stack_push(context.DAGs, DAG_arg(src, i));
		stack_push(context.DAGs, DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
		/* Set new mapping */
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = PDAG[i + 1];
	}
	free(PDAG);
	/* Ignore vars declaration */
	*pos = DAG_arity(src) - 1;
}

void
let_elim_pop(TDAG src, unsigned pos)
{
	unsigned i, offset;
	TDAG var;
	/* Remove whatever mappings and markings introduced and resetting */
	if (quantifier(DAG_symb(src))) {
		offset = stack_size(context.DAGs) - 2 * (DAG_arity(src) - 1u);
		for (i = 0; i < 2 * (DAG_arity(src) - 1u); i += 2) {
			var = stack_get(context.DAGs, i + offset);
			/* Variable was renamed */
			if (DAG_symb_DAG[DAG_symb(var)] && DAG_symb_DAG[DAG_symb(var)] != var)
				DAG_free(DAG_symb_DAG[DAG_symb(var)]);
			DAG_symb_DAG[DAG_symb(var)] = stack_get(context.DAGs, i + offset + 1);
		}
		stack_dec_n(context.DAGs, 2 * (DAG_arity(src) - 1u));
		return;
	}
	if (DAG_symb(src) != LET) return;
	/* Removing mapping from last substitution and resetting */
	offset = stack_size(context.DAGs) - (DAG_arity(src) - 1u);
	for (i = 0; i < DAG_arity(src) - 1u; i += 2) {
		DAG_free(DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i + offset))]);
		DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i + offset))] =
			stack_get(context.DAGs, i + offset + 1);
	}
	stack_dec_n(context.DAGs, DAG_arity(src) - 1u);
}

TDAG
let_elim_reduce(TDAG src)
{
	TDAG dest;
	/* renames */
	if (!DAG_arity(src) && (DAG_symb_DAG[DAG_symb(src)])) {
		dest = DAG_dup(DAG_symb_DAG[DAG_symb(src)]);
		DAG_free(src);
		return dest;
	}
	/* Let is build by construction */
	assert(DAG_symb(src) != LET);
	return src;
}

void
let_elim_init_proof(void)
{
	stack_INIT(context.DAGs);
}

void
let_elim_push_proof(TDAG src, unsigned* pos)
{
	unsigned i;
	TDAG *PDAG, tmp;
	Tstack_DAG DAGs, renaming_vars;
	TDAG_proof dest;
	Tstack_proof let_terms_proofs = NULL;
	if (quantifier(DAG_symb(src))) {
		stack_INIT(DAGs);
		stack_INIT(renaming_vars);
		for (i = 0; i < DAG_arity(src) - 1u; ++i) {
			/* Account for shadowed variable */
			stack_push(context.DAGs, DAG_arg(src, i));
			stack_push(context.DAGs, DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
			/* Variable already bound, rename it */
			if (DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]) {
				tmp = DAG_new_nullary(DAG_symb_variable(DAG_sort(DAG_arg(src, i))));
				DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_dup(tmp);
				stack_push(renaming_vars, DAG_arg(src, i));
				stack_push(renaming_vars, tmp);
				continue;
			}
			/* Mark that variables is bound */
			DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_arg(src, i);
			stack_push(DAGs, DAG_arg(src, i));
		}
		/* Number of bound variables is stored in end of list of vars/subs */
		stack_push(renaming_vars, stack_size(DAGs));
		stack_merge(DAGs, renaming_vars);
		stack_free(renaming_vars);
		/* Start transformation proof */
		proof_subproof_begin_context(ps_type_bind, DAGs, NULL);
		stack_free(DAGs);
		return;
	}
	if (DAG_symb(src) != LET) return;
	/* Store proofs of terms transformations in mapping declaration */
	stack_INIT(let_terms_proofs);
	/* Get the variable values */
	MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
	for (i = 1; i < DAG_arity(src); i += 2) {
		dest = context_tree_rec_proof(DAG_arg(src, i));
		PDAG[i] = dest.DAG;
		if (dest.proof) {
			stack_merge(let_terms_proofs, dest.proof);
			stack_free(dest.proof);
		}
	}
	/* Putting mapping in global context for later DAG_free */
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(src) - 1u; i += 2) {
		/* Necessary because shadowing is allowed */
		stack_push(context.DAGs, DAG_arg(src, i));
		stack_push(context.DAGs, DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
		/* Set new mapping */
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = PDAG[i + 1];
		/* Collect for proof */
		stack_push(DAGs, DAG_arg(src, i));
		stack_push(DAGs, PDAG[i + 1]);
	}
	free(PDAG);
	/* Start transformation proof, push context */
	stack_push(DAGs, 0);
	proof_subproof_begin_context(ps_type_let_elim, DAGs, let_terms_proofs);
	stack_free(DAGs);
	stack_free(let_terms_proofs);
	/* Ignore declarations */
	*pos = DAG_arity(src) - 1;
}

void
let_elim_pop_proof(TDAG src, unsigned pos)
{
	unsigned i, offset;
	TDAG var;
	if (quantifier(DAG_symb(src))) {
		offset = stack_size(context.DAGs) - 2 * (DAG_arity(src) - 1u);
		for (i = 0; i < 2 * (DAG_arity(src) - 1u); i += 2) {
			var = stack_get(context.DAGs, i + offset);
			/* Variable was renamed */
			if (DAG_symb_DAG[DAG_symb(var)] && DAG_symb_DAG[DAG_symb(var)] != var)
				DAG_free(DAG_symb_DAG[DAG_symb(var)]);
			DAG_symb_DAG[DAG_symb(var)] = stack_get(context.DAGs, i + offset + 1);
		}
		stack_dec_n(context.DAGs, 2 * (DAG_arity(src) - 1u));
		return;
	}
	if (DAG_symb(src) != LET) return;
	/* Removing mapping from last substitution and resetting */
	offset = stack_size(context.DAGs) - (DAG_arity(src) - 1u);
	for (i = 0; i < DAG_arity(src) - 1u; i += 2) {
		DAG_free(DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i + offset))]);
		DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i + offset))] =
			stack_get(context.DAGs, i + offset + 1);
	}
	stack_dec_n(context.DAGs, DAG_arity(src) - 1u);
}

Tproof
let_elim_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
{
	if (new_src == src) {
		if (quantifier(DAG_symb(new_src)) || DAG_symb(new_src) == LET)
			proof_subproof_remove();
		return 0;
	}
	if (quantifier(DAG_symb(src)))
		return proof_subproof_end_transformation(src, new_src);
	if (DAG_symb(src) == LET)
		return proof_subproof_end_transformation(src, new_src);
	assert(!stack_is_empty(reasons));
	/* Build proof from src to new_src */
	return proof_transformation(ps_type_cong, src, new_src, reasons);
}

TDAG_proof
let_elim_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	TDAG_proof dest;
	dest.DAG = new_src;
	dest.proof = NULL;
	/* renames */
	if (
		!DAG_arity(new_src) &&
		(DAG_symb_DAG[DAG_symb(new_src)] &&
		 DAG_symb_DAG[DAG_symb(new_src)] != new_src)) {
		assert(!proof_build);
		dest.DAG = DAG_dup(DAG_symb_DAG[DAG_symb(new_src)]);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof, proof_transformation(ps_type_refl, new_src, dest.DAG, NULL));
		DAG_free(new_src);
		return dest;
	}
	if (proof_build) {
		stack_INIT(dest.proof);
		stack_push(dest.proof, proof_build);
	}
	return dest;
}
