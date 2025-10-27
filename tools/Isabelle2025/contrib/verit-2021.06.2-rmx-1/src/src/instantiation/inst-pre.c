#define DEBUG_INST_PRE 0

#include "instantiation/inst-pre.h"

#include "instantiation/free-vars.h"
#include "pre/qnt-tidy.h"
#include "pre/qnt-trigger.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-subst.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/polarities.h"
#include "symbolic/qnt-utils.h"
#include "symbolic/recursion.h"
#include "utils/options.h"
#include "utils/statistics.h"

/*
  --------------------------------------------------------------
  Normal forms
  --------------------------------------------------------------
*/

#define cnf_count ((unsigned**)DAG_tmp)
#define cnf_count_clauses(D) cnf_count[D][0]

static inline unsigned
cnf_count_DAGs(TDAG DAG)
{
	unsigned i, total = 0;
	assert(cnf_count[DAG] && cnf_count[DAG][0]);
	for (i = 1; i <= cnf_count[DAG][0]; ++i) total += cnf_count[DAG][i];
	return total;
}

void
DAG_tmp_reset_cnf_count(TDAG DAG)
{
	unsigned i;
	if (!cnf_count[DAG]) return;
	free(cnf_count[DAG]);
	cnf_count[DAG] = NULL;
	for (i = 0; i < DAG_arity(DAG); ++i) DAG_tmp_reset_cnf_count(DAG_arg(DAG, i));
}

extern int ccfv_cnf_threshold;

static void
count_CNF_nodes(TDAG DAG)
{
	unsigned i, j, k1, k2, clauses, arg_clauses;
	Tsymb top_symbol = DAG_symb(DAG);
	assert(top_symbol != QUANTIFIER_FORALL);
	if (cnf_count[DAG]) return;
	if (DAG_literal(DAG)) {
		assert(!quantifier(top_symbol) || top_symbol == QUANTIFIER_EXISTS);
		MY_MALLOC(cnf_count[DAG], 2 * sizeof(unsigned));
		cnf_count_clauses(DAG) = 1;
		cnf_count[DAG][1] = 1;
		return;
	}
	if (top_symbol == CONNECTOR_AND) {
		clauses = 0;
		MY_MALLOC(cnf_count[DAG], sizeof(unsigned));
		for (i = 0; i < DAG_arity(DAG); ++i) {
			count_CNF_nodes(DAG_arg(DAG, i));
			if (!cnf_count_clauses(DAG_arg(DAG, i))) break;
			MY_REALLOC(
				cnf_count[DAG],
				(clauses + cnf_count_clauses(DAG_arg(DAG, i)) + 1) * sizeof(unsigned));
			for (j = 1; j <= cnf_count_clauses(DAG_arg(DAG, i)); ++j)
				cnf_count[DAG][++clauses] = cnf_count[DAG_arg(DAG, i)][j];
		}
		cnf_count_clauses(DAG) = clauses;
		/* Whether it exploded */
		if (i != DAG_arity(DAG) || cnf_count_DAGs(DAG) > ccfv_cnf_threshold) {
			cnf_count_clauses(DAG) = 0;
			return;
		}
#if DEBUG_INST_PRE > 2
		my_DAG_message(
			"CNF count of %D: %d clauses, %d DAGs\n", DAG, cnf_count_clauses(DAG),
			cnf_count_DAGs(DAG));
#endif
		return;
	}
	/* OR */
	assert(top_symbol == CONNECTOR_OR && DAG_arity(DAG));
	/* TODO: Improve this so it does not go separately at 0 */
	clauses = 0;
	count_CNF_nodes(DAG_arg(DAG, 0));
	/* Whether it exploded */
	if (!cnf_count_clauses(DAG_arg(DAG, 0))) {
		MY_MALLOC(cnf_count[DAG], sizeof(unsigned));
		cnf_count_clauses(DAG) = 0;
		return;
	}
	MY_REALLOC(
		cnf_count[DAG],
		(cnf_count_clauses(DAG_arg(DAG, 0)) + 1) * sizeof(unsigned));
	cnf_count_clauses(DAG) = cnf_count_clauses(DAG_arg(DAG, 0));
	for (i = 1; i <= cnf_count_clauses(DAG_arg(DAG, 0)); ++i)
		cnf_count[DAG][++clauses] = cnf_count[DAG_arg(DAG, 0)][i];
	for (i = 1; i < DAG_arity(DAG); ++i) {
		count_CNF_nodes(DAG_arg(DAG, i));
		/* Whether it exploded */
		if (!cnf_count_clauses(DAG_arg(DAG, i))) break;
		arg_clauses = cnf_count_clauses(DAG_arg(DAG, i));
		k2 = clauses;
		clauses = clauses * arg_clauses;
		MY_REALLOC(cnf_count[DAG], (clauses + 1) * sizeof(unsigned));
		for (j = clauses; j > 0;) {
			for (k1 = 0; k1 < arg_clauses; ++k1)
				cnf_count[DAG][j - k1] =
					cnf_count[DAG_arg(DAG, i)][arg_clauses - k1] + cnf_count[DAG][k2];
			j -= k1;
			--k2;
		}
		cnf_count_clauses(DAG) = clauses;
		/* Whether it exploded */
		if (cnf_count_DAGs(DAG) > ccfv_cnf_threshold) break;
	}
	/* Whether it exploded */
	if (i != DAG_arity(DAG)) {
		cnf_count_clauses(DAG) = 0;
		return;
	}
#if DEBUG_INST_PRE > 2
	my_DAG_message(
		"CNF count of %D: %d clauses, %d DAGs\n", DAG, cnf_count_clauses(DAG),
		cnf_count_DAGs(DAG));
#endif
}

