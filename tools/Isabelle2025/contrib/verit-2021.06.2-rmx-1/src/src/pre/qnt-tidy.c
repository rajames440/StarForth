/*
  --------------------------------------------------------------
  Module for polishing quantified formulas
  --------------------------------------------------------------
*/

#include "pre/qnt-tidy.h"

#include "proof/proof.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-subst.h"
#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "symbolic/context-handling.h"
#include "symbolic/context-recursion.h"
#include "symbolic/polarities.h"
#include "symbolic/qnt-utils.h"
#include "symbolic/recursion.h"
#include "utils/general.h"
#include "utils/types.h"
#include "veriT-config.h"

#include <limits.h>

/*
  --------------------------------------------------------------
  Renaming variables
  --------------------------------------------------------------
*/

Tstack_DAGstack sort_var_new = NULL;
Tstack_unsigned sort_var_new_c = NULL;

#define sort_var_new_get(sort, i) stack_get(stack_get(sort_var_new, sort), i)
#define sort_var_new_n(sort) stack_size(stack_get(sort_var_new, sort))
#define sort_var_new_push(sort, D) stack_push(stack_get(sort_var_new, sort), D);

/**
   \brief get a fresh variable for sort */
static TDAG
DAG_symb_var_new(Tsort sort)
{
	Tsymb symb;
	TDAG DAG;
	assert(sort < stack_size(sort_var_new_c));
	stack_set(sort_var_new_c, sort, stack_get(sort_var_new_c, sort) + 1);
	if (stack_get(sort_var_new_c, sort) > sort_var_new_n(sort)) {
		symb = DAG_symb_variable(sort);
		DAG = DAG_dup(DAG_new(symb, 0, NULL));
		sort_var_new_push(sort, DAG);
	}
	assert(stack_get(sort_var_new_c, sort) <= sort_var_new_n(sort));
	return sort_var_new_get(sort, stack_get(sort_var_new_c, sort) - 1);
}

/**
   \brief allow to reuse a variable later */
static void
DAG_symb_var_delete(Tsort sort)
{
	assert(
		sort < stack_size(sort_var_new_c) && stack_get(sort_var_new_c, sort) > 0);
	stack_set(sort_var_new_c, sort, stack_get(sort_var_new_c, sort) - 1);
}

void
DAG_symb_var_resize(unsigned n)
{
	unsigned i, old;
	old = stack_size(sort_var_new);
	for (i = n; i < old; ++i) {
		stack_apply(stack_get(sort_var_new, i), DAG_free);
		stack_free(stack_get(sort_var_new, i));
	}
	stack_resize(sort_var_new, n);
	stack_resize(sort_var_new_c, n);
	for (i = old; i < n; i++) {
		stack_set(sort_var_new_c, i, 0);
		stack_INIT(stack_get(sort_var_new, i));
	}
}

/*
  --------------------------------------------------------------
  Check free and captured variables
  --------------------------------------------------------------
*/

#ifdef DEBUG

/* TODO: have a function to check also no lambdas and lets and stuff */
static void
check_free_shadowed_vars_rec(TDAG src)
{
	unsigned i;
	if (DAG_tmp_bool[src]) return;
	DAG_tmp_bool[src] = true;
	if (DAG_symb_type(DAG_symb(src)) & SYMB_VARIABLE) {
		if (!DAG_tmp_bool[src]) my_error("free variable found\n");
		return;
	}
	if (!quantifier(DAG_symb(src))) {
		for (i = 0; i < DAG_arity(src); ++i)
			check_free_shadowed_vars_rec(DAG_arg(src, i));
		return;
	}
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		/* TODO: this is not an error, but a directive for veriT. So output
         something else */
		if (DAG_tmp_bool[DAG_arg(src, i)])
			my_error("quantified variable is reused\n");
		DAG_tmp_bool[DAG_arg(src, i)] = true;
	}
	check_free_shadowed_vars_rec(DAG_arg_last(src));
	/* Reset variables usage */
	for (i = 0; i < DAG_arity(src) - 1u; ++i)
		DAG_tmp_bool[DAG_arg(src, i)] = false;
}

