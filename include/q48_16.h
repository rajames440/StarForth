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

#ifndef Q48_16_H
#define Q48_16_H

#include <stdint.h>

/* ============================================================================
 * Q48.16 Fixed-Point Arithmetic
 * ============================================================================
 *
 * Format: uint64_t with fixed decimal point after bit 15
 *   - Bits 0-15: Fractional part (1/65536 resolution)
 *   - Bits 16-63: Integer part (up to 2^48-1)
 *
 * Example: 0x00010000 = 1.0, 0x00018000 = 1.5, 0x00010001 = 1 + 1/65536
 *
 * All operations are integer-only: no floating-point, no transcendental functions.
 * Used for inference engine calculations (variance, regression, diagnostics).
 *
 * Why Q48.16?
 * - 48-bit integer range: 0 to 281,474,976,710,655 (enough for VM metrics)
 * - 16-bit fractional: ~0.0000152 resolution (precise enough for statistical calculations)
 * - Perfect fit for 64-bit uint64_t: efficient on modern CPUs
 */

typedef uint64_t q48_16_t;

/* ============================================================================
 * Core Arithmetic Operations (All Q48.16 ↔ Q48.16)
 * ============================================================================
 */

/**
 * @brief Multiply two Q48.16 values, keeping result in Q48.16
 *
 * Math: (a / 2^16) * (b / 2^16) * 2^16 = (a * b) / 2^16
 * Returns: (a * b) >> 16
 *
 * @param a First Q48.16 operand
 * @param b Second Q48.16 operand
 * @return (a * b) >> 16 in Q48.16 format
 */
q48_16_t q48_mul(q48_16_t a, q48_16_t b);

/**
 * @brief Divide two Q48.16 values, keeping result in Q48.16
 *
 * Math: (a / 2^16) / (b / 2^16) * 2^16 = (a << 16) / b
 * Returns: (a << 16) / b
 *
 * @param a Dividend in Q48.16
 * @param b Divisor in Q48.16 (non-zero)
 * @return (a << 16) / b in Q48.16 format
 */
q48_16_t q48_div(q48_16_t a, q48_16_t b);

/**
 * @brief Add two Q48.16 values
 *
 * Math: (a / 2^16) + (b / 2^16) = (a + b) / 2^16
 * Returns: a + b (no shift needed, both have same scaling)
 *
 * @param a First Q48.16 operand
 * @param b Second Q48.16 operand
 * @return a + b in Q48.16 format
 */
static inline q48_16_t q48_add(q48_16_t a, q48_16_t b) {
    return a + b;
}

/**
 * @brief Subtract two Q48.16 values
 *
 * Math: (a / 2^16) - (b / 2^16) = (a - b) / 2^16
 * Returns: a - b (no shift needed)
 *
 * @param a Minuend in Q48.16
 * @param b Subtrahend in Q48.16
 * @return a - b in Q48.16 format
 */
static inline q48_16_t q48_sub(q48_16_t a, q48_16_t b) {
    return a - b;
}

/**
 * @brief Absolute value of Q48.16
 *
 * @param a Q48.16 value
 * @return |a| in Q48.16 format
 */
static inline q48_16_t q48_abs(q48_16_t a) {
    return (a < 0x8000000000000000ULL) ? a : (0ULL - a);
}

/* ============================================================================
 * Conversion Operations (u64 ↔ Q48.16)
 * ============================================================================
 */

/**
 * @brief Convert unsigned 64-bit integer to Q48.16
 *
 * Math: u * 2^16 (shift left by 16 bits)
 * Example: q48_from_u64(1) = 0x10000 (1.0 in Q48.16)
 *
 * @param u Unsigned 64-bit integer (max 2^48-1 to avoid overflow)
 * @return u << 16 in Q48.16 format
 */
static inline q48_16_t q48_from_u64(uint64_t u) {
    return u << 16;
}

/**
 * @brief Convert Q48.16 to unsigned 64-bit integer (truncate fractional)
 *
 * Math: q / 2^16 (shift right by 16 bits)
 * Example: q48_to_u64(0x10000) = 1
 *
 * @param q Q48.16 value
 * @return q >> 16 as unsigned 64-bit integer
 */
static inline uint64_t q48_to_u64(q48_16_t q) {
    return q >> 16;
}

/**
 * @brief Convert double to Q48.16 (for testing/initialization only)
 *
 * NOT used in inference engine (inference is integer-only).
 * Provided for testing and diagnostic tools only.
 *
 * @param d Double-precision float
 * @return d * 2^16 as Q48.16
 */
q48_16_t q48_from_double(double d);

/**
 * @brief Convert Q48.16 to double (for testing/diagnostics only)
 *
 * NOT used in inference engine (inference is integer-only).
 * Provided for logging and RStudio dashboard output.
 *
 * @param q Q48.16 value
 * @return q / 2^16 as double
 */
double q48_to_double(q48_16_t q);

/* ============================================================================
 * Approximation Operations (Integer-Only)
 * ============================================================================
 */

/**
 * @brief Approximate natural logarithm in Q48.16 (integer-only)
 *
 * Purpose: Required for decay slope inference via exponential fitting
 * Model: ln(heat[t]) = ln(h0) - slope*t
 *
 * Method: Piecewise linear approximation or Newton-Raphson iteration
 * - Input x must be > 0 (assertion check)
 * - Output is ln(x) in Q48.16 format
 * - Precision: ~5% error acceptable (tuning parameter)
 * - No floating-point operations
 *
 * Implementation Strategy:
 * 1. Use bit position (floor(log2(x))) as coarse estimate
 * 2. Refine with Newton iteration (3-4 cycles for Q48.16 precision)
 * 3. Keep all arithmetic in integer space
 *
 * Example: q48_log_approx(65536) = log(1.0) in Q48.16 ≈ 0
 *          q48_log_approx(131072) = log(2.0) in Q48.16 ≈ 0x0000B172
 *
 * @param x Value to take logarithm of (x > 0)
 * @return ln(x) in Q48.16 format
 */
q48_16_t q48_log_approx(uint64_t x);

/**
 * @brief Approximate exponential e^x in Q48.16 (integer-only, future use)
 *
 * Purpose: Might be needed for inverse operations or model validation
 * Status: Defined but not required for Phase 1 inference engine
 * Method: Similar to q48_log_approx (piecewise + Newton)
 *
 * @param q Q48.16 exponent
 * @return e^q in Q48.16 format
 */
q48_16_t q48_exp_approx(q48_16_t q);

/**
 * @brief Approximate square root in Q48.16 (integer-only)
 *
 * Purpose: Compute R² (fit quality) for regression diagnostics
 * Method: Newton-Raphson for integer-only sqrt
 *
 * @param q Q48.16 value
 * @return sqrt(q) in Q48.16 format
 */
q48_16_t q48_sqrt_approx(q48_16_t q);

/* ============================================================================
 * Diagnostic / Testing Utilities
 * ============================================================================
 */

/**
 * @brief Pretty-print Q48.16 value to string for debugging
 *
 * Format: "integer.fractional" (e.g., "1.50000")
 * Uses internal static buffer - NOT thread-safe
 *
 * @param q Q48.16 value
 * @return Pointer to static string (valid until next call)
 */
const char* q48_to_string(q48_16_t q);

/**
 * @brief Validate Q48.16 value is within safe range
 *
 * Checks: Does not overflow on mul/div operations
 * Returns: 1 if valid, 0 if risky
 *
 * @param q Q48.16 value
 * @return 1 if safe, 0 if risky
 */
int q48_is_valid(q48_16_t q);

#endif /* Q48_16_H */