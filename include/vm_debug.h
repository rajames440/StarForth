/*

                                 ***   StarForth   ***
  vm_debug.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/11/25, 12:40 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef VM_DEBUG_H
#define VM_DEBUG_H

#include "vm.h"

/* Set which VM to dump if a signal hits (call once after vm_init). */
void vm_debug_set_current_vm(VM * vm);

/* Install SIGSEGV/SIGABRT handlers that print VM state before re-raising. */
void vm_debug_install_signal_handlers(void);

/* Manually dump VM state (call this right before exit(1) on test failure). */
void vm_debug_dump_state(const VM *vm, const char *reason);

#endif /* VM_DEBUG_H */