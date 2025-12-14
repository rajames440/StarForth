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
                                  ***   StarForth   ***

  dictionary_heat_optimization.h - Phase 2: Heat-Aware Dictionary Lookup

  Inference-driven optimizations for dictionary lookup based on execution heat
  patterns from the Rolling Window of Truth:

  1. Heat percentile thresholds (25th, 50th, 75th)
  2. Bucket reorganization by co-execution patterns
  3. Heat-aware lookup strategy (search hot words first)
*/

#ifndef DICTIONARY_HEAT_OPTIMIZATION_H
#define DICTIONARY_HEAT_OPTIMIZATION_H

#include "vm.h"

/**
 * @brief Calculate heat percentile thresholds for all words in dictionary
 *
 * Computes 25th, 50th, 75th percentiles of execution_heat across all
 * dictionary entries. Used to partition lookup search order.
 */
void dict_update_heat_percentiles(VM *vm);

/**
 * @brief Reorganize dictionary buckets by inference-driven priorities
 *
 * Uses rolling window co-execution patterns to reorder words within buckets.
 * Hot/frequently co-executing words are placed first for faster lookup.
 */
void dict_reorganize_buckets_by_heat(VM *vm);

/**
 * @brief Find word with heat-aware search strategy
 *
 * Instead of newest-first, searches buckets in order:
 * 1. Top 25% by heat (likely hits)
 * 2. Middle 50% by heat
 * 3. Bottom 25% (rarely executed)
 */
DictEntry *dict_find_word_heat_aware(VM *vm, const char *name, size_t len);

/**
 * @brief Periodically reoptimize dictionary based on latest heat patterns
 *
 * Called every ~1 second or every N executions to adapt lookup strategy
 * to current workload patterns from rolling window.
 */
void dict_adaptive_optimization_pass(VM *vm);

#endif /* DICTIONARY_HEAT_OPTIMIZATION_H */

