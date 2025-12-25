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

#ifndef TIME_WORDS_H
#define TIME_WORDS_H

#include "../../../include/vm.h"

/**
 * @defgroup time_words M5 Time & Heartbeat Words
 * @{
 *
 * @brief TIME-TICKS and TIME-TRUST words for M5 heartbeat subsystem
 *
 * @details The following words provide access to the heartbeat timing subsystem:
 *
 * @par TIME-TICKS ( -- u )
 * Pushes the current monotonic heartbeat tick count
 *
 * @par TIME-TRUST ( -- q )
 * Pushes the current TIME-TRUST as a Q48.16 value
 *
 * @par TIME-TRUST. ( -- )
 * Prints the current TIME-TRUST as a decimal value
 *
 * @par TIME-INIT ( -- )
 * Initializes the heartbeat subsystem
 *
 * @par TIME-VARIANCE ( -- q )
 * Pushes the current timing variance as a Q48.16 value
 * @}
 */

/**
 * @brief Registers all time words with the virtual machine
 *
 * @param vm Pointer to the virtual machine instance
 *
 * @details This function registers all M5 time and heartbeat words
 * with the specified virtual machine instance.
 */
void register_time_words(VM *vm);

#endif /* TIME_WORDS_H */
