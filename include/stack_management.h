/*

                                 ***   StarForth   ***
  stack_management.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/**
 * @file stack_management.h
 * @brief Stack management functionality for the StarForth virtual machine
 *
 * This module provides the stack management operations required by the FORTH-79
 * virtual machine implementation. It handles both the data and return stacks,
 * including push, pop, and stack manipulation operations.
 *
 * All function implementations are located in stack_management.c while the
 * prototypes are maintained in vm.h for API consistency.
 */

/** @brief Include guard for stack_management.h */
#ifndef STACK_MANAGEMENT_H
#define STACK_MANAGEMENT_H

#include "vm.h"

/* Implementations live in stack_management.c.
   Prototypes remain in vm.h to avoid API churn. */

#endif /* STACK_MANAGEMENT_H */