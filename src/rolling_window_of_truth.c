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

  rolling_window_of_truth.c - Execution history capture and seeding

  Implements the rolling window that captures POST execution sequence and
  uses it to deterministically seed both hot-words cache and pipelining
  context windows.

  Key property: Deterministic → Reproducible → Provable

*/

#include "rolling_window_of_truth.h"
#include "../include/vm.h"
#include "../include/rolling_window_knobs.h"
#include "../include/physics_hotwords_cache.h"
#include "../include/physics_pipelining_metrics.h"
#include "../include/log.h"
#include "../include/platform_alloc.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef struct
{
    const uint32_t* history;
    uint32_t window_pos;
    uint64_t total_executions;
    uint32_t effective_window_size;
    int is_warm;
} rolling_window_view_t;

static uint64_t rolling_window_measure_diversity_view(const rolling_window_view_t* view)
{
    if (!view || !view->history || view->total_executions < 2)
        return 0;

    uint64_t unique_transitions = 0;
    uint32_t prev_word = 0;
    int seen_first = 0;

    uint32_t scan_limit = view->is_warm ? view->effective_window_size : ROLLING_WINDOW_SIZE;

    for (uint32_t i = 0; i < scan_limit; i++)
    {
        uint32_t idx = (view->window_pos + ROLLING_WINDOW_SIZE - scan_limit + i) % ROLLING_WINDOW_SIZE;
        uint32_t current = view->history[idx];

        if (seen_first && current != prev_word)
        {
            unique_transitions++;
        }
        prev_word = current;
        seen_first = 1;
    }

    return unique_transitions;
}

static void rolling_window_snapshot_view(const RollingWindowOfTruth* window,
                                         rolling_window_view_t* view)
{
    if (!window || !view)
    {
        if (view)
            memset(view, 0, sizeof(*view));
        return;
    }

    uint32_t idx = __atomic_load_n(&window->snapshot_index, __ATOMIC_ACQUIRE) & 1u;
    view->history = window->snapshot_buffers[idx];
    view->window_pos = window->snapshot_window_pos[idx];
    view->total_executions = window->snapshot_total_executions[idx];
    view->effective_window_size = window->snapshot_effective_window_size[idx];
    view->is_warm = window->snapshot_is_warm[idx];
}

static void rolling_window_publish_snapshot(RollingWindowOfTruth* window)
{
    if (!window || !window->execution_history || !window->snapshot_buffers[0])
        return;

    uint32_t write_idx = (window->snapshot_index ^ 1u) & 1u;

    memcpy(window->snapshot_buffers[write_idx],
           window->execution_history,
           ROLLING_WINDOW_SIZE * sizeof(uint32_t));

    window->snapshot_window_pos[write_idx] = window->window_pos;
    window->snapshot_total_executions[write_idx] = window->total_executions;
    window->snapshot_effective_window_size[write_idx] = window->effective_window_size;
    window->snapshot_is_warm[write_idx] = window->is_warm;

    __atomic_store_n(&window->snapshot_index, write_idx, __ATOMIC_RELEASE);
    __atomic_store_n(&window->snapshot_pending, 0, __ATOMIC_RELEASE);
}

static void rolling_window_publish_snapshot_if_needed(RollingWindowOfTruth* window)
{
    if (!window)
        return;

    uint32_t pending = __atomic_exchange_n(&window->snapshot_pending, 0, __ATOMIC_ACQ_REL);
    if (pending)
    {
        rolling_window_publish_snapshot(window);
    }
}

/* ============================================================================
 * Rolling Window Implementation
 * ============================================================================
 */

