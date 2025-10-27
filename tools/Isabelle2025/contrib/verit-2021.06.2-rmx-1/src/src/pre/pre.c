#include "symbolic/DAG-stat.h"
#include "symbolic/DAG.h"
#include "utils/general.h"
#include "utils/options.h"
#include "veriT-config.h"

#include <string.h>

#ifdef DEBUG
#include "symbolic/DAG-print.h"
#endif

#include "arith/LA-pre.h"
#include "instantiation/free-vars.h"
#include "instantiation/inst-pre.h"
#include "instantiation/inst-trigger-selection.h"
#include "instantiation/inst-trigger.h"
#include "pre/HOL.h"
#include "pre/bclauses.h"
#include "pre/bfun-elim.h"
#include "pre/binder-rename.h"
#include "pre/connectives.h"
#include "pre/distinct-elim.h"
#include "pre/fix-trigger.h"
#include "pre/ite-elim.h"
#include "pre/let-elim.h"
#include "pre/nary-elim.h"
#include "pre/pm.h"
#include "pre/pre.h"
#include "pre/qnt-tidy.h"
#include "pre/qnt-trigger.h"
#include "pre/rare-symb.h"
#include "pre/simp-formula-sat.h"
#include "pre/simp-sym.h"
#include "pre/simp-unit.h"
#include "pre/simplify.h"
#include "pre/skolem.h"
#include "pre/qsimp-by-unification.h"
#include "proof/proof.h"
#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/context-recursion-proof.h"
#include "symbolic/context-recursion.h"
#include "symbolic/qnt-utils.h"
#include "symbolic/recursion.h"

static bool pre_quantifier = true;
static bool pre_eq = false;

/**
   \addtogroup arguments_user

   - --disable-simp

   Disables simplification of expressions. */
static bool disable_simp = false;

static bool disable_unit_simp = false;
static bool disable_unit_subst_simp = false;
static bool disable_bclause = false;
static bool disable_ackermann = false;

static bool disable_qsimp = false;
static bool qsimp_delete = false;
static bool qsimp_eager = false;
static bool qsimp_solitary = false;

/**
   \addtogroup arguments_user

   - --disable-sym

   Disables symmetry breaking. */
static bool disable_sym = false;

/**
   \addtogroup arguments_user

   - --enable-assumption-simp

   Enables application of simplifications of assumptions */
static bool enable_assumption_simp = 0;

Tstack_DAG snapshot = NULL;

static void
pre_proof_snapshot(unsigned n, TDAG* Psrc)
{
	unsigned i;
	if (!snapshot) {
		stack_INIT(snapshot);
	} else
		for (i = 0; i < stack_size(snapshot); i++) DAG_free(stack_get(snapshot, i));
	stack_resize(snapshot, n);
	for (i = 0; i < n; i++) stack_set(snapshot, i, DAG_dup(Psrc[i]));
}

static void
pre_proof_compare(
	unsigned n, TDAG* Psrc, Tproof* Pproof, Tproof(f)(Tproof, TDAG))
{
	unsigned i;
	assert(stack_size(snapshot) == n);
	for (i = 0; i < n; i++)
		if (Psrc[i] != stack_get(snapshot, i)) Pproof[i] = f(Pproof[i], Psrc[i]);
}

static void
pre_proof_erase_snapshot(void)
{
	unsigned i;
	if (snapshot)
		for (i = 0; i < stack_size(snapshot); i++) DAG_free(stack_get(snapshot, i));
	stack_free(snapshot);
}

