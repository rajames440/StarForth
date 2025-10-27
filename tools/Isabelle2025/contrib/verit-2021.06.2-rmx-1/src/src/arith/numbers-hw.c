/*
  --------------------------------------------------------------
  Functions to deal with numbers
  --------------------------------------------------------------
*/

#include "numbers-hw.h"

#include "utils/general.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#define GCD_CACHE

/* PF a bound for a variable v should be understood as
   v >= val + delta \delta, where \delta is an infinitesimal value
   If delta is 0, inequality is non-strict */

bool LA_overflow = false;

/*
  --------------------------------------------------------------
  Helpers
  --------------------------------------------------------------
*/

/*
  gcd
*/

#ifdef GCD_CACHE
#include "gcd-cache.def"
#endif

/**
   \brief compute the gcd of two values
   \param a first value
   \param b second value
   \return the gcd of the two values */
unsigned
gcd(unsigned a, unsigned b)
{
	unsigned d;
	unsigned t = 0;
	if (!a) return b;
	/* PF find the common power of two */
	for (t = 0; ((a | b) & 1) == 0; ++t) {
		a >>= 1;
		b >>= 1;
	}
	/* PF then eliminate all factors of two */
	while ((a & 1) == 0) a >>= 1;
#ifdef GCD_CACHE
	/* DD could test also with: ((a | b) & ~((unsigned long) 0xFF) == 0) */
	while (a >= GCD_CACHE_SZ || b >= GCD_CACHE_SZ) {
		if (!b) return a << t;
		/* PF invariant: a is not divisible by 2 */
		while ((b & 1) == 0) b >>= 1;
		if (a < b)
			b -= a;
		else {
			d = a - b;
			a = b;
			b = d;
		}
		b >>= 1u;
	}
	return ((unsigned)GCD_CACHE_ARRAY[a][b] << t);
#else
	/* PF then compute using standard Euclid */
	do {
		/* PF invariant: a is not divisible by 2 */
		while ((b & 1) == 0) b >>= 1;
		if (a < b)
			b -= a;
		else {
			d = a - b;
			a = b;
			b = d;
		}
		b >>= 1u;
	} while (b != 0);
	return a << t;
#endif
}

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

/*
  --------------------------------------------------------------
  Delta numbers
  --------------------------------------------------------------
*/

/* LAdelta_hw_init */

/* LAdelta_hw_clear */

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

/* LAsigned_hw_set */

/* LAsigned_hw_neg */

/* LAsigned_hw_add */

/* LAsigned_hw_mult */

/* LAsigned_hw_com */

/* LAsigned_hw_set_zero */

/* LAsigned_hw_is_zero */

/* LAsigned_hw_set_one */

/* LAsigned_hw_sign_diff */

/*
  --------------------------------------------------------------
  Rationals
  --------------------------------------------------------------
*/

void
LArational_hw_set_one(TLArational_hw rational)
{
	rational->num = 1;
	rational->den = 1;
}

void
LArational_hw_set_zero(TLArational_hw rational)
{
	rational->num = 0;
	rational->den = 1;
}

bool
LArational_hw_is_zero(TLArational_hw rational)
{
	assert(rational->den);
	return rational->num == 0;
}

bool
LArational_hw_is_neg(TLArational_hw rational)
{
	assert(rational->den);
	return rational->num < 0;
}

bool
LArational_hw_is_int(TLArational_hw rational)
{
	assert(rational->den);
	return rational->den == 1;
}

