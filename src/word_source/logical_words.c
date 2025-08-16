/*

                                 ***   StarForth   ***
  logical_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/14/25, 8:34 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/* logical_words.c - FORTH-79 Logical & Comparison Words */
#include "include/logical_words.h"
#include "../../include/log.h"
#include "../../include/word_registry.h"
#include "../../include/platform/starforth_platform.h"

/* FORTH-79 defines TRUE as -1 (all bits set) and FALSE as 0 */
#define FORTH_TRUE  ((cell_t)-1)
#define FORTH_FALSE ((cell_t)0)

/* AND - Bitwise AND ( n1 n2 -- n3 ) */
static void logical_word_and(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "AND: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = n1 & n2;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "AND: %ld AND %ld = %ld", (long) n1, (long) n2, (long) result);
}

/* OR - Bitwise OR ( n1 n2 -- n3 ) */
static void logical_word_or(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "OR: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = n1 | n2;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "OR: %ld OR %ld = %ld", (long) n1, (long) n2, (long) result);
}

/* XOR - Bitwise XOR ( n1 n2 -- n3 ) */
static void logical_word_xor(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "XOR: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = n1 ^ n2;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "XOR: %ld XOR %ld = %ld", (long) n1, (long) n2, (long) result);
}

/* NOT - Bitwise NOT ( n1 -- n2 ) */
static void logical_word_not(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "NOT: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n1 = vm_pop(vm);
    cell_t result = ~n1;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "NOT: NOT %ld = %ld", (long) n1, (long) result);
}

/* 0= - Test for zero ( n -- flag ) */
static void logical_word_zero_equals(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "0=: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    cell_t result = (n == 0) ? FORTH_TRUE : FORTH_FALSE;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "0=: %ld = 0? %s", (long) n, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* 0< - Test for negative ( n -- flag ) */
static void logical_word_zero_less(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "0<: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    cell_t result = (n < 0) ? FORTH_TRUE : FORTH_FALSE;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "0<: %ld < 0? %s", (long) n, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* 0> - Test for positive ( n -- flag ) */
static void logical_word_zero_greater(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "0>: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    cell_t result = (n > 0) ? FORTH_TRUE : FORTH_FALSE;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "0>: %ld > 0? %s", (long) n, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* = - Test for equality ( n1 n2 -- flag ) */
static void logical_word_equals(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "=: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = (n1 == n2) ? FORTH_TRUE : FORTH_FALSE;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "=: %ld = %ld? %s", (long) n1, (long) n2, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* <> - Test for inequality ( n1 n2 -- flag ) */
static void logical_word_not_equals(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "<>: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = (n1 != n2) ? FORTH_TRUE : FORTH_FALSE;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "<>: %ld <> %ld? %s", (long) n1, (long) n2, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* < - Test for less than ( n1 n2 -- flag ) */
static void logical_word_less_than(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "<: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = (n1 < n2) ? FORTH_TRUE : FORTH_FALSE;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "<: %ld < %ld? %s", (long) n1, (long) n2, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* > - Test for greater than ( n1 n2 -- flag ) */
static void logical_word_greater_than(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, ">: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = (n1 > n2) ? FORTH_TRUE : FORTH_FALSE;

    vm_push(vm, result);

    log_message(LOG_DEBUG, ">: %ld > %ld? %s", (long) n1, (long) n2, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* U< - Unsigned less than ( u1 u2 -- flag ) */
static void logical_word_u_less_than(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "U<: Stack underflow");
        vm->error = 1;
        return;
    }

    uintptr_t u2 = (uintptr_t) vm_pop(vm);
    uintptr_t u1 = (uintptr_t) vm_pop(vm);
    cell_t result = (u1 < u2) ? FORTH_TRUE : FORTH_FALSE;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "U<: %lu U< %lu? %s", (unsigned long) u1, (unsigned long) u2,
                (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* U> - Unsigned greater than ( u1 u2 -- flag ) */
static void logical_word_u_greater_than(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "U>: Stack underflow");
        vm->error = 1;
        return;
    }

    uintptr_t u2 = (uintptr_t) vm_pop(vm);
    uintptr_t u1 = (uintptr_t) vm_pop(vm);
    cell_t result = (u1 > u2) ? FORTH_TRUE : FORTH_FALSE;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "U>: %lu U> %lu? %s", (unsigned long) u1, (unsigned long) u2,
                (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* WITHIN - Test if n is within bounds ( n low high -- flag ) */
static void logical_word_within(VM *vm) {
    if (vm->dsp < 2) {
        log_message(LOG_ERROR, "WITHIN: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t high = vm_pop(vm);
    cell_t low = vm_pop(vm);
    cell_t n = vm_pop(vm);
    cell_t result = (n >= low && n < high) ? FORTH_TRUE : FORTH_FALSE;

    vm_push(vm, result);

    log_message(LOG_DEBUG, "WITHIN: %ld <= %ld < %ld? %s", (long) low, (long) n, (long) high,
                (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* TRUE constant */
static void logical_word_true(VM *vm) {
    vm_push(vm, FORTH_TRUE);
    log_message(LOG_DEBUG, "TRUE: Pushed -1");
}

/* FALSE constant */
static void logical_word_false(VM *vm) {
    vm_push(vm, FORTH_FALSE);
    log_message(LOG_DEBUG, "FALSE: Pushed 0");
}

/* 0<> ( n -- f )  push TRUE (-1) if TOS != 0 else FALSE (0) */
void logical_word_zero_not_equal(VM *vm) {
    if (vm->dsp < 0) {
        /* underflow guard: need 1 item */
        vm->error = 1;
        log_message(LOG_ERROR, "0<>: stack underflow");
        return;
    }
    cell_t x = vm_pop(vm);
    vm_push(vm, x != 0 ? -1 : 0); /* Forth truth values: -1 true, 0 false */
}

/* Register all logical and comparison words */
void register_logical_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 logical and comparison words...");

    /* Bitwise operations */
    register_word(vm, "AND", logical_word_and);
    register_word(vm, "OR", logical_word_or);
    register_word(vm, "XOR", logical_word_xor);
    register_word(vm, "NOT", logical_word_not);

    /* Zero comparisons */
    register_word(vm, "0=", logical_word_zero_equals);
    register_word(vm, "0<", logical_word_zero_less);
    register_word(vm, "0>", logical_word_zero_greater);
    register_word(vm, "0<>", logical_word_zero_not_equal);

    /* Comparisons */
    register_word(vm, "=", logical_word_equals);
    register_word(vm, "<>", logical_word_not_equals);
    register_word(vm, "<", logical_word_less_than);
    register_word(vm, ">", logical_word_greater_than);

    /* Unsigned comparisons */
    register_word(vm, "U<", logical_word_u_less_than);
    register_word(vm, "U>", logical_word_u_greater_than);

    /* Range test */
    register_word(vm, "WITHIN", logical_word_within);

    /* Constants */
    register_word(vm, "TRUE", logical_word_true);
    register_word(vm, "FALSE", logical_word_false);

    log_message(LOG_INFO, "Logical and comparison words registered successfully");
}