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

#ifndef VOCABULARY_WORDS_H
#define VOCABULARY_WORDS_H

#include "../../../include/vm.h"

/**
 * @defgroup vocabulary FORTH-79 Vocabulary System Words
 * @{
 * @brief Standard vocabulary manipulation words
 *
 * @par VOCABULARY ( -- )
 *      Create new vocabulary
 * @par DEFINITIONS ( -- )
 *      Set current vocabulary
 * @par CONTEXT ( -- addr )
 *      Variable: search vocabulary pointer
 * @par CURRENT ( -- addr )
 *      Variable: definition vocabulary pointer
 * @par FORTH ( -- )
 *      Select FORTH vocabulary
 * @par ONLY ( -- )
 *      Set minimal search order
 * @par ALSO ( -- )
 *      Duplicate top of search order
 * @par ORDER ( -- )
 *      Display search order
 * @par PREVIOUS ( -- )
 *      Remove top of search order
 * @par (FIND) ( addr -- addr flag )
 *      Find word in dictionary (primitive)
 * @}
 */

/**
 * @brief Registers all vocabulary-related words in the Forth system
 * @param vm Pointer to the virtual machine instance
 */
void register_vocabulary_words(VM * vm);

#endif /* VOCABULARY_WORDS_H */