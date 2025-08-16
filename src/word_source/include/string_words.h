/*

                                 ***   StarForth   ***
  string_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef STRING_WORDS_H
#define STRING_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 String & Text Processing Words:
 * COUNT     ( addr1 -- addr2 u )        Get string length and address
 * EXPECT    ( addr u -- )               Accept input line
 * SPAN      ( -- addr )                 Variable: number of characters input
 * QUERY     ( -- )                      Accept input into TIB
 * TIB       ( -- addr )                 Terminal input buffer address
 * WORD      ( c -- addr )               Parse next word, delimited by c
 * >IN       ( -- addr )                 Input stream pointer
 * SOURCE    ( -- addr u )               Input source specification
 * BL        ( -- c )                    ASCII space character (32)
 * FIND      ( addr -- addr flag )       Find word in dictionary
 * '         ( -- xt )                   Get execution token of next word
 * [']       ( -- xt )                   Compile execution token
 * LITERAL   ( n -- )                    Compile literal number
 * [LITERAL] ( n -- )                    Compile literal at compile time
 * CONVERT   ( d1 addr1 -- d2 addr2 )    Convert string to number
 * NUMBER    ( addr -- n flag )          Convert string to single number
 * ENCLOSE   ( addr c -- addr1 n1 n2 n3 ) Parse for delimiter
 */

void register_string_words(VM * vm);

#endif /* STRING_WORDS_H */