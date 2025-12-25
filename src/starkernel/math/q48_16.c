/**
 * q48_16.c - Q48.16 Fixed-Point Arithmetic for StarKernel
 *
 * Freestanding implementation: no libc, no floating-point.
 */

#include "q48_16.h"

/* ============================================================================
 * Core Arithmetic: Multiply
 * ============================================================================
 *
 * Formula: (a / 2^16) * (b / 2^16) * 2^16 = (a * b) / 2^16
 */

q48_16_t q48_mul(q48_16_t a, q48_16_t b)
{
#ifdef __SIZEOF_INT128__
    /* Use __uint128_t if available (GCC/Clang on x86_64) */
    __uint128_t prod = (__uint128_t)a * (__uint128_t)b;
    return (q48_16_t)(prod >> 16);
#else
    /* Fallback: manual 64-bit multiplication */
    uint64_t a_hi = a >> 32;
    uint64_t a_lo = a & 0xFFFFFFFFULL;
    uint64_t b_hi = b >> 32;
    uint64_t b_lo = b & 0xFFFFFFFFULL;

    uint64_t p_ll = a_lo * b_lo;
    uint64_t p_lh = a_lo * b_hi;
    uint64_t p_hl = a_hi * b_lo;
    uint64_t p_hh = a_hi * b_hi;

    uint64_t carry = 0;
    uint64_t mid = p_lh + p_hl;
    if (mid < p_lh) carry++;

    uint64_t result_hi = p_hh + (mid >> 32) + (carry << 32);
    uint64_t result_lo = p_ll + ((mid & 0xFFFFFFFFULL) << 32);
    if (result_lo < p_ll) result_hi++;

    return (result_hi << 48) | (result_lo >> 16);
#endif
}

/* ============================================================================
 * Core Arithmetic: Divide
 * ============================================================================
 *
 * Formula: (a / 2^16) / (b / 2^16) * 2^16 = (a << 16) / b
 */

q48_16_t q48_div(q48_16_t a, q48_16_t b)
{
    if (b == 0) {
        return 0;  /* Division by zero: return 0 */
    }

    /* Check for overflow: max safe a is 2^48 */
    if (a > 0x0000FFFFFFFFFFFFULL) {
        return 0xFFFFFFFFFFFFFFFFULL;  /* Saturation */
    }

    q48_16_t shifted = a << 16;
    return shifted / b;
}

/* ============================================================================
 * Approximation: Exponential (Taylor Series, Integer-Only)
 * ============================================================================
 *
 * e^x = 1 + x + x^2/2! + x^3/3! + ...
 */

q48_16_t q48_exp_approx(q48_16_t q)
{
    if (q == 0) {
        return Q48_ONE;  /* e^0 = 1 */
    }

    /* Check for overflow (q >= 16 in Q48.16 = 0x100000) */
    if (q >= 0x100000ULL) {
        return 0xFFFFFFFFFFFFFFFFULL;  /* Overflow */
    }

    /* Taylor series: e^x = 1 + x + x^2/2! + x^3/3! + ... */
    q48_16_t result = Q48_ONE;  /* 1.0 */
    q48_16_t term = q;          /* First term = x */

    result = q48_add(result, term);

    /* Compute subsequent terms: term_n = term_{n-1} * x / n */
    for (int n = 2; n <= 10; n++) {
        term = q48_mul(term, q);
        term = q48_div(term, q48_from_u64((uint64_t)n));

        result = q48_add(result, term);

        /* Early exit if term becomes negligible */
        if (term < 50) break;
    }

    return result;
}

/* ============================================================================
 * Approximation: Natural Logarithm (Newton-Raphson, Integer-Only)
 * ============================================================================
 *
 * Uses bit position for coarse estimate, then Newton-Raphson refinement.
 * ln(x) where x is in Q48.16 format.
 */

q48_16_t q48_log_approx(q48_16_t x)
{
    if (x == 0) {
        return 0;  /* ln(0) undefined, return 0 */
    }

    if (x == Q48_ONE) {
        return 0;  /* ln(1.0) = 0 */
    }

    /* ln(2) in Q48.16: 0.693147 * 65536 = 45426 */
    const q48_16_t LN2_Q48 = 45426;

    /* Find k such that x = 2^k * m, where 1 <= m < 2 in Q48.16 */
    int k = 0;
    q48_16_t m = x;

    /* Two in Q48.16 = 0x20000 (131072) */
    if (m >= 131072) {
        while (m >= 131072) {
            m >>= 1;
            k++;
        }
    } else if (m < Q48_ONE) {
        while (m < Q48_ONE) {
            m <<= 1;
            k--;
        }
    }

    /* Compute ln(m) where 1 <= m < 2 using Newton-Raphson */
    /* Initial guess: y_0 = m - 1 (for small values near 1) */
    q48_16_t y = (m > Q48_ONE) ? (m - Q48_ONE) : 0;

    /* Newton iterations: y_{n+1} = y_n + (m - e^{y_n}) / e^{y_n} */
    for (int iter = 0; iter < 6; iter++) {
        q48_16_t exp_y = q48_exp_approx(y);
        if (exp_y == 0) break;

        if (m > exp_y) {
            q48_16_t correction = q48_div(m - exp_y, exp_y);
            y = q48_add(y, correction);
        } else if (m < exp_y) {
            q48_16_t correction = q48_div(exp_y - m, exp_y);
            if (y > correction) {
                y = q48_sub(y, correction);
            } else {
                y = 0;
            }
        }

        /* Early exit if converged */
        q48_16_t delta = (m > exp_y) ? (m - exp_y) : (exp_y - m);
        if (delta < 100) break;
    }

    /* Combine: ln(x) = k*ln(2) + ln(m) */
    if (k > 0) {
        y = q48_add(y, q48_mul(q48_from_u64((uint64_t)k), LN2_Q48));
    } else if (k < 0) {
        q48_16_t sub = q48_mul(q48_from_u64((uint64_t)(-k)), LN2_Q48);
        if (y > sub) {
            y = q48_sub(y, sub);
        } else {
            /* Result would be negative; for unsigned, return 0 */
            y = 0;
        }
    }

    return y;
}

/* ============================================================================
 * Approximation: Square Root (Newton-Raphson, Integer-Only)
 * ============================================================================
 *
 * x_{n+1} = (x_n + q/x_n) / 2
 */

q48_16_t q48_sqrt_approx(q48_16_t q)
{
    if (q == 0) {
        return 0;
    }

    if (q == Q48_ONE) {
        return Q48_ONE;  /* sqrt(1.0) = 1.0 */
    }

    /* Initial guess: q >> 1 with small offset to ensure non-zero */
    q48_16_t x = (q >> 1) + 16384;

    for (int iter = 0; iter < 8; iter++) {
        q48_16_t q_div_x = q48_div(q, x);
        q48_16_t x_next = (x + q_div_x) >> 1;

        /* Check convergence */
        q48_16_t delta = (x_next > x) ? (x_next - x) : (x - x_next);
        if (delta < 10) break;  /* Converged */

        x = x_next;
    }

    return x;
}