void
LArational_hw_str(TLArational_hw rational, char* str)
{
	unsigned i;
	signed long long num = 0;
	unsigned long long den = 1;
	if (str[0] == 0) {
		rational->num = 0;
		rational->den = 1;
		return;
	}
	if (str[0] == '#') {
		/* binary #b[01]+ */
		if (str[1] == 'b') {
			for (i = 2; str[i]; i++) {
				num <<= 1;
				if (str[i] != '0' && str[i] != '1') {
					my_error("strange number\n");
					return;
				}
				num += str[i] == '1';
				OVERFLOW_CHECK_GOTO(num > INT_MAX);
			}
			rational->num = (signed int)num;
			rational->den = (unsigned int)den;
			return;
		}
		/* binary #x[0-9A-Fa-f]+ */
		if (str[1] == 'x') {
			for (i = 2; str[i]; i++) {
				num <<= 4;
				if ((str[i] < '0' || str[i] > '9') && (str[i] < 'A' || str[i] > 'F')) {
					my_error("strange number\n");
					return;
				}
				if (str[i] >= '0' && str[i] <= '9')
					num += str[i] - '0';
				else
					num += str[i] - 'A' + 10;
				OVERFLOW_CHECK_GOTO(num > INT_MAX);
			}
			rational->num = (signed int)num;
			rational->den = (unsigned int)den;
			return;
		}
		my_error("strange number\n");
		return;
	}
	/* Numeral [\-]?(0|[1-9][0-9]*)
     Rational [\-]?[1-9][0-9]* / [0-9]+[1-9]
     Decimal [\-]?(0|[1-9][0-9]*)\.[0-9]+ */
	i = 0;
	if (str[i] == '-') i++;
	while (str[i] >= '0' && str[i] <= '9') {
		num *= 10;
		num += str[i++] - '0';
		OVERFLOW_CHECK_GOTO(num > INT_MAX);
	}
	if (str[i] == '.') {
		i++;
		while (str[i] >= '0' && str[i] <= '9') {
			num *= 10;
			den *= 10;
			num += str[i++] - '0';
			OVERFLOW_CHECK_GOTO(num > INT_MAX || den > INT_MAX);
		}
	} else if (str[i] == '/') {
		i++;
		den = 0;
		while (str[i] >= '0' && str[i] <= '9') {
			den *= 10;
			den += (unsigned)(str[i++] - '0');
			OVERFLOW_CHECK_GOTO(den > INT_MAX);
		}
	}
	if (str[i]) {
		my_error("strange number\n");
		return;
	}
	if (str[0] == '-') num *= -1;
	rational->num = (signed int)num;
	rational->den = (unsigned int)den;
	return;
on_overflow:
	rational->num = 1;
	rational->den = 1u;
}

void
LArational_hw_mpz(TLArational_hw rational, mpz_t mpz)
{
	OVERFLOW_CHECK_GOTO(mpz_cmp_si(mpz, INT_MAX) > 0);
	OVERFLOW_CHECK_GOTO(mpz_cmp_si(mpz, INT_MIN) < 0);
	rational->num = (signed int)mpz_get_si(mpz);
	rational->den = 1u;
	return;
on_overflow:
	rational->num = 1;
	rational->den = 1u;
}

void
LArational_hw_mpq(TLArational_hw rational, mpq_t mpq)
{
	OVERFLOW_CHECK_GOTO(mpz_cmp_si(mpq_numref(mpq), INT_MAX) > 0);
	OVERFLOW_CHECK_GOTO(mpz_cmp_si(mpq_denref(mpq), INT_MAX) > 0);
	OVERFLOW_CHECK_GOTO(mpz_cmp_si(mpq_numref(mpq), INT_MIN) < 0);
	OVERFLOW_CHECK_GOTO(mpz_cmp_si(mpq_denref(mpq), INT_MIN) < 0);
	rational->num = (signed int)mpz_get_si(mpq_numref(mpq));
	rational->den = (unsigned)mpz_get_ui(mpq_denref(mpq));
	return;
on_overflow:
	rational->num = 1;
	rational->den = 1u;
}

void
LArational_hw_normalize(TLArational_hw rational)
{
	int mask = rational->num >> (sizeof(int) * CHAR_BIT - 1);
	/* PF all this should not overflow, num is never INT_MIN */
	unsigned c = gcd((unsigned)((rational->num + mask) ^ mask), rational->den);
	rational->num /= (signed int)c;
	rational->den /= c;
	assert(rational->den);
}