void
check_free_shadowed_vars(TDAG src)
{
	DAG_tmp_reserve();
	check_free_shadowed_vars_rec(src);
	DAG_tmp_reset_bool(src);
	DAG_tmp_release();
}

#endif

/*
  --------------------------------------------------------------
  Join same quantifiers
  --------------------------------------------------------------
*/

void
qnt_join_init(void)
{
	context.binders = 0;
}

void
qnt_join_push(TDAG src, unsigned* pop)
{
	if (!quantifier(DAG_symb(src))) return;
	/* Mark going under binder */
	++context.binders;
}

void
qnt_join_pop(TDAG src, unsigned pop)
{
	if (!quantifier(DAG_symb(src))) return;
	/* Mark out of binder */
	--context.binders;
}

/* forall x forall y -> forall x y
   exists x exists y -> exists x y
   forall x y forall y z -> forall x y z */
TDAG
qnt_join_reduce(TDAG src)
{
	unsigned i;
	TDAG dest, next_qnt;
	Tstack_DAG DAGs;
	/* No joining */
	if (
		!quantifier(DAG_symb(src)) || DAG_symb(src) != DAG_symb(DAG_arg_last(src)))
		return src;
	next_qnt = DAG_arg_last(src);
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(src) - 1; ++i) {
		stack_push(DAGs, DAG_arg(src, i));
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_arg(src, i);
	}
	/* Only get fresh vars from next quantifier */
	for (i = 0; i < DAG_arity(next_qnt) - 1; ++i)
		if (!DAG_symb_DAG[DAG_symb(DAG_arg(next_qnt, i))])
			stack_push(DAGs, DAG_arg(next_qnt, i));
	stack_push(DAGs, DAG_arg_last(next_qnt));
	dest = DAG_dup(DAG_new_stack(DAG_symb(src), DAGs));
	for (i = 0; i < DAG_arity(src) - 1; ++i)
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_NULL;
	DAG_append_triggers(src, dest);
	DAG_append_triggers(next_qnt, dest);
	stack_free(DAGs);
	DAG_free(src);
	return dest;
}

void
qnt_join_push_proof(TDAG src, unsigned* pop)
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
	/* Mark going under binder */
	++context.binders;
}

Tproof
qnt_join_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
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

/* forall x forall y -> forall x y
   exists x exists y -> exists x y
   forall x y forall y z -> forall x y z */
TDAG_proof
qnt_join_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	unsigned i;
	TDAG next_qnt;
	TDAG_proof dest;
	Tstack_DAG DAGs;
	Tstack_proof reasons;
	Tproof proof_trans;
	dest.DAG = new_src;
	/* No joining */
	if (
		!quantifier(DAG_symb(new_src)) ||
		DAG_symb(new_src) != DAG_symb(DAG_arg_last(new_src))) {
		if (proof_build) {
			stack_INIT(dest.proof);
			stack_push(dest.proof, proof_build);
		} else
			dest.proof = NULL;
		return dest;
	}
	stack_INIT(reasons);
	if (proof_build) stack_push(reasons, proof_build);
	next_qnt = DAG_arg_last(new_src);
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(new_src) - 1; ++i) {
		stack_push(DAGs, DAG_arg(new_src, i));
		DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))] = DAG_arg(new_src, i);
	}
	/* Only get fresh vars from next quantifier */
	for (i = 0; i < DAG_arity(next_qnt) - 1; ++i)
		if (!DAG_symb_DAG[DAG_symb(DAG_arg(next_qnt, i))])
			stack_push(DAGs, DAG_arg(next_qnt, i));
	stack_push(DAGs, DAG_arg_last(next_qnt));
	dest.DAG = DAG_dup(DAG_new_stack(DAG_symb(new_src), DAGs));
	stack_push(
		reasons, proof_transformation(ps_type_qnt_join, new_src, dest.DAG, NULL));
	if (stack_size(reasons) > 1) {
		proof_trans = proof_transformation(ps_type_trans, src, dest.DAG, reasons);
		stack_reset(reasons);
		stack_push(reasons, proof_trans);
	}
	dest.proof = reasons;
	for (i = 0; i < DAG_arity(new_src) - 1; ++i)
		DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))] = DAG_NULL;
	DAG_append_triggers(new_src, dest.DAG);
	DAG_append_triggers(next_qnt, dest.DAG);
	stack_free(DAGs);
	DAG_free(new_src);
	return dest;
}

