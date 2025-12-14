/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "../include/q48_16.h"

/* ============================================================================
 * Core Arithmetic: Multiply
 * ============================================================================
 *
 * Formula: (a / 2^16) * (b / 2^16) * 2^16 = (a * b) / 2^16
 *
 * Implementation note:
 * - Compute a * b in 128-bit intermediate (if available)
 * - Shift right by 16 bits
 * - For platforms without uint128_t, use uint64_t with overflow check
 */

q48_16_t q48_mul(q48_16_t a, q48_16_t b)
{
    /* Use __uint128_t if available (GCC/Clang) */
#ifdef __SIZEOF_INT128__
    __uint128_t prod = (__uint128_t)a * (__uint128_t)b;
    return (q48_16_t)(prod >> 16);
#else
    /* Fallback: manual 64-bit multiplication with overflow detection */
    /* Split a and b into high/low parts */
    uint64_t a_hi = a >> 32;
    uint64_t a_lo = a & 0xFFFFFFFFULL;
    uint64_t b_hi = b >> 32;
    uint64_t b_lo = b & 0xFFFFFFFFULL;

    /* Compute 4 partial products */
    uint64_t p_ll = a_lo * b_lo;         /* Low * Low */
    uint64_t p_lh = a_lo * b_hi;         /* Low * High */
    uint64_t p_hl = a_hi * b_lo;         /* High * Low */
    uint64_t p_hh = a_hi * b_hi;         /* High * High */

    /* Accumulate with carry tracking */
    uint64_t carry = 0;
    uint64_t mid = p_lh + p_hl;
    if (mid < p_lh) carry++;              /* Detect overflow in middle term */

    uint64_t result_hi = p_hh + (mid >> 32) + (carry << 32);
    uint64_t result_lo = p_ll + ((mid & 0xFFFFFFFFULL) << 32);
    if (result_lo < p_ll) result_hi++;

    /* Return: (result_hi << 32 | result_lo) >> 16 */
    /* = (result_hi << 16) | (result_lo >> 16) */
    return (result_hi << 16) | (result_lo >> 16);
#endif
}

/* ============================================================================
 * Core Arithmetic: Divide
 * ============================================================================
 *
 * Formula: (a / 2^16) / (b / 2^16) * 2^16 = (a << 16) / b
 *
 * Precondition: b > 0 (division by zero check in caller)
 */

q48_16_t q48_div(q48_16_t a, q48_16_t b)
{
    if (b == 0) {
        /* Division by zero: return 0 (safe, but caller should check) */
        return 0;
    }

    /* Shift a left by 16 bits to maintain Q48.16 precision */
    /* Be careful: this could overflow if a is too large */
    /* Max safe a: 2^48 (since we're shifting left by 16) */
    if (a > 0x0000FFFFFFFFFFFFULL) {
        /* a is too large for safe shifting */
        /* Return max Q48.16 to signal saturation */
        return 0xFFFFFFFFFFFFFFFFULL;
    }

    q48_16_t shifted = a << 16;
    return shifted / b;
}

/* ============================================================================
 * Conversions: u64 ↔ Q48.16
 * ============================================================================
 */

q48_16_t q48_from_double(double d)
{
    /* Convert double to Q48.16 */
    /* This is for testing/diagnostics only, not used in inference engine */
    return (q48_16_t)(d * 65536.0 + 0.5);  /* Round to nearest */
}

double q48_to_double(q48_16_t q)
{
    /* Convert Q48.16 to double */
    /* For logging and RStudio dashboard output */
    return (double)q / 65536.0;
}

