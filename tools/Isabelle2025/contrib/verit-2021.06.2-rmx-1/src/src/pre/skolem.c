#include "pre/skolem.h"

#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/context-recursion-proof.h"
#include "symbolic/context-recursion.h"
#include "symbolic/polarities.h"
#include "symbolic/qnt-utils.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/options.h"

/* #define DEBUG_SKOLEM */

/**
   \addtogroup arguments_user

   - --enable-deep-skolem

   Enables application of deep Skolemization: Skolemizes
   within essentially universal quantifiers */
static bool enable_deep_skolem;

/*
  --------------------------------------------------------------
  Deep Skolemization
  --------------------------------------------------------------
*/

/* TODO: Handle shadowing: if Ax...Ey...Ax must rename second Ax to some new
   var. */

/**
   \brief builds a skolem term of the right sort
   \param sort term's expected sort
   \return a skolem function term applied to all weak variables in context */
static TDAG
skolem_term(Tsort sort)
{
	unsigned i;
	Tsort* sub;
	TDAG dest, *PDAG;
#ifdef DEBUG
	/* Assumes no shadowing */
	for (i = 0; i < stack_size(context.DAGs); ++i)
		assert(!DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i))]);
#endif
	if (stack_is_empty(context.DAGs))
		return DAG_new_nullary(DAG_symb_skolem(sort));
	MY_MALLOC(PDAG, stack_size(context.DAGs) * sizeof(TDAG));
	MY_MALLOC(sub, (stack_size(context.DAGs) + 1) * sizeof(Tsort));
	for (i = 0; i < stack_size(context.DAGs); ++i) {
		PDAG[i] = stack_get(context.DAGs, i);
		sub[i] = DAG_sort(PDAG[i]);
	}
	sub[i] = sort;
	dest = DAG_new(
		DAG_symb_skolem(DAG_sort_new(NULL, stack_size(context.DAGs) + 1, sub)),
		stack_size(context.DAGs), PDAG);
	return dest;
}

static bool
sko_deep_cont(TDAG DAG)
{
	/* Stop if hits a QF subformula outside quantifiers */
	if (DAG_quant(DAG) || context.binders) return true;
	return false;
}

static void
sko_deep_init(void)
{
	stack_INIT(context.pols);
	stack_push(context.pols, POL_POS);
	context.binders = 0;
	stack_INIT(context.DAGs);
}

