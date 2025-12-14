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

/**
 * @file return_stack_words.c
 * @brief Implementation of FORTH-79 standard return stack words
 * 
 * StarForth - FORTH-79 Standard and ANSI C99 ONLY
 * Last modified - 2025-09-10
 * Public domain / CC0 — No warranty.
 *
 * FORTH-79 compliance:
 * - Provide only >R, R>, R@ for return stack transfer.
 * - No direct stack addressing (no RP!, RP@) — forbidden by FORTH-79.
 */

#include "include/return_stack_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"

/**
 * @brief Move TOS from data stack to return stack (>R word)
 * @param vm Pointer to VM instance
 * @details Stack effect: ( x -- )
 */
static void return_stack_word_to_r(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, ">R: DSP underflow");
        return;
    }
    if (vm->rsp + 1 >= STACK_SIZE) {
        vm->error = 1;
        log_message(LOG_ERROR, ">R: RSP overflow");
        return;
    }
    cell_t x = vm_pop(vm);
    vm_rpush(vm, x);
}

/**
 * @brief Move TOS from return stack back to data stack (R> word)
 * @param vm Pointer to VM instance
 * @details Stack effect: ( -- x )
 */
static void return_stack_word_r_from(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "R>: RSP underflow");
        return;
    }
    if (vm->dsp + 1 >= STACK_SIZE) {
        vm->error = 1;
        log_message(LOG_ERROR, "R>: DSP overflow");
        return;
    }
    cell_t x = vm_rpop(vm);
    vm_push(vm, x);
}

/**
 * @brief Copy TOS of return stack to data stack (R@ word)
 * @param vm Pointer to VM instance
 * @details Stack effect: ( -- x )
 */
static void return_stack_word_r_fetch(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "R@: RSP underflow");
        return;
    }
    if (vm->dsp + 1 >= STACK_SIZE) {
        vm->error = 1;
        log_message(LOG_ERROR, "R@: DSP overflow");
        return;
    }
    cell_t x = vm->return_stack[vm->rsp];
    vm_push(vm, x);
}

/**
 * @brief Register all FORTH-79 return stack manipulation words
 * @param vm Pointer to VM instance
 * @details Registers >R, R>, and R@ words. Direct stack addressing words
 *          (RP!, RP@) are deliberately omitted for FORTH-79 compliance.
 */
void register_return_stack_words(VM *vm) {
    if (!vm) return;
    register_word(vm, ">R", return_stack_word_to_r);
    register_word(vm, "R>", return_stack_word_r_from);
    register_word(vm, "R@", return_stack_word_r_fetch);
}