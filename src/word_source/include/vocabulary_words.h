/*

                                 ***   StarForth   ***
  vocabulary_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


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