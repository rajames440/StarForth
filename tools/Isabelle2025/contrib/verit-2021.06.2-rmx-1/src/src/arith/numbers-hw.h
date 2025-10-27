/* -*- mode: C -*-
   --------------------------------------------------------------
   Types and functions to deal with numbers.

   There are two versions: HW (hardware) and MP (multi-precision).

   Common definitions are in *-hw-*.tpl files (tpl for template),
   where the strings hw and HW represent the version. These files
   are then processed by cpp with hw=hw, HW=HW and hw=mp, HW=MP
   to generate C code.

   Some specific definitions are found in sections of the template
   files guarded by conditional preprocessor directives, and others
   are coded directly in some C file.
   --------------------------------------------------------------
*/

#ifndef __NUMBERS_HW_H
#define __NUMBERS_HW_H

#define HW_VERSION

#include <stdbool.h>
#include <stdio.h>

/*
  --------------------------------------------------------------
  Overflow stuff
  --------------------------------------------------------------
*/

#ifdef HW_VERSION

/**
   \brief records if any overflow has taken place */
extern bool LA_overflow;

#define RETURN_IF_OVERFLOW(A) \
	{ \
		if (LA_overflow) return A; \
	}

#ifdef DEBUG_EXCEPTION_OVERFLOW
#define THROW_OVERFLOW_WARNING my_warning("overflow\n");
#else
#define THROW_OVERFLOW_WARNING
#endif

/** \brief set global overflow subject to a condition */
#define OVERFLOW_CHECK(cond) \
	{ \
		if (cond) { \
			LA_overflow = 1; \
			THROW_OVERFLOW_WARNING; \
		} \
	}
#define OVERFLOW_CHECK_GOTO(cond) \
	{ \
		if (cond) { \
			LA_overflow = 1; \
			THROW_OVERFLOW_WARNING; \
			goto on_overflow; \
		} \
	}
#define OVERFLOW_CHECK_SET(cond, val) \
	{ \
		if (cond) { \
			LA_overflow = 1; \
			THROW_OVERFLOW_WARNING; \
			val = 1; \
		} \
	}
#else /* MP_VERSION */

#define RETURN_IF_OVERFLOW(A)

#endif

#include <gmp.h>

/*
  --------------------------------------------------------------
  Utils
  --------------------------------------------------------------
*/

#ifdef HW_VERSION

/** \brief Greatest common divisor */
unsigned gcd(const unsigned a, const unsigned b);

/**
   \brief Absolute value for signed integers.
   \param[in] __in a signed value
   \param[out] __out a signed lvalue */
#define SIGNED_ABS(__in, __out) \
	{ \
		signed __mask; \
		__mask = (__in) >> (sizeof(signed int) * CHAR_BIT - 1); \
		(__out) = (unsigned)(((__in) + __mask) ^ __mask); \
	}

#endif /* HW_VERSION */

/*
  --------------------------------------------------------------
  Signed
  --------------------------------------------------------------
*/

/**
   \typedef TLAsigned_hw
   \brief signed number
   \remark C native signed long, but arbitrary size are available if needed
   \remark max signed long is 2^31-1 on 32 bits, 2^63-1 on 64 bits
   \remark min signed long is -2^31+1 on 32 bits, -2^63+1 on 64 bits
   \remark in principle, min unsigned long could be one less, but for
   simplicity in overflow detection, we will loose the LONG_MIN value
   \remark we use long long to detect overflow and compute gcd afterwards
   However, in this module, we will never let a stable value be larger
   than 32 bits */
#ifdef HW_VERSION
typedef signed int TLAsigned_hw;
#define LAsigned_hw_init(a)
#define LAsigned_hw_set(a, b) (a) = (b)
#define LAsigned_hw_clear(a)
#else
typedef mpz_t TLAsigned_hw;
#define LAsigned_hw_init mpz_init
#define LAsigned_hw_set mpz_set
#define LAsigned_hw_clear mpz_clear
#endif

/**
   \brief executes the following operation a = a * b
   \param[in,out] a lvalue storing a TLAsigned_hw
   \param[in] b a TLAsigned_hw expression */
