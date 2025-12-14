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

#ifndef RETURN_STACK_WORDS_H
#define RETURN_STACK_WORDS_H

#include "../../../include/vm.h"

/**
 * @defgroup ReturnStackOps FORTH-79 Return Stack Operations
 * @{
 *
 * @def >R
 * @brief Move item from data to return stack
 * Stack effect: ( n -- ) ( R: -- n )
 *
 * @def R>
 * @brief Move item from return to data stack
 * Stack effect: ( -- n ) ( R: n -- )
 *
 * @def R@
 * @brief Copy top of return stack to data stack
 * Stack effect: ( -- n ) ( R: n -- n )
 *
 * @def RP!
 * @brief Set return stack pointer
 * Stack effect: ( addr -- )
 *
 * @def RP@
 * @brief Get return stack pointer
 * Stack effect: ( -- addr )
 * @}
 */

/**
 * @brief Registers all return stack manipulation words with the virtual machine
 *
 * @param vm Pointer to the virtual machine instance
 */
void register_return_stack_words(VM * vm);

#endif /* RETURN_STACK_WORDS_H */