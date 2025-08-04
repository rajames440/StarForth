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

#ifndef DICTIONARY_MANIPULATION_WORDS_H
#define DICTIONARY_MANIPULATION_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 Dictionary Manipulation Words:
 * [         ( -- )                      Enter interpretation mode
 * ]         ( -- )                      Enter compilation mode
 * STATE     ( -- addr )                 Variable: compilation state
 * SMUDGE    ( -- )                      Toggle smudge bit of latest word
 * >BODY     ( xt -- addr )              Convert execution token to body
 * >NAME     ( xt -- addr )              Convert execution token to name
 * NAME>     ( addr -- xt )              Convert name to execution token
 * >LINK     ( addr -- addr )            Get link field address
 * LINK>     ( addr -- addr )            Get next word from link
 * CFA       ( addr -- xt )              Get code field address
 * LFA       ( addr -- addr )            Get link field address
 * NFA       ( addr -- addr )            Get name field address
 * PFA       ( addr -- addr )            Get parameter field address
 * TRAVERSE  ( addr n -- addr )          Move through name field
 * INTERPRET ( -- )                      Text interpreter
 */

void register_dictionary_manipulation_words(VM *vm);

#endif /* DICTIONARY_MANIPULATION_WORDS_H */
