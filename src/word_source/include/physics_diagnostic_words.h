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

/*
 * physics_diagnostic_words.h
 *
 * Interactive FORTH words for live physics feedback loop demonstration.
 */

#ifndef PHYSICS_DIAGNOSTIC_WORDS_H
#define PHYSICS_DIAGNOSTIC_WORDS_H

#include "../../include/vm.h"

/**
 * Display detailed physics metrics for the most recently executed word
 */
void forth_PHYSICS_WORD_METRICS(VM *vm);

/**
 * Calculate and show knob adjustments based on current thermal metrics
 */
void forth_PHYSICS_CALC_KNOBS(VM *vm);

/**
 * Execute a word repeatedly (burn) and show thermal feedback
 */
void forth_PHYSICS_BURN(VM *vm);

/**
 * Display the complete feedback loop: inputs → math → knobs → effect
 */
void forth_PHYSICS_SHOW_FEEDBACK(VM *vm);

/**
 * Register all physics diagnostic words with the VM
 */
void register_physics_diagnostic_words(VM *vm);

#endif /* PHYSICS_DIAGNOSTIC_WORDS_H */