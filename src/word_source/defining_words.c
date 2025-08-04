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

/* defining_words.c - FORTH-79 Defining Words */
#include "include/defining_words.h"
#include "../../include/word_registry.h"

/* FORTH-79 Defining Words to implement:
 * :         ( -- )                      Start colon definition
 * ;         ( -- )                      End colon definition
 * CREATE    ( -- )                      Create dictionary entry
 * DOES>     ( -- )                      Define runtime behavior
 * <BUILDS   ( -- )                      Create word with <BUILDS/DOES>
 * CONSTANT  ( n -- )                    Define constant
 * VARIABLE  ( -- )                      Define variable
 * 2CONSTANT ( d -- )                    Define double constant
 * 2VARIABLE ( -- )                      Define double variable
 * USER      ( n -- )                    Define user variable
 * IMMEDIATE ( -- )                      Make most recent word immediate
 * [COMPILE] ( -- )                      Force compilation of immediate word
 * COMPILE   ( -- )                      Compile inline code
 * FORGET    ( -- )                      Remove word and all after it
 */

void register_defining_words(VM *vm) {
    /* TODO: Implement and register all defining words */
}
