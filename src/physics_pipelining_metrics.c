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

  physics_pipelining_metrics.c - Word transition tracking implementation

  Tracks word-to-word transitions to support speculative execution/pipelining.
  Uses only fixed-point Q48.16 arithmetic (no floating-point).

*/

#include "physics_pipelining_metrics.h"
#include "vm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Q48.16 Fixed-Point Arithmetic Helpers
 * ============================================================================
 */

/**
 * @brief Convert a nanosecond count to Q48.16 fixed-point.
 *
 * Shifts @p ns left by 16 bits to produce a Q48.16 value where the integer
 * part represents nanoseconds (1.0 ns = 65536 in Q48.16). Used to convert
 * raw platform nanosecond timestamps into the common Q48.16 format before
 * accumulating into @c prefetch_latency_saved_q48 and
 * @c misprediction_cost_q48.
 *
 * @param ns  Integer nanosecond value to convert.
 * @return    @p ns in Q48.16 format (@p ns × 65536).
 */
static inline int64_t ns_to_q48(int64_t ns) {
    return ns << 16;
}

/**
 * @brief Convert a Q48.16 fixed-point value to integer nanoseconds (truncated).
 *
 * Shifts @p q48 right by 16 bits, discarding the fractional part. The result
 * is the integer nanosecond count represented by the Q48.16 value. Used in
 * @c transition_metrics_stats_string() to convert the accumulated
 * @c prefetch_latency_saved_q48 and @c misprediction_cost_q48 back to
 * human-readable nanosecond integers for the stats report string.
 *
 * @param q48  Value in Q48.16 format.
 * @return     Integer nanosecond equivalent (truncated, not rounded).
 */
static inline int64_t q48_to_ns(int64_t q48) {
    return q48 >> 16;
}

/**
 * @brief Divide a Q48.16 value by a @c uint64_t, returning Q48.16.
 *
 * Computes @p a_q48 / @p b using integer division. The result preserves the
 * Q48.16 scaling of @p a_q48 because @p b is dimensionless. Guards against
 * divide-by-zero by returning 0 when @p b == 0. Used by
 * @c transition_metrics_get_probability_q48() to compute the per-word
 * transition probability:
 * @code
 *   P(next = w) = (count_q48 << 16) / total_transitions
 * @endcode
 * where @c count_q48 = (uint64_t)count << 16 and @c total_transitions is the
 * denominator @p b.
 *
 * @param a_q48  Numerator in Q48.16 format.
 * @param b      Denominator as a plain @c uint64_t (dimensionless count).
 * @return       @p a_q48 / @p b in Q48.16; 0 if @p b == 0.
 */
static inline int64_t q48_div_u64(int64_t a_q48, uint64_t b) {
    if (b == 0) return 0;
    return a_q48 / (int64_t)b;
}

/**
 * @brief Multiply two Q48.16 values, returning a Q48.16 result.
 *
 * Computes @p a × @p b and right-shifts by 16 to cancel the extra scaling
 * factor introduced by multiplying two Q48.16 numbers:
 * @code
 *   (a × b) >> 16  →  Q48.16
 * @endcode
 * This is the standard fixed-point multiply for the same format on both
 * operands. Overflow is possible if both operands are large (the intermediate
 * product can reach 2^(48+48+16) = 2^112 in the worst case, which overflows
 * @c int64_t). In practice the operands are probabilities (0.0–1.0) or
 * latency fractions that keep the product well within int64 range.
 *
 * @param a  First Q48.16 operand.
 * @param b  Second Q48.16 operand.
 * @return   @p a × @p b in Q48.16.
 */
static inline int64_t q48_mul_q48(int64_t a, int64_t b) {
    return (a * b) >> 16;
}

/* ============================================================================
 * Transition Metrics Implementation
 * ============================================================================
 */

