/*

  * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
  *
  * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  * To the extent possible under law, the author(s) have dedicated all copyright and related
  * and neighboring rights to this software to the public domain worldwide.
  * This software is distributed without any warranty.
  *
  * See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

 */

/* logical_words.c - FORTH-79 Logical & Comparison Words */
#include "include/logical_words.h"
#include "../../include/log.h"
#include "../../include/word_registry.h"
#include <string.h>

/* FORTH-79 defines TRUE as -1 (all bits set) and FALSE as 0 */
#define FORTH_TRUE  ((cell_t)-1)
#define FORTH_FALSE ((cell_t)0)

/* AND - Bitwise AND ( n1 n2 -- n3 ) */
static void word_and(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "AND: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = n1 & n2;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "AND: %ld AND %ld = %ld", (long)n1, (long)n2, (long)result);
}

/* OR - Bitwise OR ( n1 n2 -- n3 ) */
static void word_or(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "OR: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = n1 | n2;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "OR: %ld OR %ld = %ld", (long)n1, (long)n2, (long)result);
}

/* XOR - Bitwise XOR ( n1 n2 -- n3 ) */
static void word_xor(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "XOR: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = n1 ^ n2;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "XOR: %ld XOR %ld = %ld", (long)n1, (long)n2, (long)result);
}

/* NOT - Bitwise NOT ( n1 -- n2 ) */
static void word_not(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "NOT: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n1 = vm_pop(vm);
    cell_t result = ~n1;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "NOT: NOT %ld = %ld", (long)n1, (long)result);
}

