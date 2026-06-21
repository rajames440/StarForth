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

/* mixed_arithmetic_words.c - FORTH-79 Mixed Arithmetic Words */
#include "include/mixed_arithmetic_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* Implementation of FORTH-79 Mixed Arithmetic Words */

/**
 * @brief M+ ( d n -- d )
 *
 * Adds the single-cell signed integer @c n to the double-cell value @c d,
 * producing a new double-cell result. Handles carry propagation: if the low
 * cell addition overflows a positive @c n, the high cell is incremented; if it
 * underflows a negative @c n, the high cell is decremented.
 *
 * Stack effect: ( d_high d_low n -- d_high' d_low' )
 *
 * @param vm Active VM; sets @c vm->error = 1 on stack underflow
 */
void mixed_math_word_m_plus(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm); // single
    cell_t dlow = vm_pop(vm); // low part of double
    cell_t dhigh = vm_pop(vm); // high part of double

    cell_t old_dlow = dlow;
    dlow += n;

    if (n > 0 && dlow < old_dlow) {
        dhigh++;
    } else if (n < 0 && dlow > old_dlow) {
        dhigh--;
    }

    // Push high first, then low (low is TOS)
    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/**
 * @brief M- ( d n -- d )
 *
 * Subtracts the single-cell signed integer @c n from the double-cell value @c d,
 * producing a new double-cell result. Mirrors @c M+ carry logic: a positive @c n
 * that causes the low cell to wrap below zero decrements the high cell, and vice versa.
 *
 * Stack effect: ( d_high d_low n -- d_high' d_low' )
 *
 * @param vm Active VM; sets @c vm->error = 1 on stack underflow
 */
void mixed_math_word_m_minus(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);

    cell_t old_dlow = dlow;
    dlow -= n;

    if (n > 0 && dlow > old_dlow) {
        dhigh--;
    } else if (n < 0 && dlow < old_dlow) {
        dhigh++;
    }

    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/**
 * @brief M* ( n1 n2 -- d )
 *
 * Multiplies two single-cell signed integers, producing a double-cell result.
 * On 64-bit builds the product is computed via @c long long and the low 32 bits
 * go to the deeper stack cell while the high 32 bits go to TOS. On 32-bit builds
 * the result fits in a single cell so the high cell is pushed as 0.
 *
 * Stack effect: ( n1 n2 -- d_low d_high )   TOS = high
 *
 * @param vm Active VM; sets @c vm->error = 1 on stack underflow
 */
void mixed_math_word_m_star(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);

    if (sizeof(cell_t) == 8) {
        long long result = (long long) n1 * (long long) n2;
        cell_t dlow = (cell_t)(result & 0xFFFFFFFFLL);
        cell_t dhigh = (cell_t)(result >> 32);
        vm_push(vm, dlow); // lo first
        vm_push(vm, dhigh); // hi last (TOS)
    } else {
        cell_t result = n1 * n2;
        vm_push(vm, result);
        vm_push(vm, 0);
    }
}

/**
 * @brief M/MOD ( d n -- rem quot )
 *
 * Divides the double-cell dividend @c d by the single-cell divisor @c n,
 * pushing the remainder deeper and the quotient on TOS. On 64-bit builds,
 * the double is reconstructed as a @c long long by shifting the high 32 bits
 * left; the UBSan-safe path uses an unsigned shift before casting to signed to
 * avoid undefined behaviour on negative values. On 32-bit builds, only handles
 * the case where the high cell is zero; sets @c vm->error = 1 otherwise.
 *
 * Stack effect: ( d_high d_low n -- remainder quotient )   TOS = quotient
 *
 * @param vm Active VM; sets @c vm->error = 1 on underflow or division by zero
 */
void mixed_math_word_m_slash_mod(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);

    if (n == 0) {
        vm->error = 1;
        return;
    }

    if (sizeof(cell_t) == 8) {
        /* UBSan fix: Perform shift on unsigned, then cast to signed (2025-12-09)
         * Left-shifting negative signed values is undefined behavior */
        long long dividend = (long long)(((unsigned long long) dhigh << 32) |
                                         ((unsigned long long) dlow & 0xFFFFFFFFLL));
        cell_t quotient = (cell_t)(dividend / n);
        cell_t remainder = (cell_t)(dividend % n);
        vm_push(vm, remainder); // remainder first (deeper)
        vm_push(vm, quotient); // quotient last (TOS)
    } else {
        if (dhigh == 0) {
            vm_push(vm, dlow % n);
            vm_push(vm, dlow / n);
        } else {
            vm->error = 1; // 128-bit not yet implemented
        }
    }
}

