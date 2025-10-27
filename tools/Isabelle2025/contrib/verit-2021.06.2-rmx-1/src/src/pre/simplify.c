/*
  --------------------------------------------------------------
  Module for simplifying formulas and terms
  --------------------------------------------------------------
*/

#include "pre/simplify.h"

#include "pre/simp-node-proof.h"
#include "symbolic/DAG-arith.h"
#include "symbolic/DAG-flag.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "symbolic/context-recursion.h"
#include "symbolic/recursion.h"
#include "utils/general.h"
#include "utils/statistics.h"
#include "utils/veriT-qsort.h"

#include <gmp.h>
#include <stdlib.h>

/* #define DEBUG_SIMPLIFY_AC */
/* #define SIMP_STAT */

#ifdef SIMP_STAT
#define STAT_INC(A, B) stats_easy_inc(#A, "simp: " B, "%4d")
#else
#define STAT_INC(A, B) ;
#endif

#define mpz_is_zero(A) (mpz_cmp_ui(A, 0u) == 0)
#define mpq_is_int(A) (mpz_cmp_ui(mpq_denref(A), 1u) == 0)
#define mpq_is_zero(A) (mpz_cmp_ui(mpq_numref(A), 0u) == 0)
#define mpq_is_one(A) ((mpz_cmp_ui(mpq_numref(A), 1u) == 0) && mpq_is_int(A))

mpq_t mpq_tmp1, mpq_tmp2;
mpz_t mpz_tmp1, mpz_tmp2;

static TDAG simp_node_reduce(TDAG src);
static TDAG simplify_and(TDAG src);

static inline bool
DAG_opposite(TDAG DAG0, TDAG DAG1)
{
	return (DAG_symb(DAG0) == CONNECTOR_NOT && DAG_arg0(DAG0) == DAG1) ||
		(DAG_symb(DAG1) == CONNECTOR_NOT && DAG_arg0(DAG1) == DAG0);
}

/*
  --------------------------------------------------------------
  Helper functions for simplification
  --------------------------------------------------------------
*/

/**
   \brief flatten terms with same top symbol
   \param src the term
   \remark useful for flattening AND, OR, +, ... */
static TDAG
simplify_flattening(TDAG src)
{
	Tsymb symb = DAG_symb(src);
	unsigned i, j;
	TDAG* PDAG;
	TDAG dest;
	for (j = 0, i = 0; i < DAG_arity(src); i++)
		j += (DAG_symb(DAG_arg(src, i)) == symb) ? DAG_arity(DAG_arg(src, i)) : 1;
	if (j == DAG_arity(src)) return src;
	MY_MALLOC(PDAG, j * sizeof(TDAG*));
	for (j = 0, i = 0; i < DAG_arity(src); i++)
		if (DAG_symb(DAG_arg(src, i)) == symb) {
			memcpy(
				PDAG + j, DAG_args(DAG_arg(src, i)),
				DAG_arity(DAG_arg(src, i)) * sizeof(TDAG));
			j += DAG_arity(src);
		} else
			PDAG[j++] = DAG_arg(src, i);
	dest = DAG_dup(DAG_new(symb, j, PDAG));
	return dest;
}

/**
   \brief eliminate every direct subDAG of src equal to sub
   \param src the term
   \param sub the subterm to eliminate
   \remark avoids building (or ), (and ) if zero-ary
   \remark useful for eliminating TRUE in conjunction, FALSE in disjunction,
   0 in sum, 1 in products */
TDAG
simplify_neutral(TDAG src, TDAG sub)
{
	unsigned i;
	TDAG dest;
	Tstack_DAG DAGs;
	dest = src;
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(src); i++)
		if (DAG_arg(src, i) != sub) stack_push(DAGs, DAG_arg(src, i));
	if (stack_is_empty(DAGs)) {
		STAT_INC(neutral, "a O b O c, c -> a O b");
		assert(DAG_symb(src) == CONNECTOR_AND || DAG_symb(src) == CONNECTOR_OR);
		dest =
			DAG_symb(src) == CONNECTOR_AND ? DAG_dup(DAG_TRUE) : DAG_dup(DAG_FALSE);
		DAG_free(src);
	} else if (stack_size(DAGs) != DAG_arity(src)) {
		STAT_INC(neutral, "a O b O c, c -> a O b");
		dest = DAG_dup(DAG_new_stack(DAG_symb(src), DAGs));
		DAG_free(src);
	}
	stack_free(DAGs);
	return dest;
}

/**
   \brief find if subterm is a direct argument (or subterm) of src
   \remark useful for simplifying TRUE in disjunction, FALSE in conjunction,
   ZERO in products */
static bool
find_arg(TDAG src, TDAG subterm)
{
	unsigned i;
	for (i = 0; i < DAG_arity(src); i++)
		if (DAG_arg(src, i) == subterm) return true;
	return false;
}

/**
   \brief eliminate duplicate args and sort
   \param[in] src the DAG
   \return the simplified DAG
   \remark destructive
   \remark Useful for A AND A AND B -> A AND B and A OR A OR B -> A OR B */
static TDAG
simplify_ACidem(TDAG src)
{
	unsigned i, j;
	unsigned n = DAG_arity(src);
	TDAG dest, *PDAG;
	if (n < 2) return src;
	for (i = 0, j = 0; i < n; i++)
		if (!DAG_misc(DAG_arg(src, i))) {
			DAG_misc_set(DAG_arg(src, i), 1);
			j++;
		}
	if (j == n) {
		for (i = 0; i < n; i++) DAG_misc_set(DAG_arg(src, i), 0);
		return src;
	}
	MY_MALLOC(PDAG, j * sizeof(TDAG));
	for (i = 0, j = 0; i < n; i++)
		if (DAG_misc(DAG_arg(src, i))) {
			PDAG[j] = DAG_arg(src, i);
			DAG_misc_set(PDAG[j], 0);
			j++;
		}
	dest = DAG_dup(DAG_new(DAG_symb(src), j, PDAG));
	DAG_free(src);
	return dest;
}

/**
   \brief find if there is a complementary pair of subterms
   \param[in] src the DAG
   \return true if there exists a complementary pair of subterms
   \pre arguments should be sorted, e.g. with simplify_ACidem
   \remark Useful for A and not A, A or not A */
static bool
find_comp(TDAG src)
{
	unsigned i;
	bool comp = false;
	for (i = 0; i < DAG_arity(src); i++)
		if (DAG_symb(DAG_arg(src, i)) == CONNECTOR_NOT)
			DAG_misc(DAG_arg0(DAG_arg(src, i))) |= 2;
		else
			DAG_misc(DAG_arg(src, i)) |= 4;
	for (i = 0; i < DAG_arity(src); i++)
		if (DAG_symb(DAG_arg(src, i)) == CONNECTOR_NOT) {
			comp |= (DAG_misc(DAG_arg0(DAG_arg(src, i))) == 6);
			DAG_misc_set(DAG_arg0(DAG_arg(src, i)), 0);
		} else {
			comp |= (DAG_misc(DAG_arg(src, i)) == 6);
			DAG_misc_set(DAG_arg(src, i), 0);
		}
	return comp;
}

