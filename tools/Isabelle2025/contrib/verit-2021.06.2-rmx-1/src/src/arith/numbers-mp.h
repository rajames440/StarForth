/* -*- mode: C -*-
   --------------------------------------------------------------
   Types and functions to deal with numbers.

   There are two versions: HW (hardware) and MP (multi-precision).

   Common definitions are in *-mp-*.tpl files (tpl for template),
   where the strings mp and MP represent the version. These files
   are then processed by cpp with mp=hw, MP=HW and mp=mp, MP=MP
   to generate C code.

   Some specific definitions are found in sections of the template
   files guarded by conditional preprocessor directives, and others
   are coded directly in some C file.
   --------------------------------------------------------------
*/

#ifndef __NUMBERS_MP_H
#define __NUMBERS_MP_H

#define MP_VERSION

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
   \typedef TLAsigned_mp
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
typedef signed int TLAsigned_mp;
#define LAsigned_mp_init(a)
#define LAsigned_mp_set(a, b) (a) = (b)
#define LAsigned_mp_clear(a)
#else
typedef mpz_t TLAsigned_mp;
#define LAsigned_mp_init mpz_init
#define LAsigned_mp_set mpz_set
#define LAsigned_mp_clear mpz_clear
#endif

/**
   \brief executes the following operation a = a * b
   \param[in,out] a lvalue storing a TLAsigned_mp
   \param[in] b a TLAsigned_mp expression */
#ifdef HW_VERSION
#define LAsigned_mp_mult(a, b) \
	{ \
		signed long long tmp1; \
		tmp1 = (signed long long)a * (signed long long)b; \
		OVERFLOW_CHECK_SET(tmp1 > INT_MAX || tmp1 <= INT_MIN, tmp1); \
		a = (signed int)tmp1; \
	}
#else
#define LAsigned_mp_mult(a, b) mpz_mul(a, a, b);
#endif

/**
   \brief executes the following operation a = -(b * c)
   \param[in] a LAsigned
   \param[in] b LAsigned
   \param[out] c LAsigned */
#ifdef HW_VERSION
#define LAsigned_mp_mult_neg(a, b, c) \
	{ \
		signed long long tmp1; \
		tmp1 = (signed long long)b * (signed long long)c; \
		OVERFLOW_CHECK_SET(tmp1 > INT_MAX || tmp1 <= INT_MIN, tmp1) \
		a = -(signed int)tmp1; \
	}
#else
#define LAsigned_mp_mult_neg(a, b, c) \
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
#define LAsigned_mp_neg(a, b) (a) = -(b)
#else
#define LAsigned_mp_neg(a, b) mpz_neg(a, b)
#endif

/**
   \brief execute the following operation a = a * c - b * d
   \param[in,out] a LAsigned
   \param[in] b LAsigned
   \param[in] c LAsigned
   \param[in] d LAsigned */
#ifdef HW_VERSION
#define LAsigned_mp_com(a, b, c, d) \
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
#define LAsigned_mp_com(a, b, c, d) \
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
#define LAsigned_mp_set_si(a, b) \
	{ \
		a = b; \
	}
#else
#define LAsigned_mp_set_si(a, b) mpz_set_si(a, b)
#endif

/**
   \brief check if a signed value is 0
   \param[in] a LAsigned
   \return true iff a is zero */
#ifdef HW_VERSION
#define LAsigned_mp_is_zero(a) ((a) == 0)
#else
#define LAsigned_mp_is_zero(a) (mpz_sgn(a) == 0)
#endif

/**
   \brief check if a signed value is < 0
   \param[in] a LAsigned
   \return true iff a is zero */
#ifdef HW_VERSION
#define LAsigned_mp_is_neg(a) ((a) < 0)
#else
#define LAsigned_mp_is_neg(a) (mpz_sgn(a) < 0)
#endif

/**
   \brief checks if a and b have different sign
   \param[in] a pointer to a LAsigned
   \param[in] b pointer to a LAsigned
   \return true iff signs differ */
#ifdef HW_VERSION
#define LAsigned_mp_neq(a, b) (a != b)
#else
#define LAsigned_mp_neq(a, b) (mpz_cmp(a, b))
#endif

