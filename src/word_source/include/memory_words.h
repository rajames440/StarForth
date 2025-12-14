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

#ifndef MEMORY_WORDS_H
#define MEMORY_WORDS_H

#include "../../../include/vm.h"

/**
 * @defgroup memory_ops FORTH-79 Memory Operations
 * @{
 * @brief Memory operation words defined in FORTH-79 standard
 *
 * @par Available operations:
 * - !     ( n addr -- )     Store n at addr
 * - @     ( addr -- n )     Fetch n from addr
 * - C!    ( c addr -- )     Store character at addr
 * - C@    ( addr -- c )     Fetch character from addr
 * - +!    ( n addr -- )     Add n to value at addr
 * - 2!    ( d addr -- )     Store double at addr
 * - 2@    ( addr -- d )     Fetch double from addr
 * - FILL  ( addr u c -- )   Fill u bytes at addr with c
 * - MOVE  ( addr1 addr2 u -- ) Move u bytes from addr1 to addr2
 * - CMOVE ( addr1 addr2 u -- ) Move u bytes, low to high addresses
 * - CMOVE> ( addr1 addr2 u -- ) Move u bytes, high to low addresses
 * - ERASE ( addr u -- )     Fill u bytes at addr with zeros
 * @}
 */

/**
 * @brief Registers all memory operation words with the Forth virtual machine
 * @param vm Pointer to the virtual machine instance
 */
void register_memory_words(VM * vm);

#endif /* MEMORY_WORDS_H */