/**
   \brief Traverses DAG applying the distributive law whenever necessary
   \param DAG a prenexed quantified formula in NNF
   \return Always return either a single literal of a conjunction of clauses
   \remark Naive (on purpose, though), exponential. Treats EXISTS as atom for
   now. */
#define cnf_of ((Tstack_DAGstack*)DAG_tmp)

void
DAG_tmp_reset_cnf(TDAG DAG)
{
	unsigned i;
	if (!cnf_of[DAG]) return;
	stack_apply(cnf_of[DAG], stack_free);
	stack_free(cnf_of[DAG]);
	for (i = 0; i < DAG_arity(DAG); ++i) DAG_tmp_reset_cnf(DAG_arg(DAG, i));
}

static void
set_CNF_rec(TDAG DAG)
{
	unsigned i, j;
	Tstack_DAG old_clause, new_clause;
	Tstack_DAGstack clauses;
	Tsymb top_symbol = DAG_symb(DAG);
	assert(top_symbol != QUANTIFIER_FORALL);
	if (cnf_of[DAG]) return;
	stack_INIT(cnf_of[DAG]);
	if (DAG_literal(DAG)) {
		/* Put literal into a new clause */
		assert(!quantifier(top_symbol) || top_symbol == QUANTIFIER_EXISTS);
		stack_INIT(new_clause);
		stack_push(new_clause, DAG);
		stack_push(cnf_of[DAG], new_clause);
		return;
	}
	if (top_symbol == CONNECTOR_AND) {
		/* Put the CNF of each argument in a new clause */
		for (i = 0; i < DAG_arity(DAG); ++i) {
			set_CNF_rec(DAG_arg(DAG, i));
			for (j = 0; j < stack_size(cnf_of[DAG_arg(DAG, i)]); ++j) {
				stack_COPY(new_clause, stack_get(cnf_of[DAG_arg(DAG, i)], j));
				stack_push(cnf_of[DAG], new_clause);
			}
		}
#if DEBUG_INST_PRE > 1
		my_DAG_message("CNF of %D:\n", DAG);
		print_Tstack_DAGstack(cnf_of[DAG]);
#endif
		return;
	}
	/* OR */
	assert(top_symbol == CONNECTOR_OR && DAG_arity(DAG));
	/* TODO: Improve this so it does not go separately at 0 */
	set_CNF_rec(DAG_arg(DAG, 0));
	for (i = 0; i < stack_size(cnf_of[DAG_arg(DAG, 0)]); ++i) {
		stack_COPY(new_clause, stack_get(cnf_of[DAG_arg(DAG, 0)], i));
		stack_push(cnf_of[DAG], new_clause);
	}
	for (i = 1; i < DAG_arity(DAG); ++i) {
		/* For each disjunct, compute its CNF. Then combine each of its clauses
         with every clause from the previous disjuncts */
		set_CNF_rec(DAG_arg(DAG, i));
		stack_INIT(clauses);
		while (!stack_is_empty(cnf_of[DAG])) {
			old_clause = stack_pop(cnf_of[DAG]);
			for (j = 0; j < stack_size(cnf_of[DAG_arg(DAG, i)]); ++j) {
				stack_COPY(new_clause, old_clause);
				stack_merge(new_clause, stack_get(cnf_of[DAG_arg(DAG, i)], j));
				stack_push(clauses, new_clause);
			}
			stack_free(old_clause);
		}
		stack_free(cnf_of[DAG]);
		stack_COPY(cnf_of[DAG], clauses);
		stack_free(clauses);
	}
#if DEBUG_INST_PRE > 1
	my_DAG_message("CNF of %D:\n", DAG);
	print_Tstack_DAGstack(cnf_of[DAG]);
#endif
}