void
LArational_hw_neg(TLArational_hw rational)
{
	/* PF all this should not overflow */
	rational->num *= -1;
}

void
LArational_hw_add(TLArational_hw rational1, TLArational_hw rational2)
{
	unsigned g = gcd(rational1->den, rational2->den);
	signed long long a, b;
	unsigned long long c;
	a = rational2->den / g;
	a *= (signed long long)rational1->num;
	OVERFLOW_CHECK_GOTO(a > INT_MAX || a <= INT_MIN);
	b = rational1->den / g;
	b *= (signed long long)rational2->num;
	OVERFLOW_CHECK_GOTO(b > INT_MAX || b <= INT_MIN);
	a += b;
	OVERFLOW_CHECK_GOTO(a > INT_MAX || a <= INT_MIN);
	rational1->num = (signed int)a;
	c = rational1->den;
	c /= g;
	c *= (unsigned long long)rational2->den;
	OVERFLOW_CHECK_GOTO(c > UINT_MAX);
	rational1->den = (unsigned int)c;
	LArational_hw_normalize(rational1);
	return;
on_overflow:
	rational1->num = 1;
	rational1->den = 1u;
}

void
LArational_hw_mult(TLArational_hw rational1, const TLArational_hw rational2)
{
	signed long long a;
	unsigned long long b;
	/* PF all this should not overflow
     Several functions use the same construction.
     Make sure all are patched if this one is found buggy
     PF2DD please review
     DD2PF Indeed I think this is buggy:
     signed long long a;
     signed int b = ...;
     signed int c = ...;
     a = b * c;
     The multiplication is carried out on the type signed int and
     later casted to signed long long. If an overflow occurs, then
     it is not represented as a carry bit in a. I found this provokes
     runtime errors with clang/MacOS.
     To avoid such problem, I systematically add the cast operator
     even if that might look or even be redundant.
  */
	a = (signed long long)rational1->num * (signed long long)rational2->num;
	b = (unsigned long long)rational1->den * (unsigned long long)rational2->den;
	OVERFLOW_CHECK_GOTO(a > INT_MAX || a <= INT_MIN || b > UINT_MAX);
	rational1->num = (int)a;
	rational1->den = (unsigned)b;
	/* IMPROVE: overflow should be only after normalization */
	LArational_hw_normalize(rational1);
	return;
on_overflow:
	rational1->num = 1;
	rational1->den = 1u;
}

void
LArational_hw_mult_s(TLArational_hw rational1, TLAsigned_hw signed2)
{
	signed long long a;
	/* PF all this should not overflow
     Several functions use the same construction.
     Make sure all are patched if this one is found buggy
     PF2DD please review
     DD2PF Indeed I think this is buggy:
     signed long long a;
     signed int b = ...;
     signed int c = ...;
     a = b * c;
     The multiplication is carried out on the type signed int and
     later casted to signed long long. If an overflow occurs, then
     it is not represented as a carry bit in a. I found this provokes
     runtime errors with clang/MacOS.
     To avoid such problem, I systematically add the cast operator
     even if that might look or even be redundant.
  */
	int mask = signed2 >> (sizeof(int) * CHAR_BIT - 1);
	/* PF all this should not overflow, num is never INT_MIN */
	unsigned c = gcd((unsigned)((signed2 + mask) ^ mask), rational1->den);
	a = (signed long long)rational1->num *
		(signed long long)(signed2 / (signed int)c);
	rational1->den /= c;
	assert(rational1->den);
	OVERFLOW_CHECK_GOTO(a > INT_MAX || a <= INT_MIN);
	rational1->num = (int)a;
	return;
on_overflow:
	rational1->num = 1;
	rational1->den = 1u;
}

