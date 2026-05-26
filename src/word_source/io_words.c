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

/* (do-string) — runtime called by compiled ." ; reads inline string from
 * threaded code via the return-stack IP, prints it, then advances IP past
 * the padded inline block.  NOT immediate.
 *
 * Inline layout immediately after the (do-string) cell in the thread:
 *   [1 length byte][n string bytes][padding to next cell boundary]
 */
static void io_runtime_do_string(VM *vm) {
    if (vm->rsp < 0) { vm->error = 1; return; }
    /* return_stack[rsp] holds ip+1 as a raw C pointer — points at inline data */
    uint8_t *data = (uint8_t *)(uintptr_t)vm->return_stack[vm->rsp];
    if (!data) { vm->error = 1; return; }
    uint8_t n = data[0];
    for (uint8_t i = 0; i < n; i++)
        putchar((unsigned char)data[1 + i]);
    fflush(stdout);
    /* advance IP past inline block (length byte + string bytes, cell-aligned) */
    size_t skip = 1 + (size_t)n;
    size_t padded = (skip + (sizeof(cell_t) - 1)) & ~(sizeof(cell_t) - 1);
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)(data + padded);
}

/* ." ( "ccc<quote>" -- )
 * Immediate.  Interpretation: parse and print the string directly.
 * Compilation: compile (do-string) + inline [len][chars][pad].
 */
static void io_word_dot_quote(VM *vm) {
    const char *src = vm->input_buffer;
    size_t pos = vm->input_pos;
    size_t end = vm->input_length;

    if (!src || pos > end) {
        vm->error = 1;
        return;
    }
    if (pos < end && src[pos] == ' ') pos++;

    size_t start = pos;
    while (pos < end && src[pos] != '"') pos++;
    if (pos >= end) { vm->error = 1; return; }

    size_t n = pos - start;
    if (n > 255) n = 255; /* single length byte — clamp silently */
    vm->input_pos = pos + 1;

    if (vm->mode == MODE_INTERPRET) {
        for (size_t i = 0; i < n; i++)
            putchar((unsigned char)src[start + i]);
        fflush(stdout);
        return;
    }

    /* Compilation: emit (do-string) cell then inline [len][chars][pad] */
    DictEntry *do_str = vm_find_word(vm, "(do-string)", 11);
    if (!do_str) { vm->error = 1; return; }
    vm_compile_word(vm, do_str);

    size_t skip = 1 + n;
    size_t padded = (skip + (sizeof(cell_t) - 1)) & ~(sizeof(cell_t) - 1);
    uint8_t *raw = (uint8_t *)vm_allot(vm, padded);
    if (!raw) { vm->error = 1; return; }
    raw[0] = (uint8_t)n;
    for (size_t i = 0; i < n; i++) raw[1 + i] = (uint8_t)src[start + i];
    for (size_t i = 1 + n; i < padded; i++) raw[i] = 0;
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
    register_word(vm, "(do-string)", io_runtime_do_string);
    register_word(vm, ".\"", io_word_dot_quote);
    vm_make_immediate(vm);
}