static TDAG
pre_HOL_to_FOL(TDAG src)
{
	TDAG dest;
	/**************** fix_triggers */
	/* DD normalizes triggers (quantification patterns)
     this should be done once and forall on every input formula
     instances should not be reprocessed */
	/* In principle this should also be applied on formulas coming from
     macros, etc.  However, since this is a favour for the user who
     writes badly formed formulas, it will not be applied on macros */
	fix_trigger_array(1, &src);

	/************* eliminate HOL constructs */
	/* This is needed at least to apply the lambdas introduced by define-fun.
	   We check if this is necessary to avoid changes to clean terms. */
	if (!is_FOL(src)) {
		dest = HOL_to_FOL(src);
		DAG_free(src);
		src = dest;
	}

	/**************** macro_subst */
	/* PF HOL->FOL: the higher order processing */
	/* binder_rename is applied on macro body before expansion so that
     - no bound variable of the macro interacts with bound/unbound vars
     of the formula
     - no free symbol of the macro interacts with binders of the formulas
     thanks to the fact that binder_rename uses fresh names
     To avoid problems with macro containing macros, this occurs
     at macro expansion in macro_substitute */
	/* requires the binder_rename invariant
     should come high in the list because it will introduce new terms
     that also need processing */
	/* dest = context_structural_recursion(src, let_elim_init, let_elim_push, */
	/*                                     let_elim_pop, let_elim_reduce, NULL); */
	dest = let_context_structural_recursion(
		src, let_elim_init, let_elim_push, let_elim_pop, let_elim_reduce);
	DAG_free(src);
	src = dest;
	/**************** bfun_elim */
	dest = bfun_elim(src);
	DAG_free(src);
	src = dest;
	return src;
}

static void
pre_HOL_to_FOL_array_proof(unsigned n, TDAG* Psrc, Tproof* Pproof)
{
	unsigned i;
	TDAG dest;
	/**************** fix_triggers */
	/* DD normalizes triggers (quantification patterns)
     this should be done once and forall on every input formula
     instances should not be reprocessed */
	/* In principle this should also be applied on formulas coming from
     macros, etc.  However, since this is a favour for the user who
     writes badly formed formulas, it will not be applied on macros */
	fix_trigger_array(n, Psrc);
	/**************** HOL_to_FOL */
	/* PF HOL->FOL: the higher order processing */
	/* binder_rename is applied on macro body before expansion so that
     - no bound variable of the macro interacts with bound/unbound vars
     of the formula
     - no free symbol of the macro interacts with binders of the formulas
     thanks to the fact that binder_rename uses fresh names
     To avoid problems with macro containing macros, this occurs
     at macro expansion in macro_substitute */
	/**************** let elimination */
	/* context_structural_recursion_array_proof(n, Psrc, Pproof, let_elim_ctx_aux_proof, CTX_NONE); */
	for (i = 0; i < n; ++i) {
		dest = context_structural_recursion_proof(
			Psrc[i], &Pproof[i], let_elim_init_proof, let_elim_push_proof,
			let_elim_pop_proof, let_elim_replacement, let_elim_reduce_proof, NULL);
		DAG_free(Psrc[i]);
		Psrc[i] = dest;
	}
	/* HOL-free, let-free below this point, but still bfuns */
	/**************** bfun_elim */
	pre_proof_snapshot(n, Psrc);
	bfun_elim_array(n, Psrc);
	pre_proof_compare(n, Psrc, Pproof, proof_bfun_elim);
	pre_proof_erase_snapshot();
}

static TDAG
distinct_elim_aux(TDAG src)
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
	MY_MALLOC(PDAG, DAG_arity(src) * (DAG_arity(src) - 1) * sizeof(TDAG) / 2);
	for (i = 0; i < DAG_arity(src); i++)
		for (j = i + 1; j < DAG_arity(src); j++)
			PDAG[k++] = DAG_neq(DAG_arg(src, i), DAG_arg(src, j));
	dest = DAG_dup(
		DAG_new(CONNECTOR_AND, DAG_arity(src) * (DAG_arity(src) - 1) / 2, PDAG));
	DAG_free(src);
	return dest;
}

#ifdef DEBUG
void
eq_norm_rec(TDAG src)
{
	if (
		DAG_symb(src) == PREDICATE_EQ &&
		(DAG_sort(DAG_arg0(src)) == SORT_BOOLEAN ||
		 DAG_sort(DAG_arg1(src)) == SORT_BOOLEAN))
		my_error("equality over propositions found\n");
}
#endif

static TDAG
pre_lang_red(TDAG src)
{
	TDAG dest;
#ifdef DEBUG
	/* Check if there are equalities between propositions */
	structural_recursion_void(src, eq_norm_rec);
#endif
	/**************** nary_elim */
	/* replace n-ary by binary operators necessary for Skolemization */
	dest = context_structural_recursion(
		src, nary_elim_node_init, nary_elim_node_push, nary_elim_node_pop,
		nary_elim_node_reduce, NULL);

	DAG_free(src);
	src = dest;
	/**************** distinct_elim */
	dest = context_structural_recursion(
		src, distinct_elim_init, distinct_elim_push, distinct_elim_pop,
		distinct_elim_reduce, NULL);

	DAG_free(src);
	return dest;
}

