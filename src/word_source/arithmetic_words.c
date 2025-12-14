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

/* arithmetic_words.c - FORTH-79 Arithmetic Words */
#include "include/arithmetic_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* FORTH-79 Arithmetic Words:
 * +         ( n1 n2 -- n3 )             Add n1 and n2
 * -         ( n1 n2 -- n3 )             Subtract n2 from n1
 * *         ( n1 n2 -- n3 )             Multiply n1 and n2
 * /         ( n1 n2 -- n3 )             Divide n1 by n2
 * MOD       ( n1 n2 -- n3 )             n1 modulo n2
 * /MOD      ( n1 n2 -- n3 n4 )          n1/n2 remainder and quotient
 * STAR-SLASH ( n1 n2 n3 -- n4 )         n1*n2/n3 with intermediate double
 * STAR-SLASH-MOD ( n1 n2 n3 -- n4 n5 )  n1*n2/n3 remainder and quotient
 * 1+        ( n -- n+1 )                Add 1
 * 1-        ( n -- n-1 )                Subtract 1
 * 2+        ( n -- n+2 )                Add 2
 * 2-        ( n -- n-2 )                Subtract 2
 * 2*        ( n -- n*2 )                Multiply by 2 (left shift)
 * 2/        ( n -- n/2 )                Divide by 2 (right shift)
 * ABS       ( n -- |n| )                Absolute value
 * NEGATE    ( n -- -n )                 Two's complement
 * MIN       ( n1 n2 -- n3 )             Minimum of n1 and n2
 * MAX       ( n1 n2 -- n3 )             Maximum of n1 and n2
 */

/* Forward declarations */
static void arithmetic_word_star_slash(VM * vm);
static void arithmetic_word_star_slash_mod(VM * vm);

/**
 * @brief Implements FORTH word "+" that adds two numbers
 * 
 * Stack effect: ( n1 n2 -- n3 )
 * Adds n1 and n2, pushing their sum n3 onto the stack.
 * 
 * @param vm Pointer to the VM structure
 */
void arithmetic_word_plus(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "+: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = n1 + n2;

    vm_push(vm, result);
    log_message(LOG_DEBUG, "+: %ld + %ld = %ld", n1, n2, result);
}

/**
 * @brief Implements FORTH word "-" that subtracts two numbers
 * 
 * Stack effect: ( n1 n2 -- n3 )
 * Subtracts n2 from n1, pushing their difference n3 onto the stack.
 * 
 * @param vm Pointer to the VM structure
 */
static void arithmetic_word_minus(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "-: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = n1 - n2;

    vm_push(vm, result);
    log_message(LOG_DEBUG, "-: %ld - %ld = %ld", n1, n2, result);
}

/**
 * @brief Implements FORTH word "*" that multiplies two numbers
 * 
 * Stack effect: ( n1 n2 -- n3 )
 * Multiplies n1 by n2, pushing their product n3 onto the stack.
 * 
 * @param vm Pointer to the VM structure
 */
static void arithmetic_word_multiply(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "*: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = n1 * n2;

    vm_push(vm, result);
    log_message(LOG_DEBUG, "*: %ld * %ld = %ld", n1, n2, result);
}

/* / ( n1 n2 -- n3 ) Divide n1 by n2 */
static void word_divide(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "/: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);

    if (n2 == 0) {
        log_message(LOG_ERROR, "/: Division by zero");
        vm->error = 1;
        return;
    }

    cell_t result = n1 / n2;
    vm_push(vm, result);
    log_message(LOG_DEBUG, "/: %ld / %ld = %ld", n1, n2, result);
}

/* MOD ( n1 n2 -- n3 ) n1 modulo n2 */
static void mixed_math_word_mod(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "MOD: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);

    if (n2 == 0) {
        log_message(LOG_ERROR, "MOD: Division by zero");
        vm->error = 1;
        return;
    }

    cell_t result = n1 % n2;
    vm_push(vm, result);
    log_message(LOG_DEBUG, "MOD: %ld MOD %ld = %ld", n1, n2, result);
}

/* /MOD ( n1 n2 -- n3 n4 ) n1/n2 remainder and quotient */
static void word_divmod(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "/MOD: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);

    if (n2 == 0) {
        log_message(LOG_ERROR, "/MOD: Division by zero");
        vm->error = 1;
        return;
    }

    cell_t remainder = n1 % n2;
    cell_t quotient = n1 / n2;

    vm_push(vm, remainder); /* remainder first */
    vm_push(vm, quotient); /* quotient on top */
    log_message(LOG_DEBUG, "/MOD: %ld /MOD %ld = %ld remainder %ld", n1, n2, quotient, remainder);
}

