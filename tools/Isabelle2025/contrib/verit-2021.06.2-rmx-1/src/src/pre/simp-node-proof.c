#include "pre/simp-node-proof.h"

#include "pre/simplify.h"
#include "symbolic/DAG-arith.h"
#include "symbolic/DAG-print.h"

/* TODO: avoid this awful workaround */
extern mpq_t mpq_tmp1, mpq_tmp2;
extern mpz_t mpz_tmp1, mpz_tmp2;
extern TDAG_proof simp_node_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_build);

#define mpz_is_zero(A) (mpz_cmp_ui(A, 0u) == 0)
#define mpq_is_int(A) (mpz_cmp_ui(mpq_denref(A), 1u) == 0)
#define mpq_is_zero(A) (mpz_cmp_ui(mpq_numref(A), 0u) == 0)
#define mpq_is_one(A) ((mpz_cmp_ui(mpq_numref(A), 1u) == 0) && mpq_is_int(A))

static inline bool
DAG_opposite(TDAG DAG0, TDAG DAG1)
{
	return (DAG_symb(DAG0) == CONNECTOR_NOT && DAG_arg0(DAG0) == DAG1) ||
		(DAG_symb(DAG1) == CONNECTOR_NOT && DAG_arg0(DAG1) == DAG0);
}

TDAG_proof
simplify_neutral_proof(TDAG src, TDAG sub)
{
	assert(DAG_symb(src) == CONNECTOR_AND || DAG_symb(src) == CONNECTOR_OR);
	TDAG_proof dest;
	Tstack_DAG DAGs;
	dest.DAG = src;
	dest.proof = NULL;
	Tproof_type type =
		DAG_symb(src) == CONNECTOR_AND ? ps_type_and_simplify : ps_type_or_simplify;

	stack_INIT(DAGs);
	for (unsigned i = 0; i < DAG_arity(src); i++)
		if (DAG_arg(src, i) != sub) stack_push(DAGs, DAG_arg(src, i));

	if (stack_is_empty(DAGs)) {
		dest.DAG =
			DAG_symb(src) == CONNECTOR_AND ? DAG_dup(DAG_TRUE) : DAG_dup(DAG_FALSE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,	proof_transformation(type, src, dest.DAG, NULL));
		DAG_free(src);
	} else if (stack_size(DAGs) != DAG_arity(src)) {
		dest.DAG = DAG_dup(DAG_new_stack(DAG_symb(src), DAGs));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof, proof_transformation(type, src, dest.DAG, NULL));
		DAG_free(src);
	}
	stack_free(DAGs);
	return dest;
}

TDAG_proof
simplify_ACidem_proof(TDAG src)
{
	assert(DAG_symb(src) == CONNECTOR_AND || DAG_symb(src) == CONNECTOR_OR);
	unsigned i, j;
	unsigned n = DAG_arity(src);
	TDAG_proof dest;
	TDAG* PDAG;
	dest.DAG = src;
	dest.proof = NULL;
	Tproof_type type =
		DAG_symb(src) == CONNECTOR_AND ? ps_type_and_simplify : ps_type_or_simplify;

	if (n < 2) return dest;
	for (i = 0, j = 0; i < n; i++)
		if (!DAG_misc(DAG_arg(src, i))) {
			DAG_misc_set(DAG_arg(src, i), 1);
			j++;
		}
	if (j == n) {
		for (i = 0; i < n; i++) DAG_misc_set(DAG_arg(src, i), 0);
		return dest;
	}
	stack_INIT(dest.proof);
	MY_MALLOC(PDAG, j * sizeof(TDAG));
	for (i = 0, j = 0; i < n; i++)
		if (DAG_misc(DAG_arg(src, i))) {
			PDAG[j] = DAG_arg(src, i);
			DAG_misc_set(PDAG[j], 0);
			j++;
		}
	dest.DAG = DAG_dup(DAG_new(DAG_symb(src), j, PDAG));
	stack_push(
		dest.proof,
		proof_transformation(type, src, dest.DAG, NULL));
	DAG_free(src);
	return dest;
}

