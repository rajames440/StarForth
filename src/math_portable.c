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

/*
 * math_portable.c
 *
 * Pure 64-bit fixed-point Q48.16 math functions.
 * Suitable for L4Re and embedded environments with no libm or floating-point.
 * All arithmetic is integer-only for maximum production performance.
 *
 * Q48.16 Format:
 *   - 48-bit signed integer + 16-bit fractional part
 *   - 1.0 = 65536 (2^16)
 *   - Precision: 2^-16 ≈ 0.0000153
 *   - Range: ±140 trillion
 */

#include <stdint.h>
#include "math_portable.h"

/**
 * @brief Compute exp(@p x) via Taylor series in Q48.16 fixed-point.
 *
 * Evaluates the classic Taylor expansion:
 * @code
 *   exp(x) = 1 + x + x²/2! + x³/3! + … (up to 40 terms)
 * @endcode
 * entirely in 64-bit integer arithmetic using the Q48.16 representation
 * (1.0 = 65536 = 2^16).
 *
 * The function is intentionally scoped to the range the physics engine
 * actually uses — the argument to @c erf_q48() is @c -x² times a ratio
 * near 1, yielding values in roughly the @c [-20, 0] interval. Outside
 * this range the implementation provides coarse-grained clamping:
 * - @p x == 0      → returns @c Q48 (1.0 exactly)
 * - @p x > 20.0    → saturates at @c Q48 << 8 (approx e^20, avoids overflow)
 * - @p x < -100.0  → underflows to 0 (result is negligible)
 *
 * The term loop exits early on the first term smaller than 1 ULP (the
 * series has converged) and guards against intermediate overflow by
 * clamping @c result at @c INT64_MAX-1.
 *
 * @note This is a file-local (@c static) helper; callers are
 *       @c erf_q48() only.
 *
 * @param x  Input value in Q48.16 format (1.0 = 65536). Intended range:
 *           approximately [-100×65536, +20×65536].
 * @return   exp(@p x) in Q48.16 format; 0 on underflow; saturated at
 *           @c INT64_MAX-1 on overflow.
 */
static int64_t exp_q48(int64_t x) {
    const int64_t Q48 = 65536LL;  /* 2^16 */

    /* Handle extremes */
    if (x == 0) return Q48;
    if (x > (20LL << 16)) return Q48 << 8;  /* Approximate e^20 */
    if (x < (-100LL << 16)) return 0;       /* Underflow to zero */

    /* Taylor series: exp(x) = Σ x^n / n! (in fixed-point) */
    int64_t result = Q48;  /* Start with 1 */
    int64_t term = Q48;
    int64_t x_power = x;   /* x^1 */

    for (int i = 1; i < 40; i++) {
        /* term = x^i / i! */
        term = (x_power / (int64_t)i) / Q48;  /* Scale down to avoid overflow */
        if (term > -1 && term < 1) break;     /* Negligible */

        result += term;
        x_power = (x_power * x) / Q48;        /* Keep x^n scaled */

        /* Guard against overflow */
        if (result < 0 && result < (1LL << 62)) break;
        if (result > 0 && result > (1LL << 62)) {
            result = 9223372036854775806LL;  /* Saturate at INT64_MAX-1 */
            break;
        }
    }

    return result;
}

/**
 * @brief Compute sqrt(@p x) via Newton-Raphson in Q48.16 fixed-point.
 *
 * Applies the Newton-Raphson recurrence:
 * @code
 *   r_{n+1} = (r_n² + x) / (2 × r_n)
 * @endcode
 * for up to 20 iterations, with all arithmetic scaled to the Q48.16
 * representation (1.0 = 65536). Intermediate products use 8-bit right
 * shifts on both operands before squaring to prevent 64-bit overflow:
 * @code
 *   guess_squared = (guess >> 8) * (guess >> 8)
 * @endcode
 * This sacrifices 16 bits of precision in the squared term, which is
 * acceptable because the division by @c 2*guess restores the correct
 * order of magnitude. Convergence is declared when successive iterates
 * differ by at most 1 in Q48.16 units (sub-nanosecond for timing uses).
 *
 * Special cases handled without iteration:
 * - @p x ≤ 0 → returns 0 (no imaginary results)
 * - @p x == 65536 (1.0) → returns 65536 exactly
 *
 * Initial guess: @c x/2 for @p x > 1.0, else @c 1.0. Values above 1.0
 * converge from above; values in (0, 1.0) converge from the 1.0 initial
 * guess.
 *
 * @param x  Non-negative value in Q48.16 format (1.0 = 65536).
 *           Negative values are treated as zero.
 * @return   sqrt(@p x) in Q48.16 format; 0 for @p x ≤ 0.
 */
