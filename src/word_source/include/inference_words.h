/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/**
 * inference_words.h — Module 26: SSM inference engine + Jacquard FORTH words
 */

#ifndef INFERENCE_WORDS_H
#define INFERENCE_WORDS_H

#include "vm.h"

/**
 * Register all inference engine / Jacquard FORTH words with the VM.
 * Module 26 in register_forth79_words().
 */
void register_inference_words(VM *vm);

#endif /* INFERENCE_WORDS_H */
