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

#ifndef ARITHMETIC_WORDS_H
#define ARITHMETIC_WORDS_H

#include "vm.h"

/* FORTH-79 Arithmetic Words:
 * +         ( n1 n2 -- n3 )              Add n1 and n2
 * -         ( n1 n2 -- n3 )              Subtract n2 from n1
 * *         ( n1 n2 -- n3 )              Multiply n1 and n2
 * /         ( n1 n2 -- n3 )              Divide n1 by n2
 * MOD       ( n1 n2 -- n3 )              Remainder of n1/n2
 * /MOD      ( n1 n2 -- n3 n4 )           n3=remainder, n4=quotient of n1/n2
 * NEGATE    ( n1 -- n2 )                 Negate n1
 * ABS       ( n1 -- n2 )                 Absolute value of n1
 * MIN       ( n1 n2 -- n3 )              Minimum of n1 and n2
 * MAX       ( n1 n2 -- n3 )              Maximum of n1 and n2
 * 1+        ( n1 -- n2 )                 Add 1 to n1
 * 1-        ( n1 -- n2 )                 Subtract 1 from n1
 * 2*        ( n1 -- n2 )                 Multiply n1 by 2 (left shift)
 * 2/        ( n1 -- n2 )                 Divide n1 by 2 (right shift)
 * STAR-SLASH ( n1 n2 n3 -- n4 )          Multiply n1*n2, divide by n3
 */

/**
 * @brief Registers all FORTH-79 arithmetic words with the virtual machine
 *
 * Registers the following words:
 * - + (add)
 * - - (subtract)
 * - * (multiply)
 * - / (divide)
 * - MOD (modulo)
 * - /MOD (divide with remainder)
 * - NEGATE
 * - ABS (absolute)
 * - MIN
 * - MAX
 * - 1+ (increment)
 * - 1- (decrement)
 * - 2* (multiply by 2)
 * - 2/ (divide by 2)
 * - STAR-SLASH (multiply then divide)
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void register_arithmetic_words(VM * vm);

#endif /* ARITHMETIC_WORDS_H */