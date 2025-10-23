/*
                                  ***   StarForth   ***

  editor_words.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.667-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/word_source/include/editor_words.h
 */

#ifndef EDITOR_WORDS_H
#define EDITOR_WORDS_H

#include "../../../include/vm.h"

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