/*
  --------------------------------------------------------------
  Functions to deal with numbers
  --------------------------------------------------------------
*/

#include "numbers-mp.h"

#include "utils/general.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#define GCD_CACHE

/* PF a bound for a variable v should be understood as
   v >= val + delta \delta, where \delta is an infinitesimal value
   If delta is 0, inequality is non-strict */

/*
  --------------------------------------------------------------
  Helpers
  --------------------------------------------------------------
*/

/*
  --------------------------------------------------------------
  Delta numbers
  --------------------------------------------------------------
*/

void
LAdelta_mp_init(TLAdelta_mp* delta)
{
	LArational_mp_init(delta->val);
	LArational_mp_init(delta->delta);
}

void
LAdelta_mp_clear(TLAdelta_mp* delta)
{
	LArational_mp_clear(delta->val);
	LArational_mp_clear(delta->delta);
}

/*
  --------------------------------------------------------------
  The methods are grouped together in functions
  for Signed, Rationals and Deltas.
  --------------------------------------------------------------
*/

/*
  --------------------------------------------------------------
  Signed
  --------------------------------------------------------------
*/

/*
  --------------------------------------------------------------
  Rationals
  --------------------------------------------------------------
*/

void
LArational_mp_set_one(TLArational_mp rational)
{
	mpq_set_ui(rational, 1ul, 1ul);
}

void
LArational_mp_set_zero(TLArational_mp rational)
{
	mpq_set_ui(rational, 0ul, 1ul);
}

bool
LArational_mp_is_zero(TLArational_mp rational)
{
	return mpq_sgn(rational) == 0;
}

bool
LArational_mp_is_neg(TLArational_mp rational)
{
	return mpq_sgn(rational) < 0;
}

bool
LArational_mp_is_int(TLArational_mp rational)
{
	mpz_t tmp;
	bool result;
	mpz_init(tmp);
	mpq_get_den(tmp, rational);
	result = mpz_cmp_si(tmp, 1L) == 0;
	mpz_clear(tmp);
	return result;
}

void
LArational_mp_str(TLArational_mp rational, char* str)
{
	unsigned i;
	unsigned j;
	TLArational_mp ten;
	if (str[0] == 0) {
		mpq_set_ui(rational, 0ul, 1ul);
		return;
	}

	/* http://gmplib.org/manual/Initializing-Rationals.html: The string can be an
     integer like "41" or a fraction like "41/152". The fraction must be in
     canonical form (see Rational Number Functions), or if not then
     mpq_canonicalize must be called. */
	if (str[0] == '#') /* per smt-lib reference, only digits: no canonicalize */
	{
		str[0] = '0';
		mpq_set_str(rational, str, 0);
		str[0] = '#';
		return;
	}
	/* Numeral [\-]?(0|[1-9][0-9]*)
     Rational [\-]?[1-9][0-9]* / [0-9]+[1-9]
     Decimal [\-]?(0|[1-9][0-9]*)\.[0-9]+ */
	i = 0;
	if (str[i] == '-') i++;
	while (str[i] >= '0' && str[i] <= '9') i++;
	if (str[i] == 0 || str[i] == '/') /* Numeral or Rational: handled by GMP */
	{
		mpq_set_str(rational, str, 10);
		mpq_canonicalize(rational);
		return;
	}
	/* Decimal is not handled by GMP */
	assert(str[i] == '.');
	str = strmake(str);
	j = i;
	do {
		str[i] = str[i + 1];
		++i;
	} while (str[i] != 0);
	--i;
	mpq_set_str(rational, str, 10);
	mpq_init(ten);
	mpq_set_ui(ten, 10UL, 1UL);
	while (j < i) {
		mpq_div(rational, rational, ten);
		++j;
	}
	mpq_canonicalize(rational);
	mpq_clear(ten);
	free(str);
}

void
LArational_mp_mpz(TLArational_mp rational, mpz_t mpz)
{
	mpq_set_z(rational, mpz);
}

void
LArational_mp_mpq(TLArational_mp rational, mpq_t mpq)
{
	mpq_set(rational, mpq);
}

void
LArational_mp_normalize(TLArational_mp rational)
{
	mpq_canonicalize(rational);
}

void
LArational_mp_neg(TLArational_mp rational)
{
	mpq_neg(rational, rational);
}

void
LArational_mp_add(TLArational_mp rational1, TLArational_mp rational2)
{
	mpq_add(rational1, rational1, rational2);
}

void
LArational_mp_mult(TLArational_mp rational1, const TLArational_mp rational2)
{
	mpq_mul(rational1, rational1, rational2);
}

