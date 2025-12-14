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