static TDAG
pre_lang_red_proof(TDAG src, Tproof* Pproof)
{
	TDAG dest;
#ifdef DEBUG
	/* Check if there are equalities between propositions */
	structural_recursion_void(src, eq_norm_rec);
#endif
	/**************** nary_elim */
	/* replace n-ary by binary operators necessary for Skolemization */
	dest = context_structural_recursion_proof(
		src, Pproof, nary_elim_node_init, nary_elim_node_push_proof,
		nary_elim_node_pop, nary_elim_node_replacement, nary_elim_node_reduce_proof,
		NULL);
	DAG_free(src);
	src = dest;
	/**************** distinct_elim */
	dest = context_structural_recursion_proof(
		src, Pproof, distinct_elim_init, distinct_elim_push_proof,
		distinct_elim_pop, distinct_elim_replacement, distinct_elim_reduce_proof,
		NULL);
	DAG_free(src);
	return dest;
}

TDAG
pre_quant_ite(TDAG src)
{
	TDAG dest;
	bool first = true, changed = false;
	/* Initializing machinery for renaming bound variables */
	if (DAG_quant(src)) {
		/* Whether there are free or reused variables */
		check_free_shadowed_vars(src);
		DAG_symb_var_resize(stack_size(DAG_sort_stack));
	}
	/* here is a loop to simplify quantifiers, eliminate skolem quant and ites */
	do {
		/* Join sequential occurrences of the same quantifier  */
		dest = context_structural_recursion(
			src, qnt_join_init, qnt_join_push, qnt_join_pop, qnt_join_reduce,
			DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);
		/* QNT ONE-POINT RULE */
		dest = context_structural_recursion(
			src, qnt_onepoint_init, qnt_onepoint_push, qnt_onepoint_pop, qnt_onepoint_reduce,
			DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);
		/* Remove variables which are not used */
		dest = context_structural_recursion(
			src, qnt_rm_unused_init, qnt_rm_unused_push, qnt_rm_unused_pop,
			qnt_rm_unused_reduce, DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);
		/* Structurally identical formulas are canonized to have the same vars */
		dest = context_structural_recursion(
			src, qnt_canon_rename_init, qnt_canon_rename_push, qnt_canon_pop,
			qnt_canon_rename_reduce, DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);
		/* Formula simplifications */
		if (!disable_simp) src = simplify_formula(src);
		/* Remove variables which are not used. The simplifications may remove
         variables from the body (e.g. x * 0), and also canonization may expose
         unused variables, like in AxEyAx. F */
		dest = context_structural_recursion(
			src, qnt_rm_unused_init, qnt_rm_unused_push, qnt_rm_unused_pop,
			qnt_rm_unused_reduce, DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);
		/* forall x. forall y forall y -> forall x. forall y1 forall y2 */
		dest = context_structural_recursion(
			src, qnt_canon_rename_init, qnt_canon_rename_push, qnt_rename_pop,
			qnt_canon_rename_reduce, DAG_quant_or_under_binder);
		check_free_shadowed_vars(dest);
		DAG_free(src);
		src = dest;

		/* ITE elimination */
		dest = ite_elim(src);
		/* if no ite elim, no new skolemizable quant */
		if (!first && src == dest) {
			DAG_free(dest);
			break;
		} else
			first = false;
		DAG_free(src);
		src = dest;
		/* remove double pol connectives over subformulas with quantifiers */
		dest = context_structural_recursion(
			src, single_pol_qnt_init, single_pol_qnt_push, single_pol_qnt_pop,
			single_pol_qnt_reduce, DAG_quant_f);
		DAG_free(src);
		src = dest;
		/* forall x. forall y forall y -> forall x. forall y1 forall y2 */
		dest = context_structural_recursion(
			src, qnt_canon_rename_init, qnt_canon_rename_push, qnt_rename_pop,
			qnt_canon_rename_reduce, DAG_quant_or_under_binder);
		check_free_shadowed_vars(dest);
		DAG_free(src);
		src = dest;
		/* skolemization, which may make new ite terms appear and, if deep_SKO is
         on, joinable quantifiers */
		dest = skolemize(src);
		changed = src != dest;
		DAG_free(src);
		src = dest;
	} while (changed);
	/* Get rid of weak existentials */
	dest = context_structural_recursion(
		src, rewrite_w_exist_init, rewrite_w_exist_push, rewrite_w_exist_pop,
		rewrite_w_exist_reduce, rewrite_w_exist_cont);
	DAG_free(src);
	src = dest;
	return src;
}