/* TODO: this comment seems outdated */
/**
   \brief removes all nested occurrences of universal quantifiers while
   accumulating the respective bounded variables
   \param DAG a formula
   \param all_vars all bound variables accumulated so far
   \param ubound_vars all universally bound variables accumulated so far
   \return a DAG without universals
   \remark not destructive to parameters, however all prenexed DAGs created
   during the process that are not necessary ultimately (the temporary
   "and/or") are destructed
   \remark does variable renaming to avoid capture (same variable under the
   scope of two quantifiers) */
static unsigned
prenex_rec(TDAG DAG, Tstack_DAG* ubound_vars)
{
	Tsymb top_symbol = DAG_symb(DAG);
	if (DAG_tmp_DAG[DAG]) return DAG_tmp_DAG[DAG] != DAG;
	if (
		!DAG_quant(DAG) || (DAG_literal(DAG) && top_symbol != QUANTIFIER_FORALL)) {
		DAG_tmp_DAG[DAG] = DAG;
		return 0;
	}
	if (top_symbol == QUANTIFIER_FORALL) {
		for (unsigned i = 0; i < DAG_arity(DAG) - 1; ++i)
			stack_push(*ubound_vars, DAG_arg(DAG, i));
		prenex_rec(DAG_arg_last(DAG), ubound_vars);
		DAG_tmp_DAG[DAG] = DAG_tmp_DAG[DAG_arg_last(DAG)];
		return 1;
	}

	unsigned res = 0;
	assert(top_symbol == CONNECTOR_OR || top_symbol == CONNECTOR_AND);
	for (unsigned i = 0; i < DAG_arity(DAG); ++i)
		res |= prenex_rec(DAG_arg(DAG, i), ubound_vars);
	if (res) {
		Tstack_DAG DAGs;
		stack_INIT(DAGs);
		for (unsigned i = 0; i < DAG_arity(DAG); ++i)
			stack_push(DAGs, DAG_tmp_DAG[DAG_arg(DAG, i)]);
		TDAG tmp_DAG = DAG_new_stack(top_symbol, DAGs);
		stack_free(DAGs);
		DAG_tmp_DAG[DAG] = tmp_DAG;
		return 1;
	}
	DAG_tmp_DAG[DAG] = DAG;
	return 0;
}

TDAG
prenex(TDAG DAG)
{
	assert(DAG_symb(DAG) == QUANTIFIER_FORALL);
	Tstack_DAG DAGs;
	stack_INIT(DAGs);
	/* Prenex universal quantifiers in DAG */
	DAG_tmp_reserve();
	prenex_rec(DAG, &DAGs);
	TDAG body_DAG = DAG_tmp_DAG[DAG_arg_last(DAG)];
	DAG_tmp_reset_DAG(DAG);
	if (body_DAG != DAG_arg_last(DAG)) {
		stack_push(DAGs, body_DAG);
		DAG = DAG_new_stack(QUANTIFIER_FORALL, DAGs);
	}
	DAG_tmp_release();
	stack_free(DAGs);
	return DAG;
}

/**
   \brief resets DAG_tmp_DAG of all subDAGs execept if they are variables among
   the given set
   \param DAG a DAG
   \param vars a set of variables */
static void
DAG_tmp_reset_DAG_except_vars(TDAG DAG, Tstack_DAG vars)
{
	unsigned i;
	if (!DAG_tmp_DAG[DAG]) return;
	if (DAG_arity(DAG) || !vars || !stack_DAG_contains(vars, DAG))
		DAG_tmp_DAG[DAG] = DAG_NULL;
	for (i = 0; i < DAG_arity(DAG); i++)
		DAG_tmp_reset_DAG_except_vars(DAG_arg(DAG, i), vars);
}

