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

#ifndef LOGICAL_WORDS_H
#define LOGICAL_WORDS_H

#include "vm.h"

/**
 * @defgroup logical_ops FORTH-79 Logical Operations
 * @{
 *
 * @brief FORTH-79 Standard logical and comparison operations
 *
 * @details Implementation of the following FORTH-79 words:
 * - AND       ( n1 n2 -- n3 )             Bitwise AND
 * - OR        ( n1 n2 -- n3 )             Bitwise OR
 * - XOR       ( n1 n2 -- n3 )             Bitwise XOR
 * - NOT       ( n1 -- n2 )                Bitwise NOT
 * - 0=        ( n -- flag )               True if n is zero
 * - 0<        ( n -- flag )               True if n is negative
 * - 0>        ( n -- flag )               True if n is positive
 * - =         ( n1 n2 -- flag )           True if n1 equals n2
 * - <         ( n1 n2 -- flag )           True if n1 < n2
 * - >         ( n1 n2 -- flag )           True if n1 > n2
 * - U<        ( u1 u2 -- flag )           True if u1 < u2 (unsigned)
 * - U>        ( u1 u2 -- flag )           True if u1 > u2 (unsigned)
 * - WITHIN    ( n low high -- flag )      True if low <= n < high
 * @}
 */

/**
 * @brief Registers all logical and comparison words with the Forth virtual machine
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void register_logical_words(VM * vm);

#endif /* LOGICAL_WORDS_H */