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

#ifndef DEFINING_WORDS_H
#define DEFINING_WORDS_H

#include "vm.h"

/**
 * @defgroup DefiningWords FORTH-79 Defining Words
 * @{
 * @brief Standard FORTH-79 defining words
 *
 * @details These words are used for creating new dictionary entries and defining words:
 * - :         ( -- )     Start colon definition
 * - ;         ( -- )     End colon definition
 * - CREATE    ( -- )     Create dictionary entry
 * - DOES>     ( -- )     Define runtime behavior
 * - <BUILDS   ( -- )     Create word with <BUILDS/DOES>
 * - CONSTANT  ( n -- )   Define constant
 * - VARIABLE  ( -- )     Define variable
 * - 2CONSTANT ( d -- )   Define double constant
 * - 2VARIABLE ( -- )     Define double variable
 * - USER      ( n -- )   Define user variable
 * - IMMEDIATE ( -- )     Make most recent word immediate
 * - [COMPILE] ( -- )     Force compilation of immediate word
 * - COMPILE   ( -- )     Compile inline code
 * - FORGET    ( -- )     Remove word and all after it
 * @}
 */

/**
 * @brief Registers all FORTH-79 defining words with the virtual machine
 * @param vm Pointer to the virtual machine instance
 */
void register_defining_words(VM * vm);

#endif /* DEFINING_WORDS_H */