void
LArational_hw_div(TLArational_hw rational1, TLArational_hw rational2)
{
	signed int mask;
	signed long long a;
	unsigned long long b;
	mask = rational2->num >> (sizeof(signed int) * CHAR_BIT - 1);
	a = (signed long long)((rational1->num + mask) ^ mask) *
		(signed long long)rational2->den;
	b = (unsigned long long)rational1->den *
		(unsigned long long)((rational2->num + mask) ^ mask);
	OVERFLOW_CHECK_GOTO(a > INT_MAX || a <= INT_MIN || b > UINT_MAX);
	rational1->num = (int)a;
	rational1->den = (unsigned)b;
	/* IMPROVE: overflow should be only after normalization */
	LArational_hw_normalize(rational1);
	return;
on_overflow:
	rational1->num = 1;
	rational1->den = 1u;
}

void
LArational_hw_div_s(TLArational_hw rational1, TLAsigned_hw signed2)
{
	unsigned long long a;
	int mask = signed2 >> (sizeof(int) * CHAR_BIT - 1);
	/* PF all this should not overflow, num is never INT_MIN */
	unsigned int c = (unsigned int)((signed2 + mask) ^ mask);
	/* change sign of num according to signed2 */
	rational1->num = (rational1->num + mask) ^ mask;
	mask = rational1->num >> (sizeof(int) * CHAR_BIT - 1);
	c = gcd(((unsigned)((rational1->num + mask) ^ mask)), c);
	rational1->num /= (signed int)c;
	mask = signed2 >> (sizeof(int) * CHAR_BIT - 1);
	a = (unsigned long long)rational1->den *
		(unsigned long long)((unsigned long)((signed2 + mask) ^ mask) / c);
	OVERFLOW_CHECK_GOTO(a > UINT_MAX);
	rational1->den = (unsigned int)a;
	return;
on_overflow:
	rational1->num = 1;
	rational1->den = 1u;
}

bool
LArational_hw_eq(TLArational_hw rational1, TLArational_hw rational2)
{
	signed long long a, b;
	a = rational1->num;
	b = rational2->num;
	a *= (signed long long)rational2->den;
	b *= (signed long long)rational1->den;
	return (a == b);
}

bool
LArational_hw_leq(TLArational_hw rational1, TLArational_hw rational2)
{
	signed long long a, b;
	a = rational1->num;
	b = rational2->num;
	a *= (signed long long)rational2->den;
	b *= (signed long long)rational1->den;
	return (a <= b);
}

bool
LArational_hw_less(TLArational_hw rational1, TLArational_hw rational2)
{
	signed long long a, b;
	/* PF all this should not overflow */
	a = rational1->num;
	b = rational2->num;
	a *= (signed long long)rational2->den;
	b *= (signed long long)rational1->den;
	return (a < b);
}

void
LArational_hw_lcm_aux(TLAsigned_hw* Plcm, TLArational_hw rational)
{
	unsigned long long a;
	assert((*Plcm) >= 0);
	a = (unsigned long long)*Plcm;
	a /= gcd((unsigned)*Plcm, rational->den);
	a *= rational->den;
	OVERFLOW_CHECK_GOTO(a > INT_MAX);
	*Plcm = (signed int)a;
	return;
on_overflow:
	*Plcm = 1;
}

void
LArational_hw_mult_to_signed_aux(TLAsigned_hw* Psigned, TLArational_hw rational)
{
	signed long long a;
	assert(*Psigned % (int)rational->den == 0);
	a = (signed long long)*Psigned;
	a /= (signed long long)rational->den;
	a *= (signed long long)rational->num;
	OVERFLOW_CHECK_GOTO(a > INT_MAX || a <= INT_MIN);
	*Psigned = (signed int)a;
	return;
on_overflow:
	*Psigned = 1;
}

/*
  --------------------------------------------------------------
  Delta numbers
  --------------------------------------------------------------
*/

