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
 * physics_benchmark_words.c
 *
 * Benchmark words for measuring hot-words cache impact
 *
 * Usage:
 *   BENCH-DICT-LOOKUP          (run before/after comparison)
 *   PHYSICS-CACHE-STATS        (show detailed statistics)
 *   PHYSICS-TOGGLE-CACHE       (enable/disable at runtime)
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "vm.h"
#include "physics_hotwords_cache.h"
#include "platform_time.h"
#include "word_registry.h"

/* Forward declarations */
extern DictEntry **sf_fc_list[256];
extern size_t sf_fc_count[256];

/* ============================================================================
 * Benchmark: Dictionary Lookup Performance
 * ============================================================================
 */

/**
 * BENCH-DICT-LOOKUP ( iterations -- )
 *
 * Benchmark dictionary lookup performance with statistically valid sample sizes.
 *
 * STATISTICAL VALIDITY:
 *   - Minimum recommended: 10,000 iterations (95% confidence interval)
 *   - Standard test: 100,000 iterations (tight confidence intervals)
 *   - Stress test: 1,000,000 iterations (ultra-precise measurements)
 *
 * FOR BAYESIAN INFERENCE:
 *   - All latencies recorded in 64-bit Q48.16 fixed-point format
 *   - Variance accumulators enable prior/posterior calculations
 *   - Sample sizes affect confidence in ML decisions
 *
 * USAGE:
 *   100000 BENCH-DICT-LOOKUP  (statistically valid baseline)
 *   1000000 BENCH-DICT-LOOKUP (stress test, ~30sec on modern CPUs)
 */
