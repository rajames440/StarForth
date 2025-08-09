/*

                                 ***   StarForth ***
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

/* FORTH-79 Dictionary & Compilation Words:
 * HERE      ( -- addr )                 Address of next free dictionary space
 * ALLOT     ( n -- )                    Allocate n bytes in dictionary
 * ,         ( n -- )                    Compile n into dictionary
 * C,        ( c -- )                    Compile character into dictionary
 * 2,        ( d -- )                    Compile double into dictionary
 * PAD       ( -- addr )                 Address of temporary text buffer
 * SP!       ( addr -- )                 Set data stack pointer
 * SP@       ( -- addr )                 Get data stack pointer
 * LATEST    ( -- addr )                 Address of most recent definition
 */

void register_dictionary_words(VM *vm);

#endif /* DICTIONARY_WORDS_H */
