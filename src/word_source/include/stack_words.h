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

#ifndef STACK_WORDS_H
#define STACK_WORDS_H

#include "../../../include/vm.h"

/**
 * @defgroup stack_ops FORTH-79 Stack Operations
 * @{
 *
 * @brief Standard FORTH-79 stack manipulation words
 *
 * @details The following FORTH words are implemented for stack manipulation:
 * - DROP      ( n -- )                    Remove top stack item
 * - DUP       ( n -- n n )                Duplicate top stack item  
 * - ?DUP      ( n -- n n | n -- 0 )       Duplicate if non-zero
 * - SWAP      ( n1 n2 -- n2 n1 )          Exchange top two stack items
 * - OVER      ( n1 n2 -- n1 n2 n1 )       Copy second stack item to top
 * - ROT       ( n1 n2 n3 -- n2 n3 n1 )    Rotate top three stack items
 * - -ROT      ( n1 n2 n3 -- n3 n1 n2 )    Reverse rotate top three items
 * - DEPTH     ( -- n )                    Return number of stack items
 * - PICK      ( n -- stack[n] )           Copy nth stack item to top
 * - ROLL      ( n -- )                    Move nth stack item to top
 * @}
 */

/**
 * @brief Registers all stack operation words with the virtual machine
 *
 * @param vm Pointer to the virtual machine instance
 */
void register_stack_words(VM * vm);

#endif /* STACK_WORDS_H */