/* ============================================================================
 * Approximation: Natural Logarithm (Integer-Only, Newton-Raphson)
 * ============================================================================
 *
 * Purpose: ln(x) in Q48.16 format for exponential decay fitting
 *
 * Algorithm:
 * 1. Use bit position as coarse approximation
 *    - If x = 2^k * m where 1 <= m < 2, then ln(x) ≈ k*ln(2) + ln(m)
 *    - ln(2) in Q48.16 = 0xB17217F7 (approx)
 *
 * 2. Refine ln(m) using Newton-Raphson on exp()
 *    - For y = ln(x), we want e^y = x
 *    - Newton: y_{n+1} = y_n + (x - e^{y_n}) / e^{y_n}
 *    - Simplified: y_{n+1} = y_n - 1 + x / e^{y_n}
 *
 * 3. Do 4-5 iterations for Q48.16 precision
 */

q48_16_t q48_log_approx(uint64_t x)
{
    if (x == 0) {
        /* ln(0) is undefined; return 0 for safety */
        return 0;
    }

    if (x == 65536) {
        /* ln(1.0 in Q48.16) = 0 */
        return 0;
    }

    /* ln(2) in Q48.16 ≈ 0.693147180559945... */
    /* 0.693147 * 65536 = 45426.5 ≈ 45426 or 45427 */
    const q48_16_t LN2_Q48 = 45426;  /* 0x0000B17E in Q48.16 */

    /* Step 1: Find bit position k such that x = 2^k * m, 1 <= m < 2 */
    int k = 0;
    uint64_t m = x;

    /* Count leading zeros to find position */
    if (m >= 131072) {  /* m >= 2 in Q48.16 */
        while (m >= 131072) {
            m >>= 1;
            k++;
        }
    } else if (m < 65536) {  /* m < 1 in Q48.16 */
        while (m < 65536) {
            m <<= 1;
            k--;
        }
    }

    /* Step 2: Compute ln(m) where 1 <= m < 2 using Newton-Raphson */
    /* Initial guess: y_0 = (m - 1) for small values */
    q48_16_t y = (m > 65536) ? (m - 65536) : 0;

    /* Newton iterations: y_{n+1} = y_n + (m - e^{y_n}) / e^{y_n} */
    for (int iter = 0; iter < 6; iter++) {
        /* Compute e^y */
        q48_16_t exp_y = q48_exp_approx(y);

        if (exp_y == 0) break;  /* Safety check */

        /* Compute (m - exp_y) / exp_y, handling both positive and negative delta */
        int64_t delta_signed = (int64_t)m - (int64_t)exp_y;
        q48_16_t correction;

        if (delta_signed > 0) {
            correction = q48_div((q48_16_t)delta_signed, exp_y);
            y = q48_add(y, correction);
        } else if (delta_signed < 0) {
            correction = q48_div((q48_16_t)(-delta_signed), exp_y);
            if (y > correction) {
                y = q48_sub(y, correction);
            } else {
                y = 0;
            }
        }

        /* Early exit if correction is very small */
        if (delta_signed < 100 && delta_signed > -100) break;
    }

    /* Step 3: Combine ln(x) = k*ln(2) + ln(m) */
    q48_16_t result = y;
    if (k > 0) {
        result = q48_add(result, q48_mul(q48_from_u64(k), LN2_Q48));
    } else if (k < 0) {
        result = q48_sub(result, q48_mul(q48_from_u64(-k), LN2_Q48));
    }

    return result;
}

/* ============================================================================
 * Approximation: Exponential (Integer-Only, Taylor Series)
 * ============================================================================
 *
 * Purpose: e^x in Q48.16 format (supporting function for ln_approx)
 *
 * Algorithm: Taylor series around x=0
 * e^x = 1 + x + x^2/2! + x^3/3! + x^4/4! + ...
 *
 * For Q48.16 arithmetic, compute 6-8 terms for good precision.
 */