/**
 * @brief MOD ( n1 n2 -- r )
 *
 * Computes @c n1 % @c n2, leaving the remainder on the stack. Sets
 * @c vm->error = 1 if @c n2 is zero (division by zero guard).
 *
 * Stack effect: ( n1 n2 -- remainder )
 *
 * @param vm Active VM; sets @c vm->error = 1 on underflow or zero divisor
 */
void mixed_math_word_mod(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);

    if (n2 == 0) {
        vm->error = 1;
        return;
    }

    vm_push(vm, n1 % n2);
}

/**
 * @brief /MOD ( n1 n2 -- rem quot )
 *
 * Divides @c n1 by @c n2, pushing the remainder deeper and the quotient on TOS.
 * Sets @c vm->error = 1 if @c n2 is zero.
 *
 * Stack effect: ( n1 n2 -- remainder quotient )   TOS = quotient
 *
 * @param vm Active VM; sets @c vm->error = 1 on underflow or zero divisor
 */
void mixed_math_word_slash_mod(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);

    if (n2 == 0) {
        vm->error = 1;
        return;
    }

    cell_t quotient = n1 / n2;
    cell_t remainder = n1 % n2;
    vm_push(vm, remainder); // deeper
    vm_push(vm, quotient); // TOS
}

/**
 * @brief STAR-SLASH aka */ ( n1 n2 n3 -- n4 )
 *
 * Computes @c (n1 * n2) / n3 using a @c long long intermediate on 64-bit builds
 * to avoid intermediate overflow. On 32-bit builds falls back to @c long double.
 * Sets @c vm->error = 1 if @c n3 is zero.
 *
 * Stack effect: ( n1 n2 n3 -- quotient )
 *
 * @param vm Active VM; sets @c vm->error = 1 on underflow or zero divisor
 */
void mixed_math_word_star_slash(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t n3 = vm_pop(vm);
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);

    if (n3 == 0) {
        vm->error = 1;
        return;
    }

    if (sizeof(cell_t) == 8) {
        long long intermediate = (long long) n1 * (long long) n2;
        vm_push(vm, (cell_t)(intermediate / n3));
    } else {
        long double intermediate = (long double) n1 * (long double) n2;
        vm_push(vm, (cell_t)(intermediate / n3));
    }
}

/**
 * @brief STAR-SLASH-MOD aka */MOD ( n1 n2 n3 -- rem quot )
 *
 * Computes @c (n1 * n2) / n3 and leaves both the remainder (deeper) and
 * quotient (TOS). Uses @c long long on 64-bit builds; falls back to
 * @c long double with integer remainder reconstruction on 32-bit builds.
 * Sets @c vm->error = 1 if @c n3 is zero.
 *
 * Stack effect: ( n1 n2 n3 -- remainder quotient )   TOS = quotient
 *
 * @param vm Active VM; sets @c vm->error = 1 on underflow or zero divisor
 */
void mixed_math_word_star_slash_mod(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t n3 = vm_pop(vm);
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);

    if (n3 == 0) {
        vm->error = 1;
        return;
    }

    if (sizeof(cell_t) == 8) {
        long long intermediate = (long long) n1 * (long long) n2;
        cell_t quotient = (cell_t)(intermediate / n3);
        cell_t remainder = (cell_t)(intermediate % n3);
        vm_push(vm, remainder); // remainder deeper
        vm_push(vm, quotient); // quotient TOS
    } else {
        long double intermediate = (long double) n1 * (long double) n2;
        cell_t quotient = (cell_t)(intermediate / n3);
        cell_t remainder = n1 * n2 - quotient * n3;
        vm_push(vm, remainder);
        vm_push(vm, quotient);
    }
}

/**
 * @brief Register all FORTH-79 mixed arithmetic words with the VM dictionary.
 *
 * Registers: @c M+, @c M-, @c M*, @c M/MOD, @c MOD, @c /MOD, @c *\/,
 * @c *\/MOD. Called during VM bootstrap.
 *
 * @param vm Active VM to register words into
 */
void register_mixed_arithmetic_words(VM *vm) {
    register_word(vm, "M+", mixed_math_word_m_plus);
    register_word(vm, "M-", mixed_math_word_m_minus);
    register_word(vm, "M*", mixed_math_word_m_star);
    register_word(vm, "M/MOD", mixed_math_word_m_slash_mod);
    register_word(vm, "MOD", mixed_math_word_mod);
    register_word(vm, "/MOD", mixed_math_word_slash_mod);
    register_word(vm, "*/", mixed_math_word_star_slash);
    register_word(vm, "*/MOD", mixed_math_word_star_slash_mod);
}