/*
  --------------------------------------------------------------
  Remove unused variables
  --------------------------------------------------------------
*/

void
qnt_rm_unused_init(void)
{
	context.binders = 0;
}

void
qnt_rm_unused_push(TDAG src, unsigned* pos)
{
	if (!quantifier(DAG_symb(src))) return;
	/* Mark going under binder */
	++context.binders;
}

void
qnt_rm_unused_pop(TDAG src, unsigned pos)
{
	if (!quantifier(DAG_symb(src))) return;
	/* Mark out of binder */
	--context.binders;
}

TDAG
qnt_rm_unused_reduce(TDAG src)
{
	unsigned i;
	TDAG dest;
	Tstack_DAG DAGs;
	/* Mark occurrence */
	if (DAG_symb_type(DAG_symb(src)) & SYMB_VARIABLE) {
		DAG_symb_DAG[DAG_symb(src)] = DAG_TRUE;
		return src;
	}
	/* Ignore whatever is not under quantifiers that has no quantifiers */
	if (!quantifier(DAG_symb(src))) return src;
	/* Retrieve variables used, resets DAG_symb_DAG */
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		if (DAG_symb_DAG[DAG_symb(DAG_arg(src, i))])
			stack_push(DAGs, DAG_arg(src, i));
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_NULL;
	}
	if (stack_size(DAGs) == DAG_arity(src) - 1)
		dest = src;
	else {
		/* PF if no variable is used, the quantifier is useless */
		if (stack_is_empty(DAGs))
			dest = DAG_dup(DAG_arg_last(src));
		else {
			stack_push(DAGs, DAG_arg_last(src));
			dest = DAG_dup(DAG_new_stack(DAG_symb(src), DAGs));
			DAG_append_triggers(src, dest);
			/* TODO: assert at some point that all variables in triggers are
             among those of new quantifier */
		}
		DAG_free(src);
	}
	stack_free(DAGs);
	return dest;
}

void
qnt_rm_unused_push_proof(TDAG src, unsigned* pos)
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
	/* Mark going under binder */
	++context.binders;
}

Tproof
qnt_rm_unused_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
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
qnt_rm_unused_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	unsigned i;
	TDAG_proof dest;
	Tstack_DAG DAGs;
	Tstack_proof reasons;
	Tproof proof_trans;
	dest.DAG = new_src;
	dest.proof = NULL;
	/* Mark occurrence */
	if (DAG_symb_type(DAG_symb(new_src)) & SYMB_VARIABLE) {
		DAG_symb_DAG[DAG_symb(new_src)] = DAG_TRUE;
		return dest;
	}
	/* Ignore whatever is not under quantifiers that has no quantifiers */
	if (!quantifier(DAG_symb(new_src))) {
		if (proof_build) {
			stack_INIT(dest.proof);
			stack_push(dest.proof, proof_build);
		}
		return dest;
	}
	stack_INIT(reasons);
	if (proof_build) stack_push(reasons, proof_build);
	/* Retrieve variables used, resets DAG_symb_DAG */
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(new_src) - 1u; ++i) {
		if (DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))])
			stack_push(DAGs, DAG_arg(new_src, i));
		DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))] = DAG_NULL;
	}
	if (stack_size(DAGs) == DAG_arity(new_src) - 1)
		dest.DAG = new_src;
	else {
		/* PF if no variable is used, the quantifier is useless */
		if (stack_is_empty(DAGs))
			dest.DAG = DAG_dup(DAG_arg_last(new_src));
		else {
			stack_push(DAGs, DAG_arg_last(new_src));
			dest.DAG = DAG_dup(DAG_new_stack(DAG_symb(new_src), DAGs));
			DAG_append_triggers(new_src, dest.DAG);
			/* TODO: assert at some point that all variables in triggers are
             among those of new quantifier */
		}
		stack_push(
			reasons,
			proof_transformation(ps_type_qnt_rm_unused, new_src, dest.DAG, NULL));
		DAG_free(new_src);
	}
	if (stack_is_empty(reasons))
		stack_free(reasons);
	else if (stack_size(reasons) > 1) {
		proof_trans = proof_transformation(ps_type_trans, src, dest.DAG, reasons);
		stack_reset(reasons);
		stack_push(reasons, proof_trans);
	}
	dest.proof = reasons;
	stack_free(DAGs);
	return dest;
}