TDAG
NNF(TDAG DAG, bool pol)
{
	Tsymb top_symbol = DAG_symb(DAG);
	if (top_symbol == CONNECTOR_NOT) return NNF(DAG_arg0(DAG), !pol);
	if (top_symbol == CONNECTOR_AND || top_symbol == CONNECTOR_OR) {
		Tsymb nnf_symbol = pol
			? top_symbol
			: (top_symbol == CONNECTOR_OR ? CONNECTOR_AND : CONNECTOR_OR);
		Tstack_DAG DAGs;
		stack_INIT(DAGs);
		for (unsigned i = 0; i < DAG_arity(DAG); ++i) {
			TDAG tmp_NNF = NNF(DAG_arg(DAG, i), pol);
			if (DAG_symb(tmp_NNF) == nnf_symbol) {
				for (unsigned j = 0; j < DAG_arity(tmp_NNF); ++j)
					stack_push(DAGs, DAG_dup(DAG_arg(tmp_NNF, j)));
				DAG_free(DAG_dup(tmp_NNF));
			} else
				stack_push(DAGs, DAG_dup(tmp_NNF));
		}
		TDAG nnf_DAG = DAG_new_stack(nnf_symbol, DAGs);
		stack_apply(DAGs, DAG_free);
		stack_free(DAGs);
		return nnf_DAG;
	}
	if (top_symbol == CONNECTOR_IMPLIES) {
		if (pol) return DAG_or2(NNF(DAG_arg0(DAG), 0), NNF(DAG_arg1(DAG), 1));
		return DAG_and2(NNF(DAG_arg0(DAG), 1), NNF(DAG_arg1(DAG), 0));
	}
	if (top_symbol == CONNECTOR_EQUIV) {
		if (pol)
			return DAG_and2(
				DAG_or2(NNF(DAG_arg0(DAG), 0), NNF(DAG_arg1(DAG), 1)),
				DAG_or2(NNF(DAG_arg1(DAG), 0), NNF(DAG_arg0(DAG), 1)));
		return DAG_or2(
			DAG_and2(NNF(DAG_arg0(DAG), 1), NNF(DAG_arg1(DAG), 0)),
			DAG_and2(NNF(DAG_arg1(DAG), 1), NNF(DAG_arg0(DAG), 0)));
		/* TODO: test if this is OK */
		/* return DAG_and2(DAG_or2(NNF(DAG_arg0(DAG), 1), */
		/*                         NNF(DAG_arg1(DAG), 1)), */
		/*                DAG_or2(NNF(DAG_arg0(DAG), 0), */
		/*                         NNF(DAG_arg1(DAG), 0))); */
	}
	if (top_symbol == CONNECTOR_ITE) {
		if (pol)
			return DAG_and2(
				DAG_or2(NNF(DAG_arg(DAG, 0), 0), NNF(DAG_arg(DAG, 1), 1)),
				DAG_or2(NNF(DAG_arg(DAG, 0), 1), NNF(DAG_arg(DAG, 2), 1)));
		return DAG_or2(
			DAG_and2(NNF(DAG_arg(DAG, 0), 1), NNF(DAG_arg(DAG, 1), 0)),
			DAG_and2(NNF(DAG_arg(DAG, 0), 0), NNF(DAG_arg(DAG, 2), 0)));
	}
	if (quantifier(top_symbol)) {
		Tstack_DAG DAGs;
		stack_INIT(DAGs);
		Tsymb nnf_symbol = pol
			? top_symbol
			: (top_symbol == QUANTIFIER_FORALL ? QUANTIFIER_EXISTS
																				 : QUANTIFIER_FORALL);
		for (unsigned i = 0; i < DAG_arity(DAG) - 1; ++i)
			stack_push(DAGs, DAG_arg(DAG, i));
		stack_push(DAGs, NNF(DAG_arg_last(DAG), pol));
		TDAG nnf_DAG = DAG_new_stack(nnf_symbol, DAGs);
		stack_free(DAGs);
		return nnf_DAG;
	}
	if (DAG_literal(DAG)) return pol ? DAG : DAG_neg(DAG);
	my_DAG_error(
		"NNF: Symbol %s is not supported.\n", DAG_symb_name2(top_symbol));
	return DAG_NULL;
}

typedef struct TDAG_pol
{
	TDAG pol[3];
} * TDAG_pol;

#define DAG_tmp_DAG_pol ((TDAG_pol*)DAG_tmp)

#define DAG_for_pol(A, p) \
	(p == POL_NEG ? DAG_tmp_DAG_pol[A]->pol[0] \
								: (p == POL_POS ? DAG_tmp_DAG_pol[A]->pol[1] \
																: DAG_tmp_DAG_pol[A]->pol[2]))

#define get_DAG(A, p) \
	((DAG_tmp_DAG_pol[A] && DAG_for_pol(A, p)) ? DAG_for_pol(A, p) : A)