/**
   \brief checks if a and b have different sign
   \param[in] a pointer to a LAsigned
   \param[in] b pointer to a LAsigned
   \return true iff signs differ */
#ifdef HW_VERSION
#define LAsigned_mp_sign_diff(a, b) \
	(((unsigned long)((a) ^ (b))) >> (sizeof(signed long) * CHAR_BIT - 1))
#else
#define LAsigned_mp_sign_diff(a, b) (mpz_sgn(a) != mpz_sgn(b))
#endif

/**
   \brief prints a on stdout
   \param[in] a LAsigned */
#ifdef HW_VERSION
#define LAsigned_mp_print(a) printf("%d", a)
#else
#define LAsigned_mp_print(a) mpz_out_str(stdout, 10, a)
#endif

/**
   \brief executes the following operation a = abs(b)
   \param[in,out] a lvalue storing a TLAsigned_mp
   \param[in] b a TLAsigned_mp expression */
#ifdef HW_VERSION
#define LAsigned_mp_abs(a, b) \
	{ \
		signed __mask; \
		__mask = (b) >> (sizeof(signed int) * CHAR_BIT - 1); \
		(a) = (((b) + __mask) ^ __mask); \
	}
#else
#define LAsigned_mp_abs(a, b) mpz_abs(a, b)
#endif

/**
   \brief executes the following operation a = gcd(a, b)
   \param[in,out] a lvalue storing a TLAsigned_mp
   \param[in] b a TLAsigned_mp expression */
#ifdef HW_VERSION
#define LAsigned_mp_gcd(a, b) a = (signed)gcd((unsigned)(a), (unsigned)(b))
#else
#define LAsigned_mp_gcd(a, b) mpz_gcd(a, a, b)
#endif

/**
   \brief tests if signed is different from one
   \param[in] a tested value */
#ifdef HW_VERSION
#define LAsigned_mp_notone(a) ((a) != 1)
#else
#define LAsigned_mp_notone(a) mpz_cmp_ui(a, 1UL)
#endif

/**
   \brief tests if signed is strictly positive
   \param[in] a tested value */
#ifdef HW_VERSION
#define LAsigned_mp_pos(a) ((a) > 0)
#else
#define LAsigned_mp_pos(a) (mpz_cmp_ui(a, 0UL) > 0)
#endif /* HW_VERSION */

/**
   \brief tests if one signed is strictly smaller than another signed
   \param[in] a first tested value
   \param[in] b second tested value */
#ifdef HW_VERSION
#define LAsigned_mp_smaller(a, b) ((a) < (b))
#else
#define LAsigned_mp_smaller(a, b) (mpz_cmp(a, b) < 0)
#endif /* HW_VERSION */

/**
   \brief Divides signed a exactly by signed b, and assigns result to a
   \param[in, out] a dividend
   \param[in] b divisor
   \pre the remainder of the division of a by b is 0 */
#ifdef HW_VERSION
#define LAsigned_mp_divex(a, b) (a) /= (b)
#else
#define LAsigned_mp_divex(a, b) mpz_divexact(a, a, b)
#endif /* HW_VERSION */

/**
   \brief Computes a key from a signed (e.g., for storing in a hash table)
   \param[in] a signed value */
#ifdef HW_VERSION
#define LAsigned_mp_key(a) ((unsigned)a)
#else
#define LAsigned_mp_key(a) ((unsigned)mpz_get_ui(a))
#endif /* HW_VERSION */

/*
  --------------------------------------------------------------
  Rationals
  --------------------------------------------------------------
*/

/**
   \typedef TLArational_mp
   \brief rational number
   \remark pair of C native int
   \remark INT_MIN is counted as overflow in num */
#ifdef HW_VERSION
typedef struct TLArational_mp
{
	signed int num;
	unsigned int den;
} TLArational_mp[1];
#define LArational_mp_init(a)
#define LArational_mp_set(a, b) ((*a) = (*b))
#define LArational_mp_clear(a)
#define LArational_mp_set_si(a, b) \
	do { \
		a->num = b; \
		a->den = 1; \
	} while (0)
