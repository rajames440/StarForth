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

#ifndef CONTROL_WORDS_H
#define CONTROL_WORDS_H

#include "vm.h"

/**
 * @defgroup control_words FORTH-79 Control Flow Words
 * @{
 * @brief Standard FORTH-79 control flow words implementation
 *
 * @def IF ( flag -- )
 * Conditional execution based on flag
 *
 * @def THEN ( -- )
 * End of IF conditional block
 *
 * @def ELSE ( -- )
 * Alternative execution path
 *
 * @def BEGIN ( -- )
 * Start of indefinite loop
 *
 * @def UNTIL ( flag -- )
 * End loop if flag is true
 *
 * @def AGAIN ( -- )
 * End infinite loop
 *
 * @def WHILE ( flag -- )
 * Test condition in BEGIN loop
 *
 * @def REPEAT ( -- )
 * End of WHILE loop
 *
 * @def DO ( limit start -- )
 * Start definite loop
 *
 * @def LOOP ( -- )
 * End DO loop, increment by 1
 *
 * @def +LOOP ( n -- )
 * End DO loop, increment by n
 *
 * @def I ( -- n )
 * Get current loop index
 *
 * @def J ( -- n )
 * Get outer loop index
 *
 * @def LEAVE ( -- )
 * Exit current loop
 *
 * @def UNLOOP ( -- )
 * Discard loop parameters
 *
 * @def EXIT ( -- )
 * Exit current word
 *
 * @def ABORT ( -- )
 * Abort execution
 *
 * @def QUIT ( -- )
 * Return to interpreter
 *
 * @def EXECUTE ( xt -- )
 * Execute word at address
 *
 * @def BRANCH ( -- )
 * Unconditional branch
 *
 * @def 0BRANCH ( flag -- )
 * Branch if flag is zero
 *
 * @def (LIT) ( -- n )
 * Push inline literal
 * @}
 */

/**
 * @brief Registers all control flow words with the Forth virtual machine
 * @param vm Pointer to the Forth virtual machine instance
 */
void register_control_words(VM * vm);

#endif /* CONTROL_WORDS_H */