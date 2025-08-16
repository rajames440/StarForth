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

/* FORTH-79 Vocabulary System Words:
 * VOCABULARY ( -- )                     Create new vocabulary
 * DEFINITIONS ( -- )                    Set current vocabulary
 * CONTEXT    ( -- addr )                Variable: search vocabulary pointer
 * CURRENT    ( -- addr )                Variable: definition vocabulary pointer
 * FORTH      ( -- )                     Select FORTH vocabulary
 * ONLY       ( -- )                     Set minimal search order
 * ALSO       ( -- )                     Duplicate top of search order
 * ORDER      ( -- )                     Display search order
 * PREVIOUS   ( -- )                     Remove top of search order
 * (FIND)     ( addr -- addr flag )      Find word in dictionary (primitive)
 */

void register_vocabulary_words(VM * vm);

#endif /* VOCABULARY_WORDS_H */