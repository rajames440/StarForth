/*
                                  ***   StarForth   ***

  memory_management.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:55:24.906-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/include/memory_management.h
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