/* 0= - Test for zero ( n -- flag ) */
static void word_zero_equals(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "0=: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n = vm_pop(vm);
    cell_t result = (n == 0) ? FORTH_TRUE : FORTH_FALSE;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "0=: %ld = 0? %s", (long)n, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* 0< - Test for negative ( n -- flag ) */
static void word_zero_less(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "0<: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n = vm_pop(vm);
    cell_t result = (n < 0) ? FORTH_TRUE : FORTH_FALSE;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "0<: %ld < 0? %s", (long)n, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* 0> - Test for positive ( n -- flag ) */
static void word_zero_greater(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "0>: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n = vm_pop(vm);
    cell_t result = (n > 0) ? FORTH_TRUE : FORTH_FALSE;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "0>: %ld > 0? %s", (long)n, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* = - Test for equality ( n1 n2 -- flag ) */
static void word_equals(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "=: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = (n1 == n2) ? FORTH_TRUE : FORTH_FALSE;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "=: %ld = %ld? %s", (long)n1, (long)n2, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* <> - Test for inequality ( n1 n2 -- flag ) */
static void word_not_equals(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "<>: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = (n1 != n2) ? FORTH_TRUE : FORTH_FALSE;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "<>: %ld <> %ld? %s", (long)n1, (long)n2, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* < - Test for less than ( n1 n2 -- flag ) */
static void word_less_than(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "<: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = (n1 < n2) ? FORTH_TRUE : FORTH_FALSE;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "<: %ld < %ld? %s", (long)n1, (long)n2, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* > - Test for greater than ( n1 n2 -- flag ) */
static void word_greater_than(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, ">: Stack underflow");
        vm->error = 1;
        return;
    }
    
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    cell_t result = (n1 > n2) ? FORTH_TRUE : FORTH_FALSE;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, ">: %ld > %ld? %s", (long)n1, (long)n2, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* U< - Unsigned less than ( u1 u2 -- flag ) */
static void word_u_less_than(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "U<: Stack underflow");
        vm->error = 1;
        return;
    }
    
    uintptr_t u2 = (uintptr_t)vm_pop(vm);
    uintptr_t u1 = (uintptr_t)vm_pop(vm);
    cell_t result = (u1 < u2) ? FORTH_TRUE : FORTH_FALSE;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "U<: %lu U< %lu? %s", (unsigned long)u1, (unsigned long)u2, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* U> - Unsigned greater than ( u1 u2 -- flag ) */
static void word_u_greater_than(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "U>: Stack underflow");
        vm->error = 1;
        return;
    }
    
    uintptr_t u2 = (uintptr_t)vm_pop(vm);
    uintptr_t u1 = (uintptr_t)vm_pop(vm);
    cell_t result = (u1 > u2) ? FORTH_TRUE : FORTH_FALSE;
    
    vm_push(vm, result);
    
    log_message(LOG_DEBUG, "U>: %lu U> %lu? %s", (unsigned long)u1, (unsigned long)u2, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* WITHIN - Test if n is within bounds ( n low high -- flag ) */
static void word_within(VM *vm) {
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
    
    log_message(LOG_DEBUG, "WITHIN: %ld <= %ld < %ld? %s", (long)low, (long)n, (long)high, (result == FORTH_TRUE) ? "TRUE" : "FALSE");
}

/* TRUE constant */
static void word_true(VM *vm) {
    vm_push(vm, FORTH_TRUE);
    log_message(LOG_DEBUG, "TRUE: Pushed -1");
}

/* FALSE constant */
static void word_false(VM *vm) {
    vm_push(vm, FORTH_FALSE);
    log_message(LOG_DEBUG, "FALSE: Pushed 0");
}

/* Register all logical and comparison words */
void register_logical_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 logical and comparison words...");
    
    /* Bitwise operations */
    if (!vm_create_word(vm, "AND", 3, word_and)) {
        log_message(LOG_ERROR, "Failed to register AND");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: AND");
    
    if (!vm_create_word(vm, "OR", 2, word_or)) {
        log_message(LOG_ERROR, "Failed to register OR");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: OR");
    
    if (!vm_create_word(vm, "XOR", 3, word_xor)) {
        log_message(LOG_ERROR, "Failed to register XOR");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: XOR");
    
    if (!vm_create_word(vm, "NOT", 3, word_not)) {
        log_message(LOG_ERROR, "Failed to register NOT");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: NOT");
    
    /* Zero comparisons */
    if (!vm_create_word(vm, "0=", 2, word_zero_equals)) {
        log_message(LOG_ERROR, "Failed to register 0=");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: 0=");
    
    if (!vm_create_word(vm, "0<", 2, word_zero_less)) {
        log_message(LOG_ERROR, "Failed to register 0<");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: 0<");
    
    if (!vm_create_word(vm, "0>", 2, word_zero_greater)) {
        log_message(LOG_ERROR, "Failed to register 0>");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: 0>");
    
    /* Comparisons */
    if (!vm_create_word(vm, "=", 1, word_equals)) {
        log_message(LOG_ERROR, "Failed to register =");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: =");
    
    if (!vm_create_word(vm, "<>", 2, word_not_equals)) {
        log_message(LOG_ERROR, "Failed to register <>");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: <>");
    
    if (!vm_create_word(vm, "<", 1, word_less_than)) {
        log_message(LOG_ERROR, "Failed to register <");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: <");
    
    if (!vm_create_word(vm, ">", 1, word_greater_than)) {
        log_message(LOG_ERROR, "Failed to register >");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: >");
    
    /* Unsigned comparisons */
    if (!vm_create_word(vm, "U<", 2, word_u_less_than)) {
        log_message(LOG_ERROR, "Failed to register U<");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: U<");
    
    if (!vm_create_word(vm, "U>", 2, word_u_greater_than)) {
        log_message(LOG_ERROR, "Failed to register U>");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: U>");
    
    /* Range test */
    if (!vm_create_word(vm, "WITHIN", 6, word_within)) {
        log_message(LOG_ERROR, "Failed to register WITHIN");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: WITHIN");
    
    /* Constants */
    if (!vm_create_word(vm, "TRUE", 4, word_true)) {
        log_message(LOG_ERROR, "Failed to register TRUE");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: TRUE");
    
    if (!vm_create_word(vm, "FALSE", 5, word_false)) {
        log_message(LOG_ERROR, "Failed to register FALSE");
        return;
    }
    log_message(LOG_DEBUG, "Registered word: FALSE");
    
    log_message(LOG_INFO, "Logical and comparison words registered successfully");
}