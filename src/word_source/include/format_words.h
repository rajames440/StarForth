/*

                                 ***   StarForth ***
  format_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef FORMAT_WORDS_H
#define FORMAT_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 Formatting & Conversion Words:
 * .         ( n -- )                    Print signed number
 * .R        ( n width -- )              Print signed number right-justified
 * U.        ( u -- )                    Print unsigned number
 * U.R       ( u width -- )              Print unsigned number right-justified
 * D.        ( d -- )                    Print double number
 * D.R       ( d width -- )              Print double number right-justified
 * .S        ( -- )                      Print stack contents
 * ?         ( addr -- )                 Print value at address
 * DUMP      ( addr u -- )               Hex dump u bytes from addr
 * <#        ( -- )                      Begin numeric conversion
 * #         ( ud1 -- ud2 )              Convert one digit
 * #>        ( ud -- addr u )            End numeric conversion
 * #S        ( ud -- 0 0 )               Convert all remaining digits
 * HOLD      ( c -- )                    Insert character in conversion
 * SIGN      ( n -- )                    Insert sign if negative
 * BASE      ( -- addr )                 Variable containing number base
 * DECIMAL   ( -- )                      Set base to 10
 * HEX       ( -- )                      Set base to 16
 * OCTAL     ( -- )                      Set base to 8
 */

void register_format_words(VM *vm);

#endif /* FORMAT_WORDS_H */
