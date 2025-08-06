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

/* io_words.c - FORTH-79 I/O & Terminal Words */
#include "include/io_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* EMIT ( c -- ) - Output character to terminal */
static void io_word_emit(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    char c = (char)vm->data_stack[vm->dsp--];
    putchar(c);
    fflush(stdout);
}

/* CR ( -- ) - Output carriage return */
static void io_word_cr(VM *vm) {
    putchar('\n');
    fflush(stdout);
}

/* KEY ( -- c ) - Input character from terminal */
static void io_word_key(VM *vm) {
    if (vm->dsp >= STACK_SIZE - 1) {
        vm->error = 1;
        return;
    }
    
    int c = getchar();
    vm->data_stack[++vm->dsp] = (cell_t)c;
}

/* ?TERMINAL ( -- flag ) - Check if input available */
static void io_word_question_terminal(VM *vm) {
    if (vm->dsp >= STACK_SIZE - 1) {
        vm->error = 1;
        return;
    }
    
    /* Simple implementation - always return false for now */
    vm->data_stack[++vm->dsp] = 0;
}

/* TYPE ( addr u -- ) - Output u characters from addr */
static void io_word_type(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "TYPE: Data stack underflow");
        vm->error = 1;
        return;
    }

    cell_t count = vm_pop(vm);
    cell_t addr = vm_pop(vm);

    // Add bounds checking
    if (addr < 0 || count < 0 || (addr + count) > VM_MEMORY_SIZE) {
        log_message(LOG_ERROR, "TYPE: Invalid range [%ld, %ld)", (long)addr, (long)(addr + count));
        vm->error = 1;
        return;
    }

    // Output characters from VM memory
    for (cell_t i = 0; i < count; i++) {
        putchar(vm->memory[addr + i]);
    }
    fflush(stdout);

    log_message(LOG_DEBUG, "TYPE: Output %ld characters from address %ld", (long)count, (long)addr);
}

/* SPACE ( -- ) - Output one space */
static void io_word_space(VM *vm) {
    putchar(' ');
    fflush(stdout);
}

/* SPACES ( n -- ) - Output n spaces */
static void io_word_spaces(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    cell_t count = vm->data_stack[vm->dsp--];
    
    if (count < 0) return;
    
    for (cell_t i = 0; i < count; i++) {
        putchar(' ');
    }
    fflush(stdout);
}

/* Register all I/O words with the VM */
void register_io_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 I/O & terminal words...");
    
    register_word(vm, "EMIT", io_word_emit);
    register_word(vm, "CR", io_word_cr);
    register_word(vm, "KEY", io_word_key);
    register_word(vm, "?TERMINAL", io_word_question_terminal);
    register_word(vm, "TYPE", io_word_type);
    register_word(vm, "SPACE", io_word_space);
    register_word(vm, "SPACES", io_word_spaces);
    
    log_message(LOG_INFO, "I/O & terminal words registered successfully");
}