/*
                                  ***   StarForth   ***

  defining_words.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.706-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/word_source/include/defining_words.h
 */

#ifndef DEFINING_WORDS_H
#define DEFINING_WORDS_H

#include "../../../include/vm.h"

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