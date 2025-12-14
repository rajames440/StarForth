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

  physics_pipelining_metrics.h - Word transition tracking for speculative execution

  Phase 1: Instrumentation for pipelining/speculative execution

  This module tracks word-to-word transitions (frequency of which words follow
  which other words) to support speculative prefetching and pipelining decisions.

  Terminology:
  - transition_heat[i]: How many times word[i] has been executed immediately
                        after this word (forms prediction basis)
  - total_transitions: Sum of all transition_heat entries
  - prefetch_*: Metrics for speculative execution success/failure

*/

#ifndef PHYSICS_PIPELINING_METRICS_H
#define PHYSICS_PIPELINING_METRICS_H

#include <stdint.h>

/* ============================================================================
 * Pipelining Feature Switch (disabled by default during Phase 1)
 * ============================================================================
 */
#ifndef ENABLE_PIPELINING
#define ENABLE_PIPELINING 0
#endif

/* ============================================================================
 * Tuning Knobs (adjustable at compile-time, future: runtime)
 * ============================================================================
 */

/**
 * SPECULATION_THRESHOLD_Q48: Minimum probability (Q48.16) to speculate on
 *                            a word transition.
 * Range: 0.10 (very aggressive) to 0.95 (very conservative)
 * Default: 0.50 (50% confidence required)
 * Q48.16 encoding: 0.50 = 0x8000LL (1LL << 15)
 */
#define SPECULATION_THRESHOLD_Q48 (0x8000LL)  /* 0.50 in Q48.16 */

/**
 * SPECULATION_DEPTH: How many words ahead to prefetch speculatively.
 * Range: 1 (next word only) to 4 (very aggressive)
 * Default: 1 (prefetch immediate successor)
 */
#define SPECULATION_DEPTH 1

/**
 * MIN_SAMPLES_FOR_SPECULATION: Minimum number of transitions observed
 *                              before making speculation decisions.
 * Range: 1 (immediate) to 100 (very conservative)
 * Default: 10 (wait for 10 transitions to gather signal)
 */
#define MIN_SAMPLES_FOR_SPECULATION 10

/**
 * MISPREDICTION_COST_Q48: Estimated cost (in nanoseconds, Q48.16 format)
 *                         of recovering from wrong speculation.
 * This is calibrated to the actual recovery time on target hardware.
 * Range: 0 to 100 ns
 * Default: 25 ns (typical cache miss penalty)
 * Q48.16 encoding: 25 ns = 25 << 16 = 0x190000LL
 */
#define MISPREDICTION_COST_Q48 (25LL << 16)  /* 25 ns in Q48.16 */

/**
 * MINIMUM_PREFETCH_ROI: Minimum expected Return-On-Investment ratio
 *                       for speculation to be worthwhile.
 * Speculation succeeds only if:
 *   (prefetch_latency_saved / total_attempts) > MINIMUM_PREFETCH_ROI
 * Range: 1.0 (always speculate) to 2.0 (very selective)
 * Default: 1.10 (10% expected improvement threshold)
 * Q48.16 encoding: 1.10 = 1.10 * (1 << 16) = 0x11999AL
 */
#define MINIMUM_PREFETCH_ROI (0x11999AL)  /* 1.10 in Q48.16 */

/**
 * TRANSITION_WINDOW_SIZE: Knob #6 - Execution context depth for prediction
 *
 * How many previous words to remember when predicting next word.
 * Larger window captures deeper patterns but uses more memory.
 *
 * Window=1: Only immediate predecessor (A→B predictions)
 * Window=2: Previous 2 words (A,B→C predictions) - DEFAULT, good balance
 * Window=4: Previous 4 words (captures multi-level patterns)
 * Window=8: Very deep (expensive, rarely needed)
 *
 * Binary Chop Search Space (Phase 2):
 *   Iteration 1: Try window=2 (baseline)
 *   Iteration 2: Try window=1 and window=4
 *   Iteration 3: Narrow to optimal (1, 2, or 4)
 *   Converge: Pick size with highest prediction accuracy
 *
 * Range: 1 to 8
 * Default: 2 (empirical sweet spot: captures context without excessive memory)
 * Trade-off: Pattern depth vs. memory usage & complexity
 *
 * Tuning: make TRANSITION_WINDOW_SIZE=1 (or 2, 4, 8)
 */
#ifndef TRANSITION_WINDOW_SIZE
#define TRANSITION_WINDOW_SIZE 2
#endif

/* ============================================================================
 * Word Transition Metrics
 * ============================================================================
 *
 * Allocated per dictionary entry to track which words follow this word.
 * This is the basis for speculative prefetching decisions.
 */

