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

/**
 * q48_16.h - Q48.16 Fixed-Point Arithmetic for StarKernel
 *
 * Format: uint64_t with fixed decimal point after bit 15
 *   - Bits 0-15: Fractional part (1/65536 resolution)
 *   - Bits 16-63: Integer part (up to 2^48-1)
 *
 * Example: 0x00010000 = 1.0, 0x00018000 = 1.5
 *
 * All operations are integer-only. NO FLOATING-POINT.
 */

#ifndef STARKERNEL_Q48_16_H
#define STARKERNEL_Q48_16_H

#include <stdint.h>

typedef uint64_t q48_16_t;

/* Q48.16 representation of 1.0 */
#define Q48_ONE  ((q48_16_t)0x10000ULL)

/* ============================================================================
 * Core Arithmetic Operations
 * ============================================================================ */

/**
 * Multiply two Q48.16 values: (a * b) >> 16
 */
q48_16_t q48_mul(q48_16_t a, q48_16_t b);

/**
 * Divide two Q48.16 values: (a << 16) / b
 * Returns 0 if b == 0.
 */
q48_16_t q48_div(q48_16_t a, q48_16_t b);

/**
 * Add two Q48.16 values
 */
static inline q48_16_t q48_add(q48_16_t a, q48_16_t b) {
    return a + b;
}

/**
 * Subtract two Q48.16 values
 */
static inline q48_16_t q48_sub(q48_16_t a, q48_16_t b) {
    return a - b;
}

/**
 * Absolute value of Q48.16 (treats as unsigned, so just returns a)
 */
static inline q48_16_t q48_abs(q48_16_t a) {
    return a;  /* Q48.16 is unsigned; for signed use, caller handles */
}

/* ============================================================================
 * Conversion Operations
 * ============================================================================ */

/**
 * Convert unsigned 64-bit integer to Q48.16: u << 16
 */
static inline q48_16_t q48_from_u64(uint64_t u) {
    return u << 16;
}

/**
 * Convert Q48.16 to unsigned 64-bit integer (truncate fractional): q >> 16
 */
static inline uint64_t q48_to_u64(q48_16_t q) {
    return q >> 16;
}

/* ============================================================================
 * Approximation Operations (Integer-Only)
 * ============================================================================ */

/**
 * Approximate natural logarithm in Q48.16 (integer-only)
 * Input: x in Q48.16 format (x > 0)
 * Output: ln(x) in Q48.16 format
 */
q48_16_t q48_log_approx(q48_16_t x);

/**
 * Approximate exponential e^x in Q48.16 (integer-only)
 * Input: q in Q48.16 format
 * Output: e^q in Q48.16 format
 */
q48_16_t q48_exp_approx(q48_16_t q);

/**
 * Approximate square root in Q48.16 (integer-only, Newton-Raphson)
 * Input: q in Q48.16 format
 * Output: sqrt(q) in Q48.16 format
 */
q48_16_t q48_sqrt_approx(q48_16_t q);

#endif /* STARKERNEL_Q48_16_H */