void
LArational_mp_mult_s(TLArational_mp rational1, TLAsigned_mp signed2)
{
	mpq_t tmp;
	mpq_init(tmp);
	mpq_set_z(tmp, signed2);
	mpq_mul(rational1, rational1, tmp);
	mpq_clear(tmp);
}

void
LArational_mp_div(TLArational_mp rational1, TLArational_mp rational2)
{
	mpq_div(rational1, rational1, rational2);
}

void
LArational_mp_div_s(TLArational_mp rational1, TLAsigned_mp signed2)
{
	mpq_t tmp;
	mpq_init(tmp);
	mpq_set_z(tmp, signed2);
	mpq_div(rational1, rational1, tmp);
	mpq_clear(tmp);
}

bool
LArational_mp_eq(TLArational_mp rational1, TLArational_mp rational2)
{
	return mpq_equal(rational1, rational2);
}

bool
LArational_mp_leq(TLArational_mp rational1, TLArational_mp rational2)
{
	return mpq_cmp(rational1, rational2) <= 0;
}

bool
LArational_mp_less(TLArational_mp rational1, TLArational_mp rational2)
{
	return mpq_cmp(rational1, rational2) < 0;
}

void
LArational_mp_lcm(TLAsigned_mp lcm, TLArational_mp rational)
{
	mpz_lcm(lcm, lcm, mpq_denref(rational));
}

void
LArational_mp_mult_to_signed(TLAsigned_mp asigned, TLArational_mp rational)
{
	mpz_div(asigned, asigned, mpq_denref(rational));
	mpz_mul(asigned, asigned, mpq_numref(rational));
}

/*
  --------------------------------------------------------------
  Delta numbers
  --------------------------------------------------------------
*/

void
LAdelta_mp_set_rat(TLAdelta_mp* dst, TLArational_mp* src)
{
	LArational_mp_set(dst->val, *src);
}

void
LAdelta_mp_set_delta(TLAdelta_mp* dst, int eps)
{
	mpq_set_si(dst->delta, (signed long int)eps, 1UL);
	/* mpq_canonicalize(delta->delta); */
}

void
LAdelta_mp_set_zero(TLAdelta_mp* delta)
{
	LArational_mp_set_zero(delta->val);
	LArational_mp_set_zero(delta->delta);
}

void
LAdelta_mp_set_one(TLAdelta_mp* delta)
{
	LArational_mp_set_one(delta->val);
	LArational_mp_set_zero(delta->delta);
}

bool
LAdelta_mp_is_zero(TLAdelta_mp* delta)
{
	return LArational_mp_is_zero(delta->val) &&
		LArational_mp_is_zero(delta->delta);
}

bool
LAdelta_mp_is_int(TLAdelta_mp* delta)
{
	return LArational_mp_is_int(delta->val) &&
		LArational_mp_is_zero(delta->delta);
}

void
LAdelta_mp_int(TLAdelta_mp* delta, int val)
{
	mpq_set_si(delta->val, (signed long int)val, 1UL);
	LArational_mp_set_zero(delta->delta);
	/* mpq_canonicalize(delta->val); */
}

void
LAdelta_mp_rat(TLAdelta_mp* delta, int num, unsigned den)
{
	mpq_set_si(delta->val, (signed long int)num, (unsigned long int)den);
	mpq_canonicalize(delta->val);
	LArational_mp_set_zero(delta->delta);
}

void
LAdelta_mp_rat_delta(TLAdelta_mp* delta, int num, unsigned den, int eps)
{
	mpq_set_si(delta->val, (signed long int)num, (unsigned long int)den);
	mpq_canonicalize(delta->val);
	mpq_set_si(delta->delta, (signed long int)eps, 1UL);
	/* mpq_canonicalize(delta->delta); */
}

bool
LAdelta_mp_eq(TLAdelta_mp* delta1, TLAdelta_mp* delta2)
{
	return LArational_mp_eq(delta1->val, delta2->val) &&
		LArational_mp_eq(delta1->delta, delta2->delta);
}

bool
LAdelta_mp_leq(TLAdelta_mp* delta1, TLAdelta_mp* delta2)
{
	return LArational_mp_less(delta1->val, delta2->val) ||
		(LArational_mp_eq(delta1->val, delta2->val) &&
		 LArational_mp_leq(delta1->delta, delta2->delta));
}

int
LAdelta_mp_cmp(TLAdelta_mp* delta1, TLAdelta_mp* delta2)
{
	int tmp = mpq_cmp(delta1->val, delta2->val);
	if (tmp) return tmp;
	return mpq_cmp(delta1->delta, delta2->delta);
}

bool
LAdelta_mp_less(TLAdelta_mp* delta1, TLAdelta_mp* delta2)
{
	return LArational_mp_less(delta1->val, delta2->val) ||
		(LArational_mp_eq(delta1->val, delta2->val) &&
		 LArational_mp_less(delta1->delta, delta2->delta));
}

