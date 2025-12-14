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
 * math_portable.h
 *
 * Portable C99 math functions in Q48.16 64-bit fixed-point format.
 * Suitable for L4Re and embedded environments with no libm.
 * All arithmetic is integer-only for maximum performance.
 *
 * Q48.16 Format:
 *   - 48-bit signed integer + 16-bit fractional part
 *   - Precision: 2^-16 ≈ 0.0000153 (nanoseconds)
 *   - Range: ±140 trillion ns (≈ 4.4 years)
 *
 * Functions:
 *   sqrt_q48(x)  - Square root of Q48.16 value via Newton-Raphson
 *   erf_q48(x)   - Error function of Q48.16 value (dimensionless)
 */

#ifndef MATH_PORTABLE_H
#define MATH_PORTABLE_H

#include <stdint.h>

#define Q48_SCALE (1LL << 16)  /* 65536 */

/**
 * Square root in Q48.16 fixed-point
 * Input: x in Q48.16 format
 * Output: sqrt(x) in Q48.16 format
 * Uses Newton-Raphson: x_{n+1} = (x_n + a/x_n) / 2
 * Converges in ~10-15 iterations
 * Returns 0 for negative inputs (guard against domain error)
 */
int64_t sqrt_q48(int64_t x);

/**
 * Error function in Q48.16 fixed-point
 * Input: x in Q48.16 format (dimensionless Z-score)
 * Output: erf(x) in Q48.16 format (range: -Q48_SCALE to +Q48_SCALE)
 * Uses Abramowitz & Stegun approximation (7.1.26)
 * Maximum error: 1.5e-7 relative to floating-point erf
 * Properties:
 *   - erf(0) = 0
 *   - erf(-x) = -erf(x) (odd function)
 *   - erf(x) approaches ±Q48_SCALE for large |x|
 */
int64_t erf_q48(int64_t x);

#endif /* MATH_PORTABLE_H */