#else
typedef mpq_t TLArational_mp;
#define LArational_mp_init mpq_init
#define LArational_mp_set mpq_set
#define LArational_mp_clear mpq_clear
#define LArational_mp_set_si mpq_set_z
#endif

/**
   \brief set a rational value to 1
   \param[out] rational a rational value */
void LArational_mp_set_one(TLArational_mp rational);

/**
   \brief set a rational value to 0
   \param[out] rational a rational value */
void LArational_mp_set_zero(TLArational_mp rational);

/**
   \brief check if a rational value is 0
   \param[in] rational a rational
   \return 1 if rational is zero */
bool LArational_mp_is_zero(TLArational_mp rational);

/**
   \brief sets rational to be the number represented by str
   \param[out] rational a rational value (in canonical form)
   \param[in] str a string */
void LArational_mp_str(TLArational_mp rational, char* str);

/**
   \brief sets rational to be the number represented by mpz
   \param[out] rational a rational value (in canonical form)
   \param[in] mpz a multiprecision integer */
void LArational_mp_mpz(TLArational_mp rational, mpz_t mpz);

/**
   \brief sets rational to be the number represented by mpq
   \param[out] rational a rational value (in canonical form)
   \param[in] mpq a multiprecision rational */
void LArational_mp_mpq(TLArational_mp rational, mpq_t mpq);

/**
   \brief normalize the value, i.e. divides num and den by their gcd
   \param[in,out] rational a rational value */
void LArational_mp_normalize(TLArational_mp rational);

/**
   \brief changes sign of rational
   \param[in,out] rational a rational value */
void LArational_mp_neg(TLArational_mp rational);

/**
   \brief check if a rational value is < 0
   \param[in] rational a rational
   \return 1 if rational is < 0 */
bool LArational_mp_is_neg(TLArational_mp rational);

/**
   \brief check if a rational value is integer
   \param[in] rational a rational
   \return 1 if rational is an integer */
bool LArational_mp_is_int(TLArational_mp rational);

/**
   \brief adds two rationals and puts result in first argument
   \param[in,out] rational1 a first rational value
   \param[in] rational2 a second rational value
   \remark normalizes */
void LArational_mp_add(TLArational_mp rational1, TLArational_mp rational2);

/**
   \brief multiplies two rationals and puts result in first argument
   \param[in,out] rational1 a first rational value
   \param[in] rational2 a second rational value
   \remark normalizes */
void LArational_mp_mult(
	TLArational_mp rational1, const TLArational_mp rational2);

/**
   \brief multiply a rational by a signed and put result in first argument
   \param[in,out] rational1 a rational value
   \param[in] signed2 a signed value
   \remark normalizes */
void LArational_mp_mult_s(TLArational_mp rational1, TLAsigned_mp signed2);

/**
   \brief divides two rationals and puts result in first argument
   \param[in,out] rational1 a rational value
   \param[in] rational2 a rational value
   \remark normalizes */
void LArational_mp_div(TLArational_mp rational1, TLArational_mp rational2);

/**
   \brief multiplies a rational by a signed and puts result in first argument
   \param[in,out] rational1 a rational value
   \param[in] signed2 a signed value
   \remark normalizes */
void LArational_mp_mult_s(TLArational_mp rational1, TLAsigned_mp signed2);

/**
   \brief divides a rational by a signed and puts result in first argument
   \param[in,out] rational1 a rational value
   \param[in] signed2 a signed value
   \remark normalizes */
void LArational_mp_div_s(TLArational_mp rational1, TLAsigned_mp signed2);

/**
   \brief compares two rationals
   \param[in] rational1 a rational value a
   \param[in] rational2 a rational value b
   \return true iff a is equal to b */
bool LArational_mp_eq(TLArational_mp rational1, TLArational_mp rational2);

/**
   \brief compares two rationals
   \param[in] rational1 a rational value a
   \param[in] rational2 a rational value b
   \return true iff a is smaller than or equal to b */
bool LArational_mp_leq(TLArational_mp rational1, TLArational_mp rational2);

