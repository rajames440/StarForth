/*

                                 ***   StarForth   ***
  double_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef DOUBLE_WORDS_H
#define DOUBLE_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 Double Number Words:
 * S>D       ( n -- d )                  Convert single to double
 * D+        ( d1 d2 -- d3 )             Add double numbers
 * D-        ( d1 d2 -- d3 )             Subtract double numbers
 * DNEGATE   ( d1 -- d2 )                Negate double number
 * DABS      ( d1 -- d2 )                Absolute value of double
 * DMAX      ( d1 d2 -- d3 )             Maximum of two doubles
 * DMIN      ( d1 d2 -- d3 )             Minimum of two doubles
 * D<        ( d1 d2 -- flag )           Compare doubles: d1 < d2
 * D=        ( d1 d2 -- flag )           Compare doubles: d1 = d2
 * 2DROP     ( d -- )                    Drop double from stack
 * 2DUP      ( d -- d d )                Duplicate double
 * 2SWAP     ( d1 d2 -- d2 d1 )          Swap two doubles
 * 2OVER     ( d1 d2 -- d1 d2 d1 )       Copy second double to top
 * 2ROT      ( d1 d2 d3 -- d2 d3 d1 )    Rotate three doubles
 * 2>R       ( d -- ) ( R: -- d )        Move double to return stack
 * 2R>       ( -- d ) ( R: d -- )        Move double from return stack
 * 2R@       ( -- d ) ( R: d -- d )      Copy double from return stack
 * D0=       ( d -- flag )               Test if double is zero
 * D0<       ( d -- flag )               Test if double is negative
 * D2*       ( d1 -- d2 )                Multiply double by 2
 * D2/       ( d1 -- d2 )                Divide double by 2
 */

void register_double_words(VM *vm);

#endif /* DOUBLE_WORDS_H */