/*
  --------------------------------------------------------------
  Functional ITE simplifications
  --------------------------------------------------------------
*/

#define IF(DAG) DAG_arg(DAG, 0)
#define THEN(DAG) DAG_arg(DAG, 1)
#define ELSE(DAG) DAG_arg(DAG, 2)

static TDAG
simplify_fite(TDAG src)
{
	TDAG dest;
	assert(DAG_symb(src) == FUNCTION_ITE);
	/* ITE TRUE T1 T2 --> T1 */
	if (IF(src) == DAG_TRUE) {
		STAT_INC(fite, "fite");
		dest = DAG_dup(THEN(src));
		DAG_free(src);
		return dest;
	}
	/* ITE FALSE T1 T2 --> T1 */
	if (IF(src) == DAG_FALSE) {
		STAT_INC(fite, "fite");
		dest = DAG_dup(ELSE(src));
		DAG_free(src);
		return dest;
	}
	/* ITE C T1 T1 --> T1 */
	if (THEN(src) == ELSE(src)) {
		STAT_INC(fite, "fite");
		dest = DAG_dup(THEN(src));
		DAG_free(src);
		return dest;
	}
	/* ITE NOT C T1 T2 --> ITE C T2 T1 */
	if (DAG_symb(IF(src)) == CONNECTOR_NOT) {
		STAT_INC(fite, "fite");
		dest = DAG_dup(DAG_new_args(
			FUNCTION_ITE, DAG_arg0(IF(src)), ELSE(src), THEN(src), DAG_NULL));
		DAG_free(src);
		return simplify_fite(dest);
	}
	/* ITE(C, ITE(C, T1, T2), T3) --> ITE(C, T1, T3) */
	if (DAG_symb(THEN(src)) == FUNCTION_ITE && IF(src) == IF(THEN(src))) {
		STAT_INC(ite_ite, "nested ite");
		dest = DAG_dup(DAG_new_args(
			FUNCTION_ITE, IF(src), THEN(THEN(src)), ELSE(src), DAG_NULL));
		DAG_free(src);
		return simplify_fite(dest);
	}
	/* ITE(C, T1, ITE(C, T2, T3)) --> ITE(C, T1, T3) */
	if (DAG_symb(ELSE(src)) == FUNCTION_ITE && IF(src) == IF(ELSE(src))) {
		STAT_INC(ite_ite, "nested ite");
		dest = DAG_dup(DAG_new_args(
			FUNCTION_ITE, IF(src), THEN(src), ELSE(ELSE(src)), DAG_NULL));
		DAG_free(src);
		return simplify_fite(dest);
	}
	return src;
}

static TDAG
simplify_ite(TDAG src)
{
	TDAG dest, tmp;
	assert(DAG_symb(src) == CONNECTOR_ITE);
	/* IF TRUE THEN A ELSE B --> A */
	if (IF(src) == DAG_TRUE) {
		STAT_INC(ite, "boolean ite");
		dest = DAG_dup(THEN(src));
		DAG_free(src);
		return dest;
	}
	/* IF FALSE THEN A ELSE B --> B */
	if (IF(src) == DAG_FALSE) {
		STAT_INC(ite, "boolean ite");
		dest = DAG_dup(ELSE(src));
		DAG_free(src);
		return dest;
	}
	/* IF C THEN TRUE ELSE FALSE --> C */
	if (THEN(src) == DAG_TRUE && ELSE(src) == DAG_FALSE) {
		STAT_INC(ite, "boolean ite");
		dest = DAG_dup(IF(src));
		DAG_free(src);
		return dest;
	}
	/* IF C THEN FALSE ELSE TRUE --> NOT C */
	if (THEN(src) == DAG_FALSE && ELSE(src) == DAG_TRUE) {
		STAT_INC(ite, "boolean ite");
		dest = DAG_dup(DAG_not(IF(src)));
		DAG_free(src);
		return simp_node_reduce(dest);
	}
	/* IF C THEN A ELSE A --> A */
	if (THEN(src) == ELSE(src)) {
		STAT_INC(ite, "boolean ite");
		dest = DAG_dup(THEN(src));
		DAG_free(src);
		return dest;
	}
	/* IF C THEN TRUE ELSE B --> C OR B */
	if (THEN(src) == DAG_TRUE) {
		STAT_INC(ite, "boolean ite");
		dest = DAG_dup(DAG_or2(IF(src), ELSE(src)));
		DAG_free(src);
		return simp_node_reduce(dest);
	}
	/* IF C THEN A ELSE FALSE --> C AND A */
	if (ELSE(src) == DAG_FALSE) {
		STAT_INC(ite, "boolean ite");
		dest = DAG_dup(DAG_and2(IF(src), THEN(src)));
		DAG_free(src);
		return simplify_and(dest);
	}
	/* IF C THEN FALSE ELSE B --> NOT C AND B */
	if (THEN(src) == DAG_FALSE) {
		STAT_INC(ite, "boolean ite");
		tmp = simp_node_reduce(DAG_dup(DAG_not(IF(src))));
		dest = DAG_dup(DAG_and2(tmp, ELSE(src)));
		DAG_free(tmp);
		DAG_free(src);
		return simplify_and(dest);
	}
	/* IF C THEN A ELSE TRUE --> NOT C OR A */
	if (ELSE(src) == DAG_TRUE) {
		STAT_INC(ite, "boolean ite");
		tmp = simp_node_reduce(DAG_dup(DAG_not(IF(src))));
		dest = DAG_dup(DAG_or2(tmp, THEN(src)));
		DAG_free(tmp);
		DAG_free(src);
		return simp_node_reduce(dest);
	}
	/* IF NOT C THEN A ELSE B --> IF C THEN B ELSE A */
	if (DAG_symb(IF(src)) == CONNECTOR_NOT) {
		STAT_INC(ite, "boolean ite");
		dest = DAG_dup(DAG_ite(DAG_arg0(IF(src)), ELSE(src), THEN(src)));
		DAG_free(src);
		return dest;
	}
	/* ITE(C, ITE(C, T1, T2), T3) --> ITE(C, T1, T3) */
	if (DAG_symb(THEN(src)) == CONNECTOR_ITE && IF(src) == IF(THEN(src))) {
		STAT_INC(ite_ite, "nested ite");
		dest = DAG_dup(DAG_ite(IF(src), THEN(THEN(src)), ELSE(src)));
		DAG_free(src);
		return dest;
	}
	/* ITE(C, T1, ITE(C, T2, T3)) --> ITE(C, T1, T3) */
	if (DAG_symb(ELSE(src)) == CONNECTOR_ITE && IF(src) == IF(ELSE(src))) {
		STAT_INC(ite_ite, "nested ite");
		dest = DAG_dup(DAG_ite(IF(src), THEN(src), ELSE(ELSE(src))));
		DAG_free(src);
		return dest;
	}
	return src;
}