/**
 * @brief Initialise a @c WordTransitionMetrics structure to its empty, ready-to-record state.
 *
 * Sets all counter and accumulator fields to zero and performs the following
 * allocations:
 *
 * - @c metrics->transition_heat is left NULL — it is lazily allocated by the
 *   first call to @c transition_metrics_record() to avoid the @c dict_size
 *   parameter being required at construction time.
 *
 * - If @c TRANSITION_WINDOW_SIZE > 0, allocates @c metrics->context_window as
 *   a zero-filled array of @c TRANSITION_WINDOW_SIZE @c uint32_t word IDs and
 *   sets @c metrics->actual_window_size accordingly. If the allocation fails,
 *   @c actual_window_size is set to 0 and context tracking degrades gracefully
 *   (all context functions become no-ops).
 *
 * - @c metrics->context_transitions is left NULL — it is lazily allocated when
 *   Phase 2 context-pattern analysis is implemented.
 *
 * Returns 0 on success or −1 if @p metrics is NULL.
 *
 * @param metrics  The @c WordTransitionMetrics to initialise; must not be NULL.
 * @return         0 on success; −1 if @p metrics is NULL.
 */
int transition_metrics_init(WordTransitionMetrics *metrics) {
    if (!metrics) return -1;

    metrics->transition_heat = NULL;  /* Lazy allocation on first use */
    metrics->total_transitions = 0;
    metrics->prefetch_attempts = 0;
    metrics->prefetch_hits = 0;
    metrics->prefetch_misses = 0;
    metrics->prefetch_latency_saved_q48 = 0;
    metrics->misprediction_cost_q48 = 0;
    metrics->max_transition_probability_q48 = 0;
    metrics->most_likely_next_word_id = 0;

    /* Phase 1 Extension: Initialize context window (window size = 2 by default) */
    if (TRANSITION_WINDOW_SIZE > 0) {
        metrics->context_window = (uint32_t *)calloc(TRANSITION_WINDOW_SIZE, sizeof(uint32_t));
        if (metrics->context_window) {
            metrics->actual_window_size = TRANSITION_WINDOW_SIZE;
            metrics->context_window_pos = 0;
        } else {
            metrics->actual_window_size = 0;  /* Degraded: no context tracking */
        }
    } else {
        metrics->actual_window_size = 0;
    }

    metrics->context_transitions = NULL;  /* Lazy allocation on first context transition */
    metrics->total_context_transitions = 0;

    return 0;
}

/**
 * @brief Record one word-to-word transition in the transition heat table.
 *
 * Called from the interpreter loop each time a word executes and there is a
 * known previous word. Increments @c metrics->transition_heat[next_word_id]
 * by one and @c metrics->total_transitions by one.
 *
 * **Lazy allocation:** The @c transition_heat array is not allocated at
 * @c transition_metrics_init() time; on the first call when
 * @c metrics->transition_heat == NULL, it is allocated as a zero-filled
 * array of @p dict_size @c uint64_t counters. If the allocation fails −1 is
 * returned and the transition is silently dropped.
 *
 * If @p next_word_id ≥ @p dict_size the call is rejected with −1 to prevent
 * an out-of-bounds write into @c transition_heat (which was allocated with
 * size @p dict_size).
 *
 * The accumulated @c transition_heat[] table is consumed by
 * @c transition_metrics_get_probability_q48() and
 * @c transition_metrics_update_cache() to determine the most-likely next word
 * for speculative prefetch.
 *
 * @param metrics       The metrics structure to update.
 * @param next_word_id  Zero-based word ID of the word that followed the
 *                      current word; must be < @p dict_size.
 * @param dict_size     Current dictionary size, used both as the bounds
 *                      check for @p next_word_id and as the allocation size
 *                      for the lazy-allocated @c transition_heat array.
 * @return              0 on success; −1 on NULL @p metrics, out-of-bounds
 *                      @p next_word_id, or allocation failure.
 */
int transition_metrics_record(WordTransitionMetrics *metrics, uint32_t next_word_id, uint32_t dict_size) {
    if (!metrics || next_word_id >= dict_size) {
        return -1;
    }

    /* Lazy allocation: only allocate transition_heat array when first transition occurs */
    if (!metrics->transition_heat) {
        metrics->transition_heat = (uint64_t *)calloc(dict_size, sizeof(uint64_t));
        if (!metrics->transition_heat) {
            return -1;  /* Memory allocation failure */
        }
    }

    /* Record the transition */
    metrics->transition_heat[next_word_id]++;
    metrics->total_transitions++;

    return 0;
}