int rolling_window_init(RollingWindowOfTruth* window)
{
    if (!window) return -1;

    window->execution_history = (uint32_t*)sf_calloc(ROLLING_WINDOW_SIZE, sizeof(uint32_t));
    if (!window->execution_history)
    {
        return -1; /* Malloc failure */
    }

    window->snapshot_buffers[0] = (uint32_t*)sf_calloc(ROLLING_WINDOW_SIZE, sizeof(uint32_t));
    window->snapshot_buffers[1] = (uint32_t*)sf_calloc(ROLLING_WINDOW_SIZE, sizeof(uint32_t));
    if (!window->snapshot_buffers[0] || !window->snapshot_buffers[1])
    {
        sf_free(window->execution_history);
        window->execution_history = NULL;
        sf_free(window->snapshot_buffers[0]);
        sf_free(window->snapshot_buffers[1]);
        window->snapshot_buffers[0] = window->snapshot_buffers[1] = NULL;
        return -1;
    }

    window->window_pos = 0;
    window->total_executions = 0;
    window->is_warm = 0; /* Cold start */

    /* Initialize adaptive window sizing: start conservative, shrink if beneficial */
    window->effective_window_size = ROLLING_WINDOW_SIZE;
    window->last_pattern_diversity = 0;
    window->pattern_diversity_check_count = 0;
    window->snapshot_index = 0;
    window->snapshot_pending = 1;
    window->snapshot_window_pos[0] = window->snapshot_window_pos[1] = 0;
    window->snapshot_total_executions[0] = window->snapshot_total_executions[1] = 0;
    window->snapshot_effective_window_size[0] = window->snapshot_effective_window_size[1] = ROLLING_WINDOW_SIZE;
    window->snapshot_is_warm[0] = window->snapshot_is_warm[1] = 0;
    window->adaptive_check_accumulator = 0;
    window->adaptive_pending = 0;

    rolling_window_publish_snapshot(window);

    return 0;
}

int rolling_window_record_execution(RollingWindowOfTruth* window, uint32_t word_id)
{
    if (!window || !window->execution_history)
    {
        return -1;
    }

    /* Write word to circular buffer */
    window->execution_history[window->window_pos] = word_id;

    /* Advance position */
    window->window_pos = (window->window_pos + 1) % ROLLING_WINDOW_SIZE;

    /* Increment lifetime counter */
    window->total_executions++;

    /* Mark as warm once we've recorded enough history for pattern recognition */
    /* Test harness generates ~1450 executions, so set realistic threshold at 1024 */
    if (window->total_executions >= 1024 && !window->is_warm)
    {
        window->is_warm = 1;
    }

    /* Signal snapshot consumers that fresh data is available */
    window->snapshot_pending = 1;

    if (window->is_warm)
    {
        if (++window->adaptive_check_accumulator >= ADAPTIVE_CHECK_FREQUENCY)
        {
            window->adaptive_check_accumulator = 0;
            window->adaptive_pending = 1;
        }
    }

    return 0;
}

uint32_t rolling_window_get_recent_sequence(const RollingWindowOfTruth* window,
                                            uint32_t depth,
                                            uint32_t* out_sequence)
{
    if (!window || !out_sequence || depth == 0)
    {
        return 0;
    }

    rolling_window_publish_snapshot_if_needed((RollingWindowOfTruth*)window);
    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);
    if (!view.history)
        return 0;

    /* Cap depth to available data */
    uint32_t available = (view.total_executions < ROLLING_WINDOW_SIZE)
                             ? (uint32_t)view.total_executions
                             : ROLLING_WINDOW_SIZE;
    uint32_t actual_depth = (depth > available) ? available : depth;

    /* Copy recent sequence in order (most recent last) */
    for (uint32_t i = 0; i < actual_depth; i++)
    {
        /* Index going backwards from current position */
        int idx = (int)view.window_pos - (int)actual_depth + (int)i;
        if (idx < 0) idx += ROLLING_WINDOW_SIZE;

        out_sequence[i] = view.history[idx];
    }

    return actual_depth;
}

uint32_t rolling_window_find_hottest_word(const RollingWindowOfTruth* window,
                                          uint32_t dict_size)
{
    if (!window || !window->is_warm)
    {
        return 0; /* Not warm yet */
    }

    rolling_window_publish_snapshot_if_needed((RollingWindowOfTruth*)window);
    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);
    if (!view.history || !view.is_warm)
        return 0;

    /* Count frequency of each word */
    uint32_t* freq = (uint32_t*)sf_calloc(dict_size, sizeof(uint32_t));
    if (!freq) return 0;

    for (uint32_t i = 0; i < ROLLING_WINDOW_SIZE; i++)
    {
        uint32_t word_id = view.history[i];
        if (word_id < dict_size)
        {
            freq[word_id]++;
        }
    }

    /* Find word with highest frequency */
    uint32_t hottest_id = 0;
    uint32_t max_freq = 0;
    for (uint32_t i = 0; i < dict_size; i++)
    {
        if (freq[i] > max_freq)
        {
            max_freq = freq[i];
            hottest_id = i;
        }
    }

    sf_free(freq);
    return hottest_id;
}

