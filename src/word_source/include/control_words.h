/*

                                 ***   StarForth   ***
  control_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef CONTROL_WORDS_H
#define CONTROL_WORDS_H

#include "../../../include/vm.h"

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