typedef struct WordTransitionMetrics {
    /**
     * transition_heat[i]: Number of times word with index i has been
     *                     executed immediately after this word.
     *
     * Dynamically allocated array of size DICTIONARY_SIZE.
     * Initialized to NULL; allocated on first use to save memory
     * for words that are never executed.
     */
    uint64_t *transition_heat;

    /** Total number of transitions observed from this word */
    uint64_t total_transitions;

    /** Number of speculative prefetch attempts made for this word */
    uint64_t prefetch_attempts;

    /** Number of successful prefetch predictions (hits) */
    uint64_t prefetch_hits;

    /** Number of failed prefetch predictions (misses) */
    uint64_t prefetch_misses;

    /**
     * Total latency saved through successful prefetching (Q48.16 nanoseconds).
     * Positive value indicates net benefit from speculation.
     */
    int64_t prefetch_latency_saved_q48;

    /**
     * Total latency cost from failed prefetch predictions (Q48.16 nanoseconds).
     * Negative value (cost) to be subtracted from savings.
     */
    int64_t misprediction_cost_q48;

    /**
     * Cached probability of most likely next word (for quick access).
     * Updated periodically to avoid recomputation.
     * Q48.16 format (0.0 to 1.0 == 0LL to 1LL<<16)
     */
    int64_t max_transition_probability_q48;

    /**
     * Index (word id) of most likely next word.
     * Used for fast prefetch without scanning transition_heat array.
     */
    uint32_t most_likely_next_word_id;

    /* ========== Context-Aware Transitions (Phase 1 Extension) ========== */

    /**
     * Circular buffer of previous word IDs for context tracking.
     * Size = TRANSITION_WINDOW_SIZE (default 2)
     * Used to build multi-word context patterns (e.g., A,B→C)
     *
     * Example with WINDOW=2:
     *   context_window[0] = older word (2 steps back)
     *   context_window[1] = recent word (1 step back)
     *   Next word completes the pattern: (context_window[0], context_window[1]) → next
     */
    uint32_t *context_window;

    /**
     * Current position in context_window circular buffer.
     * Points to slot where next word ID will be stored.
     */
    uint32_t context_window_pos;

    /**
     * Sparse hash table for context-based transitions.
     * Maps: hash(context_window) → counts_array[DICTIONARY_SIZE]
     *
     * Phase 1: Collect only (simple reference implementation)
     * Phase 2: Analyze prediction accuracy by context depth
     * Phase 3: Use for adaptive prefetching decisions
     *
     * Opaque pointer - implementation is in transition_metrics.c
     */
    void *context_transitions;

    /**
     * Total context-based transitions observed.
     * Counter for Phase 2 analysis (accuracy measurement)
     */
    uint64_t total_context_transitions;

    /**
     * Actual window size for this word's metrics.
     * May differ from TRANSITION_WINDOW_SIZE if memory allocation failed.
     * Phase 2 uses this to validate context data reliability.
     */
    uint32_t actual_window_size;

} WordTransitionMetrics;

/* ============================================================================
 * Transition Metrics API
 * ============================================================================
 */

/**
 * Initialize word transition metrics for a dictionary entry.
 * Called once per word during dictionary entry creation.
 *
 * @param metrics Pointer to uninitialized WordTransitionMetrics
 * @return 0 on success, -1 on memory allocation failure
 */
int transition_metrics_init(WordTransitionMetrics *metrics);

/**
 * Record a word transition (word_from -> word_to).
 * Called in the inner interpreter after each word execution.
 *
 * @param metrics Metrics for the current word
 * @param next_word_id Index of word that executed next
 * @param dict_size Total number of words in dictionary (for array bounds)
 * @return 0 on success, -1 on invalid word_id
 */
int transition_metrics_record(WordTransitionMetrics *metrics, uint32_t next_word_id, uint32_t dict_size);

/**
 * Get the probability (Q48.16 format) of a specific word following this word.
 *
 * Returns: (transition_heat[target] / total_transitions) in Q48.16 format
 *          or 0 if total_transitions == 0
 *
 * @param metrics Metrics for the current word
 * @param target_word_id Index of potential next word
 * @return Probability in Q48.16 format (0.0 to 1.0)
 */
int64_t transition_metrics_get_probability_q48(const WordTransitionMetrics *metrics, uint32_t target_word_id);

/**
 * Update cached most-likely-next-word information.
 * Should be called periodically (e.g., after every 100 transitions)
 * to avoid stale cache.
 *
 * @param metrics Metrics for the current word
 * @param dict_size Total number of words in dictionary
 */
void transition_metrics_update_cache(WordTransitionMetrics *metrics, uint32_t dict_size);

/**
 * Record a successful prefetch prediction.
 * Called when speculation correctly predicted the next word.
 *
 * @param metrics Metrics for the current word
 * @param latency_saved_ns Nanoseconds saved (will be Q48.16 encoded)
 */