void
LAdelta_hw_set_zero(TLAdelta_hw* delta)
{
	delta->val.num = 0;
	delta->val.den = 1;
	delta->delta.num = 0;
	delta->delta.den = 1;
}

void
LAdelta_hw_set_one(TLAdelta_hw* delta)
{
	delta->val.num = 1;
	delta->val.den = 1;
	delta->delta.num = 0;
	delta->delta.den = 1;
}

bool
LAdelta_hw_is_zero(TLAdelta_hw* delta)
{
	return delta->val.num == 0 && delta->delta.num == 0;
}

bool
LAdelta_hw_is_int(TLAdelta_hw* delta)
{
	return (LArational_hw_is_int(&delta->val) && delta->delta.num == 0);
}

void
LAdelta_hw_int(TLAdelta_hw* delta, int val)
{
	delta->val.num = val;
	delta->val.den = 1;
	delta->delta.num = 0;
	delta->delta.den = 1;
}

void
LAdelta_hw_rat(TLAdelta_hw* delta, int num, unsigned den)
{
	delta->val.num = num;
	delta->val.den = den;
	delta->delta.num = 0;
	delta->delta.den = 1;
}

void
LAdelta_hw_rat_delta(TLAdelta_hw* delta, int num, unsigned den, int eps)
{
	delta->val.num = num;
	delta->val.den = den;
	delta->delta.num = eps;
	delta->delta.den = 1;
}

bool
LAdelta_hw_eq(TLAdelta_hw* delta1, TLAdelta_hw* delta2)
{
	return LArational_hw_eq(&delta1->val, &delta2->val) &&
		LArational_hw_eq(&delta1->delta, &delta2->delta);
}

bool
LAdelta_hw_leq(TLAdelta_hw* delta1, TLAdelta_hw* delta2)
{
	signed long long a, b;
	/* PF all this should not overflow */
	a = delta1->val.num;
	b = delta2->val.num;
	a *= delta2->val.den;
	b *= delta1->val.den;
	return a < b ||
		((a == b) && LArational_hw_leq(&delta1->delta, &delta2->delta));
}

int
LAdelta_hw_cmp(TLAdelta_hw* delta1, TLAdelta_hw* delta2)
{
	signed long long a, b;
	a = delta1->val.num;
	b = delta2->val.num;
	a *= (signed long long)delta2->val.den;
	b *= (signed long long)delta1->val.den;
	a -= b;
	if (a)
		return ((int)((a >> (sizeof(signed long long) * CHAR_BIT - 1)) << 1)) + 1;
	a = delta1->delta.num;
	b = delta2->delta.num;
	a *= (signed long long)delta2->delta.den;
	b *= (signed long long)delta1->delta.den;
	a -= b;
	return ((int)((a >> (sizeof(signed long long) * CHAR_BIT - 1)) << 1)) +
		(a != 0);
}

bool
LAdelta_hw_less(TLAdelta_hw* delta1, TLAdelta_hw* delta2)
{
	signed long long a, b;
	a = delta1->val.num;
	b = delta2->val.num;
	a *= (signed long long)delta2->val.den;
	b *= (signed long long)delta1->val.den;
	return a < b ||
		((a == b) && LArational_hw_less(&delta1->delta, &delta2->delta));
}

