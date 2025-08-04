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

#ifndef SYSTEM_WORDS_H
#define SYSTEM_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 System & Environment Words:
 * COLD      ( -- )                      Cold start system
 * WARM      ( -- )                      Warm start system  
 * BYE       ( -- )                      Exit FORTH system
 * SAVE-SYSTEM ( -- )                    Save system image
 * WORDS     ( -- )                      List words in current vocabulary
 * VLIST     ( -- )                      List all words in vocabulary
 * ?STACK    ( -- )                      Check stack depth
 * PAGE      ( -- )                      Clear screen/page
 * NOP       ( -- )                      No operation
 * 79-STANDARD ( -- )                    Ensure FORTH-79 compliance
 */

void register_system_words(VM *vm);

#endif /* SYSTEM_WORDS_H */
