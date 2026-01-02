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

#ifndef IO_WORDS_H  //!< Include guard
#define IO_WORDS_H  //!< Include guard

#include "vm.h"

/* FORTH-79 I/O & Terminal Words:
 * EMIT      ( c -- )                    Output character c
 * CR        ( -- )                      Output carriage return
 * KEY       ( -- c )                    Input character from keyboard
 * ?TERMINAL ( -- flag )                 True if key pressed
 * TYPE      ( addr u -- )               Output u characters from addr
 * SPACE     ( -- )                      Output one space
 * SPACES    ( n -- )                    Output n spaces
 */

/**
 * @brief Registers all I/O related FORTH words with the virtual machine
 * @param vm Pointer to the virtual machine instance
 *
 * Registers the following FORTH-79 I/O & Terminal Words:
 * - EMIT      ( c -- )    Output character c
 * - CR        ( -- )      Output carriage return
 * - KEY       ( -- c )    Input character from keyboard
 * - ?TERMINAL ( -- flag ) True if key pressed
 * - TYPE      ( addr u -- ) Output u characters from addr
 * - SPACE     ( -- )      Output one space
 * - SPACES    ( n -- )    Output n spaces
 */
void register_io_words(VM * vm);

#endif /* IO_WORDS_H */