static TDAG
simplify_eq(TDAG src)
{
	TDAG dest;
	assert(DAG_symb(src) == PREDICATE_EQ && DAG_arity(src) == 2);
	if (DAG_is_number(DAG_arg0(src)) && DAG_is_number(DAG_arg1(src))) {
		STAT_INC(eq, "eq");
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
		dest = DAG_dup(mpq_equal(mpq_tmp1, mpq_tmp2) ? DAG_TRUE : DAG_FALSE);
		DAG_free(src);
		return dest;
	}
	if (DAG_arg0(src) == DAG_arg1(src)) {
		DAG_free(src);
		return DAG_dup(DAG_TRUE);
	}
	return src;
}

static TDAG
simplify_lift_ite(TDAG src)
/* PF apply simple case where ite function can be lifted to
   ite connector
   e1 = ite (c, e2, e3) --> if c then e1 = e2 else e1 = e3
   actually, this leads to explosions in some cases
   turned of
*/
{
	return simplify_eq(src);
#if 0
  TDAG dest, tmp1, tmp2, tmp3, tmp4;
  if (DAG_symb(src) != PREDICATE_EQ ||
      (DAG_symb(DAG_arg0(src)) != FUNCTION_ITE &&
       DAG_symb(DAG_arg1(src)) != FUNCTION_ITE) ||
      (DAG_symb(DAG_arg0(src)) == FUNCTION_ITE &&
       DAG_symb(DAG_arg1(src)) == FUNCTION_ITE))
    return src;
  STAT_INC(itelift, "safe lifting (some) ite from terms to formulas");
  tmp1 = DAG_arg0(src);
  tmp2 = DAG_arg1(src);
  if (DAG_symb(DAG_arg0(src)) != FUNCTION_ITE)
    SWAP(tmp1, tmp2);
  tmp3 = simplify_node(DAG_dup(DAG_eq(DAG_arg(tmp1, 1), tmp2)));
  tmp4 = simplify_node(DAG_dup(DAG_eq(DAG_arg(tmp1, 2), tmp2)));
  dest = DAG_dup(DAG_ite(DAG_arg(tmp1, 0), tmp3, tmp4));
  DAG_free(src);
  DAG_free(tmp3);
  DAG_free(tmp4);
  return simplify_node(dest);
#endif
}

#undef IF
#undef ELSE
#undef THEN

/*
  --------------------------------------------------------------
  Arithmetic simplifications
  --------------------------------------------------------------
*/

/**  Rewrite (/ c d) where c and d are numbers */
static TDAG
simplify_div(TDAG src)
{
	TDAG dest;
	assert(DAG_arity(src) == 2);
	assert(DAG_sort(src) == SORT_INTEGER || DAG_sort(src) == SORT_REAL);
	if (DAG_arg0(src) == DAG_arg1(src)) {
		DAG_free(src);
		/* Which type of division is this? */
		if (DAG_sort(src) == SORT_REAL) {
			mpq_set_ui(mpq_tmp1, 1u, 1u);
			return DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		} else
			return DAG_dup(DAG_new_integer(1));
	}
	if (!DAG_is_number(DAG_arg1(src))) return src;
	DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
	/* Do nothing in the case of divison by zero */
	if (mpq_is_zero(mpq_tmp2)) return src;
	if (mpq_is_one(mpq_tmp2)) {
		dest = DAG_dup(DAG_arg0(src));
		DAG_free(src);
		return dest;
	}
	if (!DAG_is_number(DAG_arg0(src))) return src;

	if (DAG_sort(src) == SORT_INTEGER) {
		assert(DAG_is_integer(DAG_arg0(src)) && DAG_is_integer(DAG_arg1(src)));
		DAG_mpz_set(mpz_tmp1, DAG_arg0(src));
		DAG_mpz_set(mpz_tmp2, DAG_arg1(src));
		mpz_fdiv_q(mpz_tmp1, mpz_tmp1, mpz_tmp2);
		dest = DAG_dup(DAG_new_integer_mpz(mpz_tmp1));
	} else {
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		mpq_div(mpq_tmp1, mpq_tmp1, mpq_tmp2);
		dest = DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
	}
	DAG_free(src);
	return dest;
}

static TDAG
simplify_prod(TDAG src)
{
	bool is_one;
	unsigned i, j = 0, numbers = 0;
	TDAG dest;
	TDAG* PDAG;
	assert(DAG_arity(src) > 1);
	assert(DAG_sort(src) == SORT_INTEGER || DAG_sort(src) == SORT_REAL);
	mpz_set_ui(mpz_tmp1, 1u);
	mpq_set_ui(mpq_tmp1, 1u, 1u);
	for (i = 0; i < DAG_arity(src); i++) {
		if (!DAG_is_number(DAG_arg(src, i))) continue;
		numbers++;
		DAG_mpq_set(mpq_tmp2, DAG_arg(src, i));
		mpq_mul(mpq_tmp1, mpq_tmp1, mpq_tmp2);
	}
	if (numbers == 0) return src;
	if (
		numbers == 1 && DAG_is_number(DAG_arg(src, 0)) && !mpq_is_zero(mpq_tmp1) &&
		!mpq_is_one(mpq_tmp1))
		return src;
	if (numbers == DAG_arity(src) || mpq_is_zero(mpq_tmp1)) {
		DAG_free(src);
		if (DAG_sort(src) == SORT_REAL)
			return DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		else
			return DAG_dup(DAG_new_integer_mpz(mpq_numref(mpq_tmp1)));
	}
	is_one = mpq_is_one(mpq_tmp1);
	if (is_one && DAG_arity(src) - numbers == 1)
		for (i = 0; i < DAG_arity(src); ++i)
			if (!DAG_is_number(DAG_arg(src, i))) {
				dest = DAG_dup(DAG_arg(src, i));
				DAG_free(src);
				return dest;
			}
	MY_MALLOC(PDAG, (DAG_arity(src) - numbers + (is_one ? 0 : 1)) * sizeof(TDAG));
	if (!is_one) {
		if (DAG_sort(src) == SORT_REAL)
			PDAG[j++] = DAG_new_rational_mpq(mpq_tmp1);
		else
			PDAG[j++] = DAG_new_integer_mpz(mpq_numref(mpq_tmp1));
	}
	for (i = 0; i < DAG_arity(src); ++i)
		if (!DAG_is_number(DAG_arg(src, i))) PDAG[j++] = DAG_arg(src, i);
	assert(j - (is_one ? 0 : 1) + numbers == DAG_arity(src));
	dest = DAG_dup(
		DAG_new(DAG_symb(src), DAG_arity(src) - numbers + (is_one ? 0 : 1), PDAG));
	DAG_free(src);
	return dest;
}