static TDAG
pre_quant_ite_proof(TDAG src, Tproof* Pproof)
{
	TDAG dest;
	bool first = true, changed = false;
	/* Initializing machinery for renaming bound variables */
	if (DAG_quant(src)) {
		/* Whether there are free or reused variables */
		check_free_shadowed_vars(src);
		DAG_symb_var_resize(stack_size(DAG_sort_stack));
	}
	do {
		/* Join sequential occurrences of the same quantifier  */
		dest = context_structural_recursion_proof(
			src, Pproof, qnt_join_init, qnt_join_push_proof, qnt_join_pop,
			qnt_join_replacement, qnt_join_reduce_proof, DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);
		/* QNT ONE-POINT RULE */
		dest = context_structural_recursion_proof(
			src, Pproof, qnt_onepoint_init, qnt_onepoint_push_proof, qnt_onepoint_pop,
			qnt_onepoint_replacement, qnt_onepoint_reduce_proof, DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);
		/* Remove variables which are not used */
		dest = context_structural_recursion_proof(
			src, Pproof, qnt_rm_unused_init, qnt_rm_unused_push_proof,
			qnt_rm_unused_pop, qnt_rm_unused_replacement, qnt_rm_unused_reduce_proof,
			DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);
		/* Structurally identical formulas are canonized to have the same vars */
		dest = context_structural_recursion_proof(
			src, Pproof, qnt_canon_rename_init, qnt_canon_rename_push_proof,
			qnt_canon_pop, qnt_canon_rename_replacement,
			qnt_canon_rename_reduce_proof, DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);
		/* Formula simplifications */
		if (!disable_simp) src = simplify_formula_proof(src, Pproof);
		/* Remove variables which are not used. The simplifications may remove
         variables from the body (e.g. x * 0), and also canonization may expose
         unused variables, like in AxEyAx. F */
		dest = context_structural_recursion_proof(
			src, Pproof, qnt_rm_unused_init, qnt_rm_unused_push_proof,
			qnt_rm_unused_pop, qnt_rm_unused_replacement, qnt_rm_unused_reduce_proof,
			DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);

		dest = context_structural_recursion_proof(
			src, Pproof, qnt_canon_rename_init, qnt_canon_rename_push_proof,
			qnt_rename_pop, qnt_canon_rename_replacement,
			qnt_canon_rename_reduce_proof, DAG_quant_or_under_binder);
		DAG_free(src);
		src = dest;
		check_free_shadowed_vars(src);

		/* ITE elimination */
		dest = ite_elim(src);
		if (src != dest) *Pproof = proof_ite_intro(src, dest, *Pproof);
		/* if no ite elim, no new skolemizable quant */
		if (!first && src == dest) {
			DAG_free(dest);
			break;
		} else
			first = false;
		DAG_free(src);
		src = dest;
		/* double pol connectives */
		dest = context_structural_recursion_proof(
			src, Pproof, single_pol_qnt_init, single_pol_qnt_push_proof,
			single_pol_qnt_pop, single_pol_qnt_replacement,
			single_pol_qnt_reduce_proof, DAG_quant_f);
		changed = (src != dest);
		DAG_free(src);
		src = dest;

		dest = context_structural_recursion_proof(
			src, Pproof, qnt_canon_rename_init, qnt_canon_rename_push_proof,
			qnt_rename_pop, qnt_canon_rename_replacement,
			qnt_canon_rename_reduce_proof, DAG_quant_or_under_binder);
		check_free_shadowed_vars(dest);
		DAG_free(src);
		src = dest;

		/* skolemization, which may make new ite terms appear and, if deep_SKO is
         on, joinable quantifiers */
		dest = skolemize_proof(src, Pproof);
		/* skolemize may make new ite terms appear */
		changed = changed || (src != dest);
		DAG_free(src);
		src = dest;
	} while (changed);
	/* get rid of weak existentials */
	dest = context_structural_recursion_proof(
		src, Pproof, rewrite_w_exist_init, rewrite_w_exist_push_proof,
		rewrite_w_exist_pop, rewrite_w_exist_replacement,
		rewrite_w_exist_reduce_proof, rewrite_w_exist_cont);
	DAG_free(src);
	src = dest;
	return src;
}