#ifdef HW_VERSION
#define LAsigned_hw_mult(a, b) \
	{ \
		signed long long tmp1; \
		tmp1 = (signed long long)a * (signed long long)b; \
		OVERFLOW_CHECK_SET(tmp1 > INT_MAX || tmp1 <= INT_MIN, tmp1); \
		a = (signed int)tmp1; \
	}
#else
#define LAsigned_hw_mult(a, b) mpz_mul(a, a, b);
#endif

/**
   \brief executes the following operation a = -(b * c)
   \param[in] a LAsigned
   \param[in] b LAsigned
   \param[out] c LAsigned */
#ifdef HW_VERSION
#define LAsigned_hw_mult_neg(a, b, c) \
	{ \
		signed long long tmp1; \
		tmp1 = (signed long long)b * (signed long long)c; \
		OVERFLOW_CHECK_SET(tmp1 > INT_MAX || tmp1 <= INT_MIN, tmp1) \
		a = -(signed int)tmp1; \
	}
#else
#define LAsigned_hw_mult_neg(a, b, c) \
	{ \
		mpz_mul(a, b, c); \
		mpz_neg(a, a); \
	}
#endif

/**
   \brief executes the following operation a = -b
   \param[out] a LAsigned
   \param[in] b LAsigned */
#ifdef HW_VERSION
#define LAsigned_hw_neg(a, b) (a) = -(b)
#else
#define LAsigned_hw_neg(a, b) mpz_neg(a, b)
#endif

/**
   \brief execute the following operation a = a * c - b * d
   \param[in,out] a LAsigned
   \param[in] b LAsigned
   \param[in] c LAsigned
   \param[in] d LAsigned */
#ifdef HW_VERSION
#define LAsigned_hw_com(a, b, c, d) \
	{ \
		signed long long tmp1, tmp2; \
		tmp1 = (signed long long)a * (signed long long)c; \
		tmp2 = (signed long long)b * (signed long long)d; \
		OVERFLOW_CHECK_SET(tmp1 > INT_MAX || tmp1 <= INT_MIN, tmp1); \
		OVERFLOW_CHECK_SET(tmp2 > INT_MAX || tmp2 <= INT_MIN, tmp2); \
		tmp1 -= tmp2; \
		OVERFLOW_CHECK_SET(tmp1 > INT_MAX || tmp1 <= INT_MIN, tmp1); \
		a = (signed int)tmp1; \
	}
#else
#define LAsigned_hw_com(a, b, c, d) \
	{ \
		mpz_mul(a, a, c); \
		mpz_submul(a, b, d); \
	}
#endif

/**
   \brief sets a LAsigned value
   \param[out] a LAsigned
   \param[in] b a signed long int */
#ifdef HW_VERSION
#define LAsigned_hw_set_si(a, b) \
	{ \
		a = b; \
	}
#else
#define LAsigned_hw_set_si(a, b) mpz_set_si(a, b)
#endif

/**
   \brief check if a signed value is 0
   \param[in] a LAsigned
   \return true iff a is zero */
#ifdef HW_VERSION
#define LAsigned_hw_is_zero(a) ((a) == 0)
#else
#define LAsigned_hw_is_zero(a) (mpz_sgn(a) == 0)
#endif

/**
   \brief check if a signed value is < 0
   \param[in] a LAsigned
   \return true iff a is zero */
#ifdef HW_VERSION
#define LAsigned_hw_is_neg(a) ((a) < 0)
#else
#define LAsigned_hw_is_neg(a) (mpz_sgn(a) < 0)
#endif

/**
   \brief checks if a and b have different sign
   \param[in] a pointer to a LAsigned
   \param[in] b pointer to a LAsigned
   \return true iff signs differ */
#ifdef HW_VERSION
#define LAsigned_hw_neq(a, b) (a != b)
#else
#define LAsigned_hw_neq(a, b) (mpz_cmp(a, b))
#endif

/**
   \brief checks if a and b have different sign
   \param[in] a pointer to a LAsigned
   \param[in] b pointer to a LAsigned
   \return true iff signs differ */