/**  Rewrite (- c) where c is a number */
static TDAG
simplify_unary_minus(TDAG src)
{
	TDAG dest;
	if (DAG_arity(src) != 1)
		my_error("simplify_unary_minus: strange arity for src\n");
	if (DAG_is_integer(DAG_arg0(src))) {
		DAG_mpz_set(mpz_tmp1, DAG_arg0(src));
		mpz_neg(mpz_tmp1, mpz_tmp1);
		dest = DAG_dup(DAG_new_integer_mpz(mpz_tmp1));
		DAG_free(src);
		return dest;
	}
	if (DAG_is_rational(DAG_arg0(src))) {
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		mpq_neg(mpq_tmp1, mpq_tmp1);
		dest = DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		DAG_free(src);
		return dest;
	}
	if (unary_minus(DAG_symb(DAG_arg0(src)))) {
		dest = DAG_dup(DAG_arg0(DAG_arg0(src)));
		DAG_free(src);
		return dest;
	}
	return src;
}

static TDAG
simplify_minus(TDAG src)
{
	TDAG dest;
	assert(DAG_arity(src) == 2);
	assert(DAG_sort(src) == SORT_INTEGER || DAG_sort(src) == SORT_REAL);
	if (DAG_arg0(src) == DAG_arg1(src)) {
		DAG_free(src);
		if (DAG_sort(src) == SORT_REAL) {
			mpq_set_ui(mpq_tmp1, 0u, 1u);
			return DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		} else
			return DAG_dup(DAG_new_integer(0));
	}
	if (DAG_is_number(DAG_arg0(src)) && DAG_is_number(DAG_arg1(src))) {
		if (DAG_is_integer(DAG_arg0(src)) && DAG_is_integer(DAG_arg1(src))) {
			DAG_mpz_set(mpz_tmp1, DAG_arg0(src));
			DAG_mpz_set(mpz_tmp2, DAG_arg1(src));
			mpz_sub(mpz_tmp1, mpz_tmp1, mpz_tmp2);
			dest = DAG_dup(DAG_new_integer_mpz(mpz_tmp1));
			DAG_free(src);
			return dest;
		}
		assert(!DAG_is_integer(DAG_arg0(src) && !DAG_is_integer(DAG_arg1(src))));
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
		mpq_sub(mpq_tmp1, mpq_tmp1, mpq_tmp2);
		dest = DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		DAG_free(src);
		return dest;
	}
	if (DAG_is_number(DAG_arg1(src))) {
		DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
		if (mpq_is_zero(mpq_tmp2)) {
			dest = DAG_dup(DAG_arg0(src));
			DAG_free(src);
			return dest;
		}
	}
	if (DAG_is_number(DAG_arg0(src))) {
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		if (mpq_is_zero(mpq_tmp1)) {
			dest = DAG_dup(DAG_new_unary(FUNCTION_UNARY_MINUS, DAG_arg1(src)));
			DAG_free(src);
			return dest;
		}
	}
	return src;
}

static TDAG
simplify_sum(TDAG src)
{
	bool is_zero;
	unsigned i, j = 0, numbers = 0;
	TDAG dest, *PDAG;
	assert(DAG_arity(src) > 1);
	assert(DAG_sort(src) == SORT_INTEGER || DAG_sort(src) == SORT_REAL);
	mpz_set_ui(mpz_tmp1, 0u);
	mpq_set_ui(mpq_tmp1, 0u, 1u);
	for (i = 0; i < DAG_arity(src); i++)
		if (DAG_is_integer(DAG_arg(src, i))) {
			numbers++;
			DAG_mpz_set(mpz_tmp2, DAG_arg(src, i));
			mpz_add(mpz_tmp1, mpz_tmp1, mpz_tmp2);
		} else if (DAG_is_rational(DAG_arg(src, i))) {
			numbers++;
			DAG_mpq_set(mpq_tmp2, DAG_arg(src, i));
			mpq_add(mpq_tmp1, mpq_tmp1, mpq_tmp2);
		}
	if (numbers == 0) return src;
	if (
		numbers == 1 && DAG_is_number(DAG_arg(src, 0)) &&
		!(mpz_is_zero(mpz_tmp1) && mpq_is_zero(mpq_tmp1)))
		return src;
	if (numbers == DAG_arity(src)) {
		DAG_free(src);
		if (DAG_sort(src) == SORT_INTEGER) {
			assert(mpq_is_int(mpq_tmp1));
			mpz_add(mpz_tmp1, mpz_tmp1, mpq_numref(mpq_tmp1));
			return DAG_dup(DAG_new_integer_mpz(mpz_tmp1));
		} else {
			mpq_set_z(mpq_tmp2, mpz_tmp1);
			mpq_add(mpq_tmp1, mpq_tmp1, mpq_tmp2);
			return DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		}
	}
	is_zero = mpq_is_zero(mpq_tmp1) && (mpz_cmp_ui(mpz_tmp1, 0) == 0);
	if (is_zero && DAG_arity(src) - numbers == 1)
		for (i = 0; i < DAG_arity(src); ++i)
			if (!DAG_is_number(DAG_arg(src, i))) {
				dest = DAG_dup(DAG_arg(src, i));
				DAG_free(src);
				return dest;
			}
	assert(DAG_arity(src) > numbers);
	MY_MALLOC(
		PDAG, (DAG_arity(src) - numbers + (is_zero ? 0 : 1)) * sizeof(TDAG));
	if (!is_zero) {
		if (DAG_sort(src) == SORT_INTEGER) {
			assert(mpq_is_zero(mpq_tmp1));
			mpz_add(mpz_tmp1, mpz_tmp1, mpq_numref(mpq_tmp1));
			PDAG[j++] = DAG_new_integer_mpz(mpz_tmp1);
		} else {
			mpq_set_z(mpq_tmp2, mpz_tmp1);
			mpq_add(mpq_tmp1, mpq_tmp1, mpq_tmp2);
			PDAG[j++] = DAG_new_rational_mpq(mpq_tmp1);
		}
	}
	for (i = 0; i < DAG_arity(src); ++i)
		if (!DAG_is_number(DAG_arg(src, i))) PDAG[j++] = DAG_arg(src, i);
	assert(j - (is_zero ? 0 : 1) + numbers == DAG_arity(src));
	dest = DAG_dup(
		DAG_new(DAG_symb(src), DAG_arity(src) - numbers + (is_zero ? 0 : 1), PDAG));
	DAG_free(src);
	return dest;
}

