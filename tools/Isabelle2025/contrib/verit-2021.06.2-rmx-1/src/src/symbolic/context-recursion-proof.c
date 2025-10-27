#include "symbolic/context-recursion-proof.h"

#include "proof/proof.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/DAG-symb.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/context-recursion.h"
#include "utils/h-util.h"

/*
  --------------------------------------------------------------
  DAG_tmp handling for proof production
  --------------------------------------------------------------
*/

typedef union TDAG_p_ctx
{
	TDAG_proof DAG_p;
	TDAG_proof pol_DAG_p[4];
} * TDAG_p_ctx;

#define DAG_p_ctx ((TDAG_p_ctx*)DAG_tmp)

/**
   \brief retrieves DAG associated with <DAG, context>, if any
   \param DAG a formula or term
   \param context a recursion context
   \return DAG associated with the given pair, if any, otherwise DAG_NULL
   \remark assumes DAG_p_ctx[DAG] is allocated */
static TDAG
DAG_for_p_ctx(TDAG DAG)
{
	assert(DAG_p_ctx[DAG]);
	if (!context.pols) return DAG_p_ctx[DAG]->DAG_p.DAG;
	return DAG_p_ctx[DAG]->pol_DAG_p[ctx_pol].DAG;
}

/**
   \brief retrieves valuation of DAG in the current recursion
   \param DAG a formula or term
   \param context a recursion context
   \return the DAG and its proof associated with the given pair, if any,
   otherwise the DAG itself without a proof */
TDAG_proof
get_DAG_p_in_p_ctx(TDAG DAG)
{
	TDAG_proof dest;
	dest.DAG = DAG;
	dest.proof = NULL;
	if (!context.pols) return DAG_p_ctx[DAG] ? DAG_p_ctx[DAG]->DAG_p : dest;
	return (DAG_p_ctx[DAG] && DAG_p_ctx[DAG]->pol_DAG_p[ctx_pol].DAG)
		? DAG_p_ctx[DAG]->pol_DAG_p[ctx_pol]
		: dest;
}

/**
   \brief associates a DAG and a proof with a <DAG, context>
   \param DAG a formula or term
   \param new_DAG the DAG to be associated
   \param proof the proof to be associated
   \param context a recursion context
   \remark assumes this is always a fresh setting */
void
set_DAG_p_ctx(TDAG DAG, TDAG new_DAG, Tstack_proof proof)
{
	assert(!DAG_p_ctx[DAG] || !DAG_for_p_ctx(DAG));
	if (!context.pols) {
		if (!DAG_p_ctx[DAG]) MY_MALLOC(DAG_p_ctx[DAG], sizeof(TDAG_proof));
		DAG_p_ctx[DAG]->DAG_p.DAG = new_DAG;
		DAG_p_ctx[DAG]->DAG_p.proof = proof;
		return;
	}
	if (!DAG_p_ctx[DAG]) {
		MY_MALLOC(DAG_p_ctx[DAG], 4 * sizeof(TDAG_proof));
		DAG_p_ctx[DAG]->pol_DAG_p[POL_NONE].DAG = DAG_NULL;
		DAG_p_ctx[DAG]->pol_DAG_p[POL_NONE].proof = NULL;
		DAG_p_ctx[DAG]->pol_DAG_p[POL_POS].DAG = DAG_NULL;
		DAG_p_ctx[DAG]->pol_DAG_p[POL_POS].proof = NULL;
		DAG_p_ctx[DAG]->pol_DAG_p[POL_NEG].DAG = DAG_NULL;
		DAG_p_ctx[DAG]->pol_DAG_p[POL_NEG].proof = NULL;
		DAG_p_ctx[DAG]->pol_DAG_p[POL_BOTH].DAG = DAG_NULL;
		DAG_p_ctx[DAG]->pol_DAG_p[POL_BOTH].proof = NULL;
	}
	DAG_p_ctx[DAG]->pol_DAG_p[ctx_pol].DAG = new_DAG;
	DAG_p_ctx[DAG]->pol_DAG_p[ctx_pol].proof = proof;
}

/**
   \brief releases all associations of sub DAGs of given DAG
   \param DAG a formula or term */
