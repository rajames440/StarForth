/*

                                 ***   StarForth   ***
  editor_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef EDITOR_WORDS_H
#define EDITOR_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 Line Editor Words:
 * L         ( -- )                      List current screen
 * T         ( n -- )                    Type line n of current screen
 * E         ( n -- )                    Edit line n of current screen
 * S         ( n addr -- )               Replace line n with string at addr
 * I         ( n -- )                    Insert before line n
 * R         ( n -- )                    Replace line n
 * D         ( n -- )                    Delete line n
 * P         ( n -- )                    Position to line n
 * COPY      ( n1 n2 -- )               Copy line n1 to line n2
 * M         ( n1 n2 -- )               Move line n1 to line n2
 * TILL      ( c -- )                   Search for character c
 * N         ( -- )                      Search for next occurrence
 * WHERE     ( -- )                     Show current position
 * TOP       ( -- )                     Go to top of screen
 * BOTTOM    ( -- )                     Go to bottom of screen
 * CLEAR     ( -- )                     Clear current screen
 */

void register_editor_words(VM * vm);

#endif /* EDITOR_WORDS_H */