#ifdef HW_VERSION
#define LAsigned_hw_sign_diff(a, b) \
	(((unsigned long)((a) ^ (b))) >> (sizeof(signed long) * CHAR_BIT - 1))
#else
#define LAsigned_hw_sign_diff(a, b) (mpz_sgn(a) != mpz_sgn(b))
#endif

/**
   \brief prints a on stdout
   \param[in] a LAsigned */
#ifdef HW_VERSION
#define LAsigned_hw_print(a) printf("%d", a)
#else
#define LAsigned_hw_print(a) mpz_out_str(stdout, 10, a)
#endif

/**
   \brief executes the following operation a = abs(b)
   \param[in,out] a lvalue storing a TLAsigned_hw
   \param[in] b a TLAsigned_hw expression */
#ifdef HW_VERSION
#define LAsigned_hw_abs(a, b) \
	{ \
		signed __mask; \
		__mask = (b) >> (sizeof(signed int) * CHAR_BIT - 1); \
		(a) = (((b) + __mask) ^ __mask); \
	}
#else
#define LAsigned_hw_abs(a, b) mpz_abs(a, b)
#endif

/**
   \brief executes the following operation a = gcd(a, b)
   \param[in,out] a lvalue storing a TLAsigned_hw
   \param[in] b a TLAsigned_hw expression */
#ifdef HW_VERSION
#define LAsigned_hw_gcd(a, b) a = (signed)gcd((unsigned)(a), (unsigned)(b))
#else
#define LAsigned_hw_gcd(a, b) mpz_gcd(a, a, b)
#endif

/**
   \brief tests if signed is different from one
   \param[in] a tested value */
#ifdef HW_VERSION
#define LAsigned_hw_notone(a) ((a) != 1)
#else
#define LAsigned_hw_notone(a) mpz_cmp_ui(a, 1UL)
#endif

/**
   \brief tests if signed is strictly positive
   \param[in] a tested value */
#ifdef HW_VERSION
#define LAsigned_hw_pos(a) ((a) > 0)
#else
#define LAsigned_hw_pos(a) (mpz_cmp_ui(a, 0UL) > 0)
#endif /* HW_VERSION */

/**
   \brief tests if one signed is strictly smaller than another signed
   \param[in] a first tested value
   \param[in] b second tested value */
#ifdef HW_VERSION
#define LAsigned_hw_smaller(a, b) ((a) < (b))
#else
#define LAsigned_hw_smaller(a, b) (mpz_cmp(a, b) < 0)
#endif /* HW_VERSION */

/**
   \brief Divides signed a exactly by signed b, and assigns result to a
   \param[in, out] a dividend
   \param[in] b divisor
   \pre the remainder of the division of a by b is 0 */
#ifdef HW_VERSION
#define LAsigned_hw_divex(a, b) (a) /= (b)
#else
#define LAsigned_hw_divex(a, b) mpz_divexact(a, a, b)
#endif /* HW_VERSION */

/**
   \brief Computes a key from a signed (e.g., for storing in a hash table)
   \param[in] a signed value */
#ifdef HW_VERSION
#define LAsigned_hw_key(a) ((unsigned)a)
#else
#define LAsigned_hw_key(a) ((unsigned)mpz_get_ui(a))
#endif /* HW_VERSION */

/*
  --------------------------------------------------------------
  Rationals
  --------------------------------------------------------------
*/

/**
   \typedef TLArational_hw
   \brief rational number
   \remark pair of C native int
   \remark INT_MIN is counted as overflow in num */
#ifdef HW_VERSION
typedef struct TLArational_hw
{
	signed int num;
	unsigned int den;
} TLArational_hw[1];
#define LArational_hw_init(a)
#define LArational_hw_set(a, b) ((*a) = (*b))
#define LArational_hw_clear(a)
#define LArational_hw_set_si(a, b) \
	do { \
		a->num = b; \
		a->den = 1; \
	} while (0)