static void
sko_deep_push(TDAG src, unsigned* pos)
{
	unsigned i;
	TDAG skolem;
	Tpol pol = ctx_pol;
	/* Update polarity */
	{
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
	if (!quantifier(DAG_symb(src))) return;
	/* Saving to know whether under quantifier */
	++context.binders;
	/* Collect variables */
	if (!QUANTIFIED_STRONG(src, ctx_pol)) {
		for (i = 0; i < DAG_arity(src) - 1u; ++i)
			stack_push(context.DAGs, DAG_arg(src, i));
		return;
	}
	if (!QUANTIFIED_STRONG(src, ctx_pol)) return;
	/* Ignore var declaration */
	*pos = DAG_arity(src) - 1u;
	/* Attach skolem terms to variables */
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		/* Assumes no shadowing */
		assert(!DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
		skolem = DAG_dup(skolem_term(DAG_sort(DAG_arg(src, i))));
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = skolem;
	}
}

static void
sko_deep_pop(TDAG src, unsigned pos)
{
	unsigned i;
	/* Update polarity */
	if (
		DAG_symb(src) == CONNECTOR_NOT ||
		(DAG_symb(src) == CONNECTOR_IMPLIES && pos == 0) ||
		((DAG_symb(src) == CONNECTOR_ITE && pos == 0) ||
		 DAG_symb(src) == CONNECTOR_EQUIV || DAG_symb(src) == CONNECTOR_XOR ||
		 (!boolean_connector(DAG_symb(src)) && !binder(DAG_symb(src))))) {
		stack_dec(context.pols);
		return;
	}

	assert(!quantifier(DAG_symb(src)) || context.binders);
	if (!quantifier(DAG_symb(src))) return;
	/* Mark out of mapping */
	--context.binders;
	/* Release context variables */
	if (!QUANTIFIED_STRONG(src, ctx_pol)) {
		assert(stack_size(context.DAGs) >= (DAG_arity(src) - 1u));
		stack_dec_n(context.DAGs, DAG_arity(src) - 1u);
		return;
	}
	/* Detach variable skolem terms and free them */
	if (QUANTIFIED_STRONG(src, ctx_pol))
		for (i = 0; i < DAG_arity(src) - 1u; ++i) {
			DAG_free(DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
			DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_NULL;
		}
}

/*
  --------------------------------------------------------------
  Shallow Skolemization
  --------------------------------------------------------------
*/

static bool
sko_shallow_cont(TDAG DAG)
{
	/* Stop if outside quantifiers and hits a not strong quant or QF formula */
	if (
		context.binders ||
		(DAG_quant(DAG) &&
		 (!quantifier(DAG_symb(DAG)) || QUANTIFIED_STRONG(DAG, ctx_pol))))
		return true;
	return false;
}

static void
sko_shallow_init(void)
{
	stack_INIT(context.pols);
	stack_push(context.pols, POL_POS);
	context.binders = 0;
	stack_INIT(context.DAGs);
}

static void
sko_shallow_push(TDAG src, unsigned* pos)
{
	unsigned i;
	TDAG skolem;
	Tpol pol = ctx_pol;
	/* Update polarity */
	{
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
	if (!quantifier(DAG_symb(src))) return;
	/* Saving to know whether mapping */
	++context.binders;
	/* Collect variables to know that in no-longer shallow setting */
	if (!QUANTIFIED_STRONG(src, ctx_pol)) {
		for (i = 0; i < DAG_arity(src) - 1u; ++i)
			stack_push(context.DAGs, DAG_arg(src, i));
		return;
	}
	/* Don't skolemize shallow strong quantifiers */
	if (!stack_is_empty(context.DAGs)) return;
	/* Ignore var declaration */
	*pos = DAG_arity(src) - 1u;
	/* Attach skolem terms to variables */
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		assert(!DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
		skolem =
			DAG_dup(DAG_new_nullary(DAG_symb_skolem(DAG_sort(DAG_arg(src, i)))));
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = skolem;
	}
}

static void
sko_shallow_pop(TDAG src, unsigned pos)
{
	unsigned i;
	/* Update polarity */
	if (
		DAG_symb(src) == CONNECTOR_NOT ||
		(DAG_symb(src) == CONNECTOR_IMPLIES && pos == 0) ||
		((DAG_symb(src) == CONNECTOR_ITE && pos == 0) ||
		 DAG_symb(src) == CONNECTOR_EQUIV || DAG_symb(src) == CONNECTOR_XOR ||
		 (!boolean_connector(DAG_symb(src)) && !binder(DAG_symb(src))))) {
		stack_dec(context.pols);
		return;
	}
	assert(!quantifier(DAG_symb(src)) || context.binders);
	if (!quantifier(DAG_symb(src))) return;
	/* Mark out of mapping */
	--context.binders;
	/* Detach variable skolem terms and free them */
	if (QUANTIFIED_STRONG(src, ctx_pol)) {
		/* Only free strong skolemized, i.e. shallow ones */
		if (stack_is_empty(context.DAGs))
			for (i = 0; i < DAG_arity(src) - 1u; ++i) {
				DAG_free(DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
				DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_NULL;
			}
		return;
	}
	/* Release context variables */
	if (!QUANTIFIED_STRONG(src, ctx_pol)) {
		assert(stack_size(context.DAGs) >= (DAG_arity(src) - 1u));
		stack_dec_n(context.DAGs, DAG_arity(src) - 1u);
		return;
	}
}

static TDAG
sko_shallow_reduce(TDAG src)
{
	TDAG dest;
	/* Replace */
	if (!DAG_arity(src) && DAG_symb_DAG[DAG_symb(src)]) {
		dest = DAG_dup(DAG_symb_DAG[DAG_symb(src)]);
		DAG_free(src);
		return dest;
	}
	return src;
}

TDAG
skolemize(TDAG src)
{
	return enable_deep_skolem
		? context_structural_recursion(
				src, sko_deep_init, sko_deep_push, sko_deep_pop, sko_shallow_reduce,
				sko_deep_cont)
		: context_structural_recursion(
				src, sko_shallow_init, sko_shallow_push, sko_shallow_pop,
				sko_shallow_reduce, sko_shallow_cont);
}

/*
  --------------------------------------------------------------
  Proof production
  --------------------------------------------------------------
*/

static void
sko_shallow_push_proof(TDAG src, unsigned* pos)
{
	unsigned i;
	Tpol pol;
	TDAG skolem;
	Tstack_DAG DAGs;
	/* Update polarity */
	{
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
	if (!quantifier(DAG_symb(src))) return;
	/* Saving to know whether mapping */
	++context.binders;
	/* Bind free variables */
	if (!QUANTIFIED_STRONG(src, ctx_pol) || !stack_is_empty(context.DAGs)) {
		stack_INIT(DAGs);
		for (i = 0; i < DAG_arity(src) - 1u; ++i) stack_push(DAGs, DAG_arg(src, i));
		stack_push(DAGs, DAG_arity(src) - 1u);
		/* Start transformation proof */
		proof_subproof_begin_context(ps_type_bind, DAGs, NULL);
		stack_free(DAGs);
		/* Collect variables to know that in no-longer shallow setting */
		if (!QUANTIFIED_STRONG(src, ctx_pol))
			for (i = 0; i < DAG_arity(src) - 1u; ++i)
				stack_push(context.DAGs, DAG_arg(src, i));
		return;
	}
	assert(QUANTIFIED_STRONG(src, ctx_pol));
	/* Ignore var declaration */
	*pos = DAG_arity(src) - 1u;
	/* Attach skolem terms to variables */
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		/* Assumes no shadowing */
		assert(!DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
		skolem =
			DAG_dup(DAG_new_nullary(DAG_symb_skolem(DAG_sort(DAG_arg(src, i)))));
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = skolem;
		/* for proof */
		stack_push(DAGs, DAG_arg(src, i));
		stack_push(DAGs, skolem);
	}
	stack_push(DAGs, 0);
	/* Start transformation proof, push context
   *
   * The skolems are stored in the proof module to be later associated with
   * their respective choice functions.
   */
	proof_subproof_begin_sko(
		DAG_symb(src) == QUANTIFIER_EXISTS ? ps_type_sko_ex : ps_type_sko_all, DAGs,
		NULL, src);
	stack_free(DAGs);
}

static void
sko_shallow_pop_proof(TDAG src, unsigned pos)
{
	unsigned i;
	/* Update polarity */
	if (
		DAG_symb(src) == CONNECTOR_NOT ||
		(DAG_symb(src) == CONNECTOR_IMPLIES && pos == 0) ||
		((DAG_symb(src) == CONNECTOR_ITE && pos == 0) ||
		 DAG_symb(src) == CONNECTOR_EQUIV || DAG_symb(src) == CONNECTOR_XOR ||
		 (!boolean_connector(DAG_symb(src)) && !binder(DAG_symb(src))))) {
		stack_dec(context.pols);
		return;
	}
	assert(!quantifier(DAG_symb(src)) || context.binders);
	if (!quantifier(DAG_symb(src))) return;
	/* Mark out of mapping */
	--context.binders;
	/* Detach variable skolem terms and free them */
	if (QUANTIFIED_STRONG(src, ctx_pol)) {
		/* Only free strong skolemized, i.e. shallow ones */
		if (stack_is_empty(context.DAGs))
			for (i = 0; i < DAG_arity(src) - 1u; ++i) {
				DAG_free(DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
				DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_NULL;
			}
		return;
	}
	/* Release context variables */
	if (!QUANTIFIED_STRONG(src, ctx_pol)) {
		assert(stack_size(context.DAGs) >= (DAG_arity(src) - 1u));
		stack_dec_n(context.DAGs, DAG_arity(src) - 1u);
		return;
	}
}

Tproof
sko_shallow_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
{
	if (new_src == src) {
		/* Kill useless inv_pol and bind subproofs */
		if (quantifier(DAG_symb(src))) proof_subproof_remove();
		return 0;
	}
	/* Build subproof from src to new_src */
	if (quantifier(DAG_symb(src)))
		return proof_subproof_end_transformation(src, new_src);
	assert(!stack_is_empty(reasons));
	return proof_transformation(ps_type_cong, src, new_src, reasons);
}

static TDAG_proof
sko_shallow_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	TDAG_proof dest;
	dest.DAG = new_src;
	dest.proof = NULL;
	/* renames */
	if (!DAG_arity(new_src) && DAG_symb_DAG[DAG_symb(new_src)]) {
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

TDAG
skolemize_proof(TDAG src, Tproof* Pproof)
{
	if (enable_deep_skolem) {
		TDAG dest = context_structural_recursion(
			src, sko_deep_init, sko_deep_push, sko_deep_pop, sko_shallow_reduce,
			sko_deep_cont);
		if (src != dest) *Pproof = proof_deep_skolemize(*Pproof, dest);
		return dest;
	}
	return context_structural_recursion_proof(
		src, Pproof, sko_shallow_init, sko_shallow_push_proof,
		sko_shallow_pop_proof, sko_shallow_replacement, sko_shallow_reduce_proof,
		sko_shallow_cont);
}

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

void
skolemize_init(void)
{
	enable_deep_skolem = false;
	options_new(
		0, "enable-deep-skolem", "Enable deep Skolemization [unstable]",
		&enable_deep_skolem);
}

void
skolemize_done(void)
{}
