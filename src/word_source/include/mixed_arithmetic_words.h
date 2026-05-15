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

#ifndef MIXED_ARITHMETIC_WORDS_H
#define MIXED_ARITHMETIC_WORDS_H

#include "vm.h"

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