static TDAG
pre_ite(TDAG src)
{
	TDAG dest;
	/* simplify formula may handle ite in a more gentle way than
     ite_elim, it should therefore come before */
	if (!disable_simp) src = simplify_formula(src);
	/* This has no effect inside quantifiers
     This thus should be applied after any rewrite rule that may
     eliminate quantifiers */
	dest = ite_elim(src);
	DAG_free(src);
	src = dest;
	return src;
}

static TDAG
pre_ite_proof(TDAG src, Tproof* Pproof)
{
	TDAG dest;
	/* simplify formula may handle ite in a more gentle way than
     ite_elim, it should therefore come before */
	if (!disable_simp) src = simplify_formula_proof(src, Pproof);
	/* This has no effect inside quantifiers
     This thus should be applied after any rewrite rule that may
     eliminate quantifiers */
	dest = ite_elim(src);
	if (src != dest) *Pproof = proof_ite_intro(src, dest, *Pproof);
	DAG_free(src);
	src = dest;
	return src;
}

static TDAG
pre_non_essential(TDAG src)
{
	TDAG dest;
	if (!disable_ackermann) {
		dest = rare_symb(src);
		DAG_free(src);
		src = dest;
	}
	if (!disable_sym) {
		dest = simp_sym(src);
		DAG_free(src);
		src = dest;
	}
	if (!disable_unit_subst_simp) src = simplify_formula_sat(src);
	return src;
}

/* This uses NO, CC, etc..., and will only replace atoms by TRUE/FALSE
   this should come late */
/* Requires to have free access to the NO stack */
/* TODO: for incrementality, it should only be activated if the NO stack is empty */
static TDAG
pre_unit(TDAG src)
{
	TDAG dest;
	Tunsigned orig_n;
	Tunsigned dest_n = DAG_count_nodes(src);
	do {
		dest = simplify_unit(src);
		if (dest == src) {
			DAG_free(dest);
			break;
		}
		DAG_free(src);
		src = simplify_formula(dest);
		orig_n = dest_n;
		dest_n = DAG_count_nodes(src);
		check_free_shadowed_vars(src);
	} while ((dest_n > 1) && /* final formula is not TRUE or FALSE */
					 /* previous decrease at least 10 % of nodes */
					 ((orig_n - dest_n) * 10 > orig_n) &&
					 /* previous decrease of at least 20 nodes */
					 ((orig_n - dest_n) > 20));

	return src;
}

TDAG
pre_process(TDAG src)
{
	TDAG dest;
	src = pre_HOL_to_FOL(src);
	assert(is_FOL_strict(src));
	/* HOL-free, let-free below this point */
	src = pre_lang_red(src);
	/* HOL-free, let-free, symbol-normalized below this point */
	/* TODO: Fix this, it's messy now because deep sko rearrange quantifiers,
     makes the static simplifications necessary again. */
	/* quantifier handling (skolem, tidy, simplify) is sensitive to HOL
     it should come after HOL elimination */
	if (pre_quantifier && DAG_quant(src))
		src = pre_quant_ite(src);
	else
		src = pre_ite(src);

	src = pre_non_essential(src);
	if (!disable_simp) src = simplify_formula(src);
	/* HB sets variables infrastructure; ground info is used in congruence
     closure, and quantifier handling, so this should come before unit
     simplification */
	/* TODO: HB The "ground" function seems to be necessary only in
     bclauses_add. If the function "DAG_is_clean" from simp-unit.c were to be
     used there, then this setting of free variables could be performed only at
     the end of pre-processing, as it should. */
	if (pre_quantifier) set_fvars(src);

	if (!disable_unit_simp) src = pre_unit(src);

	if (!disable_bclause) {
		dest = bclauses_add(src);
		DAG_free(src);
		src = simplify_formula(dest);
		check_free_shadowed_vars(src);
	}
	/* PF this should come late because = may be used or generated before,
     e.g. for ite terms */
	if (pre_eq) {
		dest = context_structural_recursion(
			src, rewrite_eq_init, rewrite_eq_push, rewrite_eq_pop, rewrite_eq_reduce,
			NULL);
		DAG_free(src);
		src = dest;
	}
	/* TODO: HB Doing again because some previous simplification is killing
     variables */
	if (pre_quantifier) set_fvars(src);

	/* Removes nested quantifiers using unification */
	if (pre_quantifier && !disable_qsimp)
		src = qsimp_by_unification(src, qsimp_delete, qsimp_eager, qsimp_solitary);

	/* HOL-free
     let-free
     symbol-normalized i.e. variety of symbols are rewritten (or n-ary
     to binary) so that no attention to those is necessary in the solver
     qnt_tidy should be applied
     ite should only occur in quantified formulas
     no strong (skolemizable) quantifier outside ite terms */
	return src;
}

