/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/**
 * q48_words.h — Module 25: Q48.16 fixed-point FORTH words
 */

#ifndef Q48_WORDS_H
#define Q48_WORDS_H

#include "vm.h"

/**
 * Register all Q48.16 FORTH words with the VM.
 * Module 25 in register_forth79_words().
 */
void register_q48_words(VM *vm);

#endif /* Q48_WORDS_H */