/*
  --------------------------------------------------------------
  Canonize
  --------------------------------------------------------------
*/

void
qnt_canon_rename_init(void)
{
	stack_INIT(context.DAGs);
	context.binders = 0;
}

void
qnt_canon_rename_push(TDAG src, unsigned* pos)
{
	unsigned i;
	TDAG tmp;
	if (!quantifier(DAG_symb(src))) return;
	/* Mark going under binder */
	++context.binders;
	/* Setting renaming */
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		/* Assumes no shadowing */
		assert(!DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
		/* Set new mapping */
		tmp = DAG_symb_var_new(DAG_sort(DAG_arg(src, i)));
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_dup(tmp);
		/* Backup variables to reset */
		stack_push(context.DAGs, DAG_arg(src, i));
	}
}

void
qnt_canon_pop(TDAG src, unsigned pos)
{
	unsigned i, offset;
	if (!quantifier(DAG_symb(src))) return;
	/* Reset binding information */
	--context.binders;
	/* Removing mapping from last substitution */
	offset = stack_size(context.DAGs) - (DAG_arity(src) - 1u);
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		DAG_free(DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i + offset))]);
		DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i + offset))] = DAG_NULL;
		/* Allow renamings to be reused (the whole point of the canonization) */
		DAG_symb_var_delete(DAG_sort(stack_get(context.DAGs, i + offset)));
	}
	stack_dec_n(context.DAGs, (DAG_arity(src) - 1u));
}

TDAG
qnt_canon_rename_reduce(TDAG src)
{
	TDAG dest;
	/* renames */
	if (!DAG_arity(src) && DAG_symb_DAG[DAG_symb(src)]) {
		dest = DAG_dup(DAG_symb_DAG[DAG_symb(src)]);
		DAG_free(src);
		return dest;
	}
	return src;
}

void
qnt_canon_rename_push_proof(TDAG src, unsigned* pos)
{
	unsigned i;
	TDAG tmp;
	Tstack_DAG DAGs;
	if (!quantifier(DAG_symb(src))) return;
	/* Mark going under binder */
	++context.binders;
	/* Setting renaming */
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		/* Assumes no shadowing */
		assert(!DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
		/* Set new mapping */
		tmp = DAG_symb_var_new(DAG_sort(DAG_arg(src, i)));
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_dup(tmp);
		stack_push(DAGs, DAG_arg(src, i));
		stack_push(DAGs, tmp);
		/* Backup variables to reset */
		stack_push(context.DAGs, DAG_arg(src, i));
	}
	/* Mark that all variables are renamed */
	stack_push(DAGs, 0);
	/* Start transformation proof(s) */
	proof_subproof_begin_context(ps_type_bind, DAGs, NULL);
	stack_free(DAGs);
}

Tproof
qnt_canon_rename_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
{
	if (new_src == src) {
		if (quantifier(DAG_symb(new_src))) proof_subproof_remove();
		return 0;
	}
	if (quantifier(DAG_symb(src)))
		return proof_subproof_end_transformation(src, new_src);
	assert(!stack_is_empty(reasons));
	/* Build proof from src to new_src */
	return proof_transformation(ps_type_cong, src, new_src, reasons);
}