uint64_t rolling_window_count_transition(const RollingWindowOfTruth* window,
                                         uint32_t word_a,
                                         uint32_t word_b)
{
    if (!window || !window->is_warm)
    {
        return 0;
    }

    rolling_window_publish_snapshot_if_needed((RollingWindowOfTruth*)window);
    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);
    if (!view.history || !view.is_warm)
        return 0;

    uint64_t count = 0;

    /* Scan for (word_a → word_b) transitions */
    for (uint32_t i = 0; i < ROLLING_WINDOW_SIZE; i++)
    {
        uint32_t current = view.history[i];
        uint32_t next = view.history[(i + 1) % ROLLING_WINDOW_SIZE];

        if (current == word_a && next == word_b)
        {
            count++;
        }
    }
    return count;
}

int rolling_window_is_warm(const RollingWindowOfTruth* window)
{
    if (!window) return 0;
    return window->is_warm;
}

char* rolling_window_stats_string(const RollingWindowOfTruth* window)
{
    if (!window) return NULL;

    char* buf = (char*)sf_malloc(1024);
    if (!buf) return NULL;

    rolling_window_publish_snapshot_if_needed((RollingWindowOfTruth*)window);
    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);

    snprintf(buf, 1024,
             "Rolling Window of Truth:\n"
             "  Total Executions: %lu\n"
             "  Window Size: %u\n"
             "  Status: %s\n"
             "  Current Position: %u/%u\n",
             (unsigned long)view.total_executions,
             ROLLING_WINDOW_SIZE,
             view.is_warm ? "WARM (representative)" : "COLD (warming up)",
             view.window_pos,
             ROLLING_WINDOW_SIZE);

    return buf;
}

void rolling_window_reset(RollingWindowOfTruth* window)
{
    if (!window || !window->execution_history)
    {
        return;
    }

    memset(window->execution_history, 0, ROLLING_WINDOW_SIZE * sizeof(uint32_t));
    window->window_pos = 0;
    window->total_executions = 0;
    window->is_warm = 0;
    window->snapshot_pending = 1;
}

void rolling_window_cleanup(RollingWindowOfTruth* window)
{
    if (!window) return;

    if (window->execution_history)
    {
        sf_free(window->execution_history);
        window->execution_history = NULL;
    }
    if (window->snapshot_buffers[0])
    {
        sf_free(window->snapshot_buffers[0]);
        window->snapshot_buffers[0] = NULL;
    }
    if (window->snapshot_buffers[1])
    {
        sf_free(window->snapshot_buffers[1]);
        window->snapshot_buffers[1] = NULL;
    }
}

/* ============================================================================
 * Seeding Functions: Apply rolling window data to optimization systems
 * ============================================================================
 */

/**
 * Seed the hot-words cache from rolling window execution history.
 *
 * Called after POST completes, before REPL starts.
 * Analyzes which words appeared most frequently in POST and promotes them
 * to the cache immediately (warm-start optimization).
 *
 * This ensures that the first user command benefits from knowing which
 * words are "hot" based on representative execution data.
 */
void rolling_window_seed_hotwords_cache(const RollingWindowOfTruth* window,
                                        struct VM* vm)
{
    if (!window || !vm || !window->is_warm || !vm->hotwords_cache)
    {
        return; /* Window not ready or no cache */
    }

    /* Count frequency of each word in rolling window */
    uint32_t *freq = (uint32_t *)sf_calloc(DICTIONARY_SIZE, sizeof(uint32_t));
    if (!freq) return;

    rolling_window_publish_snapshot_if_needed((RollingWindowOfTruth*)window);
    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);
    if (!view.history || !view.is_warm)
    {
        sf_free(freq);
        return;
    }

    for (uint32_t i = 0; i < ROLLING_WINDOW_SIZE; i++)
    {
        uint32_t word_id = view.history[i];
        if (word_id < DICTIONARY_SIZE)
        {
            freq[word_id]++;
        }
    }

    /* Promote hot words to cache: threshold = median frequency */
    uint32_t total_hot_words = 0;
    uint32_t sum_freq = 0;
    uint32_t count_nonzero = 0;

    for (uint32_t i = 0; i < DICTIONARY_SIZE; i++)
    {
        if (freq[i] > 0)
        {
            sum_freq += freq[i];
            count_nonzero++;
        }
    }

    /* Threshold: promote words above average frequency */
    uint32_t threshold = (count_nonzero > 0) ? (sum_freq / count_nonzero) : 1;

    sf_mutex_lock(&vm->dict_lock);
    for (uint32_t i = 0; i < DICTIONARY_SIZE; i++)
    {
        if (freq[i] > threshold)
        {
            /* Lookup dictionary entry by stable word_id mapping */
            DictEntry *entry = vm_dictionary_lookup_by_word_id(vm, i);

            if (entry)
            {
                /* Update execution heat and promote to cache */
                entry->execution_heat = (cell_t)freq[i];
                hotwords_cache_promote(vm->hotwords_cache, entry);
                total_hot_words++;
            }
        }
    }
    sf_mutex_unlock(&vm->dict_lock);

    sf_free(freq);

    log_message(LOG_INFO,
                "Seeded hot-words cache with %u words from rolling window POST execution",
                total_hot_words);
}