#else
typedef mpq_t TLArational_hw;
#define LArational_hw_init mpq_init
#define LArational_hw_set mpq_set
#define LArational_hw_clear mpq_clear
#define LArational_hw_set_si mpq_set_z
#endif

/**
   \brief set a rational value to 1
   \param[out] rational a rational value */
void LArational_hw_set_one(TLArational_hw rational);

/**
   \brief set a rational value to 0
   \param[out] rational a rational value */
void LArational_hw_set_zero(TLArational_hw rational);

/**
   \brief check if a rational value is 0
   \param[in] rational a rational
   \return 1 if rational is zero */
bool LArational_hw_is_zero(TLArational_hw rational);

/**
   \brief sets rational to be the number represented by str
   \param[out] rational a rational value (in canonical form)
   \param[in] str a string */
void LArational_hw_str(TLArational_hw rational, char* str);

/**
   \brief sets rational to be the number represented by mpz
   \param[out] rational a rational value (in canonical form)
   \param[in] mpz a multiprecision integer */
void LArational_hw_mpz(TLArational_hw rational, mpz_t mpz);

/**
   \brief sets rational to be the number represented by mpq
   \param[out] rational a rational value (in canonical form)
   \param[in] mpq a multiprecision rational */
void LArational_hw_mpq(TLArational_hw rational, mpq_t mpq);

/**
   \brief normalize the value, i.e. divides num and den by their gcd
   \param[in,out] rational a rational value */
void LArational_hw_normalize(TLArational_hw rational);

/**
   \brief changes sign of rational
   \param[in,out] rational a rational value */
void LArational_hw_neg(TLArational_hw rational);

/**
   \brief check if a rational value is < 0
   \param[in] rational a rational
   \return 1 if rational is < 0 */
bool LArational_hw_is_neg(TLArational_hw rational);

/**
   \brief check if a rational value is integer
   \param[in] rational a rational
   \return 1 if rational is an integer */
bool LArational_hw_is_int(TLArational_hw rational);

/**
   \brief adds two rationals and puts result in first argument
   \param[in,out] rational1 a first rational value
   \param[in] rational2 a second rational value
   \remark normalizes */
void LArational_hw_add(TLArational_hw rational1, TLArational_hw rational2);

/**
   \brief multiplies two rationals and puts result in first argument
   \param[in,out] rational1 a first rational value
   \param[in] rational2 a second rational value
   \remark normalizes */
void LArational_hw_mult(
	TLArational_hw rational1, const TLArational_hw rational2);

/**
   \brief multiply a rational by a signed and put result in first argument
   \param[in,out] rational1 a rational value
   \param[in] signed2 a signed value
   \remark normalizes */
void LArational_hw_mult_s(TLArational_hw rational1, TLAsigned_hw signed2);

/**
   \brief divides two rationals and puts result in first argument
   \param[in,out] rational1 a rational value
   \param[in] rational2 a rational value
   \remark normalizes */
void LArational_hw_div(TLArational_hw rational1, TLArational_hw rational2);

/**
   \brief multiplies a rational by a signed and puts result in first argument
   \param[in,out] rational1 a rational value
   \param[in] signed2 a signed value
   \remark normalizes */
void LArational_hw_mult_s(TLArational_hw rational1, TLAsigned_hw signed2);

/**
   \brief divides a rational by a signed and puts result in first argument
   \param[in,out] rational1 a rational value
   \param[in] signed2 a signed value
   \remark normalizes */
void LArational_hw_div_s(TLArational_hw rational1, TLAsigned_hw signed2);

/**
   \brief compares two rationals
   \param[in] rational1 a rational value a
   \param[in] rational2 a rational value b
   \return true iff a is equal to b */
bool LArational_hw_eq(TLArational_hw rational1, TLArational_hw rational2);

/**
   \brief compares two rationals
   \param[in] rational1 a rational value a
   \param[in] rational2 a rational value b
   \return true iff a is smaller than or equal to b */
bool LArational_hw_leq(TLArational_hw rational1, TLArational_hw rational2);

/**
   \brief compares two rationals
   \param[in] rational1 a rational value a
   \param[in] rational2 a rational value b
   \return true iff a is strictly smaller than b */