/**
   \brief compares two rationals
   \param[in] rational1 a rational value a
   \param[in] rational2 a rational value b
   \return true iff a is strictly smaller than b */
bool LArational_mp_less(TLArational_mp rational1, TLArational_mp rational2);

#ifdef HW_VERSION
/**
   \brief gets the lcm of the denominator of *Prational and *Plcm
   \param[in,out] lcm a signed value
   \param[in] rational a rational value
   \remark stores the result in *Plcm */
#define LArational_mp_lcm(lcm, rational) LArational_mp_lcm_aux(&(lcm), rational)
void LArational_mp_lcm_aux(TLAsigned_mp* Plcm, TLArational_mp rational);
#else
/**
   \brief gets the lcm of the denominator of *Prational and *Plcm
   \param[in,out] lcm a signed value
   \param[in] rational a rational value
   \remark stores the result in *Plcm */
void LArational_mp_lcm(TLAsigned_mp lcm, TLArational_mp rational);
#endif

#ifdef HW_VERSION
/**
   \brief multiplies asigned by rational
   \param[in,out] asigned a lvalue containing signed value
   \param[in] rational a rational value
   \pre asigned is a multiple of rational's denominator.
   \remark stores the result in asigned
   \remark the result should be integer */
#define LArational_mp_mult_to_signed(asigned, rational) \
	LArational_mp_mult_to_signed_aux(&(asigned), rational)
void LArational_mp_mult_to_signed_aux(
	TLAsigned_mp* Pasigned, TLArational_mp rational);
#else
/**
   \brief multiplies asigned by rational
   \param[in,out] asigned a signed value
   \param[in] rational a rational value
   \remark stores the result in asigned
   \remark the result should be integer */
void LArational_mp_mult_to_signed(
	TLAsigned_mp asigned, TLArational_mp rational);
#endif

/**
   \brief prints a rational
   \param[in,out] a rational
   \remark for debuging purposes */
#ifdef HW_VERSION
#define LArational_mp_print(a) printf("%d / %u", a->num, a->den)
#else
#define LArational_mp_print(a) mpq_out_str(stdout, 10, a)
#endif

/*
  --------------------------------------------------------------
  Delta numbers
  --------------------------------------------------------------
*/

/**
   \typedef TLAdelta_mp
   \brief number with delta */
#ifdef HW_VERSION
typedef struct TLAdelta_mp
{
	struct TLArational_mp val;
	struct TLArational_mp delta;
} TLAdelta_mp;
#define LAdelta_mp_init(a)
#define LAdelta_mp_set(dst, src) (*dst) = (*src)
#define LAdelta_mp_clear(a)
#else
typedef struct TLAdelta_mp
{
	TLArational_mp val;
	TLArational_mp delta;
} TLAdelta_mp;
void LAdelta_mp_init(TLAdelta_mp*);
void LAdelta_mp_set(TLAdelta_mp* dst, TLAdelta_mp* src);
void LAdelta_mp_clear(TLAdelta_mp*);
#endif

#ifdef HW_VERSION
#define LAdelta_mp_set_rat(dst, src) (*dst).val = (**src)
#define LAdelta_mp_set_delta(dst, eps) \
	(*dst).delta.num = eps; \
	(*dst).delta.den = 1;
#define LAdelta_mp_get_rat(src, num, den) \
	{ \
		(num) = ((src).val.num); \
		(den) = (signed)((src).val.den); \
	}
#else
void LAdelta_mp_set_rat(TLAdelta_mp* dst, TLArational_mp* src);
void LAdelta_mp_set_delta(TLAdelta_mp* dst, int eps);
void LAdelta_mp_get_rat(
	TLAdelta_mp* src, TLAsigned_mp* Pnum, TLAsigned_mp* Pden);
#define LAdelta_mp_get_rat(src, num, den) \
	{ \
		(num) = (mpq_numref((src).val)); \
		(den) = (mpq_denref((src).val)); \
	}
#endif

/**
   \brief sets delta value to 0
   \param[out] delta value */
void LAdelta_mp_set_zero(TLAdelta_mp* delta);

