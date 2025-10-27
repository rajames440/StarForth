#include "pre/simplify-AC.h"

#include "symbolic/DAG-prop.h"
#include "symbolic/DAG.h"
#include "symbolic/qnt-utils.h"
#include "symbolic/veriT-DAG.h"

static void simplify_AC_rec(TDAG src);

Tstack_DAG to_free;

/**
   \brief
   \param symb is an associative and commutative operator */
static bool
simplify_AC_collect(Tsymb symb, TDAG src, Tstack_DAG* DAGs)
{
	bool res = false;
	if (DAG_symb(src) != symb) {
		simplify_AC_rec(src);
		stack_push(*DAGs, DAG_tmp_DAG[src]);
		return DAG_tmp_DAG[src] != src;
	}
	for (unsigned i = 0; i < DAG_arity(src); ++i)
		res = simplify_AC_collect(symb, DAG_arg(src, i), DAGs) || res;
	return res;
}

#define is_AC(s) (s == CONNECTOR_AND || s == CONNECTOR_OR)

/**
   \pre DAG_symb(src) is an associative and commutative operator
   \brief The simplifications performed are illustrated by the
   following examples, where f is an AC symbol with idempotence:
   * (f (f x1 (f x2 x3) x4)) -> (f (f x1 x2 x3 x4)) -> (f x1 x2 x3 x4)
   * (f x1 x1 x2) -> (f x1 x2)
   * (f x1 x1) -> x1
   \note we do not worry about commutativity actually. */
static void
simplify_AC_rec(TDAG src)
{
	unsigned i;
	bool changed, arg_other_symb_simp;
	TDAG *PDAG, dest;
	Tstack_DAG DAGs, args;
	Tstack_DAGstack *Ptriggers, triggers;
	if (DAG_tmp_DAG[src]) return;
	/* Collecting processing of arguments */
	if (!is_AC(DAG_symb(src))) {
		switch (DAG_arity(src)) {
			case 0: dest = DAG_dup(src); break;
			case 1:
				simplify_AC_rec(DAG_arg0(src));
				if (DAG_arg0(src) != DAG_tmp_DAG[DAG_arg0(src)])
					dest =
						DAG_dup(DAG_new_unary(DAG_symb(src), DAG_tmp_DAG[DAG_arg0(src)]));
				else
					dest = DAG_dup(src);
				break;
			case 2:
				simplify_AC_rec(DAG_arg0(src));
				simplify_AC_rec(DAG_arg1(src));
				if (
					DAG_arg0(src) != DAG_tmp_DAG[DAG_arg0(src)] ||
					DAG_arg1(src) != DAG_tmp_DAG[DAG_arg1(src)])
					dest = DAG_dup(DAG_new_binary(
						DAG_symb(src), DAG_tmp_DAG[DAG_arg0(src)],
						DAG_tmp_DAG[DAG_arg1(src)]));
				else
					dest = DAG_dup(src);
				break;
			default:
				MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
				for (i = 0, changed = false; i < DAG_arity(src); i++) {
					simplify_AC_rec(DAG_arg(src, i));
					PDAG[i] = DAG_tmp_DAG[DAG_arg(src, i)];
					changed |= PDAG[i] != DAG_arg(src, i);
				}
				if (changed)
					dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
				else {
					free(PDAG);
					dest = DAG_dup(src);
				}
		}
		/* Moving triggers, if any */
		if (dest != src && quantifier(DAG_symb(dest))) {
			Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
			if (Ptriggers) {
				triggers = copy_triggers(*Ptriggers);
				DAG_prop_set(dest, DAG_PROP_TRIGGER, &triggers);
			}
		}
		DAG_tmp_DAG[src] = dest;
		stack_push(to_free, src);
		return;
	}
	/* AC simplification: Collect args by-passing same symbol */
	stack_INIT(args);
	arg_other_symb_simp = simplify_AC_collect(DAG_symb(src), src, &args);
	/* Remove duplicates */
	stack_INIT(DAGs);
	for (i = 0; i < stack_size(args); ++i)
		if (!DAG_misc(stack_get(args, i))) {
			stack_push(DAGs, stack_get(args, i));
			DAG_misc_set(stack_get(args, i), 1);
		}
	for (i = 0; i < stack_size(args); ++i) DAG_misc_set(stack_get(args, i), 0);
	/* Change if one argument of a different symbol was simplified, or a symbol
     was by-passed (more args than arity), or there were duplicates or only one
     element under symbol */
	if (
		arg_other_symb_simp || stack_size(args) != DAG_arity(src) ||
		stack_size(DAGs) != stack_size(args) || stack_size(DAGs) == 1)
		dest = DAG_dup(
			stack_size(DAGs) == 1 ? stack_top(DAGs)
														: DAG_new_stack(DAG_symb(src), DAGs));
	else
		dest = DAG_dup(src);
	stack_free(args);
	stack_free(DAGs);
	DAG_tmp_DAG[src] = dest;
	stack_push(to_free, src);
}

