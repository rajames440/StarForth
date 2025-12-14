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

/* logical_words.c - FORTH-79 Logical & Comparison Words */
#include "include/logical_words.h"
#include "../../include/log.h"
#include "../../include/word_registry.h"


/** @brief FORTH-79 TRUE value (-1, all bits set) */
#define FORTH_TRUE  ((cell_t)-1)
/** @brief FORTH-79 FALSE value (0) */
#define FORTH_FALSE ((cell_t)0)

/**
 * @brief FORTH word: AND ( n1 n2 -- n3 )
 * @param vm Pointer to the virtual machine instance
 *
 * Performs bitwise AND operation on top two stack values.
 * Stack effect: ( n1 n2 -- n3 ) where n3 = n1 AND n2
 */
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

/**
 * @brief Registers all FORTH-79 logical and comparison words with the VM
 * @param vm Pointer to the virtual machine instance
 *
 * Registers logical operations (AND, OR, XOR, NOT),
 * comparison operations (=, <>, <, >, etc.),
 * and constant words (TRUE, FALSE) with the virtual machine.
 */
void register_logical_words(VM *vm) {
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
}