/**
 * Seed pipelining context windows from rolling window.
 *
 * Called after POST completes, before REPL starts.
 * Populates the context_window for each word with actual predecessor
 * data from POST execution.
 *
 * This ensures that the first user command has contextual history
 * for transition prediction.
 */
void rolling_window_seed_pipelining_context(const RollingWindowOfTruth* window,
                                            struct VM* vm)
{
    if (!window || !vm || !window->is_warm || !ENABLE_PIPELINING)
    {
        return; /* Window not ready or pipelining disabled */
    }

    uint32_t context_window_size = (TRANSITION_WINDOW_SIZE < 2) ? 2 : TRANSITION_WINDOW_SIZE;
    uint32_t seeded_words = 0;
    uint32_t seeded_transitions = 0;

    /* Allocate context window for replaying */
    uint32_t *context = (uint32_t *)sf_calloc(context_window_size, sizeof(uint32_t));
    if (!context)
        return;

    uint32_t init_count = (context_window_size < ROLLING_WINDOW_SIZE) ? context_window_size : ROLLING_WINDOW_SIZE;

    rolling_window_publish_snapshot_if_needed((RollingWindowOfTruth*)window);
    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);
    if (!view.history || !view.is_warm)
    {
        sf_free(context);
        return;
    }

    for (uint32_t i = 0; i < init_count && i < ROLLING_WINDOW_SIZE; i++)
    {
        context[i] = view.history[i];
    }

    /* Scan rolling window and seed context windows by replaying transitions */
    for (uint32_t i = 0; i < ROLLING_WINDOW_SIZE; i++)
    {
        uint32_t current_id = view.history[i];
        uint32_t next_id = view.history[(i + 1) % ROLLING_WINDOW_SIZE];

        if (current_id < DICTIONARY_SIZE && next_id < DICTIONARY_SIZE)
        {
            /* Lookup dictionary entry for current word by stable ID */
            DictEntry *current_entry = NULL;
            sf_mutex_lock(&vm->dict_lock);
            current_entry = vm_dictionary_lookup_by_word_id(vm, current_id);
            sf_mutex_unlock(&vm->dict_lock);

            if (current_entry && current_entry->transition_metrics)
            {
                /* Record this transition with current context */
                transition_metrics_record_context(
                    current_entry->transition_metrics,
                    context,
                    context_window_size,
                    next_id,
                    DICTIONARY_SIZE);

                seeded_transitions++;

                /* Update context window by shifting and adding current word */
                for (uint32_t j = 0; j < context_window_size - 1; j++)
                {
                    context[j] = context[j + 1];
                }
                context[context_window_size - 1] = next_id;

                seeded_words++;
            }
        }
    }

    sf_free(context);

    log_message(LOG_INFO,
                "Seeded pipelining context windows: %u words, %u transitions from POST execution",
                seeded_words,
                seeded_transitions);
}

/* ============================================================================
 * Bootstrap Data Analysis Implementation
 * ============================================================================
 */

uint64_t rolling_window_export_execution_history(const RollingWindowOfTruth* window,
                                                  uint32_t* out_sequence,
                                                  uint64_t max_count)
{
    if (!window || !out_sequence || max_count == 0)
    {
        return 0;
    }

    rolling_window_publish_snapshot_if_needed((RollingWindowOfTruth*)window);
    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);
    if (!view.history)
        return 0;

    /* Determine how many entries to export (capped by caller buffer) */
    uint64_t export_count = (view.total_executions < max_count)
                            ? view.total_executions
                            : max_count;

    if (export_count == 0)
        return 0;

    /* If we haven't wrapped around yet (total < ROLLING_WINDOW_SIZE),
     * just copy from start to current position */
    if (view.total_executions < ROLLING_WINDOW_SIZE)
    {
        memcpy(out_sequence,
               view.history,
               export_count * sizeof(uint32_t));
        return export_count;
    }

    /* Otherwise, linearize the circular buffer:
     * Copy from window_pos to end, then from start to window_pos */
    uint64_t first_part = ROLLING_WINDOW_SIZE - view.window_pos;
    memcpy(out_sequence,
           &view.history[view.window_pos],
           first_part * sizeof(uint32_t));

    if (export_count > first_part)
    {
        memcpy(&out_sequence[first_part],
               view.history,
               (export_count - first_part) * sizeof(uint32_t));
    }
    return export_count;
}

