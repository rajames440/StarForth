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

#ifndef DICTIONARY_MANAGEMENT_H
#define DICTIONARY_MANAGEMENT_H

#include "vm.h"  /* For VM type and word_func_t */

/**
 * @brief Initialize the Forth dictionary
 *
 * @param vm Pointer to the virtual machine instance
 *
 * Initializes all dictionary structures and prepares them for use.
 * Must be called before any other dictionary operations.
 */
void dictionary_init(VM * vm);

/**
 * @brief Register a new word in the dictionary
 *
 * @param vm Pointer to the virtual machine instance
 * @param name Name of the word to register
 * @param func Function pointer to the word's implementation
 *
 * Adds a new word to the dictionary with the specified name and implementation.
 */
void dictionary_register(VM *vm, const char *name, word_func_t func);

/**
 * @brief Look up a word in the dictionary by name
 *
 * @param vm Pointer to the virtual machine instance
 * @param name Name of the word to look up
 * @return word_func_t Function pointer to the word's implementation or NULL if not found
 */
word_func_t dictionary_lookup(VM *vm, const char *name);

/**
 * @brief Clear the entire dictionary
 *
 * @param vm Pointer to the virtual machine instance
 *
 * Removes all words from the dictionary and frees associated memory.
 * This is an optional operation that may be used during cleanup.
 */
void dictionary_clear(VM * vm);

typedef struct vm_last_word_record {
    const DictEntry *entry;
    const void *func;
    char name[WORD_NAME_MAX + 1];
} vm_last_word_record_t;

const vm_last_word_record_t *vm_dictionary_last_word_record(void);
void vm_dictionary_log_last_word(struct VM *vm, const char *tag);

#endif
