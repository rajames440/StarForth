/*
                                  ***   StarForth   ***

  mixed_arithmetic_words.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.747-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/word_source/include/mixed_arithmetic_words.h
 */

#ifndef MIXED_ARITHMETIC_WORDS_H
#define MIXED_ARITHMETIC_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 Mixed Arithmetic Words (single/double precision):
 * M+        ( d1 n -- d2 )               Add single to double
 * M-        ( d1 n -- d2 )               Subtract single from double  
 * M-STAR-SLASH ( d1 n1 n2 -- d2 )        Multiply double by single, divide by single
 * SM/REM    ( d n1 -- n2 n3 )            Symmetric divide/remainder
 * FM/MOD    ( d n1 -- n2 n3 )            Floored divide/mod
 * UM*       ( u1 u2 -- ud )              Unsigned multiply to double
 * UM/MOD    ( ud u1 -- u2 u3 )           Unsigned divide/mod
 * STAR-SLASH-MOD ( n1 n2 n3 -- n4 n5 )   Intermediate multiply/divide/mod
 */

void register_mixed_arithmetic_words(VM * vm);

#endif /* MIXED_ARITHMETIC_WORDS_H */