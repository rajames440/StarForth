/*

                                 ***   StarForth   ***
  dictionary_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef DICTIONARY_WORDS_H
#define DICTIONARY_WORDS_H

#include "../../../include/vm.h"

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