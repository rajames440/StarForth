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

#ifndef DICTIONARY_MANIPULATION_WORDS_H
#define DICTIONARY_MANIPULATION_WORDS_H

#include "../../../include/vm.h"

/**
 * @defgroup dictionary_words FORTH-79 Dictionary Manipulation Words
 * @{
 * @brief Standard FORTH-79 words for dictionary manipulation
 *
 * @par [         ( -- )
 * Enter interpretation mode. Switches the system to interpret incoming words.
 *
 * @par ]         ( -- )
 * Enter compilation mode. Switches the system to compile incoming words.
 *
 * @par STATE     ( -- addr )
 * Variable containing current compilation state. Returns address of STATE variable.
 *
 * @par SMUDGE    ( -- )
 * Toggle smudge bit of latest defined word. Used during word definition.
 *
 * @par >BODY     ( xt -- addr )
 * Convert execution token to parameter field address.
 *
 * @par >NAME     ( xt -- addr )
 * Convert execution token to name field address.
 *
 * @par NAME>     ( addr -- xt )
 * Convert name field address to execution token.
 *
 * @par >LINK     ( addr -- addr )
 * Get link field address from name field address.
 *
 * @par LINK>     ( addr -- addr )
 * Get next word's address from link field.
 *
 * @par CFA       ( addr -- xt )
 * Get code field address from name field address.
 *
 * @par LFA       ( addr -- addr )
 * Get link field address from name field address.
 *
 * @par NFA       ( addr -- addr )
 * Get name field address from any field address.
 *
 * @par PFA       ( addr -- addr )
 * Get parameter field address from name field address.
 *
 * @par TRAVERSE  ( addr n -- addr )
 * Move through name field by n characters.
 *
 * @par INTERPRET ( -- )
 * Text interpreter - process input stream.
 * @}
 */

/**
 * @brief Register all dictionary manipulation words with the virtual machine
 *
 * @param vm Pointer to the virtual machine instance
 *
 * This function registers all FORTH-79 standard dictionary manipulation words
 * with the provided virtual machine instance. These words are essential for
 * dictionary management and word definition functionality.
 */
void register_dictionary_manipulation_words(VM * vm);

#endif /* DICTIONARY_MANIPULATION_WORDS_H */