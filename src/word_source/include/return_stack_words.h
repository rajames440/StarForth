/*

                                 ***   StarForth   ***
  return_stack_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef RETURN_STACK_WORDS_H
#define RETURN_STACK_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 Return Stack Operation Words:
 * >R        ( n -- ) ( R: -- n )        Move item from data to return stack
 * R>        ( -- n ) ( R: n -- )        Move item from return to data stack
 * R@        ( -- n ) ( R: n -- n )      Copy top of return stack to data stack
 * RP!       ( addr -- )                 Set return stack pointer
 * RP@       ( -- addr )                 Get return stack pointer
 */

void register_return_stack_words(VM * vm);

#endif /* RETURN_STACK_WORDS_H */