/**
 * @brief Return the transition probability to @p target_word_id in Q48.16 format.
 *
 * Computes P(next = @p target_word_id | current word) as a Q48.16 fixed-point
 * ratio:
 * @code
 *   P = (transition_heat[target_word_id] << 16) / total_transitions
 * @endcode
 * using @c q48_div_u64() to guard against divide-by-zero.
 *
 * Returns 0 if:
 * - @p metrics is NULL.
 * - @c metrics->total_transitions == 0 (no transitions recorded yet).
 * - @c metrics->transition_heat is NULL (lazy allocation has not occurred).
 *
 * The caller is responsible for ensuring @p target_word_id is within the
 * bounds of the allocated @c transition_heat array; this function does not
 * re-check bounds after verifying the NULL guard above (bounds were enforced
 * during @c transition_metrics_record()).
 *
 * Consumed by @c transition_metrics_update_cache() (scans all words to find
 * the maximum) and @c transition_metrics_should_speculate() (checks whether
 * a specific word's probability exceeds @c SPECULATION_THRESHOLD_Q48).
 *
 * @param metrics        The metrics structure to query.
 * @param target_word_id Zero-based word ID whose transition probability is
 *                       requested; must be within the dict_size used at record time.
 * @return               P(next = @p target_word_id) in Q48.16; 0 if unavailable.
 */
int64_t transition_metrics_get_probability_q48(const WordTransitionMetrics *metrics, uint32_t target_word_id) {
    if (!metrics || metrics->total_transitions == 0) {
        return 0;
    }

    /* Safety: Bounds check on target_word_id to prevent out-of-bounds read
     * Note: transition_heat is allocated with dict_size passed to transition_metrics_record()
     * Callers must ensure target_word_id < dict_size used during allocation */
    if (!metrics->transition_heat) {
        return 0;
    }

    uint64_t count = metrics->transition_heat[target_word_id];

    /* Probability = count / total_transitions, in Q48.16 format */
    /* = (count << 16) / total_transitions */
    int64_t count_q48 = (int64_t)count << 16;
    return q48_div_u64(count_q48, metrics->total_transitions);
}

/**
 * @brief Scan the transition heat table and cache the highest-probability next word.
 *
 * Iterates over all @p dict_size entries in @c metrics->transition_heat[],
 * calling @c transition_metrics_get_probability_q48() for each non-zero entry
 * and tracking the maximum. After the scan:
 * - @c metrics->max_transition_probability_q48 holds the peak probability (Q48.16).
 * - @c metrics->most_likely_next_word_id holds the word ID of that peak.
 *
 * These two cached fields allow @c transition_metrics_should_speculate() to
 * make a prefetch decision in O(1) without re-scanning the full table on every
 * word dispatch.
 *
 * If @p metrics, @c transition_heat, or @c total_transitions are not valid,
 * both fields are reset to 0 and the function returns immediately.
 *
 * Called after each @c transition_metrics_record() in the high-frequency
 * interpreter path, so the implementation must be fast. The full @p dict_size
 * scan runs in O(dict_size) and skips zero-count entries early.
 *
 * @param metrics    The metrics structure to update.
 * @param dict_size  Number of entries in @c metrics->transition_heat[];
 *                   must match the size used at allocation time.
 */
void transition_metrics_update_cache(WordTransitionMetrics *metrics, uint32_t dict_size) {
    if (!metrics || !metrics->transition_heat || metrics->total_transitions == 0) {
        metrics->max_transition_probability_q48 = 0;
        metrics->most_likely_next_word_id = 0;
        return;
    }

    int64_t max_prob_q48 = 0;
    uint32_t max_idx = 0;

    /* Find word with highest transition count */
    for (uint32_t i = 0; i < dict_size; i++) {
        if (metrics->transition_heat[i] > 0) {
            int64_t prob_q48 = transition_metrics_get_probability_q48(metrics, i);
            if (prob_q48 > max_prob_q48) {
                max_prob_q48 = prob_q48;
                max_idx = i;
            }
        }
    }

    metrics->max_transition_probability_q48 = max_prob_q48;
    metrics->most_likely_next_word_id = max_idx;
}