void transition_metrics_record_prefetch_hit(WordTransitionMetrics *metrics, int64_t latency_saved_ns);

/**
 * Record a failed prefetch prediction.
 * Called when speculation incorrectly predicted the next word.
 *
 * @param metrics Metrics for the current word
 * @param recovery_cost_ns Nanoseconds lost to misprediction recovery
 */
void transition_metrics_record_prefetch_miss(WordTransitionMetrics *metrics, int64_t recovery_cost_ns);

/**
 * Check if speculation should be attempted for a given target word.
 * Applies threshold logic and ROI analysis.
 *
 * Returns: 1 if speculation should be attempted, 0 otherwise
 *
 * @param metrics Metrics for the current word
 * @param target_word_id Index of potential next word
 * @return 1 if probability > SPECULATION_THRESHOLD and enough samples, 0 otherwise
 */
int transition_metrics_should_speculate(const WordTransitionMetrics *metrics, uint32_t target_word_id);

/**
 * Get detailed statistics string for diagnostics.
 * Caller must free the returned string.
 *
 * @param metrics Metrics to report on
 * @return Formatted statistics string (must be freed by caller)
 */
char *transition_metrics_stats_string(const WordTransitionMetrics *metrics);

/**
 * Reset all transition metrics (clear counts, free transition_heat array).
 * Called on VM reset or explicit metrics reset.
 *
 * @param metrics Metrics to reset
 */
void transition_metrics_reset(WordTransitionMetrics *metrics);

/**
 * Free all allocated memory for transition metrics.
 * Called during dictionary cleanup.
 *
 * @param metrics Metrics to clean up
 */
void transition_metrics_cleanup(WordTransitionMetrics *metrics);

/* ============================================================================
 * Context-Aware Transition API (Phase 1 Extension)
 * ============================================================================
 *
 * These functions handle multi-word pattern tracking for Phase 2 analysis.
 * Phase 1: Collect context window data (this session)
 * Phase 2: Analyze accuracy by context depth (next session)
 * Phase 3: Use for adaptive prediction (future)
 */

/**
 * Record a context-based transition (multi-word pattern).
 * Called after each word execution with the execution window.
 *
 * Example with WINDOW=2:
 *   context[0] = word_A, context[1] = word_B, next_word = word_C
 *   Records: (A,B) → C transition in sparse hash table
 *
 * Phase 1: Just collects data, no analysis
 * Phase 2: Will measure: "Do patterns with window=2 predict better than window=1?"
 *
 * @param metrics Metrics for the current word
 * @param context Array of word IDs forming the context (size = window_size)
 * @param window_size Length of context array
 * @param next_word_id Word that follows the context
 * @param dict_size Total number of words in dictionary
 * @return 0 on success, -1 on error
 */
int transition_metrics_record_context(WordTransitionMetrics *metrics,
                                       const uint32_t *context,
                                       uint32_t window_size,
                                       uint32_t next_word_id,
                                       uint32_t dict_size);

/**
 * Update the execution context window with a new word.
 * Called after each word execution to slide the window forward.
 *
 * Example with WINDOW=2:
 *   Before: [A, B] (two previous words)
 *   After calling with C: [B, C] (shift left, add C on right)
 *
 * @param metrics Metrics to update
 * @param word_id New word ID to add to window
 * @return 0 on success, -1 if context tracking not initialized
 */
int transition_metrics_update_context_window(WordTransitionMetrics *metrics, uint32_t word_id);

/**
 * Get context-based prediction accuracy statistics (Phase 2).
 * Compares prediction success rates across different window sizes.
 *
 * Phase 2 will use this to run binary chop tuning:
 *   Measure accuracy(window=1), accuracy(window=2), accuracy(window=4)
 *   Binary search converges to optimal window size
 *
 * @param metrics Metrics to analyze
 * @return Formatted string with accuracy statistics (caller must free)
 */
char *transition_metrics_context_accuracy_string(const WordTransitionMetrics *metrics);

/**
 * Get recommended window size based on collected data.
 * Phase 2 will call this as part of binary chop tuning algorithm.
 *
 * Algorithm:
 *   1. Collect data with current window size
 *   2. Measure prediction accuracy
 *   3. If accuracy improves, try larger window
 *   4. If accuracy declines, try smaller window
 *   5. Converge to optimal size
 *
 * @param metrics Metrics to analyze
 * @param current_window Current window size being tested
 * @param accuracy_at_current Measured accuracy with current window
 * @return Suggested next window size to try (1, 2, 4, or 8)
 */
uint32_t transition_metrics_binary_chop_suggest_window(const WordTransitionMetrics *metrics,
                                                       uint32_t current_window,
                                                       double accuracy_at_current);

#endif /* PHYSICS_PIPELINING_METRICS_H */
