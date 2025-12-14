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

/* stack_words.c - FORTH-79 Stack Operation Words - ANSI C99 ONLY */

/* FORTH-79 Stack Operation Words to implement:
 * DROP      ( n -- )                    Remove top stack item
 * DUP       ( n -- n n )                Duplicate top stack item
 * ?DUP      ( n -- n n | n -- 0 )       Duplicate if non-zero
 * SWAP      ( n1 n2 -- n2 n1 )          Exchange top two stack items
 * OVER      ( n1 n2 -- n1 n2 n1 )       Copy second stack item to top
 * ROT       ( n1 n2 n3 -- n2 n3 n1 )    Rotate top three stack items
 * -ROT      ( n1 n2 n3 -- n3 n1 n2 )    Reverse rotate top three items
 * DEPTH     ( -- n )                    Return number of stack items
 * PICK      ( n -- stack[n] )           Copy nth stack item to top
 * ROLL      ( n -- )                    Move nth stack item to top
 */

#include "include/stack_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/**
 * @brief Implements DROP ( n -- ) - Removes top stack item
 * @param vm Pointer to the virtual machine state
 * @details Removes the top item from the data stack. Generates an error on stack underflow.
 */
static void stack_word_drop(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "DROP: Stack underflow");
        vm->error = 1;
        return;
    }
    vm->dsp--;
    log_message(LOG_DEBUG, "DROP: Stack depth now %d", vm->dsp + 1);
}

/**
 * @brief Implements DUP ( n -- n n ) - Duplicates top stack item
 * @param vm Pointer to the virtual machine state
 * @details Duplicates the top item on the data stack. Generates an error on stack underflow/overflow.
 */
static void stack_word_dup(VM *vm) {
#ifdef STARFORTH_PERFORMANCE
    /* Fast path - skip bounds checking in performance builds */
    if (UNLIKELY(vm->dsp < 0 || vm->dsp >= STACK_SIZE - 1)) {
        vm->error = 1;
        return;
    }
    cell_t value = vm->data_stack[vm->dsp];
    vm->data_stack[++vm->dsp] = value;
#else
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "DUP: Stack underflow");
        vm->error = 1;
        return;
    }

    if (vm->dsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "DUP: Stack overflow");
        vm->error = 1;
        return;
    }

    cell_t value = vm->data_stack[vm->dsp];
    vm->data_stack[++vm->dsp] = value;
    log_message(LOG_DEBUG, "DUP: Duplicated value");
#endif
}

/**
 * @brief Implements ?DUP ( n -- n n | n -- 0 ) - Conditionally duplicates top stack item
 * @param vm Pointer to the virtual machine state
 * @details Duplicates top stack item if it's non-zero, otherwise leaves it unchanged.
 */
static void stack_word_question_dup(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "?DUP: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t value = vm->data_stack[vm->dsp];
    if (value != 0) {
        if (vm->dsp >= STACK_SIZE - 1) {
            log_message(LOG_ERROR, "?DUP: Stack overflow");
            vm->error = 1;
            return;
        }
        vm->data_stack[++vm->dsp] = value;
        log_message(LOG_DEBUG, "?DUP: Duplicated non-zero value");
    } else {
        log_message(LOG_DEBUG, "?DUP: Left zero value unchanged");
    }
}

/**
 * @brief Implements SWAP ( n1 n2 -- n2 n1 ) - Exchanges top two stack items
 * @param vm Pointer to the virtual machine state
 * @details Exchanges the positions of the top two items on the data stack.
 */
static void stack_word_swap(VM *vm) {
#ifdef STARFORTH_PERFORMANCE
    /* Fast path - minimal checking in performance builds */
    if (UNLIKELY(vm->dsp < 1)) {
        vm->error = 1;
        return;
    }

    cell_t top = vm->data_stack[vm->dsp];
    cell_t second = vm->data_stack[vm->dsp - 1];
    vm->data_stack[vm->dsp] = second;
    vm->data_stack[vm->dsp - 1] = top;
#else
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "SWAP: Insufficient stack items (need 2)");
        vm->error = 1;
        return;
    }

    cell_t top = vm->data_stack[vm->dsp];
    cell_t second = vm->data_stack[vm->dsp - 1];

    vm->data_stack[vm->dsp] = second;
    vm->data_stack[vm->dsp - 1] = top;

    log_message(LOG_DEBUG, "SWAP: Exchanged top two values");
#endif
}

/**
 * @brief Implements OVER ( n1 n2 -- n1 n2 n1 ) - Copies second stack item to top
 * @param vm Pointer to the virtual machine state
 * @details Copies the second item from the top of the stack to the top.
 */
static void stack_word_over(VM *vm) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "OVER: Insufficient stack items (need 2)");
        vm->error = 1;
        return;
    }

    if (vm->dsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "OVER: Stack overflow");
        vm->error = 1;
        return;
    }

    cell_t second = vm->data_stack[vm->dsp - 1];
    vm->data_stack[++vm->dsp] = second;

    log_message(LOG_DEBUG, "OVER: Copied second to top");
}

/**
 * @brief Implements ROT ( n1 n2 n3 -- n2 n3 n1 ) - Rotates top three stack items
 * @param vm Pointer to the virtual machine state
 * @details Rotates the top three items on the stack, moving the third item to the top.
 */