TDAG_proof
qnt_canon_rename_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	TDAG_proof dest;
	dest.DAG = new_src;
	dest.proof = NULL;
	/* renames */
	if (!DAG_arity(new_src) && DAG_symb_DAG[DAG_symb(new_src)]) {
		assert(!proof_build);
		dest.DAG = DAG_dup(DAG_symb_DAG[DAG_symb(new_src)]);
		/* TODO: it shouldn't even get inside this block if DAG_symb_DAG is not
         different from new_src */
		if (dest.DAG != new_src) {
			stack_INIT(dest.proof);
			stack_push(
				dest.proof,
				proof_transformation(ps_type_refl, new_src, dest.DAG, NULL));
		}
		DAG_free(new_src);
		return dest;
	}
	if (proof_build) {
		stack_INIT(dest.proof);
		stack_push(dest.proof, proof_build);
	}
	return dest;
}

/*
  --------------------------------------------------------------
  Quantifier normalization
  --------------------------------------------------------------
*/

void
qnt_rename_pop(TDAG src, unsigned pos)
{
	unsigned i, offset;
	if (!quantifier(DAG_symb(src))) return;
	/* Reset binding information */
	--context.binders;
	/* Removing mapping from last substitution */
	offset = stack_size(context.DAGs) - (DAG_arity(src) - 1u);
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		DAG_free(DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i + offset))]);
		DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i + offset))] = DAG_NULL;
	}
	stack_dec_n(context.DAGs, (DAG_arity(src) - 1u));
}

/*
  --------------------------------------------------------------
  quantifier simplifications
  --------------------------------------------------------------
*/

/* FORALL x . x != T OR P(x) --> P(T)
   EXISTS x . x = T AND P(x) --> P(T)
   if T only contains variables that have a larger scope that x */

static unsigned qnt_counter = 0;

#define id_set(D) (DAG_ctx[D])
#define var_id(D) (DAG_ctx[D]->DAG)
#define scope (context.DAGs)

/* TODO: Why does it not work to consider all the variables in the scope of
   quantifier? That's what scope_end would be used for, searching for a variable
   whose id is s.t. id != given_var_id && id < scope_end. Maybe because if that
   were the case than I'd need to make the replacements up to the fix point,
   applying DAG_symb_DAG quadratically? */
/**
   \brief checks whither all vars of term are up to the scope of the given
   variable and are not it
   \param DAG a formula or term
   \param given_var_id id of a given variable
   \param scope_end delimiter of given scope (open)
   \return true iff all variables in DAG have an id and id < given_var_id and id
   \remark No quantifier should occur in DAG (through ite term) */
static bool
qnt_almost_var_free(TDAG DAG, unsigned given_var_id, unsigned scope_end)
{
	unsigned i;
	if (quantifier(DAG_symb(DAG))) return false;
	/* all occurring variables must have had their id set */
	if (DAG_symb_type(DAG_symb(DAG)) & SYMB_VARIABLE)
		return (id_set(DAG) && var_id(DAG) < given_var_id);
	/* var_id(DAG) != given_var_id && var_id(DAG) < scope_end); */
	for (i = 0; i < DAG_arity(DAG); ++i)
		if (!qnt_almost_var_free(DAG_arg(DAG, i), given_var_id, scope_end))
			return false;
	return true;
}

/**
   \brief looks in DAG for an equality v = T where v is a variable of id >
   scope_start and T is a term containing no variable with id' != id and id >
   scope_end
   \param DAG a formula or term
   \param scope_start delimiter of interval (open)
   \param scope_end delimiter of interval (open)
   \param pol
   \remark sets found terms using DAG_symb_DAG on variables symbols */
