/*
                                  ***   StarForth   ***

  vm_debug.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:55:24.798-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/include/vm_debug.h
 */

/** @cond */
#ifndef VM_DEBUG_H
#define VM_DEBUG_H
/** @endcond */

#include "vm.h"

/**
 * @brief Sets the VM instance to dump state for when signals occur
 * @param vm Pointer to the VM instance to monitor
 * @note Should be called once after vm_init
 */
void vm_debug_set_current_vm(VM * vm);

/**
 * @brief Installs signal handlers for SIGSEGV and SIGABRT
 * @details The handlers will print VM state before re-raising the signal
 */
void vm_debug_install_signal_handlers(void);

/**
 * @brief Manually dumps the state of a VM instance
 * @param vm The VM instance to dump state for
 * @param reason Description of why the dump was triggered
 * @note Typically called before exit(1) on test failure
 */
void vm_debug_dump_state(const VM *vm, const char *reason);

#endif /* VM_DEBUG_H */