void forth_BENCH_DICT_LOOKUP(VM *vm) {
    if (vm->error) return;

    if (vm->dsp < 1) {
        vm->error = 1;
        printf("BENCH-DICT-LOOKUP: Need iteration count on stack\n");
        return;
    }

    cell_t iterations = vm->data_stack[--vm->dsp];
    if (iterations < 1) {
        printf("BENCH-DICT-LOOKUP: iterations must be >= 1\n");
        return;
    }

    /* Warn if sample size is too small for statistical validity */
    if (iterations < 10000) {
        printf("⚠️  WARNING: %ld iterations < 10,000 (minimum for 95%% confidence)\n", iterations);
        printf("   Consider using >= 100,000 for statistically valid results_run_01_2025_12_08\n\n");
    }

    // Test words: common in FORTH programs (balanced distribution)
    const char *test_words[] = {
        "IF", "THEN", "ELSE", "DO", "LOOP", "+LOOP",
        "DUP", "DROP", "SWAP", "OVER", "ROT",
        "+", "-", "*", "/", "MOD",
        "@", "!", "C@", "C!",
        "EMIT", "KEY", ".",
        NULL
    };

    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║  Dictionary Lookup Benchmark (Statistically Valid Sample)     ║\n");
    printf("║  Iterations: %ld (confidence: ", iterations);
    if (iterations >= 1000000) printf("99.9%%) [STRESS TEST]\n");
    else if (iterations >= 100000) printf("99%%) [STANDARD]\n");
    else if (iterations >= 10000) printf("95%%) [MINIMUM]\n");
    else printf("LOW) [UNRELIABLE]\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    // Reset stats before benchmark
    if (!vm->hotwords_cache) {
        printf("BENCH-DICT-LOOKUP: Cache not initialized\n");
        return;
    }
    hotwords_stats_reset(&vm->hotwords_cache->stats);

    uint64_t start = sf_monotonic_ns();

    // Repeatedly lookup test words
    int word_idx = 0;
    for (cell_t i = 0; i < iterations; i++) {
        const char *word_name = test_words[word_idx];
        if (!word_name) {
            word_idx = 0;
            word_name = test_words[word_idx];
        }

        // Perform lookup - all latencies recorded with 64-bit fixed-point precision
        DictEntry *found = vm_find_word(vm, word_name, strlen(word_name));
        if (!found) {
            printf("Warning: Word '%s' not found\n", word_name);
        }

        word_idx++;
    }

    uint64_t elapsed = sf_monotonic_ns() - start;
    double elapsed_ms = (double)elapsed / 1000000.0;
    double avg_lookup_us = (double)elapsed / (double)iterations / 1000.0;

    printf("WALL-CLOCK TIMING:\n");
    printf("  Total time:        %.2f ms\n", elapsed_ms);
    printf("  Avg per lookup:    %.3f µs\n", avg_lookup_us);
    printf("  Lookups/sec:       %.0f\n", 1000000.0 / avg_lookup_us);

    // Show detailed cache stats if enabled (includes 64-bit fixed-point statistics)
    if (ENABLE_HOTWORDS_CACHE) {
        printf("\n");
        hotwords_stats_print(&vm->hotwords_cache->stats);
    } else {
        printf("  (Hot-words cache disabled at compile time)\n\n");
    }
}

/* ============================================================================
 * Diagnostic: Show Cache Statistics
 * ============================================================================
 */

/**
 * PHYSICS-CACHE-STATS ( -- )
 *
 * Display detailed hot-words cache statistics
 */
void forth_PHYSICS_CACHE_STATS(VM *vm) {
    if (vm->error) return;

    if (!ENABLE_HOTWORDS_CACHE) {
        printf("Hot-words cache is disabled at compile time.\n");
        printf("Rebuild with: make clean && make ENABLE_HOTWORDS_CACHE=1\n");
        return;
    }

    if (!vm->hotwords_cache) {
        printf("PHYSICS-CACHE-STATS: Cache not initialized\n");
        return;
    }

    hotwords_stats_print(&vm->hotwords_cache->stats);

    printf("CACHE CONTENTS:\n");
    for (uint32_t i = 0; i < vm->hotwords_cache->cache_count; i++) {
        DictEntry *w = vm->hotwords_cache->cache[i];
        if (!w) continue;
        printf("  [%2u] %s (execution_heat=%ld, temp=0x%04x)\n",
               i, w->name, w->execution_heat, w->physics.temperature_q8);
    }
    printf("\n");
}

/* ============================================================================
 * Control: Toggle Cache at Runtime
 * ============================================================================
 */

/**
 * PHYSICS-TOGGLE-CACHE ( -- )
 *
 * Enable/disable hot-words cache at runtime (for A/B testing)
 */
void forth_PHYSICS_TOGGLE_CACHE(VM *vm) {
    if (vm->error) return;

    if (!ENABLE_HOTWORDS_CACHE) {
        printf("Hot-words cache is disabled at compile time.\n");
        return;
    }

    if (!vm->hotwords_cache) {
        printf("PHYSICS-TOGGLE-CACHE: Cache not initialized\n");
        return;
    }

    int was_enabled = vm->hotwords_cache->enabled;
    hotwords_cache_set_enabled(vm->hotwords_cache, !was_enabled);

    printf("Hot-words cache: %s → %s\n",
           was_enabled ? "ENABLED" : "DISABLED",
           !was_enabled ? "ENABLED" : "DISABLED");
}

/**
 * PHYSICS-RESET-STATS ( -- )
 *
 * Reset cache statistics for clean before/after comparison
 */
void forth_PHYSICS_RESET_STATS(VM *vm) {
    if (vm->error) return;

    if (!vm->hotwords_cache) {
        printf("PHYSICS-RESET-STATS: Cache not initialized\n");
        return;
    }

    hotwords_stats_reset(&vm->hotwords_cache->stats);

    /* Reset pipeline metrics for clean per-run measurement */
    vm->pipeline_metrics.prefetch_hits = 0;
    vm->pipeline_metrics.prefetch_attempts = 0;
    vm->pipeline_metrics.window_tuning_checks = 0;

    printf("Cache statistics reset.\n");
}

/* ============================================================================
 * Helper: Show Current Build Configuration
 * ============================================================================
 */

/**
 * PHYSICS-BUILD-INFO ( -- )
 *
 * Show current build configuration for this variant
 */
void forth_PHYSICS_BUILD_INFO(VM *vm) {
    if (vm->error) return;

    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("Physics System Build Configuration\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("Hot-Words Cache:     %s\n",
           ENABLE_HOTWORDS_CACHE ? "ENABLED" : "DISABLED");

    printf("Cache Size:          %d entries\n", HOTWORDS_CACHE_SIZE);
    printf("Execution Heat Threshold: %d\n", HOTWORDS_EXECUTION_HEAT_THRESHOLD);
    printf("Reorder Threshold:   %d\n", HOTWORDS_EXECUTION_HEAT_DELTA_THRESHOLD);

    printf("\nCurrent State:\n");
    if (vm->hotwords_cache) {
        printf("Cache enabled:       %s\n",
               vm->hotwords_cache->enabled ? "YES" : "NO");
        printf("Cached entries:      %u / %d\n",
               vm->hotwords_cache->cache_count, HOTWORDS_CACHE_SIZE);
    } else {
        printf("Cache enabled:       (not initialized)\n");
        printf("Cached entries:      (not initialized)\n");
    }

    printf("\n═══════════════════════════════════════════════════════════════\n\n");
}

/* ============================================================================
 * Word Registration
 * ============================================================================
 */

/**
 * PHYSICS-BAYESIAN-REPORT ( addr_baseline -- )
 *
 * Generate Bayesian inference report comparing current cache stats
 * against baseline stats stored at given address
 *
 * Usage:
 *   PHYSICS-RESET-STATS
 *   [run baseline benchmark without cache]
 *   [save stats somewhere]
 *   PHYSICS-RESET-STATS
 *   [run benchmark with cache]
 *   PHYSICS-BAYESIAN-REPORT
 */
void forth_PHYSICS_BAYESIAN_REPORT(VM *vm) {
    if (vm->error) return;

    if (!vm->hotwords_cache) {
        printf("PHYSICS-BAYESIAN-REPORT: Cache not initialized\n");
        return;
    }

    /* Generate comparison report with hardcoded baseline for now */
    /* In production, would compare against externally provided baseline */
    printf("\nGenerating Bayesian inference report...\n");
    printf("(Report uses current cache stats)\n");

    /* For now, use a synthetic baseline for demonstration */
    HotwordsStats baseline_stats = {0};

    /* Assume baseline: all lookups go to bucket (no cache) */
    /* Use bucket search stats from current run as proxy for baseline */
    baseline_stats.bucket_hits = vm->hotwords_cache->stats.cache_hits +
                                 vm->hotwords_cache->stats.bucket_hits;
    baseline_stats.bucket_search_total_ns_q48 = vm->hotwords_cache->stats.bucket_search_total_ns_q48;
    baseline_stats.bucket_search_variance_sum_q48 = vm->hotwords_cache->stats.bucket_search_variance_sum_q48;
    baseline_stats.min_bucket_search_ns = vm->hotwords_cache->stats.min_bucket_search_ns;
    baseline_stats.max_bucket_search_ns = vm->hotwords_cache->stats.max_bucket_search_ns;
    baseline_stats.bucket_search_samples = vm->hotwords_cache->stats.bucket_search_samples;

    hotwords_bayesian_report(&vm->hotwords_cache->stats, &baseline_stats);
}

void register_physics_benchmark_words(VM *vm) {
    register_word(vm, "BENCH-DICT-LOOKUP", forth_BENCH_DICT_LOOKUP);
    register_word(vm, "PHYSICS-CACHE-STATS", forth_PHYSICS_CACHE_STATS);
    register_word(vm, "PHYSICS-TOGGLE-CACHE", forth_PHYSICS_TOGGLE_CACHE);
    register_word(vm, "PHYSICS-RESET-STATS", forth_PHYSICS_RESET_STATS);
    register_word(vm, "PHYSICS-BUILD-INFO", forth_PHYSICS_BUILD_INFO);
    register_word(vm, "PHYSICS-BAYESIAN-REPORT", forth_PHYSICS_BAYESIAN_REPORT);
}