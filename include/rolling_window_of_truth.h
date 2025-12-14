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

  rolling_window_of_truth.h - Execution history for deterministic metrics seeding

  The "Rolling Window of Truth" captures actual execution sequences to:
  1. Seed initial metrics after POST (hot-words cache, pipelining context)
  2. Provide deterministic record of all word executions
  3. Enable reproducible, provable optimization decisions
  4. Form foundation for formal verification (Isabelle/HOL)

  Vision: Every decision is observable, traceable, and mathematically provable.

*/

#ifndef ROLLING_WINDOW_OF_TRUTH_H
#define ROLLING_WINDOW_OF_TRUTH_H

#include <stdint.h>
#include "vm.h"

/* ============================================================================
 * Rolling Window of Truth
 * ============================================================================
 *
 * Circular buffer of word IDs representing recent execution sequence.
 * Used to:
 * 1. Seed metrics after POST completes
 * 2. Provide execution context for pipelining decisions
 * 3. Enable reproducibility and formal verification
 *
 * NOTE: Structure is defined in vm.h to avoid circular includes.
 * See vm.h for structure definition and ROLLING_WINDOW_SIZE constant.
 */

/* ============================================================================
 * Rolling Window API
 * ============================================================================
 */

/**
 * Initialize rolling window of truth.
 * Called during VM initialization.
 *
 * @param window Pointer to uninitialized RollingWindowOfTruth
 * @return 0 on success, -1 on malloc failure
 */
int rolling_window_init(RollingWindowOfTruth * window);

/**
 * Record a word execution in the rolling window.
 * Called after each word executes (in inner interpreter).
 *
 * THREAD SAFETY: This function is NOT thread-safe. Caller MUST hold
 * vm->tuning_lock when calling from multiple threads (main VM + heartbeat).
 *
 * Called from:
 * - Main VM thread (inner interpreter loop, src/vm.c:1162)
 * - Heartbeat worker thread (through rolling_window_service, src/vm.c:743)
 *
 * MUST ALWAYS be protected by:
 *   sf_mutex_lock(&vm->tuning_lock);
 *   rolling_window_record_execution(&vm->rolling_window, word_id);
 *   sf_mutex_unlock(&vm->tuning_lock);
 *
 * Extremely lightweight (constant time):
 * - Write to circular buffer
 * - Increment counters
 * - 2-3 CPU cycles (plus lock overhead ~100 cycles uncontended)
 *
 * @param window Rolling window to update
 * @param word_id Word that just executed
 * @return 0 on success, -1 on error
 */
int rolling_window_record_execution(RollingWindowOfTruth* window, uint32_t word_id);

/**
 * Get recent execution sequence for context.
 * Returns pointer to last N words (or fewer if not enough executions yet).
 *
 * @param window Rolling window
 * @param depth How many previous words to return (1 to ROLLING_WINDOW_SIZE)
 * @param out_sequence Output array (caller allocates, must be >= depth)
 * @return Number of words written to out_sequence (may be < depth if cold start)
 */
uint32_t rolling_window_get_recent_sequence(const RollingWindowOfTruth* window,
                                            uint32_t depth,
                                            uint32_t* out_sequence);

/**
 * Get most frequent word in rolling window.
 * Useful for identifying hot words immediately after POST.
 *
 * @param window Rolling window
 * @param dict_size Total number of words in dictionary (for bounds check)
 * @return Word ID of most frequent execution, or 0 if window not warm
 */
uint32_t rolling_window_find_hottest_word(const RollingWindowOfTruth* window,
                                          uint32_t dict_size);

/**
 * Get transition frequency: how often word_b follows word_a in window.
 *
 * Useful for seeding pipelining context predictions.
 *
 * @param window Rolling window
 * @param word_a First word in sequence
 * @param word_b Second word in sequence
 * @return Count of (word_a → word_b) transitions in window
 */
uint64_t rolling_window_count_transition(const RollingWindowOfTruth* window,
                                         uint32_t word_a,
                                         uint32_t word_b);

/**
 * Check if window is warm (contains representative data).
 * Seeding functions should check this before using window data.
 *
 * @param window Rolling window
 * @return 1 if window is warm (>= ROLLING_WINDOW_SIZE executions), 0 otherwise
 */
int rolling_window_is_warm(const RollingWindowOfTruth* window);

/**
 * Get detailed statistics string for diagnostics.
 *
 * @param window Rolling window
 * @return Formatted string (caller must free)
 */
char* rolling_window_stats_string(const RollingWindowOfTruth* window);

/**
 * Reset rolling window (clear all data).
 *
 * @param window Rolling window to reset
 */
void rolling_window_reset(RollingWindowOfTruth * window);

/**
 * Free all memory allocated for rolling window.
 * Called during VM cleanup.
 *
 * @param window Rolling window to clean up
 */
void rolling_window_cleanup(RollingWindowOfTruth * window);

/* ============================================================================
 * Bootstrap Data Analysis API
 * ============================================================================
 *
 * Used to analyze rolling window data and determine statistically significant
 * window sizes for cold-start systems (un-physics VM).
 */

/**
 * Export full execution history for external analysis.
 * Used by bootstrap analyzer to determine optimal window size.
 *
 * Returns the complete circular buffer linearized in chronological order.
 * Caller allocates output buffer; function fills it with execution sequence.
 *
 * @param window Rolling window to export
 * @param out_sequence Output buffer (caller allocates, must be >= total_executions)
 * @param max_count Maximum entries to write
 * @return Number of word IDs written to out_sequence
 */
uint64_t rolling_window_export_execution_history(const RollingWindowOfTruth* window,
                                                  uint32_t* out_sequence,
                                                  uint64_t max_count);

/**
 * Analyze pattern capture rate for a given window size.
 * Measures what percentage of unique transitions are captured.
 *
 * Algorithm:
 * 1. Scan execution history with sliding window of given size
 * 2. Count unique (context, next_word) patterns observed
 * 3. Return: (patterns_captured / total_unique_patterns) * 100
 *
 * Used for offline analysis to determine reasonable starting window size.
 *
 * @param window Rolling window with execution data
 * @param test_window_size Window size to test (e.g., 256, 512, 1024)
 * @return Pattern capture rate (0-100%)
 */
double rolling_window_pattern_capture_rate(const RollingWindowOfTruth* window,
                                            uint32_t test_window_size);

/* ============================================================================
 * Adaptive Window Sizing API
 * ============================================================================
 *
 * Automatic continuous self-tuning of window size during execution.
 * Called automatically during normal operation - no user intervention needed.
 */

/**
 * Measure pattern diversity in rolling window.
 * Counts unique adjacent transitions (word_a → word_b) to indicate workload variability.
 *
 * Used by dictionary heat optimization to decide lookup strategy:
 * - High diversity (>70) = use heat-aware lookup (patterns are varied)
 * - Low diversity (<30) = use naive lookup (patterns are stable)
 *
 * @param window Rolling window to analyze
 * @return Count of unique transitions in window
 */
uint64_t rolling_window_measure_diversity(const RollingWindowOfTruth* window);

/**
 * Check and apply adaptive window shrinking.
 * Called periodically by rolling_window_record_execution.
 * Measures pattern diversity and shrinks window if diminishing returns detected.
 *
 * This function is called automatically - users never invoke it directly.
 * It runs every 256 executions after window becomes warm.
 *
 * @param window Rolling window (modified in-place if shrinking occurs)
 */
void rolling_window_check_adaptive_shrink(RollingWindowOfTruth* window);
void rolling_window_service(RollingWindowOfTruth* window);

#endif /* ROLLING_WINDOW_OF_TRUTH_H */