void
LAdelta_hw_addmult(TLAdelta_hw* delta0, TLAdelta_hw* delta1, TLAsigned_hw a)
{
	signed long long num2;
	unsigned long long den2;
	num2 = (signed long long)delta0->val.num * (signed long long)delta1->val.den;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	delta0->val.num = (signed int)num2;
	num2 = (signed long long)delta1->val.num * (signed long long)delta0->val.den;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	num2 *= (signed long long)a;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	num2 += (signed long long)delta0->val.num;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	delta0->val.num = (signed int)num2;
	den2 =
		(unsigned long long)delta0->val.den * (unsigned long long)delta1->val.den;
	OVERFLOW_CHECK_GOTO(den2 > INT_MAX);
	delta0->val.den = (unsigned int)den2;
	num2 = delta0->delta.num * (signed int)delta1->delta.den;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	delta0->delta.num = (signed int)num2;
	num2 =
		(signed long long)delta1->delta.num * (signed long long)delta0->delta.den;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	num2 *= (signed long long)a;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	num2 += delta0->delta.num;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	delta0->delta.num = (signed int)num2;
	den2 = (unsigned long long)delta0->delta.den *
		(unsigned long long)delta1->delta.den;
	OVERFLOW_CHECK_GOTO(den2 > INT_MAX);
	delta0->delta.den = (unsigned int)den2;
	return;
on_overflow:
	delta0->val.num = 1;
	delta0->val.den = 1u;
	delta0->delta.num = 1;
	delta0->delta.den = 1u;
}

void
LAdelta_hw_minus(TLAdelta_hw* delta0, TLAdelta_hw* delta1, TLAdelta_hw* delta2)
{
	signed long long num2;
	unsigned long long den2;
	den2 =
		(unsigned long long)delta1->val.den * (unsigned long long)delta2->val.den;
	OVERFLOW_CHECK_GOTO(den2 > INT_MAX);
	delta0->val.den = (unsigned int)den2;
	num2 = (signed long long)delta1->val.num * (signed long long)delta2->val.den;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	delta0->val.num = (signed int)num2;
	num2 = (signed long long)delta2->val.num * (signed long long)delta1->val.den;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	num2 = (signed long long)delta0->val.num - num2;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	delta0->val.num = (signed int)num2;
	den2 = (unsigned long long)delta1->delta.den *
		(unsigned long long)delta2->delta.den;
	OVERFLOW_CHECK_GOTO(den2 > INT_MAX);
	delta0->delta.den = (unsigned int)den2;
	num2 =
		(signed long long)delta1->delta.num * (signed long long)delta2->delta.den;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	delta0->delta.num = (signed int)num2;
	num2 =
		(signed long long)delta2->delta.num * (signed long long)delta1->delta.den;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	num2 = (signed long long)delta0->delta.num - num2;
	OVERFLOW_CHECK_GOTO(num2 > INT_MAX || num2 <= INT_MIN);
	delta0->delta.num = (signed int)num2;
	return;
on_overflow:
	delta0->val.num = 1;
	delta0->val.den = 1u;
	delta0->delta.num = 1;
	delta0->delta.den = 1u;
}

void
LAdelta_hw_div_opp(TLAdelta_hw* delta0, TLAsigned_hw a)
{
	signed long mask = a >> (sizeof(int) * CHAR_BIT - 1);
	/* PF all this should not overflow, num is never INT_MIN */
	unsigned long long den2;
	den2 = (unsigned long long)delta0->val.den *
		(unsigned long long)((a + mask) ^ mask);
	OVERFLOW_CHECK_GOTO(den2 > INT_MAX);
	delta0->val.den = (unsigned int)den2;
	delta0->val.num = (a < 0) ? delta0->val.num : -delta0->val.num;
	den2 = (unsigned long long)delta0->delta.den *
		(unsigned long long)((a + mask) ^ mask);
	OVERFLOW_CHECK_GOTO(den2 > INT_MAX);
	delta0->delta.den = (unsigned int)den2;
	delta0->delta.num = (a < 0) ? delta0->delta.num : -delta0->delta.num;
	return;
on_overflow:
	delta0->val.num = 1;
	delta0->val.den = 1u;
	delta0->delta.num = 1;
	delta0->delta.den = 1u;
}

void
LAdelta_hw_print(TLAdelta_hw* delta)
{
	printf("%d", delta->val.num);
	if (delta->val.den != 1) printf("/%d", delta->val.den);
	if (delta->delta.num) {
		printf("+ %d", delta->delta.num);
		if (delta->delta.den != 1) printf("/%d", delta->delta.den);
		printf(" d");
	}
}

