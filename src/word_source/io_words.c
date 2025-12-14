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

/* io_words.c - FORTH-79 I/O & Terminal Words */
#include "include/io_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <stdio.h>


/**
 * @brief FORTH word EMIT - Output character to terminal
 * @param vm Pointer to the VM structure
 * @details Stack effect: ( c -- )
 *          Outputs the character from top of stack to the terminal
 */
static void io_word_emit(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    char c = (char) vm->data_stack[vm->dsp--];
    putchar(c);
    fflush(stdout);
}

/**
 * @brief FORTH word CR - Output carriage return
 * @param vm Pointer to the VM structure
 * @details Stack effect: ( -- )
 *          Outputs a newline character to the terminal
 */
static void io_word_cr(VM *vm) {
    putchar('\n');
    fflush(stdout);
}

/**
 * @brief FORTH word KEY - Input character from terminal
 * @param vm Pointer to the VM structure
 * @details Stack effect: ( -- c )
 *          Reads one character from terminal and pushes it to stack
 */
static void io_word_key(VM *vm) {
    if (vm->dsp >= STACK_SIZE - 1) {
        vm->error = 1;
        return;
    }

    int c = getchar();
    vm->data_stack[++vm->dsp] = (cell_t) c;
}

/**
 * @brief FORTH word ?TERMINAL - Check if input is available
 * @param vm Pointer to the VM structure
 * @details Stack effect: ( -- flag )
 *          Pushes true if input is available, false otherwise
 */
static void io_word_question_terminal(VM *vm) {
    if (vm->dsp >= STACK_SIZE - 1) {
        vm->error = 1;
        return;
    }

    /* Simple implementation - always return false for now */
    vm->data_stack[++vm->dsp] = 0;
}

/**
 * @brief FORTH word TYPE - Output string of characters
 * @param vm Pointer to the VM structure
 * @details Stack effect: ( addr u -- )
 *          Outputs u characters from memory starting at addr
 */
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
        log_message(LOG_ERROR, "TYPE: Invalid range [%ld, %ld)", (long) addr, (long) (addr + count));
        vm->error = 1;
        return;
    }

    // Output characters from VM memory
    for (cell_t i = 0; i < count; i++) {
        putchar(vm->memory[addr + i]);
    }
    fflush(stdout);

    log_message(LOG_DEBUG, "TYPE: Output %ld characters from address %ld", (long) count, (long) addr);
}

/**
 * @brief FORTH word SPACE - Output one space character
 * @param vm Pointer to the VM structure
 * @details Stack effect: ( -- )
 *          Outputs a single space character to terminal
 */
static void io_word_space(VM *vm) {
    putchar(' ');
    fflush(stdout);
}

/**
 * @brief FORTH word SPACES - Output multiple spaces
 * @param vm Pointer to the VM structure
 * @details Stack effect: ( n -- )
 *          Outputs n space characters to terminal
 */
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

/**
 * @brief Register all I/O words with the VM
 * @param vm Pointer to the VM structure
 * @details Registers all FORTH-79 I/O and terminal words with the virtual machine
 */
void register_io_words(VM *vm) {
    register_word(vm, "EMIT", io_word_emit);
    register_word(vm, "CR", io_word_cr);
    register_word(vm, "KEY", io_word_key);
    register_word(vm, "?TERMINAL", io_word_question_terminal);
    register_word(vm, "TYPE", io_word_type);
    register_word(vm, "SPACE", io_word_space);
    register_word(vm, "SPACES", io_word_spaces);
}