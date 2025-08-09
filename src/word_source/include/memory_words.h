/*

                                 ***   StarForth   ***
  memory_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef MEMORY_WORDS_H
#define MEMORY_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 Memory Operation Words:
 * !         ( n addr -- )               Store n at addr
 * @         ( addr -- n )               Fetch n from addr
 * C!        ( c addr -- )               Store character at addr
 * C@        ( addr -- c )               Fetch character from addr
 * +!        ( n addr -- )               Add n to value at addr
 * 2!        ( d addr -- )               Store double at addr
 * 2@        ( addr -- d )               Fetch double from addr
 * FILL      ( addr u c -- )             Fill u bytes at addr with c
 * MOVE      ( addr1 addr2 u -- )        Move u bytes from addr1 to addr2
 * CMOVE     ( addr1 addr2 u -- )        Move u bytes, low to high addresses
 * CMOVE>    ( addr1 addr2 u -- )        Move u bytes, high to low addresses
 * ERASE     ( addr u -- )               Fill u bytes at addr with zeros
 */

void register_memory_words(VM *vm);

#endif /* MEMORY_WORDS_H */
