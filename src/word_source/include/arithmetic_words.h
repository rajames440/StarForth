/*

                                 ***   StarForth   ***
  arithmetic_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef ARITHMETIC_WORDS_H
#define ARITHMETIC_WORDS_H

#include "../../../include/vm.h"

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