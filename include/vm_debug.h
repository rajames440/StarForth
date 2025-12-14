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