/**
 * @brief Record a successful speculative prefetch and the latency it saved.
 *
 * Increments @c metrics->prefetch_hits by one and accumulates @p latency_saved_ns
 * (converted to Q48.16 via @c ns_to_q48()) into
 * @c metrics->prefetch_latency_saved_q48. The accumulated total is reported by
 * @c transition_metrics_stats_string() as the aggregate latency benefit of
 * correct speculation.
 *
 * Called by the speculative-execution path when a prefetched word matches the
 * word that was actually executed next.
 *
 * @param metrics          The metrics structure to update; no-op if NULL.
 * @param latency_saved_ns The number of nanoseconds saved by avoiding a
 *                         full dictionary lookup on this particular hit.
 */
void transition_metrics_record_prefetch_hit(WordTransitionMetrics *metrics, int64_t latency_saved_ns) {
    if (!metrics) return;

    metrics->prefetch_hits++;
    metrics->prefetch_latency_saved_q48 += ns_to_q48(latency_saved_ns);
}

/**
 * @brief Record a failed speculative prefetch and its recovery overhead.
 *
 * Increments @c metrics->prefetch_misses by one and accumulates
 * @p recovery_cost_ns (converted to Q48.16 via @c ns_to_q48()) into
 * @c metrics->misprediction_cost_q48. The accumulated total is the aggregate
 * penalty paid for incorrect speculation and is reported by
 * @c transition_metrics_stats_string() as "Misprediction Cost".
 *
 * The net benefit of the pipelining subsystem is:
 * @code
 *   net_ns = prefetch_latency_saved_q48 - misprediction_cost_q48
 * @endcode
 * A positive net indicates that speculation is profitable overall.
 *
 * Called by the speculative-execution path when the prefetched word does not
 * match the word that was actually executed next (branch misprediction analogue).
 *
 * @param metrics           The metrics structure to update; no-op if NULL.
 * @param recovery_cost_ns  The number of nanoseconds spent unwinding the
 *                          incorrect speculation (cache flush, state restore).
 */
void transition_metrics_record_prefetch_miss(WordTransitionMetrics *metrics, int64_t recovery_cost_ns) {
    if (!metrics) return;

    metrics->prefetch_misses++;
    metrics->misprediction_cost_q48 += ns_to_q48(recovery_cost_ns);
}

/**
 * @brief Decide whether to speculatively prefetch @p target_word_id.
 *
 * Applies a two-gate policy before authorising speculation:
 *
 * 1. **Minimum sample gate:** @c metrics->total_transitions must be ≥
 *    @c MIN_SAMPLES_FOR_SPECULATION. Below this threshold the transition
 *    probabilities are statistically unreliable — speculation would be
 *    indistinguishable from guessing.
 *
 * 2. **Probability threshold gate:** the Q48.16 probability returned by
 *    @c transition_metrics_get_probability_q48(metrics, @p target_word_id)
 *    must be ≥ @c SPECULATION_THRESHOLD_Q48. Words with very low conditional
 *    probability of following the current word are not worth prefetching.
 *
 * Both gates must pass for the function to return 1. A Phase 4 ROI check
 * (comparing expected latency savings against expected misprediction cost) is
 * planned but not yet implemented; a TODO comment marks the extension point.
 *
 * Returns 0 if @p metrics or @c transition_heat are NULL, or if either gate
 * fails.
 *
 * @param metrics        The metrics structure to consult.
 * @param target_word_id Zero-based ID of the candidate next word.
 * @return               1 if speculation is recommended; 0 otherwise.
 */
int transition_metrics_should_speculate(const WordTransitionMetrics *metrics, uint32_t target_word_id) {
    if (!metrics || !metrics->transition_heat) {
        return 0;  /* No data yet */
    }

    /* Check: do we have enough samples? */
    if (metrics->total_transitions < MIN_SAMPLES_FOR_SPECULATION) {
        return 0;  /* Not enough data */
    }

    /* Get probability of this word following */
    int64_t prob_q48 = transition_metrics_get_probability_q48(metrics, target_word_id);

    /* Check: does probability exceed threshold? */
    if (prob_q48 < SPECULATION_THRESHOLD_Q48) {
        return 0;  /* Probability too low */
    }

    /* TODO: Phase 4 - Add ROI check here */
    /* For now, threshold check is sufficient */

    return 1;  /* Should speculate */
}

