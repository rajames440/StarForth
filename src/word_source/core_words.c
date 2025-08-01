// src/word_source/core_words.c
#include "core_words.h"
#include "vm.h"
#include "log.h"
#include <stdio.h>

/* Helper stack operations */
static int pop1(VM *vm, cell_t *val) {
    if (vm->data_sp <= 0) {
        log_message(LOG_ERROR, "Stack underflow");
        vm->error = 1;
        return 0;
    }
    *val = vm->data_stack[--vm->data_sp];
    return 1;
}

static int pop2(VM *vm, cell_t *a, cell_t *b) {
    if (vm->data_sp < 2) {
        log_message(LOG_ERROR, "Stack underflow");
        vm->error = 1;
        return 0;
    }
    *b = vm->data_stack[--vm->data_sp];
    *a = vm->data_stack[--vm->data_sp];
    return 1;
}

static void push1(VM *vm, cell_t val) {
    if (vm->data_sp >= 1024) {
        log_message(LOG_ERROR, "Stack overflow");
        vm->error = 1;
        return;
    }
    vm->data_stack[vm->data_sp++] = val;
    vm->error = 0;
}

/* Word implementations */

void word_dot(VM *vm) {
    cell_t val;
    if (!pop1(vm, &val)) return;
    printf("%d\n", (int)val);
}

void word_drop(VM *vm) {
    cell_t dummy;
    if (!pop1(vm, &dummy)) return;
}

void word_dup(VM *vm) {
    cell_t val;
    if (!pop1(vm, &val)) return;
    push1(vm, val);
    push1(vm, val);
}

void word_swap(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, b);
    push1(vm, a);
}

void word_over(VM *vm) {
    if (vm->data_sp < 2) {
        log_message(LOG_ERROR, "Stack underflow");
        vm->error = 1;
        return;
    }
    cell_t val = vm->data_stack[vm->data_sp - 2];
    push1(vm, val);
}

void word_rot(VM *vm) {
    if (vm->data_sp < 3) {
        log_message(LOG_ERROR, "Stack underflow");
        vm->error = 1;
        return;
    }
    cell_t c = vm->data_stack[vm->data_sp - 3];
    vm->data_stack[vm->data_sp - 3] = vm->data_stack[vm->data_sp - 2];
    vm->data_stack[vm->data_sp - 2] = vm->data_stack[vm->data_sp - 1];
    vm->data_stack[vm->data_sp - 1] = c;
}

void word_minus_rot(VM *vm) {
    if (vm->data_sp < 3) {
        log_message(LOG_ERROR, "Stack underflow");
        vm->error = 1;
        return;
    }
    cell_t c = vm->data_stack[vm->data_sp - 1];
    vm->data_stack[vm->data_sp - 1] = vm->data_stack[vm->data_sp - 2];
    vm->data_stack[vm->data_sp - 2] = vm->data_stack[vm->data_sp - 3];
    vm->data_stack[vm->data_sp - 3] = c;
}

void word_depth(VM *vm) {
    push1(vm, vm->data_sp);
}

void word_emit(VM *vm) {
    cell_t val;
    if (!pop1(vm, &val)) return;
    putchar((int)val);
}

void word_cr(VM *vm) {
    putchar('\n');
}

void word_key(VM *vm) {
    int c = getchar();
    if (c == EOF) c = 0;
    push1(vm, (cell_t)c);
}

void word_tuck(VM *vm) {
    if (vm->data_sp < 2) {
        log_message(LOG_ERROR, "Stack underflow");
        vm->error = 1;
        return;
    }
    cell_t a = vm->data_stack[vm->data_sp - 1];
    cell_t b = vm->data_stack[vm->data_sp - 2];
    push1(vm, a);
    vm->data_stack[vm->data_sp - 2] = a;
    vm->data_stack[vm->data_sp - 3] = b;
}

void word_store(VM *vm) {  /* '!' */
    cell_t val, addr;
    if (!pop2(vm, &addr, &val)) return;
    /* For safety, cast addr to pointer */
    *((cell_t *) (uintptr_t)addr) = val;
}

void word_fetch(VM *vm) {  /* '@' */
    cell_t addr;
    if (!pop1(vm, &addr)) return;
    cell_t val = *((cell_t *) (uintptr_t)addr);
    push1(vm, val);
}

void word_plus_store(VM *vm) {  /* '+!' */
    cell_t val, addr, prev;
    if (!pop2(vm, &addr, &val)) return;
    prev = *((cell_t *) (uintptr_t)addr);
    *((cell_t *) (uintptr_t)addr) = prev + val;
}

void word_lit(VM *vm) {
    log_message(LOG_ERROR, "word_lit should never be executed at runtime");
    vm->error = 1;
}

void word_branch(VM *vm) {
    log_message(LOG_ERROR, "word_branch should be handled at compile time");
    vm->error = 1;
}

void word_zero_branch(VM *vm) {
    log_message(LOG_ERROR, "word_zero_branch should be handled at compile time");
    vm->error = 1;
}

void word_semicolon(VM *vm) {
    log_message(LOG_ERROR, "word_semicolon should be handled at compile time");
    vm->error = 1;
}

void word_colon(VM *vm) {
    log_message(LOG_ERROR, "word_colon should be handled at compile time");
    vm->error = 1;
}

void word_immediate(VM *vm) {
    log_message(LOG_ERROR, "word_immediate should be handled at compile time");
    vm->error = 1;
}

void word_exit(VM *vm) {
    log_message(LOG_ERROR, "word_exit should be handled at compile time");
    vm->error = 1;
}

void word_nop(VM *vm) {
    /* Does nothing */
}

/* Registration function */
void register_core_words(VM *vm) {
    register_word(vm, ".", word_dot);
    register_word(vm, "DROP", word_drop);
    register_word(vm, "DUP", word_dup);
    register_word(vm, "SWAP", word_swap);
    register_word(vm, "OVER", word_over);
    register_word(vm, "ROT", word_rot);
    register_word(vm, "-ROT", word_minus_rot);
    register_word(vm, "DEPTH", word_depth);
    register_word(vm, "EMIT", word_emit);
    register_word(vm, "CR", word_cr);
    register_word(vm, "KEY", word_key);
    register_word(vm, "TUCK", word_tuck);
    register_word(vm, "!", word_store);
    register_word(vm, "@", word_fetch);
    register_word(vm, "+!", word_plus_store);
    register_word(vm, "LIT", word_lit);
    register_word(vm, "BRANCH", word_branch);
    register_word(vm, "0BRANCH", word_zero_branch);
    register_word(vm, ";", word_semicolon);
    register_word(vm, ":", word_colon);
    register_word(vm, "IMMEDIATE", word_immediate);
    register_word(vm, "EXIT", word_exit);
    register_word(vm, "NOP", word_nop);
}