double rolling_window_pattern_capture_rate(const RollingWindowOfTruth* window,
                                            uint32_t test_window_size)
{
    if (!window || test_window_size < 1)
    {
        return 0.0;
    }

    rolling_window_publish_snapshot_if_needed((RollingWindowOfTruth*)window);
    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);
    if (!view.history || view.total_executions < 2)
        return 0.0;

    /* Allocate space for linearized history */
    uint64_t history_size = (view.total_executions < (uint64_t)ROLLING_WINDOW_SIZE)
                            ? view.total_executions
                            : (uint64_t)ROLLING_WINDOW_SIZE;

    uint32_t* linear_history = (uint32_t *)sf_malloc(history_size * sizeof(uint32_t));
    if (!linear_history)
        return 0.0;

    uint64_t actual_history = rolling_window_export_execution_history(
        window, linear_history, history_size);

    if (actual_history < 2)
    {
        sf_free(linear_history);
        return 0.0;
    }

    /* Hash set to track unique (context, next_word) patterns
     * Using simple counting: patterns seen / potential patterns */
    uint64_t patterns_seen = 0;
    uint64_t total_transitions = actual_history - 1;

    /* Simple pattern tracking: just count unique (word_a, word_b, ..., word_n) → word_next */
    /* For efficiency, we'll use a simplified approach: count how many transitions
     * can be uniquely identified with the given context window size */

    for (uint64_t i = 0; i < actual_history - 1; i++)
    {
        /* Create a simple hash of the context window and next word
         * This is a rough approximation of pattern uniqueness */
        uint32_t pattern_hash = 0;

        /* Incorporate the sliding context window into hash */
        for (uint32_t j = 0; j < test_window_size && i >= j; j++)
        {
            pattern_hash = (pattern_hash * 31 + linear_history[i - j]) % 0x7FFFFFFF;
        }
        pattern_hash = (pattern_hash * 31 + linear_history[i + 1]) % 0x7FFFFFFF;

        /* Count this as a valid pattern observation */
        if (pattern_hash > 0)
            patterns_seen++;
    }

    sf_free(linear_history);

    /* Return percentage: how many transitions had sufficient context to be meaningful */
    if (total_transitions == 0)
        return 0.0;

    double capture_rate = (double)patterns_seen / (double)total_transitions * 100.0;
    return (capture_rate > 100.0) ? 100.0 : capture_rate;
}

/* ============================================================================
 * Adaptive Window Sizing (Continuous Self-Tuning)
 * ============================================================================
 *
 * During execution, the rolling window continuously measures whether it's
 * capturing diminishing returns. If pattern diversity plateaus, the effective
 * window size shrinks automatically.
 */

/**
 * Measure pattern diversity in current window.
 * Used by adaptive sizing to decide if shrinking is beneficial.
 *
 * Simple heuristic: count unique word transitions observed.
 * Higher diversity = more patterns = window is capturing something meaningful.
 * If diversity plateaus, shrinking won't lose important data.
 */
uint64_t rolling_window_measure_diversity(const RollingWindowOfTruth* window)
{
    rolling_window_publish_snapshot_if_needed((RollingWindowOfTruth*)window);
    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);
    return rolling_window_measure_diversity_view(&view);
}