extern unsigned ctx_nb;

void
pre_process_array_proof(unsigned n, TDAG* Psrc, Tproof* Pproof)
{
	unsigned i;
	pre_HOL_to_FOL_array_proof(n, Psrc, Pproof);
	for (i = 0; i < n; ++i) assert(is_FOL_strict(Psrc[i]));
	/* HOL-free, let-free below this point */
	for (i = 0; i < n; i++) Psrc[i] = pre_lang_red_proof(Psrc[i], &Pproof[i]);
	/* HOL-free, let-free, symbol-normalized below this point */

	/* TODO: Fix this, it's messy now because deep sko rearrange quantifiers,
     makes the static simplifications necessary again. */
	/* quantifier handling (skolem, tidy, simplify) is sensitive to HOL
     it should come after HOL elimination */
	for (i = 0; i < n; ++i)
		if (pre_quantifier && DAG_quant(Psrc[i]))
			Psrc[i] = pre_quant_ite_proof(Psrc[i], &Pproof[i]);
		else
			Psrc[i] = pre_ite_proof(Psrc[i], &Pproof[i]);
	if (!disable_simp)
		for (i = 0; i < n; ++i)
			Psrc[i] = simplify_formula_proof(Psrc[i], &Pproof[i]);
	/* this should come very last because it only tags formulas
     ground bit is used in congruence closure, and quantifier handling,
     so this should come before unit simplification */
	/* HB sets variables infrastructure */
	if (pre_quantifier)
		for (i = 0; i < n; ++i)
			if (DAG_quant(Psrc[i])) set_fvars(Psrc[i]);
	/* PF this should come late because = may be used or generated before,
     e.g. for ite terms */
	/* PF this should come late because = may be used or generated before,
     e.g. for ite terms */
	if (pre_eq)
		/* context_structural_recursion_array_proof(n, Psrc, Pproof, let_elim_ctx_aux_proof, CTX_NONE); */
		for (i = 0; i < n; ++i) {
			TDAG dest = context_structural_recursion_proof(
				Psrc[i], &Pproof[i], rewrite_eq_init, rewrite_eq_push_proof,
				rewrite_eq_pop, rewrite_eq_replacement, rewrite_eq_reduce_proof, NULL);
			DAG_free(Psrc[i]);
			Psrc[i] = dest;
		}
	/* HOL-free
     let-free
     symbol-normalized i.e. variety of symbols are rewritten (or n-ary
     to binary) so that no attention to those is necessary in the solver
     qnt_tidy should be applied
     ite should only occur in quantified formulas
     no strong (skolemizable) quantifier outside ite terms */
}

TDAG
pre_process_instance(TDAG src)
{
	TDAG quant, instance, lemma;
	assert(DAG_arity(src) == 2);
	quant = DAG_arg0(src);
	instance = DAG_dup(DAG_arg1(src));
	if (pre_quantifier && DAG_quant(instance))
		instance = pre_quant_ite(instance);
	else
		instance = pre_ite(instance);
	if (!disable_simp) instance = simplify_formula(instance);
	lemma = DAG_dup(DAG_or2(quant, instance));

	/* this should come very last because it only tags formulas
     ground bit is used in congruence closure, and quantifier handling,
     so this should come before unit simplification */
	check_free_shadowed_vars(lemma);
	/* TODO: how to avoid working on ground terms which were already checked? */
	/* sets variables infrastructure */
	set_fvars(lemma);
	DAG_free(src);
	DAG_free(instance);
	return lemma;
}