#define set_DAG_pol(A, pol, B) \
	if (!DAG_tmp_DAG_pol[A]) { \
		MY_MALLOC(DAG_tmp_DAG_pol[A], 3 * sizeof(TDAG)); \
		DAG_tmp_DAG_pol[A]->pol[0] = DAG_NULL; \
		DAG_tmp_DAG_pol[A]->pol[1] = DAG_NULL; \
		DAG_tmp_DAG_pol[A]->pol[2] = DAG_NULL; \
	} \
	if (pol == POL_NEG) \
		DAG_tmp_DAG_pol[A]->pol[0] = B; \
	else if (pol == POL_POS) \
		DAG_tmp_DAG_pol[A]->pol[1] = B; \
	else \
		DAG_tmp_DAG_pol[A]->pol[2] = B;

static void
DAG_tmp_reset_DAG_pol(TDAG DAG)
{
	unsigned i;
	if (!DAG_tmp_DAG_pol[DAG]) return;
	if (DAG_tmp_DAG_pol[DAG]->pol[0]) DAG_free(DAG_tmp_DAG_pol[DAG]->pol[0]);
	if (DAG_tmp_DAG_pol[DAG]->pol[1]) DAG_free(DAG_tmp_DAG_pol[DAG]->pol[1]);
	if (DAG_tmp_DAG_pol[DAG]->pol[2]) DAG_free(DAG_tmp_DAG_pol[DAG]->pol[2]);
	free(DAG_tmp_DAG_pol[DAG]);
	DAG_tmp_DAG_pol[DAG] = NULL;
	for (i = 0; i < DAG_arity(DAG); ++i) DAG_tmp_reset_DAG_pol(DAG_arg(DAG, i));
}

void
set_NFs(TDAG DAG)
{
	TDAG tmp_DAG = DAG_dup(NNF(DAG, true));
	TDAG prenex_DAG = DAG_dup(prenex(tmp_DAG));
	DAG_free(tmp_DAG);

	/* Computes a set of sets representing CNF of given DAG */
	DAG_tmp_reserve();
	count_CNF_nodes(DAG_arg_last(prenex_DAG));
	/* Whether it exploded */
	bool cnf_explosion = !cnf_count[DAG_arg_last(prenex_DAG)] ||
		!cnf_count_clauses(DAG_arg_last(prenex_DAG));
	DAG_tmp_reset_cnf_count(DAG_arg_last(prenex_DAG));
	DAG_tmp_release();
	if (cnf_explosion) {
		DAG_free(prenex_DAG);
		return;
	}

	DAG_tmp_reserve();
	set_CNF_rec(DAG_arg_last(prenex_DAG));

	Tstack_DAG tmp;
	Tstack_DAGstack cnf;
	stack_INIT(cnf);
	for (unsigned i = 0; i < stack_size(cnf_of[DAG_arg_last(prenex_DAG)]); ++i) {
		stack_COPY(tmp, stack_get(cnf_of[DAG_arg_last(prenex_DAG)], i));
		stack_apply(tmp, DAG_dup);
		stack_push(cnf, tmp);
	}

	DAG_prop_set(DAG, DAG_PROP_CNF, &cnf);
	DAG_tmp_reset_cnf(DAG_arg_last(prenex_DAG));
	DAG_tmp_release();

	Tstack_DAG quantifier_args = NULL;
	if (proof_on) stack_INIT_s(quantifier_args, DAG_arity(DAG));
	/* TODO: only one DAG_tmp for all... have a set_fvars_array/stack? */
	/* Sets bound variables of all DAGs in CNF */
	for (unsigned i = 0; i < stack_size(cnf); ++i) {
		for (unsigned j = 0; j < stack_size(stack_get(cnf, i)); ++j)
			set_fvars(stack_get(stack_get(cnf, i), j));

		/* Create the lemma implying the CNF */
		if (proof_on) {
			tmp = stack_get(cnf,i);
			TDAG cnf_or = stack_size(tmp)>1 ? DAG_new_stack(CONNECTOR_OR, tmp) : stack_get(tmp, 0);
			set_fvars(cnf_or);

			/* The clause is ground and the lemma should not be used */
			if (!get_fvars(cnf_or))
				continue;

			stack_merge(quantifier_args, get_fvars(cnf_or));
			stack_push(quantifier_args, cnf_or);

			TDAG cnf_q = DAG_new_stack(QUANTIFIER_FORALL, quantifier_args);
			stack_reset(quantifier_args);
			TDAG imp = DAG_dup(DAG_or2(DAG_not(DAG), cnf_q));

			Tproof_step step = proof_step_new();
			/* Placeholder ! */
			step->type = ps_type_qnt_cnf;
			proof_step_add_DAG(step, imp);
			Tproof proof = steps_push(step);
			proof_set_lemma_id(imp, proof);
		}
	}
	if (proof_on) stack_free(quantifier_args);

	DAG_free(prenex_DAG);
}