/**
 * @brief Allocate and return a human-readable transition metrics summary string.
 *
 * Allocates a 2048-byte buffer and fills it with a multi-line report:
 * - Total transitions recorded.
 * - Prefetch attempts, hits (with hit-rate %), and misses.
 * - Aggregate latency saved by correct speculation (from
 *   @c prefetch_latency_saved_q48, converted to integer ns via @c q48_to_ns()).
 * - Aggregate misprediction cost (from @c misprediction_cost_q48).
 * - Net benefit (saved − cost) in nanoseconds.
 * - The most-likely next word ID and its raw Q48.16 probability.
 *
 * The caller is responsible for calling @c free() on the returned pointer.
 * Returns NULL if @p metrics is NULL or @c malloc() fails.
 *
 * @param metrics  The metrics structure to summarise.
 * @return         Heap-allocated NUL-terminated stats string; caller must @c free().
 *                 NULL on allocation failure or NULL @p metrics.
 */
char *transition_metrics_stats_string(const WordTransitionMetrics *metrics) {
    if (!metrics) return NULL;

    /* Allocate buffer for stats string */
    char *buf = (char *)malloc(2048);
    if (!buf) return NULL;

    int saved_ns = q48_to_ns(metrics->prefetch_latency_saved_q48);
    int cost_ns = q48_to_ns(metrics->misprediction_cost_q48);
    int net_ns = saved_ns - cost_ns;

    /* Calculate hit rate */
    uint64_t total_speculations = metrics->prefetch_attempts;
    double hit_rate = (total_speculations > 0) ? (100.0 * metrics->prefetch_hits / total_speculations) : 0.0;

    snprintf(buf, 2048,
             "Transition Statistics:\n"
             "  Total Transitions: %lu\n"
             "  Prefetch Attempts: %lu\n"
             "  Prefetch Hits: %lu (%.1f%% hit rate)\n"
             "  Prefetch Misses: %lu\n"
             "  Latency Saved: %d ns\n"
             "  Misprediction Cost: %d ns\n"
             "  Net Benefit: %d ns\n"
             "  Most Likely Next: word[%u] (P=0x%lx Q48.16)\n",
             metrics->total_transitions,
             metrics->prefetch_attempts,
             metrics->prefetch_hits, hit_rate,
             metrics->prefetch_misses,
             saved_ns,
             cost_ns,
             net_ns,
             metrics->most_likely_next_word_id,
             metrics->max_transition_probability_q48);

    return buf;
}

/**
 * @brief Reset a @c WordTransitionMetrics to empty without freeing the context window.
 *
 * Frees and NULLs @c metrics->transition_heat (so the next
 * @c transition_metrics_record() will re-allocate it), then zeroes all
 * counter and accumulator fields. Also zeroes the context-window array
 * (if allocated) and resets @c context_window_pos to 0, and frees and NULLs
 * @c context_transitions. @c actual_window_size and the @c context_window
 * pointer itself are preserved — the window stays allocated and ready for
 * reuse.
 *
 * Intended for DoE benchmark runs that need a clean statistical baseline
 * between treatment cells without the overhead of full teardown and
 * reinitialisation. Cheaper than @c transition_metrics_cleanup() +
 * @c transition_metrics_init() because it avoids reallocating the context
 * window.
 *
 * @param metrics  The metrics structure to reset; no-op if NULL.
 */
void transition_metrics_reset(WordTransitionMetrics *metrics) {
    if (!metrics) return;

    if (metrics->transition_heat) {
        free(metrics->transition_heat);
        metrics->transition_heat = NULL;
    }

    metrics->total_transitions = 0;
    metrics->prefetch_attempts = 0;
    metrics->prefetch_hits = 0;
    metrics->prefetch_misses = 0;
    metrics->prefetch_latency_saved_q48 = 0;
    metrics->misprediction_cost_q48 = 0;
    metrics->max_transition_probability_q48 = 0;
    metrics->most_likely_next_word_id = 0;

    /* Phase 1 Extension: Reset context window */
    if (metrics->context_window) {
        memset(metrics->context_window, 0, metrics->actual_window_size * sizeof(uint32_t));
        metrics->context_window_pos = 0;
    }

    if (metrics->context_transitions) {
        free(metrics->context_transitions);
        metrics->context_transitions = NULL;
    }

    metrics->total_context_transitions = 0;
}

