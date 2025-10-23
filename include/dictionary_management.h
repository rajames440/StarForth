/*
                                  ***   StarForth   ***

  dictionary_management.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:55:24.929-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/include/dictionary_management.h
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

#endif