TDAG
simplify_AC(TDAG src)
{
	unsigned i;
	TDAG dest;
	stack_INIT(to_free);
	DAG_tmp_reserve();
	simplify_AC_rec(src);
	dest = DAG_dup(DAG_tmp_DAG[src]);
	/* DAG_tmp_free_DAG(src); */
	for (i = 0; i < stack_size(to_free); ++i) {
		DAG_free(DAG_tmp_DAG[stack_get(to_free, i)]);
		DAG_tmp_DAG[stack_get(to_free, i)] = DAG_NULL;
	}
	stack_free(to_free);
	DAG_tmp_release();
	DAG_free(src);
	return dest;
}

#define DAG_tmp_DAG_p ((TDAG_proof**)DAG_tmp)

static TDAG_proof simplify_AC_rec_tree_proof(TDAG src);
static void simplify_AC_rec_proof(TDAG src);

/**
   \brief
   \param symb is an associative and commutative operator */
static void
simplify_AC_collect_tree_proof(
	Tsymb symb, TDAG src, Tstack_DAG* DAGs, Tstack_proof* reasons)
{
	unsigned i;
	TDAG_proof dest;
	if (DAG_symb(src) != symb) {
		dest = simplify_AC_rec_tree_proof(src);
		stack_push(*DAGs, dest.DAG);
		if (dest.proof) {
			stack_merge(*reasons, dest.proof);
			stack_free(dest.proof);
		} else
			DAG_free(src);
		return;
	}
	for (i = 0; i < DAG_arity(src); ++i)
		simplify_AC_collect_tree_proof(symb, DAG_arg(src, i), DAGs, reasons);
}

static TDAG_proof
simplify_AC_rec_tree_proof(TDAG src)
{
	unsigned i;
	TDAG_proof dest;
	Tstack_proof reasons;
	Tstack_DAG DAGs, args;
	Tstack_DAGstack *Ptriggers, triggers;
	stack_INIT(DAGs);
	if (!is_AC(DAG_symb(src))) {
		if (quantifier(DAG_symb(src))) {
			for (i = 0; i < DAG_arity(src) - 1u; ++i)
				stack_push(DAGs, DAG_dup(DAG_arg(src, i)));
			/* Start transformation proof */
			stack_push(DAGs, DAG_arity(src) - 1u);
			proof_subproof_begin_context(ps_type_bind, DAGs, NULL);
			stack_dec(DAGs);
			/* Apply transformation on matrix */
			dest = simplify_AC_rec_tree_proof(DAG_arg_last(src));
			if (dest.proof) {
				stack_free(dest.proof);
				stack_push(DAGs, dest.DAG);
				dest.DAG = DAG_dup(DAG_new_stack(DAG_symb(src), DAGs));
				stack_INIT(dest.proof);
				stack_push(
					dest.proof, proof_subproof_end_transformation(src, dest.DAG));
				/* Moving triggers, if any */
				{
					Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
					if (Ptriggers) {
						triggers = copy_triggers(*Ptriggers);
						DAG_prop_set(dest.DAG, DAG_PROP_TRIGGER, &triggers);
					}
				}
			} else {
				proof_subproof_remove();
				DAG_free(DAG_arg_last(src));
				dest.DAG = DAG_dup(src);
				dest.proof = NULL;
			}
		} else {
			stack_INIT(reasons);
			for (i = 0; i < DAG_arity(src); i++) {
				dest = simplify_AC_rec_tree_proof(DAG_arg(src, i));
				stack_push(DAGs, dest.DAG);
				assert(!dest.proof || dest.DAG != DAG_arg(src, i));
				if (dest.proof) {
					stack_merge(reasons, dest.proof);
					stack_free(dest.proof);
				}
			}
			dest.DAG = DAG_dup(
				stack_is_empty(reasons) ? src : DAG_new_stack(DAG_symb(src), DAGs));
			/* Congruence */
			if (!stack_is_empty(reasons)) {
				stack_INIT(dest.proof);
				stack_push(
					dest.proof,
					proof_transformation(ps_type_cong, src, dest.DAG, reasons));
			} else
				dest.proof = NULL;
			stack_free(reasons);
		}
		stack_apply(DAGs, DAG_free);
		stack_free(DAGs);
		return dest;
	}
	/* AC simplification: Collect args by-passing same symbol */
	stack_INIT(reasons);
	stack_INIT(args);
	simplify_AC_collect_tree_proof(DAG_symb(src), src, &args, &reasons);
	/* Remove duplicates */
	for (i = 0; i < stack_size(args); ++i)
		if (!DAG_misc(stack_get(args, i))) {
			stack_push(DAGs, stack_get(args, i));
			DAG_misc_set(stack_get(args, i), 1);
		}
	for (i = 0; i < stack_size(args); ++i) DAG_misc_set(stack_get(args, i), 0);
	/* Change if one argument of a different symbol was simplified, or a symbol
     was by-passed (more args than arity), or there were duplicates or only one
     element under symbol */
	if (
		!stack_is_empty(reasons) || stack_size(args) != DAG_arity(src) ||
		stack_size(DAGs) != stack_size(args) || stack_size(DAGs) == 1) {
		dest.DAG = DAG_dup(
			stack_size(DAGs) == 1 ? stack_top(DAGs)
														: DAG_new_stack(DAG_symb(src), DAGs));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ac_simp, src, dest.DAG, reasons));
	} else {
		dest.DAG = DAG_dup(src);
		dest.proof = NULL;
	}
	stack_free(reasons);
	stack_free(args);
	stack_free(DAGs);
	return dest;
}