TDAG
pre_process_instance_proof(TDAG src, Tproof* Pproof)
{
	TDAG quant, instance, lemma;
	Tproof proof;
	assert(DAG_arity(src) == 2);
	quant = DAG_arg0(src);
	instance = DAG_dup(DAG_arg1(src));
	/* quantifier handling (skolem, tidy, simplify) is sensitive to HOL
     it should come after HOL elimination */
	proof_subproof_begin(ps_type_subproof);
	proof = proof_add_input(instance);
	if (pre_quantifier && DAG_quant(instance))
		instance = pre_quant_ite_proof(instance, &proof);
	else
		instance = pre_ite_proof(instance, &proof);
	if (!disable_simp) instance = simplify_formula_proof(instance, &proof);
	if (DAG_arg1(src) != instance) {
		Tproof proof1, proof2, proof3;
		proof = proof_subproof_end_input();
		lemma = DAG_dup(DAG_or2(quant, instance));
		proof1 = proof_or(*Pproof);
		proof2 = proof_or_neg(lemma, 0);
		proof3 = proof_or_neg(lemma, 1);
		*Pproof = proof_resolve(4, proof1, proof, proof2, proof3);
	} else {
		proof_subproof_remove();
		lemma = DAG_dup(src);
	}
	/* this should come very last because it only tags formulas
     ground bit is used in congruence closure, and quantifier handling,
     so this should come before unit simplification */
	check_free_shadowed_vars(lemma);
	/* TODO: how to avoid working on ground terms which were already checked? */
	/* sets variables infrastructure */
	set_fvars(lemma);
	DAG_free(src);
	DAG_free(instance);
	return lemma;
}

void
pre_init(void)
{
	qnt_tidy_init();
	skolemize_init();
	simp_sym_init();
	bclauses_init();
	options_new(
		0, "disable-simp", "Disable simplification of expressions.", &disable_simp);
	options_new(
		0, "disable-unit-simp",
		"Disable unit clause propagation as simplification."
		"Only available in non-interactive mode",
		&disable_unit_simp);
	options_new(
		0, "disable-unit-subst-simp",
		"Disables unit clause rewriting as simplification."
		"Only available in non-interactive mode",
		&disable_unit_subst_simp);
	options_new(
		0, "disable-ackermann",
		"Disable local Ackermannization and elimination of rare symbols.",
		&disable_ackermann);
	options_new(
		0, "disable-qsimp",
		"Disable simplification of quantified formulas by unification.",
		&disable_qsimp);
	options_new(
		0, "qsimp-delete",
		"Delete input assertions after they have been simplified at the cost of completeness.",
		&qsimp_delete);
	options_new(
		0, "qsimp-eager",
		"Apply deep simplification by unification on all subterms.",
		&qsimp_eager);
	options_new(
		0, "qsimp-solitary",
		"Use solitary heurstic when simplifying.",
		&qsimp_solitary);
	options_new(0, "disable-sym", "Disable symmetry breaking.", &disable_sym);
	options_new(
		0, "enable-assumption-simp", "Enable simplifications of assumptions",
		&enable_assumption_simp);
	options_new(
		0, "disable-bclause", "Do not optimize for binary clauses.",
		&disable_bclause);
	pre_quantifier = true;
}

void
pre_logic(char* logic)
{
	if (
		strcmp(logic, "QF_UF") == 0 || strcmp(logic, "QF_UFIDL") == 0 ||
		strcmp(logic, "QF_IDL") == 0 || strcmp(logic, "QF_RDL") == 0 ||
		strcmp(logic, "QF_LRA") == 0 || strcmp(logic, "QF_UFLRA") == 0 ||
		strcmp(logic, "QF_LIA") == 0 || strcmp(logic, "QF_UFLIA") == 0 ||
		strcmp(logic, "QF_LIRA") == 0)
		pre_quantifier = false;
	else
		pre_quantifier = true;
	if (
		strcmp(logic, "QF_RDL") == 0 || strcmp(logic, "QF_IDL") == 0 ||
		strcmp(logic, "QF_LRA") == 0 || strcmp(logic, "QF_LIA") == 0 ||
		strcmp(logic, "QF_LIRA") == 0)
		pre_eq = true;
}

void
pre_done(void)
{
	bclauses_done();
	simp_sym_done();
	qnt_tidy_done();
	skolemize_done();
}
