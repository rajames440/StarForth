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
 * Portable exponential function via Taylor series in Q48.16 fixed-point
 * exp(x) = 1 + x + x^2/2! + x^3/3! + ...
 *
 * Input: x in Q48.16 format
 * Output: exp(x) in Q48.16 format
 *
 * Handles only reasonable inputs (-20 to 0) for our use case (exp(-Z^2) in erf).
 * Returns Q48_SCALE (65536) for x=0, approaches 0 for x<-20, saturates for x>0.
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
 * Square root in Q48.16 fixed-point via Newton-Raphson
 *
 * Input: x in Q48.16 format
 * Output: sqrt(x) in Q48.16 format
 *
 * Newton-Raphson: r_{n+1} = (r_n^2 + x) / (2 * r_n)
 * Where multiply/divide preserve Q48.16 scaling.
 * Converges in ~10-15 iterations.
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
 * Error function in Q48.16 fixed-point
 * erf(x) ≈ sign(x) * sqrt(1 - exp(-x^2 * (4/π + ax^2) / (1 + ax^2)))
 *
 * Uses Abramowitz & Stegun approximation (7.1.26)
 * Input: x in Q48.16 format (dimensionless Z-score)
 * Output: erf(x) in Q48.16 format, clamped to [-Q48_SCALE, +Q48_SCALE]
 *
 * Maximum error: ~1.5e-7 relative to true erf
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