/* STAR-SLASH ( n1 n2 n3 -- n4 ) n1*n2/n3 with intermediate double */
static void arithmetic_word_star_slash(VM *vm) {
    if (vm->dsp < 2) {
        log_message(LOG_ERROR, "*/: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n3 = vm_pop(vm); /* divisor */
    cell_t n2 = vm_pop(vm); /* multiplier */
    cell_t n1 = vm_pop(vm); /* multiplicand */

    if (n3 == 0) {
        log_message(LOG_ERROR, "*/: Division by zero");
        vm->error = 1;
        return;
    }

    /* Use 64-bit intermediate to avoid overflow */
    int64_t intermediate = n1 * n2;
    cell_t result = (intermediate / n3);

    vm_push(vm, result);
    log_message(LOG_DEBUG, "*/: %ld * %ld / %ld = %ld", n1, n2, n3, result);
}

/* STAR-SLASH-MOD ( n1 n2 n3 -- n4 n5 ) n1*n2/n3 remainder and quotient */
static void arithmetic_word_star_slash_mod(VM *vm) {
    if (vm->dsp < 2) {
        log_message(LOG_ERROR, "*/MOD: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n3 = vm_pop(vm); /* divisor */
    cell_t n2 = vm_pop(vm); /* multiplier */
    cell_t n1 = vm_pop(vm); /* multiplicand */

    if (n3 == 0) {
        log_message(LOG_ERROR, "*/MOD: Division by zero");
        vm->error = 1;
        return;
    }

    /* Use 64-bit intermediate to avoid overflow */
    int64_t intermediate = n1 * n2;
    cell_t remainder = (intermediate % n3);
    cell_t quotient = (intermediate / n3);

    vm_push(vm, remainder); /* remainder first */
    vm_push(vm, quotient); /* quotient on top */
}

/* 1+ ( n -- n+1 ) Add 1 */
static void arithmetic_word_one_plus(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "1+: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    vm_push(vm, n + 1);
    log_message(LOG_DEBUG, "1+: %ld + 1 = %ld", n, (n + 1));
}

/* 1- ( n -- n-1 ) Subtract 1 */
static void arithmetic_word_one_minus(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "1-: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    vm_push(vm, n - 1);
    log_message(LOG_DEBUG, "1-: %ld - 1 = %ld", n, (n - 1));
}

/* 2+ ( n -- n+2 ) Add 2 */
static void arithmetic_word_two_plus(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "2+: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    vm_push(vm, n + 2);
    log_message(LOG_DEBUG, "2+: %ld + 2 = %ld", n, (n + 2));
}

/* 2- ( n -- n-2 ) Subtract 2 */
static void arithmetic_word_two_minus(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "2-: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    vm_push(vm, n - 2);
    log_message(LOG_DEBUG, "2-: %ld - 2 = %ld", n, (n - 2));
}

/* 2* ( n -- n*2 ) Multiply by 2 (left shift) */
static void arithmetic_word_two_multiply(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "2*: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    vm_push(vm, n << 1);
    log_message(LOG_DEBUG, "2*: %ld * 2 = %ld", n, (n << 1));
}

/* 2/ ( n -- n/2 ) Divide by 2 (right shift) */
static void arithmetic_word_two_divide(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "2/: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    vm_push(vm, n >> 1);
    log_message(LOG_DEBUG, "2/: %ld / 2 = %ld", n, (n >> 1));
}

/* ABS ( n -- |n| ) Absolute value */
static void arithmetic_word_abs(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "ABS: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    cell_t result = (n < 0) ? -n : n;
    vm_push(vm, result);
    log_message(LOG_DEBUG, "ABS: |%ld| = %ld", n, result);
}

/* NEGATE ( n -- -n ) Two's complement */
static void arithmetic_word_negate(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "NEGATE: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    vm_push(vm, -n);
    log_message(LOG_DEBUG, "NEGATE: -%ld = %ld", n, (-n));
}

/* MIN ( n1 n2 -- n3 ) Minimum of n1 and n2 */
static void arithmetic_word_min(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "MIN: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = (n1 < n2) ? n1 : n2;

    vm_push(vm, result);
    log_message(LOG_DEBUG, "MIN: min(%ld, %ld) = %ld", n1, n2, result);
}

/* MAX ( n1 n2 -- n3 ) Maximum of n1 and n2 */
static void arithmetic_word_max(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "MAX: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = (n1 > n2) ? n1 : n2;

    vm_push(vm, result);
    log_message(LOG_DEBUG, "MAX: max(%ld, %ld) = %ld", n1, n2, result);
}

/**
 * @brief Registers all FORTH-79 arithmetic words with the VM
 *
 * This function registers all the standard FORTH-79 arithmetic operations
 * including basic arithmetic, advanced math operations, increment/decrement
 * operations, and comparison functions.
 *
 * @param vm Pointer to the VM structure where words will be registered
 */
void register_arithmetic_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 arithmetic words...");

    /* Basic arithmetic */
    register_word(vm, "+", arithmetic_word_plus);
    register_word(vm, "-", arithmetic_word_minus);
    register_word(vm, "*", arithmetic_word_multiply);
    register_word(vm, "/", word_divide);
    register_word(vm, "MOD", mixed_math_word_mod);
    register_word(vm, "/MOD", word_divmod);

    /* Advanced arithmetic */
    register_word(vm, "*/", arithmetic_word_star_slash);
    register_word(vm, "*/MOD", arithmetic_word_star_slash_mod);

    /* Increment/decrement */
    register_word(vm, "1+", arithmetic_word_one_plus);
    register_word(vm, "1-", arithmetic_word_one_minus);
    register_word(vm, "2+", arithmetic_word_two_plus);
    register_word(vm, "2-", arithmetic_word_two_minus);
    register_word(vm, "2*", arithmetic_word_two_multiply);
    register_word(vm, "2/", arithmetic_word_two_divide);

    /* Sign and comparison */
    register_word(vm, "ABS", arithmetic_word_abs);
    register_word(vm, "NEGATE", arithmetic_word_negate);
    register_word(vm, "MIN", arithmetic_word_min);
    register_word(vm, "MAX", arithmetic_word_max);
}