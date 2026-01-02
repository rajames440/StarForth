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

#ifndef FORMAT_WORDS_H
#define FORMAT_WORDS_H

#include "vm.h"

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

/**
 * @brief Registers all formatting and conversion words with the Forth virtual machine
 * @param vm Pointer to the Forth virtual machine instance
 */
void register_format_words(VM * vm);

#endif /* FORMAT_WORDS_H */