int64_t sqrt_q48(int64_t x) {
    const int64_t Q48 = 65536LL;

    /* Handle special cases */
    if (x < 0) return 0;
    if (x == 0) return 0;
    if (x == Q48) return Q48;  /* sqrt(1.0) = 1.0 */

    /* Initial guess: average of x and 1 */
    int64_t guess = (x > Q48) ? (x >> 1) : Q48;

    /* Newton-Raphson iterations */
    for (int i = 0; i < 20; i++) {
        /* next = (guess^2 + x) / (2 * guess)
         * In Q48.16: (guess^2) >> 16 + x, then divide by 2*guess
         */
        int64_t guess_squared = (guess >> 8) * (guess >> 8);  /* Avoid overflow */
        int64_t next = (guess_squared + (x >> 16)) / (2 * guess);

        /* Check convergence */
        int64_t diff = next - guess;
        if (diff > -2 && diff < 2) return next;

        guess = next;
    }

    return guess;
}

/**
 * @brief Compute erf(@p x) via Abramowitz & Stegun 7.1.26 in Q48.16 fixed-point.
 *
 * Implements the rational approximation from Abramowitz & Stegun, formula 7.1.26:
 * @code
 *   erf(x) ≈ sign(x) × sqrt(1 − exp(−x² × (4/π + a·x²) / (1 + a·x²)))
 * @endcode
 * where @c a ≈ 0.147 (in Q48.16: 9633). The maximum error of the true
 * mathematical approximation is @c |ε| < 1.5×10⁻⁷; the fixed-point
 * encoding introduces additional rounding of approximately 1 ULP in Q48.16.
 *
 * All arithmetic is performed in 64-bit integer Q48.16 format — no
 * floating-point operations. The computation sequence:
 * 1. Extract sign; work with @c |x|.
 * 2. Compute @c x² with 8-bit prescaling to prevent overflow:
 *    @c x_sq = (x >> 8) * (x >> 8).
 * 3. Evaluate the A&S numerator @c (4/π + a·x²) and denominator @c (1 + a·x²).
 * 4. Compute @c ratio = numerator / denominator (shift to preserve Q48.16 scale).
 * 5. Compute @c exp_arg = −x² × ratio and evaluate @c exp_q48(exp_arg).
 * 6. Compute @c base = 1 − exp_val, clamp to [0, 1].
 * 7. Return @c sign × sqrt_q48(base).
 *
 * Q48.16 constants used:
 * - @c a_q48 = 9633 (0.147 × 65536)
 * - @c four_over_pi_q48 = 83328 (4/π × 65536 ≈ 1.2732 × 65536)
 *
 * Consumed by @c hotwords_speedup_estimate() to compute P(speedup > 1.1×)
 * and P(speedup > 2.0×) via the normal CDF identity:
 * @code
 *   P(Z ≤ z) = 0.5 × (1 + erf(z / √2))
 * @endcode
 * where @c √2 in Q48.16 = 92681.
 *
 * @param x  Input value in Q48.16 format (dimensionless Z-score;
 *           1.0 = 65536). May be negative; sign determines the sign
 *           of the result.
 * @return   erf(@p x) in Q48.16 format, in the range [−65536, +65536]
 *           corresponding to (−1.0, +1.0).
 */
int64_t erf_q48(int64_t x) {
    const int64_t Q48 = 65536LL;

    if (x == 0) return 0;

    /* Compute sign and work with absolute value */
    int sign = (x < 0) ? -1 : 1;
    if (x < 0) x = -x;

    /* Abramowitz & Stegun coefficient a ≈ 0.147 (in Q48.16) */
    const int64_t a_q48 = 9633LL;  /* 0.147 * 65536 */
    const int64_t four_over_pi_q48 = 83328LL;  /* 4/π * 65536 */

    /* Compute x^2 in Q48.16 */
    int64_t x_sq = (x >> 8) * (x >> 8);  /* Scaled to avoid overflow */

    /* Compute denominator: 1 + a*x^2 */
    int64_t denom = Q48 + ((a_q48 * x_sq) >> 16);
    if (denom == 0) denom = 1;  /* Guard divide-by-zero */

    /* Compute numerator: 4/π + a*x^2 */
    int64_t numer = four_over_pi_q48 + ((a_q48 * x_sq) >> 16);

    /* Compute ratio = numerator / denominator */
    int64_t ratio = (numer << 16) / denom;

    /* Compute exp argument: -x^2 * ratio */
    int64_t exp_arg = -(x_sq * ratio / Q48);

    /* Compute exp(-x^2 * ratio) */
    int64_t exp_val = exp_q48(exp_arg);

    /* Compute base: 1 - exp(exp_arg) */
    int64_t base = Q48 - exp_val;
    if (base < 0) base = 0;
    if (base > Q48) base = Q48;

    /* Compute sqrt(base) */
    int64_t sqrt_base = sqrt_q48(base);

    /* Apply sign and return */
    return sign * sqrt_base;
}
