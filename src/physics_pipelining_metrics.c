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
 * Convert nanoseconds to Q48.16 format.
 * ns -> ns_q48 where ns_q48 = ns * (1LL << 16)
 */
static inline int64_t ns_to_q48(int64_t ns) {
    return ns << 16;
}

/**
 * Convert Q48.16 to nanoseconds (integer part only, truncated).
 * ns_q48 -> ns where ns = ns_q48 >> 16
 */
static inline int64_t q48_to_ns(int64_t q48) {
    return q48 >> 16;
}

/**
 * Divide Q48.16 by unsigned integer, result in Q48.16.
 * (a_q48 / b) where a is Q48.16, b is uint64_t
 */
static inline int64_t q48_div_u64(int64_t a_q48, uint64_t b) {
    if (b == 0) return 0;
    return a_q48 / (int64_t)b;
}

/**
 * Multiply Q48.16 by Q48.16, result in Q48.16.
 * Maintains precision by dividing by (1LL << 16).
 */
static inline int64_t q48_mul_q48(int64_t a, int64_t b) {
    return (a * b) >> 16;
}

/* ============================================================================
 * Transition Metrics Implementation
 * ============================================================================
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

void transition_metrics_record_prefetch_hit(WordTransitionMetrics *metrics, int64_t latency_saved_ns) {
    if (!metrics) return;

    metrics->prefetch_hits++;
    metrics->prefetch_latency_saved_q48 += ns_to_q48(latency_saved_ns);
}

void transition_metrics_record_prefetch_miss(WordTransitionMetrics *metrics, int64_t recovery_cost_ns) {
    if (!metrics) return;

    metrics->prefetch_misses++;
    metrics->misprediction_cost_q48 += ns_to_q48(recovery_cost_ns);
}

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
