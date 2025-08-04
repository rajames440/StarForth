/*

  * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
  *
  * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  * To the extent possible under law, the author(s) have dedicated all copyright and related
  * and neighboring rights to this software to the public domain worldwide.
  * This software is distributed without any warranty.
  *
  * See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

 */

/* control_words.c - FORTH-79 Control Flow Words */
#include "include/control_words.h"
#include "../../include/word_registry.h"

/* FORTH-79 Control Flow Words to implement:
 * IF        ( flag -- )                 Conditional execution
 * THEN      ( -- )                      End of IF
 * ELSE      ( -- )                      Alternative execution
 * BEGIN     ( -- )                      Start indefinite loop
 * UNTIL     ( flag -- )                 End loop if flag true
 * AGAIN     ( -- )                      End infinite loop
 * WHILE     ( flag -- )                 Test in BEGIN loop
 * REPEAT    ( -- )                      End of WHILE loop
 * DO        ( limit start -- )          Start definite loop
 * LOOP      ( -- )                      End DO loop, increment by 1
 * +LOOP     ( n -- )                    End DO loop, increment by n
 * I         ( -- n )                    Current loop index
 * J         ( -- n )                    Outer loop index
 * LEAVE     ( -- )                      Exit current loop
 * UNLOOP    ( -- )                      Discard loop parameters
 * EXIT      ( -- )                      Exit current word
 * ABORT     ( -- )                      Abort execution
 * QUIT      ( -- )                      Return to interpreter
 * EXECUTE   ( xt -- )                   Execute word at address
 * BRANCH    ( -- )                      Unconditional branch
 * 0BRANCH   ( flag -- )                 Branch if flag is zero
 * (LIT)     ( -- n )                    Push inline literal
 */

void register_control_words(VM *vm) {
    /* TODO: Implement and register all control flow words */
}