static TDAG
simplify_less(TDAG src)
{
	TDAG dest;
	assert(DAG_symb(src) == PREDICATE_LESS && DAG_arity(src) == 2);
	if (DAG_is_number(DAG_arg0(src)) && DAG_is_number(DAG_arg1(src))) {
		STAT_INC(less, "less");
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
		dest = DAG_dup(mpq_cmp(mpq_tmp1, mpq_tmp2) < 0 ? DAG_TRUE : DAG_FALSE);
		DAG_free(src);
		return dest;
	}
	if (DAG_arg0(src) == DAG_arg1(src)) {
		DAG_free(src);
		return DAG_dup(DAG_FALSE);
	}
	return src;
}

static TDAG
simplify_leq(TDAG src)
{
	TDAG dest;
	assert(DAG_symb(src) == PREDICATE_LEQ && DAG_arity(src) == 2);
	if (DAG_is_number(DAG_arg0(src)) && DAG_is_number(DAG_arg1(src))) {
		STAT_INC(less, "less");
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
		dest = DAG_dup(mpq_cmp(mpq_tmp1, mpq_tmp2) <= 0 ? DAG_TRUE : DAG_FALSE);
		DAG_free(src);
		return dest;
	}
	if (DAG_arg0(src) == DAG_arg1(src)) {
		DAG_free(src);
		return DAG_dup(DAG_TRUE);
	}
	return src;
}

/**
   Rewrite t >= t' to t' <= t
   Rewrite t < t' to \neg t' <= t
   Rewrite t > t' to \neg t <= t' */
static TDAG
rewrite_arith_compare(TDAG src)
{
	TDAG dest;
	if (DAG_symb(src) == PREDICATE_GREATEREQ) {
		dest = DAG_dup(DAG_new_binary(PREDICATE_LEQ, DAG_arg1(src), DAG_arg0(src)));
		DAG_free(src);
		return dest;
	}
	if (DAG_symb(src) == PREDICATE_LESS) {
		dest = DAG_dup(
			DAG_not(DAG_new_binary(PREDICATE_LEQ, DAG_arg1(src), DAG_arg0(src))));
		DAG_free(src);
		return dest;
	}
	if (DAG_symb(src) == PREDICATE_GREATER) {
		dest = DAG_dup(
			DAG_not(DAG_new_binary(PREDICATE_LEQ, DAG_arg0(src), DAG_arg1(src))));
		DAG_free(src);
		return dest;
	}
	return src;
}

/*
  --------------------------------------------------------------
  Boolean connectives simplifications
  --------------------------------------------------------------
*/

static TDAG
simplify_and(TDAG src)
{
	TDAG dest;
	/* Result dupped only if different from src */
	src = simplify_neutral(src, DAG_TRUE);
	src = simplify_ACidem(src);
	if (find_arg(src, DAG_FALSE) || find_comp(src)) {
		STAT_INC(and, "boolean AND");
		DAG_free(src);
		return DAG_dup(DAG_FALSE);
	}
	if (DAG_arity(src) == 1) {
		dest = DAG_dup(DAG_arg0(src));
		DAG_free(src);
		return dest;
	}
	return src;
}

static TDAG
simplify_or(TDAG src)
{
	TDAG dest;
	/* A_1 OR ... A_j OR FALSE OR A_{j+1} OR ... A_n --> A_1 OR ... A_n */
	src = simplify_neutral(src, DAG_FALSE);
	src = simplify_ACidem(src);
	if (find_arg(src, DAG_TRUE) || find_comp(src)) {
		STAT_INC(or, "boolean OR");
		DAG_free(src);
		return DAG_dup(DAG_TRUE);
	}
	if (DAG_arity(src) == 1) {
		dest = DAG_dup(DAG_arg0(src));
		DAG_free(src);
		return dest;
	}
	return src;
}

static TDAG
simplify_not(TDAG src)
{
	TDAG dest;
	assert(DAG_symb(src) == CONNECTOR_NOT);
	/* NOT NOT A --> A */
	if (DAG_symb(DAG_arg0(src)) == CONNECTOR_NOT) {
		STAT_INC(not, "boolean NOT");
		dest = DAG_dup(DAG_arg0(DAG_arg0(src)));
		DAG_free(src);
		return dest;
	}
	/* NOT FALSE --> TRUE */
	if (DAG_arg0(src) == DAG_FALSE) {
		STAT_INC(not, "boolean NOT");
		DAG_free(src);
		return DAG_dup(DAG_TRUE);
	}
	/* NOT TRUE --> FALSE */
	if (DAG_arg0(src) == DAG_TRUE) {
		STAT_INC(not, "boolean NOT");
		DAG_free(src);
		return DAG_dup(DAG_FALSE);
	}
	return src;
}

static TDAG
simplify_implies(TDAG src)
{
	TDAG dest;
	assert(DAG_symb(src) == CONNECTOR_IMPLIES);
	/* (NOT A) => (NOT B) --> B => A */
	if (
		DAG_symb(DAG_arg0(src)) == CONNECTOR_NOT &&
		DAG_symb(DAG_arg1(src)) == CONNECTOR_NOT) {
		STAT_INC(implies, "boolean =>");
		dest =
			DAG_dup(DAG_implies(DAG_arg0(DAG_arg1(src)), DAG_arg0(DAG_arg0(src))));
		DAG_free(src);
		return simplify_implies(dest);
	}
	/* FALSE => A, A => TRUE --> TRUE */
	if (DAG_arg0(src) == DAG_FALSE || DAG_arg1(src) == DAG_TRUE) {
		STAT_INC(implies, "boolean =>");
		DAG_free(src);
		return DAG_dup(DAG_TRUE);
	}
	/* TRUE => A --> A */
	else if (DAG_arg0(src) == DAG_TRUE) {
		STAT_INC(implies, "boolean =>");
		dest = DAG_dup(DAG_arg1(src));
		DAG_free(src);
		return dest;
	}
	/* A => FALSE --> NOT A */
	else if (DAG_arg1(src) == DAG_FALSE) {
		STAT_INC(implies, "boolean =>");
		dest = DAG_dup(DAG_not(DAG_arg0(src)));
		DAG_free(src);
		return simplify_not(dest);
	}
	/* A => A --> TRUE */
	if (DAG_arg0(src) == DAG_arg1(src)) {
		STAT_INC(implies, "boolean =>");
		DAG_free(src);
		return DAG_dup(DAG_TRUE);
	}
	/* (NOT A) => A --> A */
	/* A => NOT A --> NOT A */
	if (DAG_opposite(DAG_arg0(src), DAG_arg1(src))) {
		STAT_INC(implies, "boolean =>");
		DAG_free(src);
		return DAG_dup(DAG_arg1(src));
	}
	/* (P => Q) => Q --> P OR Q */
	if (
		DAG_symb(DAG_arg0(src)) == CONNECTOR_IMPLIES &&
		DAG_arg1(DAG_arg0(src)) == DAG_arg1(src)) {
		STAT_INC(implies, "boolean =>");
		dest = DAG_dup(DAG_or2(DAG_arg0(DAG_arg0(src)), DAG_arg1(src)));
		DAG_free(src);
		return dest;
	}
	return src;
}