static bool
qnt_lookup_eq(TDAG DAG, unsigned scope_start, unsigned scope_end, Tpol pol)
{
	unsigned i;
	bool res = false;
	while (quantifier(DAG_symb(DAG))) DAG = DAG_arg_last(DAG);
	if (DAG_symb(DAG) == PREDICATE_EQ && pol == POL_POS) {
		if (
			(DAG_symb_type(DAG_symb(DAG_arg0(DAG))) & SYMB_VARIABLE) &&
			id_set(DAG_arg0(DAG)) && var_id(DAG_arg0(DAG)) > scope_start &&
			qnt_almost_var_free(DAG_arg1(DAG), var_id(DAG_arg0(DAG)), scope_end) &&
			!DAG_symb_DAG[DAG_symb(DAG_arg0(DAG))]) {
			DAG_symb_DAG[DAG_symb(DAG_arg0(DAG))] = DAG_dup(DAG_arg1(DAG));
			return true;
		}
		if (
			(DAG_symb_type(DAG_symb(DAG_arg1(DAG))) & SYMB_VARIABLE) &&
			id_set(DAG_arg1(DAG)) && var_id(DAG_arg1(DAG)) > scope_start &&
			qnt_almost_var_free(DAG_arg0(DAG), var_id(DAG_arg1(DAG)), scope_end) &&
			!DAG_symb_DAG[DAG_symb(DAG_arg1(DAG))]) {
			DAG_symb_DAG[DAG_symb(DAG_arg1(DAG))] = DAG_dup(DAG_arg0(DAG));
			return true;
		}
		return false;
	}
	if (DAG_symb(DAG) == CONNECTOR_NOT)
		return qnt_lookup_eq(DAG_arg0(DAG), scope_start, scope_end, INV_POL(pol));
	if (
		(DAG_symb(DAG) == CONNECTOR_OR && pol == POL_NEG) ||
		(DAG_symb(DAG) == CONNECTOR_AND && pol == POL_POS)) {
		for (i = 0; i < DAG_arity(DAG); ++i)
			res = qnt_lookup_eq(DAG_arg(DAG, i), scope_start, scope_end, pol) || res;
		return res;
	}
	if (DAG_symb(DAG) == CONNECTOR_IMPLIES && pol == POL_NEG) {
		res = qnt_lookup_eq(DAG_arg0(DAG), scope_start, scope_end, POL_POS);
		return qnt_lookup_eq(DAG_arg1(DAG), scope_start, scope_end, POL_NEG) || res;
	}
	return false;
}

void
qnt_onepoint_init(void)
{
	stack_INIT(scope);
	context.binders = 0;
}

void
qnt_onepoint_push(TDAG src, unsigned* pos)
{
	unsigned i;
	if (!quantifier(DAG_symb(src))) return;
	/* Mark going under binder */
	++context.binders;
	/* Start of scope */
	stack_push(scope, qnt_counter);
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		MY_MALLOC(DAG_ctx[DAG_arg(src, i)], sizeof(TDAG));
		var_id(DAG_arg(src, i)) = ++qnt_counter;
	}
	/* End of scope */
	stack_push(scope, qnt_counter + 1);
}

void
qnt_onepoint_pop(TDAG src, unsigned pos)
{}

