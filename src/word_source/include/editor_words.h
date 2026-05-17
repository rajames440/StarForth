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

#ifndef EDITOR_WORDS_H
#define EDITOR_WORDS_H

#include "vm.h"

/**
 * @defgroup editor_words FORTH-79 Line Editor Words
 * @{
 *
 * @brief Line editor commands implementation
 *
 * @fn void L ( -- )
 * @brief List current screen
 *
 * @fn void T ( n -- )
 * @brief Type line n of current screen
 * @param[in] n Line number to type
 *
 * @fn void E ( n -- )
 * @brief Edit line n of current screen
 * @param[in] n Line number to edit
 *
 * @fn void S ( n addr -- )
 * @brief Replace line n with string at addr
 * @param[in] n Line number to replace
 * @param[in] addr Address of replacement string
 *
 * @fn void I ( n -- )
 * @brief Insert before line n
 * @param[in] n Line number to insert before
 *
 * @fn void R ( n -- )
 * @brief Replace line n
 * @param[in] n Line number to replace
 *
 * @fn void D ( n -- )
 * @brief Delete line n
 * @param[in] n Line number to delete
 *
 * @fn void P ( n -- )
 * @brief Position to line n
 * @param[in] n Line number to position to
 *
 * @fn void COPY ( n1 n2 -- )
 * @brief Copy line n1 to line n2
 * @param[in] n1 Source line number
 * @param[in] n2 Destination line number
 *
 * @fn void M ( n1 n2 -- )
 * @brief Move line n1 to line n2
 * @param[in] n1 Source line number
 * @param[in] n2 Destination line number
 *
 * @fn void TILL ( c -- )
 * @brief Search for character c
 * @param[in] c Character to search for
 *
 * @fn void N ( -- )
 * @brief Search for next occurrence
 *
 * @fn void WHERE ( -- )
 * @brief Show current position
 *
 * @fn void TOP ( -- )
 * @brief Go to top of screen
 *
 * @fn void BOTTOM ( -- )
 * @brief Go to bottom of screen
 *
 * @fn void CLEAR ( -- )
 * @brief Clear current screen
 *
 * @}
 */

/**
 * @brief Register all editor words with the Forth virtual machine
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void register_editor_words(VM * vm);

#endif /* EDITOR_WORDS_H */