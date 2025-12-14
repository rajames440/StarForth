/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

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

/**
 * @brief Registers all double precision number manipulation words with the Forth virtual machine
 * @param vm Pointer to the Forth virtual machine instance
 */
void register_double_words(VM * vm);

#endif /* DOUBLE_WORDS_H */