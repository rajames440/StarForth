/*

                                 ***   StarForth   ***
  stack_management.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/14/25, 8:59 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


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

/* Data stack */
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

/* Return stack */
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