void
DAG_tmp_free_DAG_p_ctx(TDAG DAG)
{
	unsigned i;
	if (!DAG_p_ctx[DAG]) return;
	if (!context.pols) {
		DAG_free(DAG_p_ctx[DAG]->DAG_p.DAG);
		if (DAG_p_ctx[DAG]->DAG_p.proof) stack_free(DAG_p_ctx[DAG]->DAG_p.proof);
	} else {
		assert(
			!DAG_p_ctx[DAG]->pol_DAG_p[POL_NONE].DAG &&
			!DAG_p_ctx[DAG]->pol_DAG_p[POL_NONE].proof);
		if (DAG_p_ctx[DAG]->pol_DAG_p[POL_POS].DAG) {
			DAG_free(DAG_p_ctx[DAG]->pol_DAG_p[POL_POS].DAG);
			if (DAG_p_ctx[DAG]->pol_DAG_p[POL_POS].proof)
				stack_free(DAG_p_ctx[DAG]->pol_DAG_p[POL_POS].proof);
		}
		if (DAG_p_ctx[DAG]->pol_DAG_p[POL_NEG].DAG) {
			DAG_free(DAG_p_ctx[DAG]->pol_DAG_p[POL_NEG].DAG);
			if (DAG_p_ctx[DAG]->pol_DAG_p[POL_NEG].proof)
				stack_free(DAG_p_ctx[DAG]->pol_DAG_p[POL_NEG].proof);
		}
		if (DAG_p_ctx[DAG]->pol_DAG_p[POL_BOTH].DAG) {
			DAG_free(DAG_p_ctx[DAG]->pol_DAG_p[POL_BOTH].DAG);
			if (DAG_p_ctx[DAG]->pol_DAG_p[POL_BOTH].proof)
				stack_free(DAG_p_ctx[DAG]->pol_DAG_p[POL_BOTH].proof);
		}
	}
	free(DAG_p_ctx[DAG]);
	DAG_p_ctx[DAG] = NULL;
	for (i = 0; i < DAG_arity(DAG); i++) DAG_tmp_free_DAG_p_ctx(DAG_arg(DAG, i));
}

/*
  --------------------------------------------------------------
  Context dependent structural recursion - proof producing
  --------------------------------------------------------------
*/

static void (*context_rec_push)(TDAG, unsigned*) = NULL;
static void (*context_rec_pop)(TDAG, unsigned) = NULL;
static Tproof (*context_rec_replace)(TDAG, TDAG, Tstack_proof) = NULL;
static TDAG_proof (*context_rec_reduce)(TDAG, TDAG, Tproof) = NULL;
static bool (*context_rec_cont)(TDAG) = NULL;

TDAG_proof
context_tree_rec_proof(TDAG src)
{
	unsigned i, j, pos;
	TDAG_proof dest, tmp;
	Tstack_DAG DAGs, trigger;
	Tstack_DAGstack *Ptriggers, triggers = NULL;
	Tstack_proof reasons;
	if (context_rec_cont && !context_rec_cont(src)) {
		dest.DAG = DAG_dup(src);
		dest.proof = NULL;
		return dest;
	}
	stack_INIT(DAGs);
	stack_INIT(reasons);
	if (binder_all(DAG_symb(src))) {
		pos = 0;
		context_rec_push(src, &pos);
		/* transformations on var/mapping lists shouldn't appear in proof */
		for (i = pos; i < DAG_arity(src) - 1u; ++i)
			stack_push(DAGs, DAG_tree_subst(DAG_arg(src, i)));
		dest = context_tree_rec_proof(DAG_arg_last(src));
		stack_push(DAGs, dest.DAG);
		assert(!dest.proof || dest.DAG != DAG_arg_last(src));
		if (dest.proof) {
			stack_merge(reasons, dest.proof);
			stack_free(dest.proof);
			dest.DAG = DAG_dup(
				(stack_size(DAGs) == 1 ? stack_top(DAGs)
															 : DAG_new_stack(DAG_symb(src), DAGs)));
			/* Updating triggers, if any */
			if (quantifier(DAG_symb(src))) {
				Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
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
		} else
			dest.DAG = DAG_dup(src);
		context_rec_pop(src, pos);
	} else {
		for (i = 0; i < DAG_arity(src); ++i) {
			context_rec_push(src, &i);
			dest = context_tree_rec_proof(DAG_arg(src, i));
			context_rec_pop(src, i);
			stack_push(DAGs, dest.DAG);
			assert(!dest.proof || dest.DAG != DAG_arg(src, i));
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
			}
		}
		dest.DAG = DAG_dup(
			stack_is_empty(reasons) ? src : DAG_new_stack(DAG_symb(src), DAGs));
	}
	stack_apply(DAGs, DAG_free);
	stack_free(DAGs);
	tmp = context_rec_reduce(
		src, dest.DAG, context_rec_replace(dest.DAG, src, reasons));
	stack_free(reasons);
	return tmp;
}