static TDAG
simplify_equiv(TDAG src)
{
	TDAG dest;
	assert(DAG_symb(src) == CONNECTOR_EQUIV);
	/* (NOT A) EQV (NOT B) --> A EQV B */
	if (
		DAG_symb(DAG_arg0(src)) == CONNECTOR_NOT &&
		DAG_symb(DAG_arg1(src)) == CONNECTOR_NOT) {
		STAT_INC(equiv, "boolean EQV");
		dest = DAG_dup(DAG_equiv(DAG_arg0(DAG_arg0(src)), DAG_arg0(DAG_arg1(src))));
		DAG_free(src);
		return simplify_equiv(dest);
	}
	/* A EQV A --> TRUE */
	if (DAG_arg0(src) == DAG_arg1(src)) {
		STAT_INC(equiv, "boolean EQV");
		DAG_free(src);
		return DAG_dup(DAG_TRUE);
	}
	/* A EQV NOT A, NOT A EQV A --> FALSE */
	if (DAG_opposite(DAG_arg0(src), DAG_arg1(src))) {
		STAT_INC(equiv, "boolean EQV");
		DAG_free(src);
		return DAG_dup(DAG_FALSE);
	}
	/* TRUE EQV A --> A */
	if (DAG_arg0(src) == DAG_TRUE) {
		STAT_INC(equiv, "boolean EQV");
		dest = DAG_dup(DAG_arg1(src));
		DAG_free(src);
		return dest;
	}
	/* A EQV TRUE --> A */
	if (DAG_arg1(src) == DAG_TRUE) {
		STAT_INC(equiv, "boolean EQV");
		dest = DAG_dup(DAG_arg0(src));
		DAG_free(src);
		return dest;
	}
	/* FALSE EQV A --> NOT A */
	if (DAG_arg0(src) == DAG_FALSE) {
		STAT_INC(equiv, "boolean EQV");
		dest = DAG_dup(DAG_not(DAG_arg1(src)));
		DAG_free(src);
		return simplify_not(dest);
	}
	/* A EQV FALSE --> NOT A */
	if (DAG_arg1(src) == DAG_FALSE) {
		STAT_INC(equiv, "boolean EQV");
		dest = DAG_dup(DAG_not(DAG_arg0(src)));
		DAG_free(src);
		return simplify_not(dest);
	}
	return src;
}

/*
  --------------------------------------------------------------
  Node simplification
  --------------------------------------------------------------
*/

static TDAG
simplify_quantifier(TDAG src)
{
	if (!quantifier(DAG_symb(src))) return src;
	if (DAG_arg_last(src) == DAG_FALSE) {
		DAG_free(src);
		return DAG_dup(DAG_FALSE);
	}
	if (DAG_arg_last(src) == DAG_TRUE) {
		DAG_free(src);
		return DAG_dup(DAG_TRUE);
	}
	return src;
}

static void
simp_node_init(void)
{}

static void
simp_node_push(TDAG src, unsigned* pos)
{}

static void
simp_node_pop(TDAG src, unsigned pos)
{}

static TDAG
simp_node_reduce(TDAG src)
{
	if (DAG_symb(src) == CONNECTOR_AND) return simplify_and(src);
	if (DAG_symb(src) == CONNECTOR_OR) return simplify_or(src);
	if (DAG_symb(src) == CONNECTOR_NOT) return simplify_not(src);
	if (DAG_symb(src) == CONNECTOR_IMPLIES) return simplify_implies(src);
	if (DAG_symb(src) == CONNECTOR_EQUIV) return simplify_equiv(src);
	if (DAG_symb(src) == CONNECTOR_ITE) return simplify_ite(src);
	if (DAG_symb(src) == PREDICATE_EQ) return simplify_lift_ite(src);
	if (
		DAG_symb(src) == PREDICATE_LESS || DAG_symb(src) == PREDICATE_GREATEREQ ||
		DAG_symb(src) == PREDICATE_GREATER)
		src = rewrite_arith_compare(src);
	/* TODO: test put this guy before the previous one */
	if (DAG_symb(src) == PREDICATE_LESS) return simplify_less(src);
	if (DAG_symb(src) == PREDICATE_LEQ) return simplify_leq(src);
	if (DAG_symb(src) == FUNCTION_ITE) return simplify_fite(src);
	if (DAG_symb(src) == FUNCTION_SUM) return simplify_sum(src);
	if (DAG_symb(src) == FUNCTION_MINUS) return simplify_minus(src);
	if (unary_minus(DAG_symb(src))) return simplify_unary_minus(src);
	if (DAG_symb(src) == FUNCTION_PROD) return simplify_prod(src);
	if (DAG_symb(src) == FUNCTION_DIV) return simplify_div(src);
	if (quantifier(DAG_symb(src))) return simplify_quantifier(src);
	return src;
}

static void
simp_node_push_proof(TDAG src, unsigned* pos)
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

