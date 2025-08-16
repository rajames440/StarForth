/*

                                 ***   StarForth   ***
  io_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef IO_WORDS_H
#define IO_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 I/O & Terminal Words:
 * EMIT      ( c -- )                    Output character c
 * CR        ( -- )                      Output carriage return
 * KEY       ( -- c )                    Input character from keyboard
 * ?TERMINAL ( -- flag )                 True if key pressed
 * TYPE      ( addr u -- )               Output u characters from addr
 * SPACE     ( -- )                      Output one space
 * SPACES    ( n -- )                    Output n spaces
 */

void register_io_words(VM * vm);

#endif /* IO_WORDS_H */