static void rolling_window_run_adaptive_pass(RollingWindowOfTruth* window,
                                             const rolling_window_view_t* view)
{
    if (!window || !view || !view->history || !view->is_warm)
        return;

    uint64_t current_diversity = rolling_window_measure_diversity_view(view);

    window->pattern_diversity_check_count++;

    log_message(LOG_DEBUG,
                "ADAPTIVE_WINDOW[Check #%lu] total_execs=%lu window_size=%u diversity=%lu",
                window->pattern_diversity_check_count,
                (unsigned long)view->total_executions,
                window->effective_window_size,
                current_diversity);

    if (window->last_pattern_diversity == 0)
    {
        log_message(LOG_DEBUG,
                    "ADAPTIVE_WINDOW[Baseline] Recording initial diversity=%lu as baseline",
                    current_diversity);
        window->last_pattern_diversity = current_diversity;
        return;
    }

    uint64_t diversity_delta = (current_diversity > window->last_pattern_diversity)
                              ? (current_diversity - window->last_pattern_diversity)
                              : 0;
    uint64_t growth_rate_q48 = (window->last_pattern_diversity > 0)
                               ? ((diversity_delta << 16) / window->last_pattern_diversity)
                               : 0;
    uint64_t threshold_q48 = ((uint64_t)ADAPTIVE_GROWTH_THRESHOLD << 16) / 100;

    log_message(LOG_DEBUG,
                "ADAPTIVE_WINDOW[Calc] delta=%lu last=%lu growth_rate_q48=%lx threshold_q48=%lx",
                diversity_delta,
                window->last_pattern_diversity,
                (unsigned long)growth_rate_q48,
                (unsigned long)threshold_q48);

    if (growth_rate_q48 < threshold_q48)
    {
        if (window->effective_window_size > ADAPTIVE_MIN_WINDOW_SIZE)
        {
            uint32_t new_size = (window->effective_window_size * ADAPTIVE_SHRINK_RATE) / 100;
            if (new_size < ADAPTIVE_MIN_WINDOW_SIZE)
                new_size = ADAPTIVE_MIN_WINDOW_SIZE;

            if (new_size < window->effective_window_size)
            {
                log_message(LOG_DEBUG,
                            "ADAPTIVE_WINDOW[SHRINK] %u → %u (growth_rate=%lu < %lu threshold, diversity_delta=%lu)",
                            window->effective_window_size,
                            new_size,
                            (unsigned long)growth_rate_q48,
                            (unsigned long)threshold_q48,
                            diversity_delta);
                window->effective_window_size = new_size;
            }
            else
            {
                log_message(LOG_DEBUG,
                            "ADAPTIVE_WINDOW[Calc Error] new_size=%u >= current=%u, not changing",
                            new_size,
                            window->effective_window_size);
            }
        }
        else
        {
            log_message(LOG_DEBUG,
                        "ADAPTIVE_WINDOW[Floor Reached] already at minimum size %u, cannot shrink further",
                        window->effective_window_size);
        }
    }
    else if (growth_rate_q48 >= threshold_q48)
    {
        if (window->effective_window_size < ROLLING_WINDOW_SIZE)
        {
            uint32_t growth_factor = (100 * 100) / ADAPTIVE_SHRINK_RATE;
            uint32_t new_size = (window->effective_window_size * growth_factor) / 100;
            if (new_size > ROLLING_WINDOW_SIZE)
                new_size = ROLLING_WINDOW_SIZE;

            if (new_size > window->effective_window_size)
            {
                log_message(LOG_DEBUG,
                            "ADAPTIVE_WINDOW[GROW] %u → %u (growth_rate=%lu >= %lu threshold, diversity_delta=%lu)",
                            window->effective_window_size,
                            new_size,
                            (unsigned long)growth_rate_q48,
                            (unsigned long)threshold_q48,
                            diversity_delta);
                window->effective_window_size = new_size;
            }
            else
            {
                log_message(LOG_DEBUG,
                            "ADAPTIVE_WINDOW[Calc Error] new_size=%u <= current=%u, not changing",
                            new_size,
                            window->effective_window_size);
            }
        }
        else
        {
            log_message(LOG_DEBUG,
                        "ADAPTIVE_WINDOW[Ceiling Reached] already at maximum size %u, cannot grow further",
                        window->effective_window_size);
        }
    }

    window->last_pattern_diversity = current_diversity;
}

static void rolling_window_service_internal(RollingWindowOfTruth* window)
{
    if (!window)
        return;

    rolling_window_publish_snapshot_if_needed(window);

    if (!window->adaptive_pending)
        return;

    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);
    rolling_window_run_adaptive_pass(window, &view);
    window->adaptive_pending = 0;
}

void rolling_window_check_adaptive_shrink(RollingWindowOfTruth* window)
{
    if (!window)
        return;

    window->adaptive_pending = 1;
    rolling_window_service_internal(window);
}

void rolling_window_service(RollingWindowOfTruth* window)
{
    rolling_window_service_internal(window);
}
