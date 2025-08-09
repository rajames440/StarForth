/*

                                 ***   StarForth   ***
  mixed_arithmetic_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/* mixed_arithmetic_words.c - FORTH-79 Mixed Arithmetic Words */
#include "include/mixed_arithmetic_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* Implementation of FORTH-79 Mixed Arithmetic Words */

/* M+ ( d n -- d )  Add single to double */
void mixed_math_word_m_plus(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t n     = vm_pop(vm);     // single
    cell_t dlow  = vm_pop(vm);     // low part of double
    cell_t dhigh = vm_pop(vm);     // high part of double

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

/* M- ( d n -- d )  Subtract single from double */
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

/* M* ( n1 n2 -- d )  Multiply singles, produce double (lo, hi) */
void mixed_math_word_m_star(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);

    // TODO: Only supports 64-bit "doubles" (not true 128-bit); port real double-word math for L4Re portability
    if (sizeof(cell_t) == 8) {
        long long result = (long long)n1 * (long long)n2;
        cell_t dlow = (cell_t)(result & 0xFFFFFFFFLL);
        cell_t dhigh = (cell_t)(result >> 32);
        vm_push(vm, dlow);   // lo first
        vm_push(vm, dhigh);  // hi last (TOS)
    } else {
        cell_t result = n1 * n2;
        vm_push(vm, result);
        vm_push(vm, 0);
    }
}

/* M/MOD ( d n -- rem quot )  Divide double by single */
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

    // TODO: Only supports 64-bit "doubles" (not true 128-bit); port real double-word math for L4Re portability
    if (sizeof(cell_t) == 8) {
        long long dividend = ((long long)dhigh << 32) | ((unsigned long long)dlow & 0xFFFFFFFFLL);
        cell_t quotient = (cell_t)(dividend / n);
        cell_t remainder = (cell_t)(dividend % n);
        vm_push(vm, remainder);  // remainder first (deeper)
        vm_push(vm, quotient);   // quotient last (TOS)
    } else {
        if (dhigh == 0) {
            vm_push(vm, dlow % n);
            vm_push(vm, dlow / n);
        } else {
            vm->error = 1;  // 128-bit not yet implemented
        }
    }
}

/* MOD ( n1 n2 -- r ) */
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

/* /MOD ( n1 n2 -- rem quot ) */
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
    vm_push(vm, remainder);  // deeper
    vm_push(vm, quotient);   // TOS
}

/* STAR-SLASH ( n1 n2 n3 -- n4 )  n1*n2/n3 */
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

    // TODO: Only supports 64-bit "doubles" (not true 128-bit); port real double-word math for L4Re portability
    if (sizeof(cell_t) == 8) {
        long long intermediate = (long long)n1 * (long long)n2;
        vm_push(vm, (cell_t)(intermediate / n3));
    } else {
        long double intermediate = (long double)n1 * (long double)n2;
        vm_push(vm, (cell_t)(intermediate / n3));
    }
}

/* STAR-SLASH-MID ( n1 n2 n3 -- rem quot ) */
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

    // TODO: Only supports 64-bit "doubles" (not true 128-bit); port real double-word math for L4Re portability
    if (sizeof(cell_t) == 8) {
        long long intermediate = (long long)n1 * (long long)n2;
        cell_t quotient = (cell_t)(intermediate / n3);
        cell_t remainder = (cell_t)(intermediate % n3);
        vm_push(vm, remainder);  // remainder deeper
        vm_push(vm, quotient);   // quotient TOS
    } else {
        long double intermediate = (long double)n1 * (long double)n2;
        cell_t quotient = (cell_t)(intermediate / n3);
        cell_t remainder = n1 * n2 - quotient * n3;
        vm_push(vm, remainder);
        vm_push(vm, quotient);
    }
}

/* FORTH-79 Mixed Arithmetic Word Registration */
void register_mixed_arithmetic_words(VM *vm) {
    log_message(LOG_INFO, "Registering mixed arithmetic words...");

    register_word(vm, "M+", mixed_math_word_m_plus);
    register_word(vm, "M-", mixed_math_word_m_minus);
    register_word(vm, "M*", mixed_math_word_m_star);
    register_word(vm, "M/MOD", mixed_math_word_m_slash_mod);
    register_word(vm, "MOD", mixed_math_word_mod);
    register_word(vm, "/MOD", mixed_math_word_slash_mod);
    register_word(vm, "*/", mixed_math_word_star_slash);
    register_word(vm, "*/MOD", mixed_math_word_star_slash_mod);

    log_message(LOG_INFO, "Mixed arithmetic words registered and tested");
}