static Tproof
simp_node_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
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
simp_node_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	TDAG_proof dest;
	Tstack_proof reasons;
	Tproof proof_trans;
	dest.DAG = new_src;
	stack_INIT(reasons);
	if (proof_build) stack_push(reasons, proof_build);
	do {
		new_src = dest.DAG;
		/* TODO: have this guy in a separate function */
		if (DAG_symb(new_src) == CONNECTOR_AND) {
			dest = simplify_neutral_proof(new_src, DAG_TRUE);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
			}
			new_src = dest.DAG;
			if (DAG_symb(new_src) != CONNECTOR_AND) continue;
			dest = simplify_ACidem_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
			}
			new_src = dest.DAG;
			dest = simplify_and_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can it be further simplified? If no, break */
			}
		}
		/* TODO: have this guy in a separate function */
		else if (DAG_symb(new_src) == CONNECTOR_OR) {
			dest = simplify_neutral_proof(new_src, DAG_FALSE);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
			}
			new_src = dest.DAG;
			if (DAG_symb(new_src) != CONNECTOR_OR) continue;
			dest = simplify_ACidem_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
			}
			new_src = dest.DAG;
			dest = simplify_or_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can it be further simplified? If no, break */
			}
		} else if (DAG_symb(new_src) == CONNECTOR_NOT) {
			dest = simplify_not_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can it be further simplified? If no, break */
			}
		} else if (DAG_symb(new_src) == CONNECTOR_IMPLIES) {
			dest = simplify_implies_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
			}
		} else if (DAG_symb(new_src) == CONNECTOR_EQUIV) {
			dest = simplify_equiv_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
			}
		} else if (DAG_symb(new_src) == CONNECTOR_ITE) {
			dest = simplify_ite_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
			}
		} else if (DAG_symb(new_src) == PREDICATE_EQ) {
			dest = simplify_eq_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can't be further simplified */
				break;
			}
			/* dest = simplify_litf_ite_proof(new_src); */
		} else if (DAG_symb(new_src) == PREDICATE_LESS) {
			dest = simplify_less_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can't be further simplified */
				break;
			}
		} else if (
			DAG_symb(new_src) == PREDICATE_LESS ||
			DAG_symb(new_src) == PREDICATE_GREATEREQ ||
			DAG_symb(new_src) == PREDICATE_GREATER) {
			dest = rewrite_arith_compare_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
			}
		} else if (DAG_symb(new_src) == PREDICATE_LEQ) {
			dest = simplify_leq_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can't be further simplified */
				break;
			}
		} else if (DAG_symb(new_src) == FUNCTION_ITE) {
			dest = simplify_fite_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
			}
		} else if (DAG_symb(new_src) == FUNCTION_SUM) {
			dest = simplify_sum_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can it be further simplified? If no, break */
			}
		} else if (DAG_symb(new_src) == FUNCTION_MINUS) {
			dest = simplify_minus_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can it be further simplified? If no, break */
			}
		} else if (unary_minus(DAG_symb(new_src))) {
			dest = simplify_unary_minus_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can it be further simplified? If no, break */
			}
		} else if (DAG_symb(new_src) == FUNCTION_PROD) {
			dest = simplify_prod_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can it be further simplified? If no, break */
			}
		} else if (DAG_symb(new_src) == FUNCTION_DIV) {
			dest = simplify_div_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can it be further simplified? If no, break */
			}
		} else if (quantifier(DAG_symb(new_src))) {
			dest = simplify_quantifier_proof(new_src);
			if (dest.proof) {
				stack_merge(reasons, dest.proof);
				stack_free(dest.proof);
				/* can't be further simplified */
				break;
			}
		}
	} while (dest.DAG != new_src);
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

/*
  --------------------------------------------------------------
  Booleansimplification
  --------------------------------------------------------------
*/

static void
simp_boolean_init(void)
{}

static void
simp_boolean_push(TDAG src, unsigned* pos)
{}

static void
simp_boolean_pop(TDAG src, unsigned pos)
{}

static TDAG
simp_boolean_reduce(TDAG src)
{
	TDAG D0, D1, dest = src;
	if (DAG_symb(src) == CONNECTOR_NOT) {
		D0 = DAG_arg0(src);
		/* !(P -> Q) ==> P & !Q */
		if (DAG_symb(D0) == CONNECTOR_IMPLIES)
			dest = DAG_dup(DAG_and2(DAG_arg0(D0), DAG_not(DAG_arg1(D0))));
		/* !(P | Q) ==> !P & !Q */
		else if (DAG_symb(D0) == CONNECTOR_OR && DAG_arity(src) == 2)
			dest = DAG_dup(DAG_and2(DAG_not(DAG_arg0(D0)), DAG_not(DAG_arg1(D0))));
		/* !(P & Q) ==> !P | !Q */
		else if (DAG_symb(D0) == CONNECTOR_AND && DAG_arity(src) == 2)
			dest = DAG_dup(DAG_or2(DAG_not(DAG_arg0(D0)), DAG_not(DAG_arg1(D0))));
	} else if (DAG_symb(src) == CONNECTOR_IMPLIES) {
		D0 = DAG_arg0(src);
		D1 = DAG_arg1(src);
		/* P -> (Q -> R) ==> (P & Q) -> R */
		if (DAG_symb(D1) == CONNECTOR_IMPLIES)
			dest = DAG_dup(DAG_implies(DAG_and2(D0, DAG_arg0(D1)), DAG_arg1(D1)));
		/* (P -> Q) -> Q ==> P | Q */
		else if (DAG_symb(D0) == CONNECTOR_IMPLIES && DAG_arg1(D0) == D1)
			dest = DAG_dup(DAG_or2(DAG_arg0(D0), D1));
	}
	/* P & (P -> Q) ==> P & Q or (P -> Q) & P ==> P & Q */
	else if (
		DAG_symb(src) == CONNECTOR_AND && DAG_arity(src) == 2 &&
		((DAG_symb(DAG_arg1(src)) == CONNECTOR_IMPLIES &&
			DAG_arg0(src) == DAG_arg0(DAG_arg1(src))) ||
		 (DAG_symb(DAG_arg0(src)) == CONNECTOR_IMPLIES &&
			DAG_arg1(src) == DAG_arg0(DAG_arg0(src)))))
		dest = DAG_dup(
			DAG_symb(DAG_arg1(src)) == CONNECTOR_IMPLIES
				? DAG_and2(DAG_arg0(src), DAG_arg1(DAG_arg1(src)))
				: DAG_and2(DAG_arg1(src), DAG_arg1(DAG_arg0(src))));
	if (dest == src) return src;
	DAG_free(src);
	return dest;
}

static void
simp_boolean_push_proof(TDAG src, unsigned* pos)
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
simp_boolean_replacement(TDAG new_src, TDAG src, Tstack_proof reasons)
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