bool LArational_hw_less(TLArational_hw rational1, TLArational_hw rational2);

#ifdef HW_VERSION
/**
   \brief gets the lcm of the denominator of *Prational and *Plcm
   \param[in,out] lcm a signed value
   \param[in] rational a rational value
   \remark stores the result in *Plcm */
#define LArational_hw_lcm(lcm, rational) LArational_hw_lcm_aux(&(lcm), rational)
void LArational_hw_lcm_aux(TLAsigned_hw* Plcm, TLArational_hw rational);
#else
/**
   \brief gets the lcm of the denominator of *Prational and *Plcm
   \param[in,out] lcm a signed value
   \param[in] rational a rational value
   \remark stores the result in *Plcm */
void LArational_hw_lcm(TLAsigned_hw lcm, TLArational_hw rational);
#endif

#ifdef HW_VERSION
/**
   \brief multiplies asigned by rational
   \param[in,out] asigned a lvalue containing signed value
   \param[in] rational a rational value
   \pre asigned is a multiple of rational's denominator.
   \remark stores the result in asigned
   \remark the result should be integer */
#define LArational_hw_mult_to_signed(asigned, rational) \
	LArational_hw_mult_to_signed_aux(&(asigned), rational)
void LArational_hw_mult_to_signed_aux(
	TLAsigned_hw* Pasigned, TLArational_hw rational);
#else
/**
   \brief multiplies asigned by rational
   \param[in,out] asigned a signed value
   \param[in] rational a rational value
   \remark stores the result in asigned
   \remark the result should be integer */
void LArational_hw_mult_to_signed(
	TLAsigned_hw asigned, TLArational_hw rational);
#endif

/**
   \brief prints a rational
   \param[in,out] a rational
   \remark for debuging purposes */
#ifdef HW_VERSION
#define LArational_hw_print(a) printf("%d / %u", a->num, a->den)
#else
#define LArational_hw_print(a) mpq_out_str(stdout, 10, a)
#endif

/*
  --------------------------------------------------------------
  Delta numbers
  --------------------------------------------------------------
*/

/**
   \typedef TLAdelta_hw
   \brief number with delta */
#ifdef HW_VERSION
typedef struct TLAdelta_hw
{
	struct TLArational_hw val;
	struct TLArational_hw delta;
} TLAdelta_hw;
#define LAdelta_hw_init(a)
#define LAdelta_hw_set(dst, src) (*dst) = (*src)
#define LAdelta_hw_clear(a)
#else
typedef struct TLAdelta_hw
{
	TLArational_hw val;
	TLArational_hw delta;
} TLAdelta_hw;
void LAdelta_hw_init(TLAdelta_hw*);
void LAdelta_hw_set(TLAdelta_hw* dst, TLAdelta_hw* src);
void LAdelta_hw_clear(TLAdelta_hw*);
#endif

#ifdef HW_VERSION
#define LAdelta_hw_set_rat(dst, src) (*dst).val = (**src)
#define LAdelta_hw_set_delta(dst, eps) \
	(*dst).delta.num = eps; \
	(*dst).delta.den = 1;
#define LAdelta_hw_get_rat(src, num, den) \
	{ \
		(num) = ((src).val.num); \
		(den) = (signed)((src).val.den); \
	}
#else
void LAdelta_hw_set_rat(TLAdelta_hw* dst, TLArational_hw* src);
void LAdelta_hw_set_delta(TLAdelta_hw* dst, int eps);
void LAdelta_hw_get_rat(
	TLAdelta_hw* src, TLAsigned_hw* Pnum, TLAsigned_hw* Pden);
#define LAdelta_hw_get_rat(src, num, den) \
	{ \
		(num) = (mpq_numref((src).val)); \
		(den) = (mpq_denref((src).val)); \
	}
#endif

/**
   \brief sets delta value to 0
   \param[out] delta value */
void LAdelta_hw_set_zero(TLAdelta_hw* delta);

/**
   \brief sets delta value to 1
   \param[out] delta value */