TDAG
qnt_onepoint_reduce(TDAG src)
{
	unsigned i, j;
	bool rewriting;
	TDAG dest, tmp;
	Tstack_DAG DAGs, trigger;
	Tstack_DAGstack *Ptriggers, triggers;
	if (!DAG_arity(src) && DAG_symb_DAG[DAG_symb(src)]) {
		dest = DAG_dup(DAG_symb_DAG[DAG_symb(src)]);
		DAG_free(src);
		return dest;
	}
	if (!quantifier(DAG_symb(src))) return src;
	/* If finds simplified variables, the fix point of mapping is a ground term */
	assert(stack_size(scope) >= 2);
	rewriting = qnt_lookup_eq(
		DAG_arg_last(src), stack_top_n(scope, 2), stack_top(scope),
		DAG_symb(src) == QUANTIFIER_EXISTS ? POL_POS : POL_NEG);
	stack_dec_n(scope, 2);
	if (rewriting) {
		/* Build simplified formula */
		stack_INIT(DAGs);
		for (i = 0; i < DAG_arity(src) - 1u; ++i)
			if (!DAG_symb_DAG[DAG_symb(DAG_arg(src, i))])
				stack_push(DAGs, DAG_dup(DAG_arg(src, i)));
			else {
				/* Necessary because need fix point of substitution */
				tmp = DAG_symb_DAG[DAG_symb(DAG_arg(src, i))];
				DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = context_tree_rec(tmp);
				DAG_free(tmp);
			}
		assert(stack_size(DAGs) < DAG_arity(src) - 1);
		/* Apply substitution on matrix */
		stack_push(DAGs, context_tree_rec(DAG_arg_last(src)));
		/* Quantifier eliminated */
		if (stack_size(DAGs) == 1)
			dest = DAG_dup(stack_top(DAGs));
		else {
			dest = DAG_dup(DAG_new_stack(DAG_symb(src), DAGs));
			/* renaming triggers */
			{
				Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
				if (Ptriggers) {
					stack_INIT(triggers);
					for (i = 0; i < stack_size(*Ptriggers); ++i) {
						trigger = stack_get(*Ptriggers, i);
						stack_inc(triggers);
						stack_INIT(stack_top(triggers));
						for (j = 0; j < stack_size(trigger); ++j)
							stack_push(
								stack_top(triggers), context_tree_rec(stack_get(trigger, j)));
					}
					DAG_prop_set(dest, DAG_PROP_TRIGGER, &triggers);
				}
			}
		}
		stack_apply(DAGs, DAG_free);
		stack_free(DAGs);
	} else
		dest = src;
	/* Reset bound information */
	--context.binders;
	/* Resets variables */
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		if (DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]) {
			DAG_free(DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
			DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_NULL;
		}
		free(DAG_ctx[DAG_arg(src, i)]);
		DAG_ctx[DAG_arg(src, i)] = NULL;
	}
	/* Cannot free src before because necessary for resetting */
	if (rewriting) DAG_free(src);
	return dest;
}

void
qnt_onepoint_push_proof(TDAG src, unsigned* pos)
{
	unsigned i;
	Tstack_DAG DAGs;
	if (!quantifier(DAG_symb(src))) return;
	/* Mark going under binder */
	++context.binders;
	/* Start of scope */
	stack_push(scope, qnt_counter);
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		MY_MALLOC(DAG_ctx[DAG_arg(src, i)], sizeof(TDAG));
		var_id(DAG_arg(src, i)) = ++qnt_counter;
		stack_push(DAGs, DAG_arg(src, i));
	}
	stack_push(DAGs, DAG_arity(src) - 1u);
	/* End of scope */
	stack_push(scope, qnt_counter + 1);
	/* Start transformation */
	proof_subproof_begin_context(ps_type_bind, DAGs, NULL);
	stack_free(DAGs);
}

Tproof
qnt_onepoint_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
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

/* TODO: fix this guy, it should behave as LET does? The search for
   simplifications should happen at the push or at the reduction? */
