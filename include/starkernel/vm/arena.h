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
 * arena.h - VM arena management for StarKernel builds.
 *
 * Hosted builds never include this header. Kernel integration layers use it
 * to provision the PMM-backed VM arena before handing memory to the VM core.
 */

#ifndef STARKERNEL_VM_ARENA_H
#define STARKERNEL_VM_ARENA_H

#ifdef __STARKERNEL__

#include <stddef.h>
#include <stdint.h>

uint64_t sk_vm_arena_alloc(void);
void     sk_vm_arena_free(void);
uint8_t *sk_vm_arena_ptr(void);
size_t   sk_vm_arena_size(void);
int      sk_vm_arena_is_initialized(void);
void     sk_vm_arena_assert_guards(const char *tag);

#endif /* __STARKERNEL__ */

#endif /* STARKERNEL_VM_ARENA_H */