void
LAdelta_mp_addmult(TLAdelta_mp* delta0, TLAdelta_mp* delta1, TLAsigned_mp a)
{
	/* This is a more high level implementation*/
	mpq_t tmp_q;

	mpq_init(tmp_q);
	mpq_set_z(tmp_q, a);

	mpq_mul(tmp_q, tmp_q, delta1->val);
	mpq_add(delta0->val, delta0->val, tmp_q);

	mpq_set_z(tmp_q, a);
	mpq_mul(tmp_q, tmp_q, delta1->delta);
	mpq_add(delta0->delta, delta0->delta, tmp_q);

	mpq_clear(tmp_q);
}

void
LAdelta_mp_minus(TLAdelta_mp* delta0, TLAdelta_mp* delta1, TLAdelta_mp* delta2)
{
	mpq_sub(delta0->val, delta1->val, delta2->val);
	mpq_sub(delta0->delta, delta1->delta, delta2->delta);
}

void
LAdelta_mp_div_opp(TLAdelta_mp* delta0, TLAsigned_mp a)
{
	mpq_t tmp;
	mpq_init(tmp);
	mpq_set_z(tmp, a);

	mpq_neg(delta0->val, delta0->val);
	mpq_div(delta0->val, delta0->val, tmp);

	mpq_neg(delta0->delta, delta0->delta);
	mpq_div(delta0->delta, delta0->delta, tmp);

	mpq_clear(tmp);
}

void
LAdelta_mp_print(TLAdelta_mp* delta)
{
	mpq_out_str(stdout, 10, delta->val);
	if (mpq_sgn(delta->delta)) {
		fprintf(stdout, "+ ");
		mpq_out_str(stdout, 10, delta->delta);
		fprintf(stdout, "d ");
	}
}

void
LAdelta_mp_set(TLAdelta_mp* a, TLAdelta_mp* b)
{
	LArational_mp_set(a->val, b->val);
	LArational_mp_set(a->delta, b->delta);
}

/*
  Rationale:

  let n1 == x->val.num , d1 == x->val.den, n2 = x->delta.num, d2 = x->delta.den

  pre: d1 > 0, d2 > 0

  case 1. d1 == 1:

  1.1 case n2  >= 0:
  x'= { val = { num = n1, den = d1 },  delta = { num = 0, den = 1 } }

  1.2 case n2 < 0:
  x' = { val = { num = n1 - 1, den = d1 }, delta = { num = 0, den = 1} }

  case 2. d1 != 1

  2.1 case n1 < 0:
  let q = floor((-n1) / d1)
  x' = { val = { num = -q-1, den = 1}, delta = { num = 0, den = 1} }

  2.2 case n1 >= 0:
  let q = floor(n1 / d1)
  x' = { val = { num = q, den = 1}, delta = { num = 0, den = 1 } }

*/
void
LAdelta_mp_floor(TLAdelta_mp* x)
{
	mpz_t num, den, q;
	mpz_init(num);
	mpz_init(den);
	mpz_init(q);
	mpq_get_num(num, x->val);
	mpq_get_den(den, x->val);
	if (mpz_cmp_ui(den, 1UL) == 0) {
		mpz_set(q, num);
		if (mpq_sgn(x->delta) < 0) mpz_sub_ui(q, q, 1UL);
	} else if (mpz_sgn(num) < 0) {
		mpz_neg(num, num);
		mpz_fdiv_q(q, num, den);
		mpz_add_ui(q, q, 1L);
		mpz_neg(q, q);
	} else
		mpz_fdiv_q(q, num, den);

	mpq_set_z(x->val, q);
	mpq_set_ui(x->delta, 0UL, 1UL);
	mpz_clear(num);
	mpz_clear(den);
	mpz_clear(q);
}

void
LAdelta_mp_increment(TLAdelta_mp* delta)
{
	mpq_t mpq_one;
	assert(LAdelta_mp_is_int(delta));
	mpq_init(mpq_one);
	mpq_set_ui(mpq_one, 1L, 1L);
	mpq_add(delta->val, delta->val, mpq_one);
	mpq_clear(mpq_one);
}

void
LAdelta_mp_decrement(TLAdelta_mp* delta)
{
	mpq_t mpq_one;
	assert(LAdelta_mp_is_int(delta));
	mpq_init(mpq_one);
	mpq_set_ui(mpq_one, 1L, 1L);
	mpq_sub(delta->val, delta->val, mpq_one);
	mpq_clear(mpq_one);
}

void
LAdelta_mp_normalize(TLAdelta_mp* delta)
{
	LArational_mp_normalize(delta->val);
}
