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
 * @file memory_management.h
 * @brief Memory management functions for the StarForth virtual machine
 *
 * This file contains functions for managing memory allocation and alignment
 * in the StarForth virtual machine implementation.
 */

#ifndef VM_MEMORY_H
#define VM_MEMORY_H
#include "vm.h"

/**
 * @brief Allocates memory in the virtual machine's memory space
 * @param vm Pointer to the virtual machine instance
 * @param bytes Number of bytes to allocate
 * @return Pointer to the allocated memory or NULL if allocation fails
 */
void *vm_allot(VM *vm, size_t bytes);

/**
 * @brief Aligns the virtual machine's memory pointer to the next word boundary
 * @param vm Pointer to the virtual machine instance
 */
void vm_align(VM * vm);
#endif