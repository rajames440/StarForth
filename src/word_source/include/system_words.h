/*

                                 ***   StarForth   ***
  system_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef SYSTEM_WORDS_H
#define SYSTEM_WORDS_H

#include "../../../include/vm.h"

/**
 * @defgroup system_words FORTH-79 System & Environment Words
 * @{
 *
 * @brief System and environment control words for FORTH-79
 *
 * @details The following words provide system and environment control functionality:
 *
 * @par COLD ( -- )
 * Performs a cold start of the system, reinitializing all system variables
 *
 * @par WARM ( -- )
 * Performs a warm start of the system, preserving some system state
 *
 * @par BYE ( -- )
 * Exits the FORTH system and returns to the host operating system
 *
 * @par SAVE-SYSTEM ( -- )
 * Saves the current system image to disk
 *
 * @par WORDS ( -- )
 * Lists all words in the current vocabulary
 *
 * @par VLIST ( -- )
 * Lists all words in all vocabularies
 *
 * @par ?STACK ( -- )
 * Checks the stack depth and reports any stack errors
 *
 * @par PAGE ( -- )
 * Clears the screen or outputs a form feed
 *
 * @par NOP ( -- )
 * Performs no operation
 *
 * @par 79-STANDARD ( -- )
 * Ensures FORTH-79 standard compliance
 * @}
 */

/**
 * @brief Registers all system words with the virtual machine
 *
 * @param vm Pointer to the virtual machine instance
 *
 * @details This function registers all FORTH-79 system and environment words
 * with the specified virtual machine instance, making them available for use
 * in the FORTH environment.
 */
void register_system_words(VM * vm);

#endif /* SYSTEM_WORDS_H */