#if 0

void LAdelta_hw_ceil(TLAdelta_hw * x);

void
LAdelta_hw_ceil(TLAdelta_hw * x)
{
  /* x->val is either:
     - an integer
     if x->delta > 0, then ceil(x) is {x->val + 1, 0}
     if x->delta <= 0, then ceil(x) is {x->val, 0}
     - a negative non-integral rational
     ceil(x) is {quotient(- x->val.num, x->val.den), 0}
     - a positive non-integral rational
     ceil(x) is {quotient(x->val.num, x->val.den) + 1, 0}
  */
  if (x->val.den == 1)
    {
      if (x->delta.num > 0)
        {
          OVERFLOW_CHECK_GOTO(x->val.num == INT_MAX);
          ++x->val.num;
        }
    }
  else if (x->val.num < 0)
    {
      signed long long num2;
      signed long long den2;
      num2 = (signed long long) x->val.num;
      num2 = -num2;
      den2 = (signed long long) x->val.den;
      x->val.num = (signed) -(num2/den2);
    }
  else
    {
      signed long long num2;
      signed long long den2;
      num2 = (signed long long) x->val.num;
      den2 = (signed long long) x->val.den;
      x->val.num = (signed) (1 + num2/den2);
    }
  x->val.den = 1;
  x->delta.num = 0;
  x->delta.den = 1;
  return;
 on_overflow:
  x->val.num = 1;
  x->val.den = 1u;
  x->delta.num = 1;
  x->delta.den = 1u;
}

#endif

void
LAdelta_hw_floor(TLAdelta_hw* x)
{
	/* x->val is either:
     - an integer
     if x->delta >= 0, then floor(x) is {x->val, 0}
     if x->delta < 0, then floor(x) is {x->val - 1, 0}
     - a negative non-integral rational
     floor(x) is {- quotient(- x->val.num, x->val.den) - 1, 0}
     - a positive non-integral rational
     floor is {quotient(x->val.num, x->val.den), 0}
  */
	if (x->val.den == 1) {
		if (x->delta.num < 0) {
			OVERFLOW_CHECK_GOTO(x->val.num == INT_MIN);
			x->val.num -= 1;
		}
	} else if (x->val.num < 0) {
		signed long long num2 = (signed long long)x->val.num;
		signed long long den2 = (signed long long)x->val.den;
		x->val.num = (signed)(-1 - (-num2) / den2);
	} else {
		unsigned long long num2 = (unsigned long long)x->val.num;
		unsigned long long den2 = (unsigned long long)x->val.den;
		x->val.num = (signed)(num2 / den2);
	}
	x->val.den = 1;
	x->delta.num = 0;
	x->delta.den = 1;
	return;
on_overflow:
	x->val.num = 1;
	x->val.den = 1u;
	x->delta.num = 1;
	x->delta.den = 1u;
}

void
LAdelta_hw_increment(TLAdelta_hw* delta)
{
	assert(LAdelta_hw_is_int(delta));
	OVERFLOW_CHECK_GOTO(delta->val.num == INT_MAX);
	delta->val.num += 1;
	return;
on_overflow:
	delta->val.num = 1;
	delta->val.den = 1u;
	delta->delta.num = 1;
	delta->delta.den = 1u;
}

void
LAdelta_hw_decrement(TLAdelta_hw* delta)
{
	assert(LAdelta_hw_is_int(delta));
	OVERFLOW_CHECK_GOTO(delta->val.num == INT_MIN);
	delta->val.num -= 1;
	return;
on_overflow:
	delta->val.num = 1;
	delta->val.den = 1u;
	delta->delta.num = 1;
	delta->delta.den = 1u;
}

void
LAdelta_hw_normalize(TLAdelta_hw* delta)
{
	LArational_hw_normalize(&(delta->val));
}

/* LArational_mp_set */

/* LAdelta_mp_set */