TDAG_proof
qnt_onepoint_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	unsigned i, j;
	bool rewriting;
	TDAG_proof dest;
	Tstack_proof rewriting_proofs = NULL;
	Tstack_DAG DAGs, trigger;
	Tstack_DAGstack *Ptriggers, triggers;
	dest.DAG = new_src;
	dest.proof = NULL;
	if (!DAG_arity(new_src) && DAG_symb_DAG[DAG_symb(new_src)]) {
		assert(!proof_build && DAG_symb_DAG[DAG_symb(new_src)] != new_src);
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
	if (!quantifier(DAG_symb(new_src))) return dest;
	/* If finds simplified variables, the fix point of mapping is a ground term */
	assert(stack_size(scope) >= 2);
	rewriting = qnt_lookup_eq(
		DAG_arg_last(new_src), stack_top_n(scope, 2), stack_top(scope),
		DAG_symb(new_src) == QUANTIFIER_EXISTS ? POL_POS : POL_NEG);
	stack_dec_n(scope, 2);
	/* Build simplified formula */
	if (rewriting) {
		Tstack_DAG context, subst;
		stack_INIT(context);
		stack_INIT(subst);
		stack_INIT(DAGs);
		assert(!rewriting_proofs);
		stack_INIT(rewriting_proofs);
		for (i = 0; i < DAG_arity(new_src) - 1u; ++i)
			if (!DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))]) {
				/* This is a variable that will not be replaced */
				stack_push(DAGs, DAG_dup(DAG_arg(new_src, i)));
				stack_push(context, DAG_arg(new_src, i));
			}
			else {
				/* Necessary because need fix point of substitution */
				stack_push(subst, DAG_arg(new_src, i));
				dest =
					context_tree_rec_proof(DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))]);
				/* Whether fix point different from mapping */
				if (dest.proof) {
					/* We currently skip those proofs since they can be wrong. For now
					   the reconstruction has to proof the rewrites itself. */
					/* See: https://gitlab.inria.fr/hschurr/verit-rmx/-/issues/8 */
					/* stack_merge(rewriting_proofs, dest.proof); */
					stack_free(dest.proof);
				}
				stack_push(subst, dest.DAG);
				DAG_free(DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))]);
				DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))] = dest.DAG;
			}
		assert(stack_size(DAGs) < DAG_arity(new_src) - 1);

		/* Start transformation proof, push context */
		unsigned nb_bound_vars = stack_size(context);
		stack_merge(context, subst);
		stack_push(context, nb_bound_vars);
		proof_subproof_begin_context(ps_type_onepoint, context, rewriting_proofs);
		stack_free(context);
		stack_free(subst);

		stack_free(rewriting_proofs);

		/* Apply substitution on matrix; TODO: OK to ignore its proof_id, right? */
		dest = context_tree_rec_proof(DAG_arg_last(new_src));
		stack_push(DAGs, dest.DAG);
		/* Quantifier eliminated */
		if (stack_size(DAGs) == 1)
			dest.DAG = DAG_dup(stack_top(DAGs));
		else {
			dest.DAG = DAG_dup(DAG_new_stack(DAG_symb(new_src), DAGs));
			/* renaming triggers */
			{
				Ptriggers = DAG_prop_get(new_src, DAG_PROP_TRIGGER);
				if (Ptriggers) {
					stack_INIT(triggers);
					for (i = 0; i < stack_size(*Ptriggers); ++i) {
						trigger = stack_get(*Ptriggers, i);
						stack_inc(triggers);
						stack_INIT(stack_top(triggers));
						for (j = 0; j < stack_size(trigger); ++j)
							stack_push(
								stack_top(triggers), DAG_tree_subst(stack_get(trigger, j)));
					}
					DAG_prop_set(dest.DAG, DAG_PROP_TRIGGER, &triggers);
				}
			}
		}

		/* TODO: kinda mysterious why I have to do this */
		if (dest.proof)
			stack_free(dest.proof);

		Tstack_proof reasons;
		stack_INIT(reasons);
		if (proof_build) stack_push(reasons, proof_build);
		stack_push(reasons, proof_subproof_end_transformation(new_src, dest.DAG));
		if (stack_size(reasons) > 1) {
			Tproof proof_trans = proof_transformation(ps_type_trans, src, dest.DAG, reasons);
			stack_reset(reasons);
			stack_push(reasons, proof_trans);
		}
		dest.proof = reasons;

		stack_apply(DAGs, DAG_free);
		stack_free(DAGs);
	} else
		dest.DAG = new_src;
	/* Reset bound information */
	--context.binders;
	/* Resets variables */
	for (i = 0; i < DAG_arity(new_src) - 1u; ++i) {
		if (DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))]) {
			DAG_free(DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))]);
			DAG_symb_DAG[DAG_symb(DAG_arg(new_src, i))] = DAG_NULL;
		}
		free(DAG_ctx[DAG_arg(new_src, i)]);
		DAG_ctx[DAG_arg(new_src, i)] = NULL;
	}
	if (dest.DAG != new_src) DAG_free(new_src);
	return dest;
}

/*
  --------------------------------------------------------------
  Public functions
  --------------------------------------------------------------
*/

void
qnt_tidy_init(void)
{
	qnt_counter = 0;
	stack_INIT(sort_var_new);
	stack_INIT(sort_var_new_c);
}

void
qnt_tidy_done(void)
{
	DAG_symb_var_resize(0);
	stack_free(sort_var_new);
	stack_free(sort_var_new_c);
}