/**
 * @brief Release all heap memory held by a @c WordTransitionMetrics.
 *
 * Frees and NULLs:
 * - @c metrics->transition_heat — the per-word transition count array.
 * - @c metrics->context_window — the sliding context-window array.
 * - @c metrics->context_transitions — the Phase 2 sparse hash table (if allocated).
 *
 * Does NOT zero the struct fields beyond NULLing the freed pointers. The
 * caller must not use @p metrics after this call without a subsequent
 * @c transition_metrics_init(). Contrast with @c transition_metrics_reset(),
 * which keeps the context window allocated for reuse.
 *
 * No-op if @p metrics is NULL.
 *
 * @param metrics  The metrics structure to clean up; no-op if NULL.
 */
void transition_metrics_cleanup(WordTransitionMetrics *metrics) {
    if (!metrics) return;

    if (metrics->transition_heat) {
        free(metrics->transition_heat);
        metrics->transition_heat = NULL;
    }

    /* Phase 1 Extension: Clean up context window */
    if (metrics->context_window) {
        free(metrics->context_window);
        metrics->context_window = NULL;
    }

    if (metrics->context_transitions) {
        free(metrics->context_transitions);
        metrics->context_transitions = NULL;
    }
}

/* ============================================================================
 * Context-Aware Transition Functions (Phase 1 Extension)
 * ============================================================================
 *
 * Phase 1: Collect data only (these functions are stubs)
 * Phase 2: Implement analysis logic
 * Phase 3: Use for adaptive decisions
 */

/**
 * @brief Slide the context window forward by one word, appending @p word_id.
 *
 * Implements a fixed-length sliding window over the recent word-ID history.
 * Each call shifts all existing entries one position to the left (dropping
 * the oldest), then writes @p word_id into the last slot:
 * @code
 *   context_window[0..n-2] = context_window[1..n-1]
 *   context_window[n-1] = word_id
 * @endcode
 * This produces a FIFO queue of the last @c actual_window_size word IDs
 * that can be passed to @c transition_metrics_record_context() for
 * context-sensitive transition tracking (Phase 2).
 *
 * Returns −1 and is a no-op if:
 * - @p metrics is NULL.
 * - @c context_window is NULL (allocation at init failed — degraded mode).
 * - @c actual_window_size == 0.
 *
 * @param metrics  The metrics structure whose context window is updated.
 * @param word_id  The word ID just executed; appended to the right of the window.
 * @return         0 on success; −1 if the context window is unavailable.
 */
int transition_metrics_update_context_window(WordTransitionMetrics *metrics, uint32_t word_id) {
    if (!metrics || !metrics->context_window || metrics->actual_window_size == 0) {
        return -1;  /* Context tracking not initialized */
    }

    /* Slide window forward: shift elements left, add new word on right */
    for (uint32_t i = 0; i < metrics->actual_window_size - 1; i++) {
        metrics->context_window[i] = metrics->context_window[i + 1];
    }

    /* Add new word at end of window */
    metrics->context_window[metrics->actual_window_size - 1] = word_id;

    return 0;
}

/**
 * @brief Record a context-conditioned transition (Phase 1 collection stub).
 *
 * Accepts a @p context window of @p window_size preceding word IDs and the
 * @p next_word_id that followed. In Phase 1 only the total count is tracked:
 * @c metrics->total_context_transitions is incremented by one, providing a
 * baseline count that Phase 2 analysis can use to size its hash tables.
 *
 * **Phase 2 TODO:** Build a sparse hash table keyed on the context pattern
 * @c (context[0], …, context[window_size-1]) to record per-context next-word
 * distributions. The Phase 2 analysis will feed
 * @c transition_metrics_binary_chop_suggest_window() with accuracy data to
 * tune the optimal window size.
 *
 * Returns −1 (no-op) if @p metrics or @p context are NULL, @p window_size is
 * 0, or @p next_word_id ≥ @p dict_size.
 *
 * @param metrics       The metrics structure to update.
 * @param context       Array of @p window_size word IDs preceding @p next_word_id.
 * @param window_size   Number of entries in @p context; must be > 0.
 * @param next_word_id  Word ID that followed the context sequence.
 * @param dict_size     Current dictionary size; used for bounds checking
 *                      @p next_word_id.
 * @return              0 on success; −1 on invalid arguments.
 */