q48_16_t q48_exp_approx(q48_16_t q)
{
    /* Handle special cases */
    if (q == 0) {
        return 65536;  /* e^0 = 1 in Q48.16 */
    }

    /* Check for overflow/underflow */
    int64_t q_signed = (int64_t)q;

    if (q_signed >= 1048576) {  /* q >= 16 */
        return 0xFFFFFFFFFFFFFFFFULL;  /* Overflow */
    }

    if (q_signed <= -1048576) {  /* q <= -16 */
        return 0;  /* Underflow to 0 */
    }

    /* For negative x, compute e^(-x) = 1 / e^x */
    int is_negative = (q_signed < 0);
    q48_16_t x = is_negative ? (q48_16_t)(-q_signed) : q;

    /* Taylor series: e^x = 1 + x + x^2/2! + x^3/3! + ... */
    q48_16_t result = 65536;  /* 1.0 in Q48.16 */
    q48_16_t term = x;        /* First term = x */

    result = q48_add(result, term);

    /* Compute subsequent terms */
    for (int n = 2; n <= 10; n++) {
        /* term = term * x / n */
        term = q48_mul(term, x);
        term = q48_div(term, q48_from_u64(n));

        result = q48_add(result, term);

        /* Early exit if term becomes negligible */
        if (term < 50) break;
    }

    /* If original was negative, return 1/result */
    if (is_negative) {
        result = q48_div(65536, result);  /* 1.0 / result */
    }

    return result;
}

/* ============================================================================
 * Approximation: Square Root (Integer-Only, Newton-Raphson)
 * ============================================================================
 *
 * Purpose: sqrt(q) in Q48.16 for R² computation
 *
 * Algorithm: Newton-Raphson on x^2 = q
 * x_{n+1} = (x_n + q/x_n) / 2
 */

q48_16_t q48_sqrt_approx(q48_16_t q)
{
    if (q == 0) {
        return 0;
    }

    if (q == 65536) {  /* sqrt(1.0) = 1.0 */
        return 65536;
    }

    /* Initial guess: x_0 = q >> 1 (roughly sqrt in Q48.16 space) */
    q48_16_t x = (q >> 1) + 16384;  /* Ensure non-zero start */

    for (int iter = 0; iter < 8; iter++) {
        /* Newton-Raphson: x_{n+1} = (x_n + q/x_n) / 2 */
        q48_16_t q_div_x = q48_div(q, x);
        q48_16_t x_next = (x + q_div_x) >> 1;

        /* Check convergence */
        q48_16_t delta = (x_next > x) ? (x_next - x) : (x - x_next);
        if (delta < 10) break;  /* Converged to within 0.00015 */

        x = x_next;
    }

    return x;
}

/* ============================================================================
 * Diagnostic / Testing Utilities
 * ============================================================================
 */

const char* q48_to_string(q48_16_t q)
{
    static char buf[64];
    uint64_t integer_part = q >> 16;
    uint64_t frac_part = q & 0xFFFFULL;

    /* Convert fractional part to 5 decimal digits */
    /* frac_part / 65536 * 100000 = frac_part * 100000 / 65536 */
    uint32_t frac_digits = (frac_part * 100000ULL) / 65536;

    snprintf(buf, sizeof(buf), "%lu.%05u", integer_part, frac_digits);
    return buf;
}

int q48_is_valid(q48_16_t q)
{
    /* Q48.16 is always valid if it fits in uint64_t */
    /* More sophisticated validation could check for overflow risk */
    return 1;  /* For now, all uint64_t values are valid Q48.16 */
}

/* ============================================================================
 * Future: FORTH Word Wrappers
 * ============================================================================
 *
 * When integrated with FORTH word registry, these functions become:
 *
 * Q.+     ( q1 q2 -- q_sum )    -> forth_Q_ADD
 * Q.-     ( q1 q2 -- q_diff )   -> forth_Q_SUB
 * Q.*     ( q1 q2 -- q_prod )   -> forth_Q_MUL
 * Q./     ( q1 q2 -- q_quot )   -> forth_Q_DIV
 * Q.ABS   ( q -- |q| )          -> forth_Q_ABS
 * Q.LOG   ( u -- ln(u) )        -> forth_Q_LOG
 * Q.EXP   ( q -- e^q )          -> forth_Q_EXP
 * Q.SQRT  ( q -- sqrt(q) )      -> forth_Q_SQRT
 *
 * Stack convention: All Q48.16 values pushed/popped as uint64_t
 */