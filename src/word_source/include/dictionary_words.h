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

#ifndef DICTIONARY_WORDS_H
#define DICTIONARY_WORDS_H

#include "vm.h"

/**
 * @defgroup dictionary FORTH-79 Dictionary & Compilation Words
 * @{
 * 
 * @brief Dictionary and compilation words as defined in FORTH-79 Standard
 *
 * @word HERE ( -- addr )    Returns address of next free dictionary space
 * @word ALLOT ( n -- )      Allocates n bytes in dictionary
 * @word , ( n -- )          Compiles n into dictionary
 * @word C, ( c -- )         Compiles character into dictionary
 * @word 2, ( d -- )         Compiles double into dictionary
 * @word PAD ( -- addr )     Returns address of temporary text buffer
 * @word SP! ( addr -- )     Sets data stack pointer
 * @word SP@ ( -- addr )     Gets data stack pointer
 * @word LATEST ( -- addr )  Returns address of most recent definition
 * @}
 */

/**
 * @brief Registers all dictionary and compilation words with the Forth VM
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void register_dictionary_words(VM * vm);

#endif /* DICTIONARY_WORDS_H */