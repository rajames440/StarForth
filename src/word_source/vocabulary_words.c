/*

  * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
  *
  * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  * To the extent possible under law, the author(s) have dedicated all copyright and related
  * and neighboring rights to this software to the public domain worldwide.
  * This software is distributed without any warranty.
  *
  * See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

 */

/* vocabulary_words.c - FORTH-79 Vocabulary System Words */
#include "include/vocabulary_words.h"
#include "../../include/word_registry.h"

/* FORTH-79 Vocabulary System Words to implement:
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

void register_vocabulary_words(VM *vm) {
    /* TODO: Implement and register all vocabulary system words */
}