static void
context_DAG_rec_proof(TDAG src)
{
	unsigned i, pos;
	TDAG_proof dest, tmp, arg0, arg1;
	TDAG* PDAG;
	Tstack_proof reasons;
	if (DAG_p_ctx[src] && DAG_for_p_ctx(src)) return;
	if (context_rec_cont && !context_rec_cont(src)) {
		set_DAG_p_ctx(src, DAG_dup(src), NULL);
		return;
	}
	if (binder_all(DAG_symb(src))) {
		dest = context_tree_rec_proof(src);
		set_DAG_p_ctx(src, dest.DAG, dest.proof);
		return;
	}
	stack_INIT(reasons);
	switch (DAG_arity(src)) {
		case 0: dest.DAG = DAG_dup(src); break;
		case 1:
			pos = 0;
			context_rec_push(src, &pos);
			context_DAG_rec_proof(DAG_arg0(src));
			arg0 = get_DAG_p_in_p_ctx(DAG_arg0(src));
			context_rec_pop(src, pos);
			if (arg0.proof) {
				assert(DAG_arg0(src) != arg0.DAG);
				dest.DAG = DAG_dup(DAG_new_unary(DAG_symb(src), arg0.DAG));
				stack_merge(reasons, arg0.proof);
			} else
				dest.DAG = DAG_dup(src);
			break;
		case 2:
			pos = 0;
			context_rec_push(src, &pos);
			context_DAG_rec_proof(DAG_arg0(src));
			arg0 = get_DAG_p_in_p_ctx(DAG_arg0(src));
			context_rec_pop(src, 0);
			pos = 1;
			context_rec_push(src, &pos);
			context_DAG_rec_proof(DAG_arg1(src));
			arg1 = get_DAG_p_in_p_ctx(DAG_arg1(src));
			context_rec_pop(src, 1);
			if (arg0.proof || arg1.proof) {
				assert(DAG_arg0(src) != arg0.DAG || DAG_arg1(src) != arg1.DAG);
				dest.DAG = DAG_dup(DAG_new_binary(DAG_symb(src), arg0.DAG, arg1.DAG));
				if (DAG_arg0(src) != arg0.DAG) stack_merge(reasons, arg0.proof);
				if (DAG_arg1(src) != arg1.DAG) stack_merge(reasons, arg1.proof);
			} else
				dest.DAG = DAG_dup(src);
			break;
		default:
			MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
			for (i = 0; i < DAG_arity(src); i++) {
				context_rec_push(src, &i);
				context_DAG_rec_proof(DAG_arg(src, i));
				arg0 = get_DAG_p_in_p_ctx(DAG_arg(src, i));
				context_rec_pop(src, i);
				assert(!arg0.proof || arg0.DAG != DAG_arg(src, i));
				if (arg0.proof) stack_merge(reasons, arg0.proof);
				PDAG[i] = arg0.DAG;
			}
			if (!stack_is_empty(reasons))
				dest.DAG = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
			else {
				free(PDAG);
				dest.DAG = DAG_dup(src);
			}
	}
	tmp = context_rec_reduce(
		src, dest.DAG, context_rec_replace(dest.DAG, src, reasons));
	stack_free(reasons);
	set_DAG_p_ctx(src, tmp.DAG, tmp.proof);
}

/**
   \remark f_reduce must be destructive for the first parameter */
TDAG
context_structural_recursion_proof(
	TDAG src, Tproof* Pproof, void (*f_init)(void),
	void (*f_push)(TDAG, unsigned*), void (*f_pop)(TDAG, unsigned),
	Tproof (*f_replace)(TDAG, TDAG, Tstack_proof),
	TDAG_proof (*f_reduce)(TDAG, TDAG, Tproof), bool (*cont)(TDAG))
{
	TDAG equiv;
	TDAG_proof dest;
	Tstack_proof reasons;
	f_init();

	context_rec_push = f_push;
	context_rec_pop = f_pop;
	context_rec_replace = f_replace;
	context_rec_reduce = f_reduce;
	context_rec_cont = cont;

	DAG_tmp_reserve();
	context_DAG_rec_proof(src);
	dest = get_DAG_p_in_p_ctx(src);
	DAG_dup(dest.DAG);
	if (dest.proof) {
		/* resolution with equiv, transformation and equivalence leads to result */
		stack_INIT(reasons);
		assert(stack_size(dest.proof) == 1);
		stack_push(reasons, *Pproof);
		stack_merge(reasons, dest.proof);
		/* equiv derivation of end result; TODO: need this dup/free?? */
		equiv = DAG_dup(DAG_equiv(src, dest.DAG));
		stack_push(reasons, proof_equiv_pos2(equiv));
		DAG_free(equiv);

		*Pproof = proof_step_conclusion(ps_type_th_resolution, dest.DAG, reasons);
		stack_free(reasons);
	}
	DAG_tmp_free_DAG_p_ctx(src);
	DAG_tmp_release();
	ctx_free(context);
	return dest.DAG;
}