void LAdelta_hw_set_one(TLAdelta_hw* delta);

/**
   \brief checks if delta is 0
   \param[in] delta value
   \return true iff zero */
bool LAdelta_hw_is_zero(TLAdelta_hw* delta);

/**
   \brief checks if delta is an integer
   \param[in] delta value
   \return true iff delta is integer-valued
   \pre value delta must be normalized
   \sa LAdelta_hw_normalize */
bool LAdelta_hw_is_int(TLAdelta_hw* delta);

/**
   \brief sets value of delta according to int
   \param[out] delta
   \param[in] val its value */
void LAdelta_hw_int(TLAdelta_hw* delta, int val);

/**
   \brief sets value of delta according to arguments
   \param[out] delta
   \param[in] num numerator for its value
   \param[in] den denominator for its value */
void LAdelta_hw_rat(TLAdelta_hw* delta, int num, unsigned den);

/**
   \brief sets value of delta according to arguments
   \param[out] delta
   \param[in] num numerator for its value
   \param[in] den denominator for its value
   \param[in] eps coefficient of delta value */
void LAdelta_hw_rat_delta(TLAdelta_hw* delta, int num, unsigned den, int eps);

/**
   \brief checks if two delta values are equal
   \param[in] delta1 value
   \param[in] delta2 value
   \return true if delta1 == delta2 */
bool LAdelta_hw_eq(TLAdelta_hw* delta1, TLAdelta_hw* delta2);

/**
   \brief checks if a delta value is less than or equal to another
   \param[in] delta1 value
   \param[in] delta2 value
   \return true iff delta1 <= delta2 */
bool LAdelta_hw_leq(TLAdelta_hw* delta1, TLAdelta_hw* delta2);

/**
   \brief checks if a delta value is less than another
   \param[in] delta1 value
   \param[in] delta2 value
   \return true iff delta1 < delta2 */
bool LAdelta_hw_less(TLAdelta_hw* delta1, TLAdelta_hw* delta2);

/**
   \brief checks if two delta value are not equal
   \param[in] delta1 value
   \param[in] delta2 value
   \return true iff delta1 < delta2 */
bool LAdelta_hw_neq(TLAdelta_hw* delta1, TLAdelta_hw* delta2);

/**
   \brief compares two delta values
   \param[in] delta1 value
   \param[in] delta2 value
   \return negative if delta1 < delta2, positive if delta1 > delta2,
   zero otherwise */
int LAdelta_hw_cmp(TLAdelta_hw* delta1, TLAdelta_hw* delta2);

/**
   \brief computes delta0 = delta0 + delta1 * a
   \param[in,out] delta0 value
   \param[in] delta1 value
   \param[in] a value */
void LAdelta_hw_addmult(
	TLAdelta_hw* delta0, TLAdelta_hw* delta1, TLAsigned_hw a);

/**
   \brief computes delta0 = delta1 - delta2
   \param[out] delta0 value
   \param[in] delta1 value
   \param[in] delta2 value */
void LAdelta_hw_minus(
	TLAdelta_hw* delta0, TLAdelta_hw* delta1, TLAdelta_hw* delta2);

/**
   \brief computes delta0 = - delta0 / a
   \param[in,out] delta0 value
   \param[in] a value */
void LAdelta_hw_div_opp(TLAdelta_hw* delta0, TLAsigned_hw a);

/**
   \brief print value of delta
   \param[in] delta */
void LAdelta_hw_print(TLAdelta_hw* delta);

/**
   \brief computes \f$delta = \lfloor delta \rfloor\f$
   \param[in, out] delta value */
void LAdelta_hw_floor(TLAdelta_hw* delta);

/**
   \brief computes delta = delta + 1
   \param[in,out] delta value */
void LAdelta_hw_increment(TLAdelta_hw* delta);

/**
   \brief computes delta = delta - 1
   \param[in,out] delta value */
void LAdelta_hw_decrement(TLAdelta_hw* delta);

/**
   \param[in,out] delta value */
void LAdelta_hw_normalize(TLAdelta_hw* delta);

#endif
