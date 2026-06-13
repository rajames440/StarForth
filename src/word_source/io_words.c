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
#include <string.h>


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

/* ." ( "ccc<quote>" -- )   IMMEDIATE
 *
 * Interpretation: parse string until closing " and print it immediately.
 * Compilation:   compile (s") + inline [len][chars][pad], then compile TYPE.
 *
 * This is FORTH-79 required.  It was missing from the original word set.
 */
static void io_word_dot_quote(VM *vm)
{
    const char *src = vm->input_buffer;
    size_t pos = vm->input_pos;
    size_t end = vm->input_length;

    if (!src || pos > end) {
        vm->error = 1;
        log_message(LOG_ERROR, ".\": input buffer not ready");
        return;
    }

    /* Skip single leading space after ." */
    if (pos < end && src[pos] == ' ') pos++;

    size_t start = pos;
    while (pos < end && src[pos] != '"') pos++;

    if (pos >= end) {
        vm->error = 1;
        log_message(LOG_ERROR, ".\": missing closing quote");
        return;
    }

    size_t n = pos - start;
    if (n > 255) {
        vm->error = 1;
        log_message(LOG_ERROR, ".\": string too long (%zu bytes)", n);
        return;
    }

    vm->input_pos = pos + 1; /* consume closing quote */

    if (vm->mode == MODE_COMPILE) {
        /* Compile (s") + inline string block */
        DictEntry *s_rt = vm_find_word(vm, "(s\")", 4);
        if (!s_rt) {
            vm->error = 1;
            log_message(LOG_ERROR, ".\": (s\") not found");
            return;
        }
        vm_compile_word(vm, s_rt);

        size_t padded = (1 + n + (sizeof(cell_t) - 1)) & ~(sizeof(cell_t) - 1);
        uint8_t *raw = (uint8_t *)vm_allot(vm, padded);
        if (!raw) { vm->error = 1; return; }
        raw[0] = (uint8_t)n;
        for (size_t i = 0; i < n; i++) raw[1 + i] = (uint8_t)src[start + i];
        for (size_t i = 1 + n; i < padded; i++) raw[i] = 0;

        /* Compile TYPE to print the string at run-time */
        DictEntry *type_w = vm_find_word(vm, "TYPE", 4);
        if (!type_w) {
            vm->error = 1;
            log_message(LOG_ERROR, ".\": TYPE not found");
            return;
        }
        vm_compile_word(vm, type_w);
        return;
    }

    /* Interpret mode: print immediately */
    fwrite(src + start, 1, n, stdout);
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

    /* ." is FORTH-79 required; IMMEDIATE so it parses at compile time */
    register_word(vm, ".\"", io_word_dot_quote);
    vm_make_immediate(vm);
}