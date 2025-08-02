/* core_word------s.c */
#include "core_words.h"
#include "log.h"
#include <stdio.h>
#include <stdint.h>

/* Simple memory pool for safe testing */
#define MEM_POOL_SIZE 1024
static cell_t memory_pool[MEM_POOL_SIZE];

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

/* Helper: Check if address is in safe memory range */
static int is_safe_address(cell_t addr) {
    /* Allow addresses in our memory pool range */
    uintptr_t pool_start = (uintptr_t)memory_pool;
    uintptr_t pool_end = pool_start + sizeof(memory_pool);
    uintptr_t address = (uintptr_t)addr;
    
    return (address >= pool_start && address < pool_end && 
            (address - pool_start) % sizeof(cell_t) == 0);
}

/* Core Word Implementations */

void word_dot(VM *vm) {
    cell_t val;
    if (!pop1(vm, &val)) return;
    printf("%d ", (int)val);
}

void word_drop(VM *vm) {
    cell_t dummy;
    (void)pop1(vm, &dummy);
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
    push1(vm, vm->data_stack[vm->data_sp - 2]);
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
    push1(vm, (cell_t)vm->data_sp);
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
    
    if (!is_safe_address(addr)) {
        log_message(LOG_ERROR, "Invalid memory address: %d", (int)addr);
        vm->error = 1;
        return;
    }
    
    *((cell_t *)(uintptr_t)addr) = val;
}

void word_fetch(VM *vm) {  /* '@' */
    cell_t addr;
    if (!pop1(vm, &addr)) return;
    
    if (!is_safe_address(addr)) {
        log_message(LOG_ERROR, "Invalid memory address: %d", (int)addr);
        vm->error = 1;
        return;
    }
    
    cell_t val = *((cell_t *)(uintptr_t)addr);
    push1(vm, val);
}

void word_plus_store(VM *vm) {  /* '+!' */
    cell_t val, addr, prev;
    if (!pop2(vm, &addr, &val)) return;
    
    if (!is_safe_address(addr)) {
        log_message(LOG_ERROR, "Invalid memory address: %d", (int)addr);
        vm->error = 1;
        return;
    }
    
    prev = *((cell_t *)(uintptr_t)addr);
    *((cell_t *)(uintptr_t)addr) = prev + val;
}

/* Add a word to get a safe memory address */
void word_here(VM *vm) {
    /* Return address of first memory pool location */
    push1(vm, (cell_t)(uintptr_t)memory_pool);
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

/* These compile-time words should never execute at runtime */
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
    /* no operation */
}

/* Forward declaration for register_word function */
extern void register_word(VM *vm, const char *name, word_func_t func);

/* Registration */
void register_core_words(VM *vm) {
    register_word(vm, ".", word_dot);
    register_word(vm, "DROP", word_drop);
    register_word(vm, "DUP", word_dup);
    register_word(vm, "SWAP", word_swap);
    register_word(vm, "OVER", word_over);
    register_word(vm, "ROT", word_rot);
    register_word(vm, "-ROT", word_minus_rot);
    register_word(vm, "DEPTH", word_depth);
    register_word(vm, "TUCK", word_tuck);
    register_word(vm, "!", word_store);
    register_word(vm, "@", word_fetch);
    register_word(vm, "+!", word_plus_store);
    register_word(vm, "HERE", word_here);  /* Safe memory address */
    register_word(vm, "EMIT", word_emit);
    register_word(vm, "CR", word_cr);
    register_word(vm, "KEY", word_key);
    register_word(vm, "LIT", word_lit);
    register_word(vm, "BRANCH", word_branch);
    register_word(vm, "0BRANCH", word_zero_branch);
    register_word(vm, ";", word_semicolon);
    register_word(vm, ":", word_colon);
    register_word(vm, "IMMEDIATE", word_immediate);
    register_word(vm, "EXIT", word_exit);
    register_word(vm, "NOP", word_nop);
}

void core_register_words(VM *vm, void (*register_word_func)(VM *, const char *, word_func_t)) {
    register_word_func(vm, ".", word_dot);
    register_word_func(vm, "DROP", word_drop);
    register_word_func(vm, "DUP", word_dup);
    register_word_func(vm, "SWAP", word_swap);
    register_word_func(vm, "OVER", word_over);
    register_word_func(vm, "ROT", word_rot);
    register_word_func(vm, "-ROT", word_minus_rot);
    register_word_func(vm, "DEPTH", word_depth);
    register_word_func(vm, "TUCK", word_tuck);
    register_word_func(vm, "!", word_store);
    register_word_func(vm, "@", word_fetch);
    register_word_func(vm, "+!", word_plus_store);
    register_word_func(vm, "HERE", word_here);
    register_word_func(vm, "EMIT", word_emit);
    register_word_func(vm, "CR", word_cr);
    register_word_func(vm, "KEY", word_key);
    register_word_func(vm, "LIT", word_lit);
    register_word_func(vm, "BRANCH", word_branch);
    register_word_func(vm, "0BRANCH", word_zero_branch);
    register_word_func(vm, ";", word_semicolon);
    register_word_func(vm, ":", word_colon);
    register_word_func(vm, "IMMEDIATE", word_immediate);
    register_word_func(vm, "EXIT", word_exit);
    register_word_func(vm, "NOP", word_nop);
}