int transition_metrics_record_context(WordTransitionMetrics *metrics,
                                       const uint32_t *context,
                                       uint32_t window_size,
                                       uint32_t next_word_id,
                                       uint32_t dict_size) {
    if (!metrics || !context || window_size == 0 || next_word_id >= dict_size) {
        return -1;
    }

    /* Phase 1: Just count total context transitions for validation */
    metrics->total_context_transitions++;

    /* TODO Phase 2: Build sparse hash table of context patterns
     * For now, we collect data but don't analyze it.
     * This is the foundation for Phase 2's binary chop tuning.
     */

    return 0;
}

/**
 * @brief Allocate and return a context-window status report string (Phase 1 stub).
 *
 * Allocates a 1024-byte buffer and fills it with a brief Phase 1 collection
 * status message:
 * - @c actual_window_size — the active window width (0 if allocation failed).
 * - @c total_context_transitions — the number of context transitions recorded.
 * - A "[Phase 1 Collection - Phase 2 Analysis Pending]" status line.
 *
 * Phase 2 will replace this stub with a real accuracy report showing per-window-size
 * prediction accuracy derived from the sparse context-pattern hash table. The
 * accuracy data will feed @c transition_metrics_binary_chop_suggest_window()
 * to converge on the optimal context window width.
 *
 * The caller is responsible for calling @c free() on the returned pointer.
 * Returns NULL if @p metrics is NULL or @c malloc() fails.
 *
 * @param metrics  The metrics structure to report on.
 * @return         Heap-allocated NUL-terminated string; caller must @c free().
 *                 NULL on allocation failure or NULL @p metrics.
 */
char *transition_metrics_context_accuracy_string(const WordTransitionMetrics *metrics) {
    if (!metrics) return NULL;

    char *buf = (char *)malloc(1024);
    if (!buf) return NULL;

    /* Phase 1: Stub - just report collection status */
    snprintf(buf, 1024,
             "Context Metrics Status:\n"
             "  Actual Window Size: %u\n"
             "  Total Context Transitions: %lu\n"
             "  Status: [Phase 1 Collection - Phase 2 Analysis Pending]\n",
             metrics->actual_window_size,
             metrics->total_context_transitions);

    return buf;
}

/**
 * @brief Suggest a new context window size using binary-chop search (Phase 1 stub).
 *
 * In Phase 2 this function will implement a binary-chop search over window sizes
 * to converge on the width that maximises context-prediction accuracy:
 * - Start at window = 2.
 * - If accuracy improves when doubling, try 4; if it declines, try 1.
 * - Continue halving/doubling until the accuracy delta falls below a threshold.
 *
 * **Phase 1 stub behaviour:** Doubles the current window each call up to 8,
 * then wraps back to 1:
 * - @p current_window == 1 → suggests 2
 * - @p current_window == 2 → suggests 4
 * - @p current_window == 4 → suggests 8
 * - @p current_window ≥ 8 → suggests 1 (cycle back)
 * - Any other value → returns @p current_window unchanged.
 *
 * The @p accuracy_at_current parameter (prediction accuracy [0.0, 1.0] at the
 * current window size) is accepted for API stability with the Phase 2 caller
 * but is not used in the stub.
 *
 * @param metrics              The metrics structure (unused in Phase 1 stub;
 *                             reserved for Phase 2 accuracy history).
 * @param current_window       Currently active context window size.
 * @param accuracy_at_current  Prediction accuracy at @p current_window
 *                             (unused in Phase 1 stub; range [0.0, 1.0]).
 * @return                     Suggested new window size.
 */
uint32_t transition_metrics_binary_chop_suggest_window(const WordTransitionMetrics *metrics,
                                                       uint32_t current_window,
                                                       double accuracy_at_current) {
    /* Phase 1: Stub - just return doubled window size per user's request
     * Phase 2 will implement actual binary chop search:
     *   - Start at window=2
     *   - If accuracy improves, try doubling (2→4)
     *   - If accuracy declines, try halving (4→2)
     *   - Converge to optimal size
     */

    if (current_window >= 8) return 1;   /* Maximum: back to 1 */
    if (current_window == 1) return 2;   /* 1 → 2 */
    if (current_window == 2) return 4;   /* 2 → 4 (doubling) */
    if (current_window == 4) return 8;   /* 4 → 8 (doubling) */

    return current_window;  /* Fallback */
}