static void stack_word_rot(VM *vm) {
    if (vm->dsp < 2) {
        log_message(LOG_ERROR, "ROT: Insufficient stack items (need 3)");
        vm->error = 1;
        return;
    }

    cell_t n3 = vm->data_stack[vm->dsp]; /* top */
    cell_t n2 = vm->data_stack[vm->dsp - 1]; /* second */
    cell_t n1 = vm->data_stack[vm->dsp - 2]; /* third */

    /* n1 n2 n3 -> n2 n3 n1 */
    vm->data_stack[vm->dsp] = n1;
    vm->data_stack[vm->dsp - 1] = n3;
    vm->data_stack[vm->dsp - 2] = n2;

    log_message(LOG_DEBUG, "ROT: Rotated top three items");
}

/**
 * @brief Implements -ROT ( n1 n2 n3 -- n3 n1 n2 ) - Reverse rotates top three items
 * @param vm Pointer to the virtual machine state
 * @details Performs the reverse rotation of the top three stack items.
 */
static void stack_word_minus_rot(VM *vm) {
    if (vm->dsp < 2) {
        log_message(LOG_ERROR, "-ROT: Insufficient stack items (need 3)");
        vm->error = 1;
        return;
    }

    cell_t n3 = vm->data_stack[vm->dsp]; /* top */
    cell_t n2 = vm->data_stack[vm->dsp - 1]; /* second */
    cell_t n1 = vm->data_stack[vm->dsp - 2]; /* third */

    /* n1 n2 n3 -> n3 n1 n2 */
    vm->data_stack[vm->dsp] = n2;
    vm->data_stack[vm->dsp - 1] = n1;
    vm->data_stack[vm->dsp - 2] = n3;

    log_message(LOG_DEBUG, "-ROT: Reverse rotated top three items");
}

/**
 * @brief Implements DEPTH ( -- n ) - Returns number of stack items
 * @param vm Pointer to the virtual machine state
 * @details Pushes the current number of items on the data stack onto the stack.
 */
static void stack_word_depth(VM *vm) {
    if (vm->dsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "DEPTH: Stack overflow");
        vm->error = 1;
        return;
    }

    cell_t depth = vm->dsp + 1;
    vm->data_stack[++vm->dsp] = depth;

    log_message(LOG_DEBUG, "DEPTH: Stack depth returned");
}

/**
 * @brief Implements PICK ( n -- stack[n] ) - Copies nth stack item to top
 * @param vm Pointer to the virtual machine state
 * @details Copies the nth item from the stack to the top, where n is popped from the stack.
 */
static void stack_word_pick(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "PICK: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm->data_stack[vm->dsp];

    /* n=0 means duplicate TOS, n=1 means copy second item, etc. */
    if (n < 0 || n >= vm->dsp + 1) {
        log_message(LOG_ERROR, "PICK: Invalid index %ld (stack depth: %d)", (long) n, vm->dsp + 1);
        vm->error = 1;
        return;
    }

    /* Don't consume the index for PICK - it copies the nth item to replace the index */
    cell_t value = vm->data_stack[vm->dsp - n];
    vm->data_stack[vm->dsp] = value;

    log_message(LOG_DEBUG, "PICK: Copied item at index %ld to top", (long) n);
}

/**
 * @brief Implements ROLL ( n -- ) - Moves nth stack item to top
 * @param vm Pointer to the virtual machine state
 * @details Moves the nth item from the stack to the top, shifting intermediate items down.
 */
static void stack_word_roll(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "ROLL: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t n = vm->data_stack[vm->dsp--]; /* Pop n */

    if (n < 0 || n > vm->dsp + 1) {
        log_message(LOG_ERROR, "ROLL: Invalid index %ld (stack depth after pop: %d)", (long) n, vm->dsp + 1);
        vm->error = 1;
        return;
    }

    if (n == 0) {
        /* n=0 means do nothing (no item to move) */
        log_message(LOG_DEBUG, "ROLL: n=0, no operation");
        return;
    }

    if (n == 1) {
        /* n=1 means top item is already at top, do nothing */
        log_message(LOG_DEBUG, "ROLL: n=1, no operation needed");
        return;
    }

    /* Save the item to be moved to top */
    cell_t value = vm->data_stack[vm->dsp + 1 - n];

    /* Shift items down to fill the gap */
    for (int i = vm->dsp + 1 - n; i < vm->dsp + 1; i++) {
        vm->data_stack[i] = vm->data_stack[i + 1];
    }

    /* Put the saved item on top */
    vm->data_stack[vm->dsp + 1] = value;

    log_message(LOG_DEBUG, "ROLL: Moved item at index %ld to top", (long) n);
}

/**
 * @brief Registers all stack operation words with the virtual machine
 * @param vm Pointer to the virtual machine state
 * @details Registers all FORTH-79 standard stack operation words with the word registry.
 */
void register_stack_words(VM *vm) {
    register_word(vm, "DROP", stack_word_drop);
    register_word(vm, "DUP", stack_word_dup);
    register_word(vm, "?DUP", stack_word_question_dup);
    register_word(vm, "SWAP", stack_word_swap);
    register_word(vm, "OVER", stack_word_over);
    register_word(vm, "ROT", stack_word_rot);
    register_word(vm, "-ROT", stack_word_minus_rot);
    register_word(vm, "DEPTH", stack_word_depth);
    register_word(vm, "PICK", stack_word_pick);
    register_word(vm, "ROLL", stack_word_roll);
}