static TDAG_proof
simp_boolean_reduce_proof(TDAG src, TDAG new_src, Tproof proof_build)
{
	TDAG D0, D1;
	TDAG_proof dest;
	Tstack_proof reasons;
	Tproof proof_trans;
	dest.DAG = new_src;
	stack_INIT(reasons);
	if (proof_build) stack_push(reasons, proof_build);
	if (DAG_symb(new_src) == CONNECTOR_NOT) {
		D0 = DAG_arg0(new_src);
		/* !(P -> Q) ==> P & !Q */
		if (DAG_symb(D0) == CONNECTOR_IMPLIES)
			dest.DAG = DAG_dup(DAG_and2(DAG_arg0(D0), DAG_not(DAG_arg1(D0))));
		/* !(P | Q) ==> !P & !Q */
		else if (DAG_symb(D0) == CONNECTOR_OR && DAG_arity(new_src) == 2)
			dest.DAG =
				DAG_dup(DAG_and2(DAG_not(DAG_arg0(D0)), DAG_not(DAG_arg1(D0))));
		/* !(P & Q) ==> !P | !Q */
		else if (DAG_symb(D0) == CONNECTOR_AND && DAG_arity(new_src) == 2)
			dest.DAG = DAG_dup(DAG_or2(DAG_not(DAG_arg0(D0)), DAG_not(DAG_arg1(D0))));
	} else if (DAG_symb(new_src) == CONNECTOR_IMPLIES) {
		D0 = DAG_arg0(new_src);
		D1 = DAG_arg1(new_src);
		/* P -> (Q -> R) ==> (P & Q) -> R */
		if (DAG_symb(D1) == CONNECTOR_IMPLIES)
			dest.DAG = DAG_dup(DAG_implies(DAG_and2(D0, DAG_arg0(D1)), DAG_arg1(D1)));
		/* (P -> Q) -> Q ==> P | Q */
		else if (DAG_symb(D0) == CONNECTOR_IMPLIES && DAG_arg1(D0) == D1)
			dest.DAG = DAG_dup(DAG_or2(DAG_arg0(D0), D1));
	}
	/* P & (P -> Q) ==> P & Q or (P -> Q) & P ==> P & Q */
	else if (
		DAG_symb(new_src) == CONNECTOR_AND && DAG_arity(new_src) == 2 &&
		((DAG_symb(DAG_arg1(new_src)) == CONNECTOR_IMPLIES &&
			DAG_arg0(new_src) == DAG_arg0(DAG_arg1(new_src))) ||
		 (DAG_symb(DAG_arg0(new_src)) == CONNECTOR_IMPLIES &&
			DAG_arg1(new_src) == DAG_arg0(DAG_arg0(new_src)))))
		dest.DAG = DAG_dup(
			DAG_symb(DAG_arg1(new_src)) == CONNECTOR_IMPLIES
				? DAG_and2(DAG_arg0(new_src), DAG_arg1(DAG_arg1(new_src)))
				: DAG_and2(DAG_arg1(new_src), DAG_arg1(DAG_arg0(new_src))));
	if (dest.DAG == new_src) {
		if (stack_is_empty(reasons)) stack_free(reasons);
		dest.proof = reasons;
		return dest;
	}
	stack_push(
		reasons,
		proof_transformation(ps_type_bool_simplify, new_src, dest.DAG, NULL));
	DAG_free(new_src);
	if (stack_size(reasons) > 1) {
		proof_trans = proof_transformation(ps_type_trans, src, dest.DAG, reasons);
		stack_reset(reasons);
		stack_push(reasons, proof_trans);
	}
	dest.proof = reasons;
	return dest;
}

/*
  --------------------------------------------------------------
  Elimination of duplicate args at top level
  --------------------------------------------------------------
*/


/* TODO: Is this superseeded by simplify-AC */
static TDAG
eliminate_duplicate_arg_new(TDAG src)
{
	unsigned i;
	TDAG dest;
	Tstack_DAG DAGs;
	stack_INIT(DAGs);
	for (i = 0; i < DAG_arity(src); ++i)
		if (!DAG_tmp_DAG[DAG_arg(src, i)]) {
			DAG_tmp_DAG[DAG_arg(src, i)] = DAG_arg(src, i);
			stack_push(DAGs, DAG_arg(src, i));
		}
	for (i = 0; i < stack_size(DAGs); ++i)
		DAG_tmp_DAG[stack_get(DAGs, i)] = DAG_NULL;
	dest = DAG_dup(DAG_new_stack(DAG_symb(src), DAGs));
	stack_free(DAGs);
	DAG_free(src);
	return dest;
}

/*
  --------------------------------------------------------------
  Public functions
  --------------------------------------------------------------
*/

#include "pre/simplify-AC.h"

TDAG
simplify_formula(TDAG src)
{
	unsigned i;
	TDAG dest, *PDAG;
	/* Boolean simplifications */
	dest = nosub_context_structural_recursion(
		src, simp_boolean_init, simp_boolean_push, simp_boolean_pop,
		simp_boolean_reduce, NULL);
	DAG_free(src);
	src = dest;
	/* AC simplifications */
	src = simplify_AC(src);
	/* Formula simplification preserving top-level conjunction structure */
	if (DAG_symb(src) == CONNECTOR_AND) {
		MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
		memcpy(PDAG, DAG_args(src), DAG_arity(src) * sizeof(TDAG));
		/* PF for some reasons, shuffling the arguments of the top-level
         conjunction has a negative effect on efficiency */
		for (i = 0; i < DAG_arity(src); i++) DAG_dup(PDAG[i]);
		nosub_context_structural_recursion_array(
			DAG_arity(src), PDAG, simp_node_init, simp_node_push, simp_node_pop,
			simp_node_reduce, NULL);
		dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
		for (i = 0; i < DAG_arity(dest); i++) DAG_free(DAG_arg(dest, i));
	} else
		dest = nosub_context_structural_recursion(
			src, simp_node_init, simp_node_push, simp_node_pop, simp_node_reduce,
			NULL);
	DAG_free(src);
	/* TODO: use simplify neutral or AC..... */
	/* PF if top level symbol is or/and, eliminate duplicates */
	if (DAG_symb(dest) == CONNECTOR_OR || DAG_symb(dest) == CONNECTOR_AND) {
		DAG_tmp_reserve();
		dest = eliminate_duplicate_arg_new(dest);
		DAG_tmp_release();
	}
	return dest;
}

TDAG
simplify_formula_proof(TDAG src, Tproof* Pproof)
{
	TDAG dest;
	/* Boolean simplifications */
	dest = context_structural_recursion_proof(
		src, Pproof, simp_boolean_init, simp_boolean_push_proof, simp_boolean_pop,
		simp_boolean_replacement, simp_boolean_reduce_proof, NULL);
	DAG_free(src);
	src = dest;
	/* AC simplification */
	src = simplify_AC_proof(src, Pproof);
	/* General formula transformations */
	dest = context_structural_recursion_proof(
		src, Pproof, simp_node_init, simp_node_push_proof, simp_node_pop,
		simp_node_replacement, simp_node_reduce_proof, NULL);
	DAG_free(src);
	return dest;
}

void
simplify_init(void)
{
	mpz_init(mpz_tmp1);
	mpz_init(mpz_tmp2);
	mpq_init(mpq_tmp1);
	mpq_init(mpq_tmp2);
}

void
simplify_done(void)
{
	mpz_clear(mpz_tmp1);
	mpz_clear(mpz_tmp2);
	mpq_clear(mpq_tmp1);
	mpq_clear(mpq_tmp2);
}
