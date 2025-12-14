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

  dictionary_heat_diagnostic_words.c - FORTH Interface for Dictionary Heat Optimization

  Introspection words to observe and control heat-aware dictionary lookup behavior.
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "vm.h"
#include "word_registry.h"
#include "platform_time.h"
#include "dictionary_heat_optimization.h"

/* ============================================================================
 * HEAT-PERCENTILES ( -- 25th 50th 75th )
 *
 * Return current heat percentile thresholds used in heat-aware lookup
 * ============================================================================
 */
void forth_HEAT_PERCENTILES(VM *vm) {
    if (vm->error) return;

    if (vm->dsp + 3 > STACK_SIZE) {
        vm->error = 1;
        return;
    }

    vm->data_stack[vm->dsp++] = vm->heat_threshold_25th;
    vm->data_stack[vm->dsp++] = vm->heat_threshold_50th;
    vm->data_stack[vm->dsp++] = vm->heat_threshold_75th;
}

/* ============================================================================
 * LOOKUP-STRATEGY@ ( -- strategy )
 *
 * Return current lookup strategy (0=naive, 1=heat-aware)
 * ============================================================================
 */
void forth_LOOKUP_STRATEGY_FETCH(VM *vm) {
    if (vm->error) return;

    if (vm->dsp >= STACK_SIZE) {
        vm->error = 1;
        return;
    }

    vm->data_stack[vm->dsp++] = (cell_t)vm->lookup_strategy;
}

/* ============================================================================
 * LOOKUP-STRATEGY! ( strategy -- )
 *
 * Force lookup strategy (0=naive, 1=heat-aware)
 * ============================================================================
 */
void forth_LOOKUP_STRATEGY_STORE(VM *vm) {
    if (vm->error) return;

    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t strategy = vm->data_stack[--vm->dsp];
    if (strategy == 0 || strategy == 1) {
        vm->lookup_strategy = (int)strategy;
    }
}

/* ============================================================================
 * REORG-BUCKETS ( -- )
 *
 * Force immediate bucket reorganization by heat
 * ============================================================================
 */
void forth_REORG_BUCKETS(VM *vm) {
    if (vm->error) return;

    dict_reorganize_buckets_by_heat(vm);
    dict_update_heat_percentiles(vm);
}

/* ============================================================================
 * SHOW-HEAT-OPTIMIZATION ( -- )
 *
 * Display current heat optimization status and strategy
 * ============================================================================
 */
void forth_SHOW_HEAT_OPTIMIZATION(VM *vm) {
    if (vm->error) return;

    printf("\n=== Dictionary Heat Optimization Status ===\n");

    printf("Lookup Strategy: ");
    if (vm->lookup_strategy == 0) {
        printf("NAIVE (newest-first search)\n");
    } else if (vm->lookup_strategy == 1) {
        printf("HEAT-AWARE (hot words first)\n");
    } else {
        printf("UNKNOWN (%d)\n", vm->lookup_strategy);
    }

    printf("Heat Percentile Thresholds:\n");
    printf("  25th percentile: %ld\n", vm->heat_threshold_25th);
    printf("  50th percentile: %ld\n", vm->heat_threshold_50th);
    printf("  75th percentile: %ld\n", vm->heat_threshold_75th);

    printf("\nHeat Zones:\n");
    printf("  TOP 25%%   (hot)   : >= %ld heat\n", vm->heat_threshold_75th);
    printf("  MIDDLE 50%% (warm)  : %ld - %ld heat\n",
           vm->heat_threshold_25th, vm->heat_threshold_75th);
    printf("  BOTTOM 25%% (cool)  : < %ld heat\n", vm->heat_threshold_25th);

    printf("\n");
}

/* ============================================================================
 * COMPARE-LOOKUPS ( iterations -- )
 *
 * Quick benchmark: compare naive vs heat-aware lookup performance
 * Measures time to find frequently used words
 * ============================================================================
 */
void forth_COMPARE_LOOKUPS(VM *vm) {
    if (vm->error) return;

    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t iterations = vm->data_stack[--vm->dsp];
    if (iterations <= 0) {
        printf("Need positive iteration count\n");
        return;
    }

    printf("\n=== Dictionary Lookup Comparison ===\n");
    printf("Iterations: %ld\n", iterations);

    /* Common test words */
    const char *test_words[] = {"DUP", "DROP", "SWAP", "@", "!", "EMIT", NULL};

    /* Strategy 1: Naive (current strategy) */
    printf("\nNaive (newest-first) lookup:\n");
    int orig_strategy = vm->lookup_strategy;
    vm->lookup_strategy = 0;

    uint64_t start_ns = sf_monotonic_ns();
    for (cell_t i = 0; i < iterations; i++) {
        for (int j = 0; test_words[j]; j++) {
            vm_find_word(vm, test_words[j], strlen(test_words[j]));
        }
    }
    uint64_t naive_ns = sf_monotonic_ns() - start_ns;

    /* Strategy 2: Heat-aware */
    printf("Heat-aware lookup:\n");
    dict_reorganize_buckets_by_heat(vm);
    vm->lookup_strategy = 1;

    start_ns = sf_monotonic_ns();
    for (cell_t i = 0; i < iterations; i++) {
        for (int j = 0; test_words[j]; j++) {
            vm_find_word(vm, test_words[j], strlen(test_words[j]));
        }
    }
    uint64_t heat_ns = sf_monotonic_ns() - start_ns;

    /* Restore original strategy */
    vm->lookup_strategy = orig_strategy;

    /* Report results_run_01_2025_12_08 */
    printf("\n=== Benchmark Results ===\n");
    printf("Naive time:      %.3f ms\n", (double)naive_ns / 1000000.0);
    printf("Heat-aware time: %.3f ms\n", (double)heat_ns / 1000000.0);

    if (heat_ns > 0) {
        double speedup = (double)naive_ns / (double)heat_ns;
        if (speedup > 1.0) {
            printf("Speedup: %.2f%% faster\n", (speedup - 1.0) * 100.0);
        } else {
            printf("Slowdown: %.2f%% slower\n", (1.0 - speedup) * 100.0);
        }
    }

    printf("\n");
}

/* ============================================================================
 * Word Registration
 * ============================================================================
 */
void register_dictionary_heat_diagnostic_words(VM *vm) {
    register_word(vm, "HEAT-PERCENTILES", forth_HEAT_PERCENTILES);
    register_word(vm, "LOOKUP-STRATEGY@", forth_LOOKUP_STRATEGY_FETCH);
    register_word(vm, "LOOKUP-STRATEGY!", forth_LOOKUP_STRATEGY_STORE);
    register_word(vm, "REORG-BUCKETS", forth_REORG_BUCKETS);
    register_word(vm, "SHOW-HEAT-OPTIMIZATION", forth_SHOW_HEAT_OPTIMIZATION);
    register_word(vm, "COMPARE-LOOKUPS", forth_COMPARE_LOOKUPS);
}

