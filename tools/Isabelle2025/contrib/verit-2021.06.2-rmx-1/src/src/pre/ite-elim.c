/*
  --------------------------------------------------------------
  ite (functions) removing in formulas
  --------------------------------------------------------------
*/

#include "pre/ite-elim.h"

#include "proof/proof.h"
#include "symbolic/DAG-subst.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"
#include "utils/nonce.h"
#include "utils/statistics.h"
#include "utils/types.h"

#include <assert.h>
#include <stdlib.h>

/* #define DEBUG_ITE_ELIM */
/* #define ITE_STAT */

#ifdef ITE_STAT
static int stat_ite = 0;
#endif

typedef struct TDAG_ite
{
	TDAG ite, cst;
} TDAG_ite;

TSstack(_DAG_ite, TDAG_ite);

static Tstack_DAG_ite ite_terms;

static Tnonce ite_nonce;
/*
  --------------------------------------------------------------
  Creating const for ite terms
  --------------------------------------------------------------
*/

static inline TDAG
ite_get_cst(TDAG DAG)
{
#ifdef ITE_STAT
	stat_inc(Pstats, stat_ite);
#endif
	nonce_next(&ite_nonce);
	return DAG_dup(
		DAG_new_args(DAG_symb_new(ite_nonce.buffer, 0, DAG_sort(DAG)), NULL));
}

/*
  --------------------------------------------------------------
  Inspecting formulas for ite terms
  --------------------------------------------------------------
*/

/**
   \brief adds src to ite_terms if top symbol is ite
   \param src the term to check */
static bool
get_ite_terms_cont(TDAG src)
{
	return !binder(DAG_symb(src)) && DAG_symb(src) != APPLY_LAMBDA;
}

bool get_ite_terms_found = false;

/**
   \brief collect all ite terms in DAG
   \param DAG the term in which to collect ite terms
   \remark sets get_ite_terms_found to true if contains (old or new) ites */
static void
get_ite_terms(TDAG src)
{
	unsigned i;
	if (DAG_symb(src) == FUNCTION_ITE) {
		get_ite_terms_found = true;
		if (DAG_tmp_bool[src]) return;
		stack_inc(ite_terms);
		stack_top(ite_terms).ite = DAG_dup(src);
		stack_top(ite_terms).cst = DAG_NULL;
	}
	if (DAG_tmp_bool[src]) return;
	DAG_tmp_bool[src] = true;
	if (binder(DAG_symb(src)) || DAG_symb(src) == APPLY_LAMBDA) return;
	for (i = 0; i < DAG_arity(src); i++) get_ite_terms(DAG_arg(src, i));
}

/*
  --------------------------------------------------------------
  Public functions
  --------------------------------------------------------------
*/

void
ite_elim_init(void)
{
#ifdef ITE_STAT
	stat_ite = stat_new(Pstats, "ite", "Introduced ite terms", "%4d");
#endif
	stack_INIT(ite_terms);
	nonce_init(&ite_nonce, "@ite");
}

void
ite_elim_done(void)
{
	unsigned i;
	nonce_free(&ite_nonce);
	for (i = 0; i < stack_size(ite_terms); i++) {
		DAG_free(stack_get(ite_terms, i).ite);
		DAG_free(stack_get(ite_terms, i).cst);
	}
	stack_free(ite_terms);
}

TDAG
ite_elim(TDAG DAG)
{
	TDAG result;
	Tstack_DAG args;
	unsigned i, n;

	/* Avoid collecting again old ite terms */
	n = stack_size(ite_terms);
	DAG_tmp_reserve();
	for (i = 0; i < n; ++i) DAG_tmp_bool[stack_get(ite_terms, i).ite] = true;

	/* Get ite-term list */
	get_ite_terms_found = false;
	get_ite_terms(DAG);
	DAG_tmp_reset_bool(DAG);
	for (i = 0; i < n; ++i) DAG_tmp_bool[stack_get(ite_terms, i).ite] = false;
	DAG_tmp_release();
	if (!get_ite_terms_found) return DAG_dup(DAG);
	DAG_tmp_reserve();

	/* Set DAG_tmp_DAG for the old ites */
	for (i = 0; i < n; i++)
		DAG_tmp_DAG[stack_get(ite_terms, i).ite] = stack_get(ite_terms, i).cst;

	/* Get a substitute term for each of the new ites */
	for (; i < stack_size(ite_terms); ++i) {
		TDAG ite = stack_get(ite_terms, i).ite;
		TDAG tmp = ite_get_cst(ite);
		/* PF 2 steps required: ite_get_cst may change DAG_tmp_DAG */
		DAG_tmp_DAG[ite] = tmp;
		assert(!stack_get(ite_terms, i).cst);
		stack_get(ite_terms, i).cst = tmp;
	}
	/* Get formula with substitute terms */
	if (!DAG_tmp_subst_cond(DAG, get_ite_terms_cont))
		my_error("ite_eliminate: internal error\n");

	/* Notify proof module of what the introduced constants are naming */
	if (proof_on) notify_ites((Tstack_DAG_assoc)ite_terms, n);

	/* Build defining formula for substitute terms */
	if (n < stack_size(ite_terms)) {
		stack_INIT_s(args, 1 + stack_size(ite_terms) - n);
		stack_push(args, DAG_tmp_DAG[DAG]);
		for (i = n; i < stack_size(ite_terms); ++i) {
			TDAG ite = stack_get(ite_terms, i).ite;
			TDAG condition, then_case, else_case;
			DAG_tmp_subst_cond(DAG_arg(ite, 0), get_ite_terms_cont);
			condition = DAG_tmp_DAG[DAG_arg(ite, 0)];
			DAG_tmp_subst_cond(DAG_arg(ite, 1), get_ite_terms_cont);
			then_case = DAG_tmp_DAG[DAG_arg(ite, 1)];
			DAG_tmp_subst_cond(DAG_arg(ite, 2), get_ite_terms_cont);
			else_case = DAG_tmp_DAG[DAG_arg(ite, 2)];
			ite = DAG_tmp_DAG[ite];
			ite = DAG_ite(condition, DAG_eq(ite, then_case), DAG_eq(ite, else_case));
			stack_push(args, ite);
		}
		result = DAG_dup(DAG_new_stack(CONNECTOR_AND, args));
		stack_free(args);
	} else
		result = DAG_dup(DAG_tmp_DAG[DAG]);

	/* Clean */
	DAG_tmp_reset_DAG(DAG);
	/* Set DAG_tmp_DAG for the old ites */
	for (i = 0; i < stack_size(ite_terms); i++)
		DAG_tmp_DAG[stack_get(ite_terms, i).ite] = DAG_NULL;
	DAG_tmp_release();
#ifdef DEBUG_ITE_ELIM
	may_DAG_message(
		"Before ite elimination\n%D\nAfter ite elimination\n%D\n", DAG, result);
#endif
	return result;
}
