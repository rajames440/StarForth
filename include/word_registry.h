/*

                                 ***   StarForth   ***
  word_registry.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


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