/**
   \brief
   \param symb is an associative and commutative operator */
static void
simplify_AC_collect_proof(
	Tsymb symb, TDAG src, Tstack_DAG* DAGs, Tstack_proof* reasons)
{
	unsigned i;
	if (DAG_symb(src) != symb) {
		simplify_AC_rec_proof(src);
		stack_push(*DAGs, DAG_tmp_DAG_p[src]->DAG);
		if (DAG_tmp_DAG_p[src]->proof)
			stack_merge(*reasons, DAG_tmp_DAG_p[src]->proof);
		return;
	}
	for (i = 0; i < DAG_arity(src); ++i)
		simplify_AC_collect_proof(symb, DAG_arg(src, i), DAGs, reasons);
}

static void
simplify_AC_rec_proof(TDAG src)
{
	unsigned i;
	TDAG_proof dest;
	Tstack_proof reasons;
	TDAG* PDAG;
	Tstack_DAG DAGs, args;
	if (DAG_tmp_DAG_p[src]) return;
	/* Collecting processing of arguments */
	if (!is_AC(DAG_symb(src))) {
		if (quantifier(DAG_symb(src)))
			dest = simplify_AC_rec_tree_proof(src);
		else {
			stack_INIT(reasons);
			switch (DAG_arity(src)) {
				case 0: dest.DAG = DAG_dup(src); break;
				case 1:
					simplify_AC_rec_proof(DAG_arg0(src));
					if (DAG_tmp_DAG_p[DAG_arg0(src)]->proof) {
						dest.DAG = DAG_dup(
							DAG_new_unary(DAG_symb(src), DAG_tmp_DAG_p[DAG_arg0(src)]->DAG));
						stack_merge(reasons, DAG_tmp_DAG_p[DAG_arg0(src)]->proof);
					} else
						dest.DAG = DAG_dup(src);
					break;
				case 2:
					simplify_AC_rec_proof(DAG_arg0(src));
					simplify_AC_rec_proof(DAG_arg1(src));
					if (
						DAG_tmp_DAG_p[DAG_arg0(src)]->proof ||
						DAG_tmp_DAG_p[DAG_arg1(src)]->proof) {
						dest.DAG = DAG_dup(DAG_new_binary(
							DAG_symb(src), DAG_tmp_DAG_p[DAG_arg0(src)]->DAG,
							DAG_tmp_DAG_p[DAG_arg1(src)]->DAG));
						if (DAG_tmp_DAG_p[DAG_arg0(src)]->proof)
							stack_merge(reasons, DAG_tmp_DAG_p[DAG_arg0(src)]->proof);
						if (DAG_tmp_DAG_p[DAG_arg1(src)]->proof)
							stack_merge(reasons, DAG_tmp_DAG_p[DAG_arg1(src)]->proof);
					} else
						dest.DAG = DAG_dup(src);
					break;
				default:
					MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
					for (i = 0; i < DAG_arity(src); i++) {
						simplify_AC_rec_proof(DAG_arg(src, i));
						PDAG[i] = DAG_tmp_DAG_p[DAG_arg(src, i)]->DAG;
						if (DAG_tmp_DAG_p[DAG_arg(src, i)]->proof)
							stack_merge(reasons, DAG_tmp_DAG_p[DAG_arg(src, i)]->proof);
					}
					if (!stack_is_empty(reasons))
						dest.DAG = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
					else {
						free(PDAG);
						dest.DAG = DAG_dup(src);
					}
			}
			/* Congruence */
			if (!stack_is_empty(reasons)) {
				stack_INIT(dest.proof);
				stack_push(
					dest.proof,
					proof_transformation(ps_type_cong, src, dest.DAG, reasons));
			} else
				dest.proof = NULL;
			stack_free(reasons);
		}
		MY_MALLOC(DAG_tmp_DAG_p[src], sizeof(TDAG_proof));
		DAG_tmp_DAG_p[src]->DAG = dest.DAG;
		DAG_tmp_DAG_p[src]->proof = dest.proof;
		stack_push(to_free, src);
		return;
	}
	/* AC simplification: Collect args by-passing same symbol */
	stack_INIT(reasons);
	stack_INIT(args);
	simplify_AC_collect_proof(DAG_symb(src), src, &args, &reasons);
	/* Remove duplicates */
	stack_INIT(DAGs);
	for (i = 0; i < stack_size(args); ++i)
		if (!DAG_misc(stack_get(args, i))) {
			stack_push(DAGs, stack_get(args, i));
			DAG_misc_set(stack_get(args, i), 1);
		}
	for (i = 0; i < stack_size(args); ++i) DAG_misc_set(stack_get(args, i), 0);
	/* Change if one argument of a different symbol was simplified, or a symbol
     was by-passed (more args than arity), or there were duplicates or only one
     element under symbol */
	if (
		!stack_is_empty(reasons) || stack_size(args) != DAG_arity(src) ||
		stack_size(DAGs) != stack_size(args) || stack_size(DAGs) == 1) {
		dest.DAG = DAG_dup(
			stack_size(DAGs) == 1 ? stack_top(DAGs)
														: DAG_new_stack(DAG_symb(src), DAGs));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ac_simp, src, dest.DAG, reasons));
	} else {
		dest.DAG = DAG_dup(src);
		dest.proof = NULL;
	}
	stack_free(reasons);
	stack_free(args);
	stack_free(DAGs);
	MY_MALLOC(DAG_tmp_DAG_p[src], sizeof(TDAG_proof));
	DAG_tmp_DAG_p[src]->DAG = dest.DAG;
	DAG_tmp_DAG_p[src]->proof = dest.proof;
	stack_push(to_free, src);
}

TDAG
simplify_AC_proof(TDAG src, Tproof* Pproof)
{
	TDAG equiv;
	TDAG_proof dest;
	Tstack_proof reasons;
	stack_INIT(to_free);
	DAG_tmp_reserve();
	simplify_AC_rec_proof(src);
	dest.DAG = DAG_dup(DAG_tmp_DAG_p[src]->DAG);
	dest.proof = DAG_tmp_DAG_p[src]->proof;
	if (dest.proof) {
		/* resolution with equiv, transformation and equivalence leads to
         result */
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
	for (unsigned i = 0; i < stack_size(to_free); ++i) {
		assert(DAG_tmp_DAG_p[stack_get(to_free, i)]);
		DAG_free(DAG_tmp_DAG_p[stack_get(to_free, i)]->DAG);
		if (DAG_tmp_DAG_p[stack_get(to_free, i)]->proof)
			stack_free(DAG_tmp_DAG_p[stack_get(to_free, i)]->proof);
		free(DAG_tmp_DAG_p[stack_get(to_free, i)]);
		DAG_tmp_DAG_p[stack_get(to_free, i)] = NULL;
	}
	stack_free(to_free);
	DAG_tmp_release();
	DAG_free(src);
	return dest.DAG;
}
