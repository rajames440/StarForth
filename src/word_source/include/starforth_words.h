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

#ifndef STARFORTH_WORDS_H
#define STARFORTH_WORDS_H

#include "vm.h"

/** @name StarForth Implementation Vocabulary Words */
///@{

/**
 * @brief Fetch entropy value from address
 * @param vm Pointer to VM instance
 * @stack ( addr -- n )
 */
void starforth_word_entropy_fetch(VM * vm);

/**
 * @brief Store entropy value to address
 * @param vm Pointer to VM instance
 * @stack ( n addr -- )
 */
void starforth_word_entropy_store(VM * vm);

/**
 * @brief Calculate word entropy
 * @param vm Pointer to VM instance
 * @stack ( -- )
 */
void starforth_word_word_entropy(VM * vm);

/**
 * @brief Reset entropy values
 * @param vm Pointer to VM instance
 * @stack ( -- )
 */
void starforth_word_reset_entropy(VM * vm);

/**
 * @brief Display top N words by entropy
 * @param vm Pointer to VM instance
 * @stack ( n -- )
 */
void starforth_word_top_words(VM * vm);

/**
 * @brief Shebang-style comment for init-4.4th metadata
 * @param vm Pointer to VM instance
 * @stack ( -- )
 */
void starforth_word_paren_dash(VM * vm);

/**
 * @brief Initialize system from init-4.4th
 * @param vm Pointer to VM instance
 * @stack ( -- )
 */
void starforth_word_init(VM * vm);

/**
 * @brief Set PRNG seed for reproducible random sequences
 * @param vm Pointer to VM instance
 * @stack ( n -- )
 */
void starforth_word_seed(VM * vm);

/**
 * @brief Generate bounded random number
 * @param vm Pointer to VM instance
 * @stack ( lo hi -- n )
 * @note Returns random value in inclusive range [lo, hi]
 */
void starforth_word_random(VM * vm);

/**
 * @brief Wait/sleep for specified milliseconds
 * @param vm Pointer to VM instance
 * @stack ( n -- )
 * @note General-purpose delay, also useful for VM cooling between benchmarks
 */
void starforth_word_wait(VM * vm);

///@}

/**
 * @brief Register all StarForth words with VM
 * @param vm Pointer to VM instance where words will be registered
 */
void register_starforth_words(VM * vm);

#endif /* STARFORTH_WORDS_H */