/*
  --------------------------------------------------------------
  Functional ITE simplifications
  --------------------------------------------------------------
*/

#define IF(DAG) DAG_arg(DAG, 0)
#define THEN(DAG) DAG_arg(DAG, 1)
#define ELSE(DAG) DAG_arg(DAG, 2)

TDAG_proof
simplify_fite_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	assert(DAG_symb(src) == FUNCTION_ITE);
	/* ITE TRUE T1 T2 --> T1 */
	if (IF(src) == DAG_TRUE) {
		dest.DAG = DAG_dup(THEN(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* ITE FALSE T1 T2 --> T2 */
	if (IF(src) == DAG_FALSE) {
		dest.DAG = DAG_dup(ELSE(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* ITE C T1 T1 --> T1 */
	if (THEN(src) == ELSE(src)) {
		dest.DAG = DAG_dup(THEN(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* ITE NOT C T1 T2 --> ITE C T2 T1 */
	if (DAG_symb(IF(src)) == CONNECTOR_NOT) {
		dest.DAG = DAG_dup(DAG_new_args(
			FUNCTION_ITE, DAG_arg0(IF(src)), ELSE(src), THEN(src), DAG_NULL));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* ITE(C, ITE(C, T1, T2), T3) --> ITE(C, T1, T3) */
	if (DAG_symb(THEN(src)) == FUNCTION_ITE && IF(src) == IF(THEN(src))) {
		dest.DAG = DAG_dup(DAG_new_args(
			FUNCTION_ITE, IF(src), THEN(THEN(src)), ELSE(src), DAG_NULL));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* ITE(C, T1, ITE(C, T2, T3)) --> ITE(C, T1, T3) */
	if (DAG_symb(ELSE(src)) == FUNCTION_ITE && IF(src) == IF(ELSE(src))) {
		dest.DAG = DAG_dup(DAG_new_args(
			FUNCTION_ITE, IF(src), THEN(src), ELSE(ELSE(src)), DAG_NULL));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

TDAG_proof
simplify_ite_proof(TDAG src)
{
	TDAG_proof dest, tmp;
	dest.DAG = src;
	dest.proof = NULL;
	assert(DAG_symb(src) == CONNECTOR_ITE);
	/* IF TRUE THEN A ELSE B --> A */
	if (IF(src) == DAG_TRUE) {
		dest.DAG = DAG_dup(THEN(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* IF FALSE THEN A ELSE B --> B */
	if (IF(src) == DAG_FALSE) {
		dest.DAG = DAG_dup(ELSE(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* IF C THEN TRUE ELSE FALSE --> C */
	if (THEN(src) == DAG_TRUE && ELSE(src) == DAG_FALSE) {
		dest.DAG = DAG_dup(IF(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* IF C THEN FALSE ELSE TRUE --> NOT C */
	if (THEN(src) == DAG_FALSE && ELSE(src) == DAG_TRUE) {
		dest.DAG = DAG_dup(DAG_not(IF(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* IF C THEN A ELSE A --> A */
	if (THEN(src) == ELSE(src)) {
		dest.DAG = DAG_dup(THEN(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* IF C THEN TRUE ELSE B --> C OR B */
	if (THEN(src) == DAG_TRUE) {
		dest.DAG = DAG_dup(DAG_or2(IF(src), ELSE(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* IF C THEN A ELSE FALSE --> C AND A */
	if (ELSE(src) == DAG_FALSE) {
		dest.DAG = DAG_dup(DAG_and2(IF(src), THEN(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* IF C THEN FALSE ELSE B --> NOT C AND B */
	if (THEN(src) == DAG_FALSE) {
		tmp = simp_node_reduce_proof(src, DAG_dup(DAG_not(IF(src))), 0);
		dest.DAG = DAG_dup(DAG_and2(tmp.DAG, ELSE(src)));
		stack_INIT(dest.proof);
		if (tmp.proof) {
			stack_merge(dest.proof, tmp.proof);
			stack_free(tmp.proof);
		}
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(tmp.DAG);
		DAG_free(src);
		return dest;
	}
	/* IF C THEN A ELSE TRUE --> NOT C OR A */
	if (ELSE(src) == DAG_TRUE) {
		tmp = simp_node_reduce_proof(src, DAG_dup(DAG_not(IF(src))), 0);
		dest.DAG = DAG_dup(DAG_or2(tmp.DAG, THEN(src)));
		stack_INIT(dest.proof);
		if (tmp.proof) {
			stack_merge(dest.proof, tmp.proof);
			stack_free(tmp.proof);
		}
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(tmp.DAG);
		DAG_free(src);
		return dest;
	}
	/* IF NOT C THEN A ELSE B --> IF C THEN B ELSE A */
	if (DAG_symb(IF(src)) == CONNECTOR_NOT) {
		dest.DAG = DAG_dup(DAG_ite(DAG_arg0(IF(src)), ELSE(src), THEN(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* ITE(C, ITE(C, T1, T2), T3) --> ITE(C, T1, T3) */
	if (DAG_symb(THEN(src)) == CONNECTOR_ITE && IF(src) == IF(THEN(src))) {
		dest.DAG = DAG_dup(DAG_ite(IF(src), THEN(THEN(src)), ELSE(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* ITE(C, T1, ITE(C, T2, T3)) --> ITE(C, T1, T3) */
	if (DAG_symb(ELSE(src)) == CONNECTOR_ITE && IF(src) == IF(ELSE(src))) {
		dest.DAG = DAG_dup(DAG_ite(IF(src), THEN(src), ELSE(ELSE(src))));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_ite_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

TDAG_proof
simplify_eq_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	assert(DAG_symb(src) == PREDICATE_EQ && DAG_arity(src) == 2);
	if (DAG_is_number(DAG_arg0(src)) && DAG_is_number(DAG_arg1(src))) {
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
		dest.DAG = DAG_dup(mpq_equal(mpq_tmp1, mpq_tmp2) ? DAG_TRUE : DAG_FALSE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_eq_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (DAG_arg0(src) == DAG_arg1(src)) {
		dest.DAG = DAG_dup(DAG_TRUE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_eq_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

#undef IF
#undef ELSE
#undef THEN

/*
  --------------------------------------------------------------
  Arithmetic simplifications
  --------------------------------------------------------------
*/

/** \brief  Rewrite (/ c d) where c and d are numbers */
TDAG_proof
simplify_div_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	assert(DAG_arity(src) == 2);
	assert(DAG_sort(src) == SORT_INTEGER || DAG_sort(src) == SORT_REAL);
	if (DAG_arg0(src) == DAG_arg1(src)) {
		/* Which type of division is this? */
		if (DAG_sort(src) == SORT_REAL) {
			mpq_set_ui(mpq_tmp1, 1u, 1u);
			dest.DAG = DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		} else
			dest.DAG = DAG_dup(DAG_new_integer(1));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_div_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (!DAG_is_number(DAG_arg1(src))) return dest;
	DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
	/* Do nothing in the case of divison by zero */
	if (mpq_is_zero(mpq_tmp2)) return dest;
	if (mpq_is_one(mpq_tmp2)) {
		dest.DAG = DAG_dup(DAG_arg0(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_div_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (!DAG_is_number(DAG_arg0(src))) return dest;

	if (DAG_sort(src) == SORT_INTEGER) {
		assert(DAG_is_integer(DAG_arg0(src)) && DAG_is_integer(DAG_arg1(src)));
		DAG_mpz_set(mpz_tmp1, DAG_arg0(src));
		DAG_mpz_set(mpz_tmp2, DAG_arg1(src));
		mpz_fdiv_q(mpz_tmp1, mpz_tmp1, mpz_tmp2);
		dest.DAG = DAG_dup(DAG_new_integer_mpz(mpz_tmp1));
	} else {
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		mpq_div(mpq_tmp1, mpq_tmp1, mpq_tmp2);
		dest.DAG = DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
	}
	stack_INIT(dest.proof);
	stack_push(
		dest.proof,
		proof_transformation(ps_type_div_simplify, src, dest.DAG, NULL));
	DAG_free(src);
	return dest;
}

TDAG_proof
simplify_prod_proof(TDAG src)
{
	bool is_one;
	unsigned i, j = 0, numbers = 0;
	TDAG* PDAG;
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
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
	if (numbers == 0) return dest;
	if (
		numbers == 1 && DAG_is_number(DAG_arg(src, 0)) && !mpq_is_zero(mpq_tmp1) &&
		!mpq_is_one(mpq_tmp1))
		return dest;
	if (numbers == DAG_arity(src) || mpq_is_zero(mpq_tmp1)) {
		if (DAG_sort(src) == SORT_REAL)
			dest.DAG = DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		else
			dest.DAG = DAG_dup(DAG_new_integer_mpz(mpq_numref(mpq_tmp1)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_prod_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	is_one = mpq_is_one(mpq_tmp1);
	if (is_one && DAG_arity(src) - numbers == 1)
		for (i = 0; i < DAG_arity(src); ++i)
			if (!DAG_is_number(DAG_arg(src, i))) {
				dest.DAG = DAG_dup(DAG_arg(src, i));
				stack_INIT(dest.proof);
				stack_push(
					dest.proof,
					proof_transformation(ps_type_prod_simplify, src, dest.DAG, NULL));
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
	dest.DAG = DAG_dup(
		DAG_new(DAG_symb(src), DAG_arity(src) - numbers + (is_one ? 0 : 1), PDAG));
	stack_INIT(dest.proof);
	stack_push(
		dest.proof,
		proof_transformation(ps_type_prod_simplify, src, dest.DAG, NULL));
	DAG_free(src);
	return dest;
}

/**  Rewrite (- c) where c is a number */
TDAG_proof
simplify_unary_minus_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	if (DAG_arity(src) != 1)
		my_error("simplify_unary_minus: strange arity for src\n");
	if (DAG_is_integer(DAG_arg0(src))) {
		DAG_mpz_set(mpz_tmp1, DAG_arg0(src));
		mpz_neg(mpz_tmp1, mpz_tmp1);
		dest.DAG = DAG_dup(DAG_new_integer_mpz(mpz_tmp1));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_minus_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (DAG_is_rational(DAG_arg0(src))) {
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		mpq_neg(mpq_tmp1, mpq_tmp1);
		dest.DAG = DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_minus_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (unary_minus(DAG_symb(DAG_arg0(src)))) {
		dest.DAG = DAG_dup(DAG_arg0(DAG_arg0(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_minus_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

TDAG_proof
simplify_minus_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	assert(DAG_arity(src) == 2);
	assert(DAG_sort(src) == SORT_INTEGER || DAG_sort(src) == SORT_REAL);
	if (DAG_arg0(src) == DAG_arg1(src)) {
		if (DAG_sort(src) == SORT_REAL) {
			mpq_set_ui(mpq_tmp1, 0u, 1u);
			dest.DAG = DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		} else
			dest.DAG = DAG_dup(DAG_new_integer(0));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_minus_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (DAG_is_number(DAG_arg0(src)) && DAG_is_number(DAG_arg1(src))) {
		if (DAG_is_integer(DAG_arg0(src)) && DAG_is_integer(DAG_arg1(src))) {
			DAG_mpz_set(mpz_tmp1, DAG_arg0(src));
			DAG_mpz_set(mpz_tmp2, DAG_arg1(src));
			mpz_sub(mpz_tmp1, mpz_tmp1, mpz_tmp2);
			dest.DAG = DAG_dup(DAG_new_integer_mpz(mpz_tmp1));
			stack_INIT(dest.proof);
			stack_push(
				dest.proof,
				proof_transformation(ps_type_minus_simplify, src, dest.DAG, NULL));
			DAG_free(src);
			return dest;
		}
		assert(!DAG_is_integer(DAG_arg0(src) && !DAG_is_integer(DAG_arg1(src))));
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
		mpq_sub(mpq_tmp1, mpq_tmp1, mpq_tmp2);
		dest.DAG = DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_minus_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (DAG_is_number(DAG_arg1(src))) {
		DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
		if (mpq_is_zero(mpq_tmp2)) {
			dest.DAG = DAG_dup(DAG_arg0(src));
			stack_INIT(dest.proof);
			stack_push(
				dest.proof,
				proof_transformation(ps_type_minus_simplify, src, dest.DAG, NULL));
			DAG_free(src);
			return dest;
		}
	}
	if (DAG_is_number(DAG_arg0(src))) {
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		if (mpq_is_zero(mpq_tmp1)) {
			dest.DAG = DAG_dup(DAG_new_unary(FUNCTION_UNARY_MINUS, DAG_arg1(src)));
			stack_INIT(dest.proof);
			stack_push(
				dest.proof,
				proof_transformation(ps_type_minus_simplify, src, dest.DAG, NULL));
			DAG_free(src);
			return dest;
		}
	}
	return dest;
}

TDAG_proof
simplify_sum_proof(TDAG src)
{
	bool is_zero;
	unsigned i, j = 0, numbers = 0;
	TDAG* PDAG;
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
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
	if (numbers == 0) return dest;
	if (
		numbers == 1 && DAG_is_number(DAG_arg(src, 0)) &&
		!(mpz_is_zero(mpz_tmp1) && mpq_is_zero(mpq_tmp1)))
		return dest;
	if (numbers == DAG_arity(src)) {
		if (DAG_sort(src) == SORT_INTEGER) {
			assert(mpq_is_int(mpq_tmp1));
			mpz_add(mpz_tmp1, mpz_tmp1, mpq_numref(mpq_tmp1));
			dest.DAG = DAG_dup(DAG_new_integer_mpz(mpz_tmp1));
		} else {
			mpq_set_z(mpq_tmp2, mpz_tmp1);
			mpq_add(mpq_tmp1, mpq_tmp1, mpq_tmp2);
			dest.DAG = DAG_dup(DAG_new_rational_mpq(mpq_tmp1));
		}

		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_sum_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	is_zero = mpq_is_zero(mpq_tmp1) && (mpz_cmp_ui(mpz_tmp1, 0) == 0);
	if (is_zero && DAG_arity(src) - numbers == 1)
		for (i = 0; i < DAG_arity(src); ++i)
			if (!DAG_is_number(DAG_arg(src, i))) {
				dest.DAG = DAG_dup(DAG_arg(src, i));
				stack_INIT(dest.proof);
				stack_push(
					dest.proof,
					proof_transformation(ps_type_sum_simplify, src, dest.DAG, NULL));
				DAG_free(src);
				return dest;
			}
	assert(DAG_arity(src) > numbers);
	MY_MALLOC(
		PDAG, (DAG_arity(src) - numbers + (is_zero ? 0 : 1)) * sizeof(TDAG));
	// bug: j is to big. should this code here even happen if j is zero?
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
	assert(j - (is_zero ? 0 : 1) + numbers == DAG_arity(src)); // bug 4 - 0 + 2 = 5
	dest.DAG = DAG_dup(
		DAG_new(DAG_symb(src), DAG_arity(src) - numbers + (is_zero ? 0 : 1), PDAG));
	stack_INIT(dest.proof);
	stack_push(
		dest.proof,
		proof_transformation(ps_type_sum_simplify, src, dest.DAG, NULL));
	DAG_free(src);
	return dest;
}

TDAG_proof
simplify_less_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	assert(DAG_symb(src) == PREDICATE_LESS && DAG_arity(src) == 2);
	if (DAG_is_number(DAG_arg0(src)) && DAG_is_number(DAG_arg1(src))) {
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
		dest.DAG = DAG_dup(mpq_cmp(mpq_tmp1, mpq_tmp2) < 0 ? DAG_TRUE : DAG_FALSE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_comp_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (DAG_arg0(src) == DAG_arg1(src)) {
		dest.DAG = DAG_dup(DAG_FALSE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_comp_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

TDAG_proof
simplify_leq_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	assert(DAG_symb(src) == PREDICATE_LEQ && DAG_arity(src) == 2);
	if (DAG_is_number(DAG_arg0(src)) && DAG_is_number(DAG_arg1(src))) {
		DAG_mpq_set(mpq_tmp1, DAG_arg0(src));
		DAG_mpq_set(mpq_tmp2, DAG_arg1(src));
		dest.DAG = DAG_dup(mpq_cmp(mpq_tmp1, mpq_tmp2) <= 0 ? DAG_TRUE : DAG_FALSE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_comp_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (DAG_arg0(src) == DAG_arg1(src)) {
		dest.DAG = DAG_dup(DAG_TRUE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_comp_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

/**
   Rewrite t >= t' to t' <= t
   Rewrite t < t' to \neg t' <= t
   Rewrite t > t' to \neg t <= t' */
TDAG_proof
rewrite_arith_compare_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	if (DAG_symb(src) == PREDICATE_GREATEREQ) {
		dest.DAG =
			DAG_dup(DAG_new_binary(PREDICATE_LEQ, DAG_arg1(src), DAG_arg0(src)));
		DAG_free(src);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_comp_simplify, src, dest.DAG, NULL));
		return dest;
	}
	if (DAG_symb(src) == PREDICATE_LESS) {
		dest.DAG = DAG_dup(
			DAG_not(DAG_new_binary(PREDICATE_LEQ, DAG_arg1(src), DAG_arg0(src))));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_comp_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (DAG_symb(src) == PREDICATE_GREATER) {
		dest.DAG = DAG_dup(
			DAG_not(DAG_new_binary(PREDICATE_LEQ, DAG_arg0(src), DAG_arg1(src))));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_comp_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

/*
  --------------------------------------------------------------
  Boolean connectives simplifications
  --------------------------------------------------------------
*/

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

TDAG_proof
simplify_and_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	if (find_arg(src, DAG_FALSE) || find_comp(src)) {
		dest.DAG = DAG_dup(DAG_FALSE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_and_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (DAG_arity(src) == 1) {
		dest.DAG = DAG_dup(DAG_arg0(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_and_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

TDAG_proof
simplify_or_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	if (find_arg(src, DAG_TRUE) || find_comp(src)) {
		dest.DAG = DAG_dup(DAG_TRUE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_or_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (DAG_arity(src) == 1) {
		dest.DAG = DAG_dup(DAG_arg0(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_or_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

TDAG_proof
simplify_not_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	assert(DAG_symb(src) == CONNECTOR_NOT);
	/* NOT NOT A --> A */
	if (DAG_symb(DAG_arg0(src)) == CONNECTOR_NOT) {
		dest.DAG = DAG_dup(DAG_arg0(DAG_arg0(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_not_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* NOT FALSE --> TRUE */
	if (DAG_arg0(src) == DAG_FALSE) {
		dest.DAG = DAG_dup(DAG_TRUE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_not_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* NOT TRUE --> FALSE */
	if (DAG_arg0(src) == DAG_TRUE) {
		dest.DAG = DAG_dup(DAG_FALSE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_not_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

TDAG_proof
simplify_implies_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	assert(DAG_symb(src) == CONNECTOR_IMPLIES);
	/* (NOT A) => (NOT B) --> B => A */
	if (
		DAG_symb(DAG_arg0(src)) == CONNECTOR_NOT &&
		DAG_symb(DAG_arg1(src)) == CONNECTOR_NOT) {
		dest.DAG =
			DAG_dup(DAG_implies(DAG_arg0(DAG_arg1(src)), DAG_arg0(DAG_arg0(src))));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_implies_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* FALSE => A, A => TRUE --> TRUE */
	if (DAG_arg0(src) == DAG_FALSE || DAG_arg1(src) == DAG_TRUE) {
		dest.DAG = DAG_dup(DAG_TRUE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_implies_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* TRUE => A --> A */
	else if (DAG_arg0(src) == DAG_TRUE) {
		dest.DAG = DAG_dup(DAG_arg1(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_implies_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* A => FALSE --> NOT A */
	else if (DAG_arg1(src) == DAG_FALSE) {
		dest.DAG = DAG_dup(DAG_not(DAG_arg0(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_implies_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* A => A --> TRUE */
	if (DAG_arg0(src) == DAG_arg1(src)) {
		dest.DAG = DAG_dup(DAG_TRUE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_implies_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* (NOT A) => A --> A */
	/* A => NOT A --> NOT A */
	if (DAG_opposite(DAG_arg0(src), DAG_arg1(src))) {
		dest.DAG = DAG_dup(DAG_arg1(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_implies_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* (P => Q) => Q --> P OR Q */
	if (
		DAG_symb(DAG_arg0(src)) == CONNECTOR_IMPLIES &&
		DAG_arg1(DAG_arg0(src)) == DAG_arg1(src)) {
		dest.DAG = DAG_dup(DAG_or2(DAG_arg0(DAG_arg0(src)), DAG_arg1(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_implies_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

TDAG_proof
simplify_equiv_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	assert(DAG_symb(src) == CONNECTOR_EQUIV);
	/* (NOT A) EQV (NOT B) --> A EQV B */
	if (
		DAG_symb(DAG_arg0(src)) == CONNECTOR_NOT &&
		DAG_symb(DAG_arg1(src)) == CONNECTOR_NOT) {
		dest.DAG =
			DAG_dup(DAG_equiv(DAG_arg0(DAG_arg0(src)), DAG_arg0(DAG_arg1(src))));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_equiv_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* A EQV A --> TRUE */
	if (DAG_arg0(src) == DAG_arg1(src)) {
		dest.DAG = DAG_dup(DAG_TRUE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_equiv_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* A EQV NOT A, NOT A EQV A --> FALSE */
	if (DAG_opposite(DAG_arg0(src), DAG_arg1(src))) {
		dest.DAG = DAG_dup(DAG_FALSE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_equiv_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* TRUE EQV A --> A */
	if (DAG_arg0(src) == DAG_TRUE) {
		dest.DAG = DAG_dup(DAG_arg1(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_equiv_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* A EQV TRUE --> A */
	if (DAG_arg1(src) == DAG_TRUE) {
		dest.DAG = DAG_dup(DAG_arg0(src));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_equiv_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* FALSE EQV A --> NOT A */
	if (DAG_arg0(src) == DAG_FALSE) {
		dest.DAG = DAG_dup(DAG_not(DAG_arg1(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_equiv_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	/* A EQV FALSE --> NOT A */
	if (DAG_arg1(src) == DAG_FALSE) {
		dest.DAG = DAG_dup(DAG_not(DAG_arg0(src)));
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_equiv_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

/*
  --------------------------------------------------------------
  Node simplification
  --------------------------------------------------------------
*/

TDAG_proof
simplify_quantifier_proof(TDAG src)
{
	TDAG_proof dest;
	dest.DAG = src;
	dest.proof = NULL;
	if (!quantifier(DAG_symb(src))) return dest;
	if (DAG_arg_last(src) == DAG_FALSE) {
		dest.DAG = DAG_dup(DAG_FALSE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_qnt_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	if (DAG_arg_last(src) == DAG_TRUE) {
		dest.DAG = DAG_dup(DAG_TRUE);
		stack_INIT(dest.proof);
		stack_push(
			dest.proof,
			proof_transformation(ps_type_qnt_simplify, src, dest.DAG, NULL));
		DAG_free(src);
		return dest;
	}
	return dest;
}

