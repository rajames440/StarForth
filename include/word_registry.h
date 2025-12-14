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

#ifndef WORD_REGISTRY_H
#define WORD_REGISTRY_H

#include "vm.h"

/**
 * @brief Registers a single FORTH word in the virtual machine
 * @param vm Pointer to the virtual machine instance
 * @param name Name of the FORTH word to register
 * @param func Function pointer to the word's implementation
 */
void register_word(VM *vm, const char *name, word_func_t func);

/**
 * @brief Registers all standard FORTH-79 words in the virtual machine
 * @param vm Pointer to the virtual machine instance
 */
void register_forth79_words(VM * vm);

#endif /* WORD_REGISTRY_H */