/**
   \brief sets delta value to 1
   \param[out] delta value */
void LAdelta_mp_set_one(TLAdelta_mp* delta);

/**
   \brief checks if delta is 0
   \param[in] delta value
   \return true iff zero */
bool LAdelta_mp_is_zero(TLAdelta_mp* delta);

/**
   \brief checks if delta is an integer
   \param[in] delta value
   \return true iff delta is integer-valued
   \pre value delta must be normalized
   \sa LAdelta_mp_normalize */
bool LAdelta_mp_is_int(TLAdelta_mp* delta);

/**
   \brief sets value of delta according to int
   \param[out] delta
   \param[in] val its value */
void LAdelta_mp_int(TLAdelta_mp* delta, int val);

/**
   \brief sets value of delta according to arguments
   \param[out] delta
   \param[in] num numerator for its value
   \param[in] den denominator for its value */
void LAdelta_mp_rat(TLAdelta_mp* delta, int num, unsigned den);

/**
   \brief sets value of delta according to arguments
   \param[out] delta
   \param[in] num numerator for its value
   \param[in] den denominator for its value
   \param[in] eps coefficient of delta value */
void LAdelta_mp_rat_delta(TLAdelta_mp* delta, int num, unsigned den, int eps);

/**
   \brief checks if two delta values are equal
   \param[in] delta1 value
   \param[in] delta2 value
   \return true if delta1 == delta2 */
bool LAdelta_mp_eq(TLAdelta_mp* delta1, TLAdelta_mp* delta2);

/**
   \brief checks if a delta value is less than or equal to another
   \param[in] delta1 value
   \param[in] delta2 value
   \return true iff delta1 <= delta2 */
bool LAdelta_mp_leq(TLAdelta_mp* delta1, TLAdelta_mp* delta2);

/**
   \brief checks if a delta value is less than another
   \param[in] delta1 value
   \param[in] delta2 value
   \return true iff delta1 < delta2 */
bool LAdelta_mp_less(TLAdelta_mp* delta1, TLAdelta_mp* delta2);

/**
   \brief checks if two delta value are not equal
   \param[in] delta1 value
   \param[in] delta2 value
   \return true iff delta1 < delta2 */
bool LAdelta_mp_neq(TLAdelta_mp* delta1, TLAdelta_mp* delta2);

/**
   \brief compares two delta values
   \param[in] delta1 value
   \param[in] delta2 value
   \return negative if delta1 < delta2, positive if delta1 > delta2,
   zero otherwise */
int LAdelta_mp_cmp(TLAdelta_mp* delta1, TLAdelta_mp* delta2);

/**
   \brief computes delta0 = delta0 + delta1 * a
   \param[in,out] delta0 value
   \param[in] delta1 value
   \param[in] a value */
void LAdelta_mp_addmult(
	TLAdelta_mp* delta0, TLAdelta_mp* delta1, TLAsigned_mp a);

/**
   \brief computes delta0 = delta1 - delta2
   \param[out] delta0 value
   \param[in] delta1 value
   \param[in] delta2 value */
void LAdelta_mp_minus(
	TLAdelta_mp* delta0, TLAdelta_mp* delta1, TLAdelta_mp* delta2);

/**
   \brief computes delta0 = - delta0 / a
   \param[in,out] delta0 value
   \param[in] a value */
void LAdelta_mp_div_opp(TLAdelta_mp* delta0, TLAsigned_mp a);

/**
   \brief print value of delta
   \param[in] delta */
void LAdelta_mp_print(TLAdelta_mp* delta);

/**
   \brief computes \f$delta = \lfloor delta \rfloor\f$
   \param[in, out] delta value */
void LAdelta_mp_floor(TLAdelta_mp* delta);

/**
   \brief computes delta = delta + 1
   \param[in,out] delta value */
void LAdelta_mp_increment(TLAdelta_mp* delta);

/**
   \brief computes delta = delta - 1
   \param[in,out] delta value */
void LAdelta_mp_decrement(TLAdelta_mp* delta);

/**
   \param[in,out] delta value */
void LAdelta_mp_normalize(TLAdelta_mp* delta);

#endif
