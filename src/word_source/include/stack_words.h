/*

                                 ***   StarForth   ***
  stack_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef STACK_WORDS_H
#define STACK_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 Stack Operation Words to implement:
 * DROP      ( n -- )                    Remove top stack item
 * DUP       ( n -- n n )                Duplicate top stack item  
 * ?DUP      ( n -- n n | n -- 0 )       Duplicate if non-zero
 * SWAP      ( n1 n2 -- n2 n1 )          Exchange top two stack items
 * OVER      ( n1 n2 -- n1 n2 n1 )       Copy second stack item to top
 * ROT       ( n1 n2 n3 -- n2 n3 n1 )    Rotate top three stack items
 * -ROT      ( n1 n2 n3 -- n3 n1 n2 )    Reverse rotate top three items
 * DEPTH     ( -- n )                    Return number of stack items
 * PICK      ( n -- stack[n] )           Copy nth stack item to top
 * ROLL      ( n -- )                    Move nth stack item to top
 */

void register_stack_words(VM * vm);

#endif /* STACK_WORDS_H */