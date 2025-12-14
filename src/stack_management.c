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
 * @file stack_management.c
 * @brief Implementation of stack operations for the Forth virtual machine
 * 
 * This module provides the core stack manipulation functions for both the data stack
 * and return stack of the Forth virtual machine. It includes optimized assembly 
 * implementations for x86_64 and ARM64 architectures when available.
 */

#include "vm.h"
#include "log.h"
#include "profiler.h"

#if defined(USE_ASM_OPT) && USE_ASM_OPT
#  if defined(ARCH_X86_64)
#    include "vm_asm_opt.h"
#  elif defined(ARCH_ARM64) || defined(__aarch64__) || defined(__arm64__)
#    include "vm_asm_opt_arm64.h"
#  endif
#endif

/**
 * @brief Push a value onto the data stack
 *
 * @param vm Pointer to the VM structure
 * @param value Value to push onto the stack
 *
 * @note Performs bounds checking to prevent stack overflow
 * @note Uses assembly optimization if available for x86_64 or ARM64
 */
void vm_push(VM *vm, cell_t value) {
    PROFILE_INC_STACK_OP();

#if defined(USE_ASM_OPT) && USE_ASM_OPT && (defined(ARCH_X86_64) || defined(ARCH_ARM64))
    /* Use optimized assembly version for x86_64 or ARM64 */
    vm_push_asm(vm, value);
#ifndef NDEBUG
    if (!vm->error) {
        log_message(LOG_DEBUG, "PUSH: %ld (dsp=%d)", value, vm->dsp);
    }
#endif
#else
    /* Standard C implementation */
    if (vm->dsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "Stack overflow");
        vm->error = 1;
        return;
    }
    vm->data_stack[++vm->dsp] = value;
    log_message(LOG_DEBUG, "PUSH: %ld (dsp=%d)", value, vm->dsp);
#endif
}

/**
 * @brief Pop a value from the data stack
 *
 * @param vm Pointer to the VM structure
 * @return The popped value
 *
 * @note Returns 0 and sets error flag if stack underflow occurs
 * @note Uses assembly optimization if available for x86_64 or ARM64
 */
cell_t vm_pop(VM *vm) {
    PROFILE_INC_STACK_OP();

#if defined(USE_ASM_OPT) && USE_ASM_OPT && (defined(ARCH_X86_64) || defined(ARCH_ARM64))
    /* Use optimized assembly version for x86_64 */
    cell_t value = vm_pop_asm(vm);
#ifndef NDEBUG
    if (!vm->error) {
        log_message(LOG_DEBUG, "POP: %ld (dsp=%d)", value, vm->dsp);
    }
#endif
    return value;
#else
    /* Standard C implementation */
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "Stack underflow (dsp=%d)", vm->dsp);
        vm->error = 1;
        return 0;
    }
    cell_t value = vm->data_stack[vm->dsp--];
    log_message(LOG_DEBUG, "POP: %ld (dsp=%d)", value, vm->dsp);
    return value;
#endif
}

/**
 * @brief Push a value onto the return stack
 *
 * @param vm Pointer to the VM structure
 * @param value Value to push onto the return stack
 *
 * @note Sets error flag if stack overflow occurs
 * @note Uses assembly optimization if available for x86_64 or ARM64
 */
void vm_rpush(VM *vm, cell_t value) {
#if defined(USE_ASM_OPT) && USE_ASM_OPT && (defined(ARCH_X86_64) || defined(ARCH_ARM64))
    /* Use optimized assembly version for x86_64 */
    vm_rpush_asm(vm, value);
#else
    /* Standard C implementation */
    if (vm->rsp >= STACK_SIZE - 1) {
        vm->error = 1; /* Return stack overflow */
        return;
    }
    vm->return_stack[++vm->rsp] = value;
#endif
}

/**
 * @brief Pop a value from the return stack
 *
 * @param vm Pointer to the VM structure
 * @return The popped value
 *
 * @note Returns 0 and sets error flag if stack underflow occurs
 * @note Uses assembly optimization if available for x86_64 or ARM64
 */
cell_t vm_rpop(VM *vm) {
#if defined(USE_ASM_OPT) && USE_ASM_OPT && (defined(ARCH_X86_64) || defined(ARCH_ARM64))
    /* Use optimized assembly version for x86_64 */
    return vm_rpop_asm(vm);
#else
    /* Standard C implementation */
    if (vm->rsp < 0) {
        vm->error = 1; /* Return stack underflow */
        return 0;
    }
    cell_t value = vm->return_stack[vm->rsp--];
    return value;
#endif
}