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

/**
 * @brief Immutable snapshot view of a @c RollingWindowOfTruth.
 *
 * Provides a read-only window into one of the two double-buffered snapshot
 * arrays. Reader threads (the heartbeat, pipelining, and diversity subsystems)
 * obtain a @c rolling_window_view_t via @c rolling_window_snapshot_view()
 * rather than accessing the live @c RollingWindowOfTruth fields directly;
 * this avoids TOCTOU races with the writer thread that calls
 * @c rolling_window_record_execution().
 *
 * Fields mirror the snapshot-time copies of the corresponding
 * @c RollingWindowOfTruth members:
 * - @c history — pointer into @c snapshot_buffers[idx]; size @c ROLLING_WINDOW_SIZE.
 * - @c window_pos — write cursor at snapshot time.
 * - @c total_executions — monotonic execution count at snapshot time.
 * - @c effective_window_size — adaptive window width at snapshot time.
 * - @c is_warm — whether the window had reached ≥ 1024 executions at snapshot.
 */
typedef struct
{
    const uint32_t* history;
    uint32_t window_pos;
    uint64_t total_executions;
    uint32_t effective_window_size;
    int is_warm;
} rolling_window_view_t;

/**
 * @brief Count unique consecutive-pair transitions in a snapshot view.
 *
 * Scans the @p view's history buffer over the most recent
 * @c effective_window_size entries (when warm) or all @c ROLLING_WINDOW_SIZE
 * entries (when cold) and counts the number of adjacent pairs
 * @c (history[i], history[i+1]) where @c history[i+1] ≠ @c history[i].
 *
 * This is the primary pattern-diversity metric used by:
 * - @c dict_adaptive_optimization_pass() — to choose between heat-aware and
 *   naive lookup strategies (diversity > 70 → heat-aware).
 * - @c rolling_window_run_adaptive_pass() — to decide whether to shrink or
 *   grow @c effective_window_size.
 *
 * Returns 0 if @p view is NULL, has no history, or has fewer than 2
 * total executions.
 *
 * @param view  Immutable snapshot view; must have @c history != NULL.
 * @return      Count of unique adjacent-pair transitions in the active window.
 */
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

/**
 * @brief Populate a @c rolling_window_view_t from the current double-buffer snapshot.
 *
 * Reads @c window->snapshot_index with @c __ATOMIC_ACQUIRE to determine which
 * of the two snapshot buffers (@c snapshot_buffers[0] or @c [1]) is the most
 * recently published. Fills @p view with the corresponding history pointer and
 * metadata fields (@c window_pos, @c total_executions, @c effective_window_size,
 * @c is_warm). If @p window or @p view is NULL, @p view is zeroed and the
 * function returns early.
 *
 * Callers hold no lock — the acquire barrier on @c snapshot_index ensures they
 * see a coherent snapshot that was fully written before
 * @c rolling_window_publish_snapshot() released it via @c __ATOMIC_RELEASE.
 *
 * @param window  The @c RollingWindowOfTruth to sample.
 * @param view    Output @c rolling_window_view_t; zeroed on NULL @p window.
 */
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

/**
 * @brief Publish the live execution history into the inactive double-buffer slot.
 *
 * Implements the write side of the lock-free double buffer:
 * 1. Identifies the inactive slot as @c write_idx = @c snapshot_index XOR 1.
 * 2. Copies @c window->execution_history (the live circular buffer) into
 *    @c snapshot_buffers[write_idx] via @c memcpy.
 * 3. Copies the current @c window_pos, @c total_executions,
 *    @c effective_window_size, and @c is_warm into the corresponding
 *    @c snapshot_* parallel arrays.
 * 4. Flips @c snapshot_index to @p write_idx with @c __ATOMIC_RELEASE, making
 *    the new snapshot visible to readers.
 * 5. Clears @c snapshot_pending with @c __ATOMIC_RELEASE.
 *
 * Must only be called from the writer (main VM thread). No-op if @p window,
 * @c window->execution_history, or @c snapshot_buffers[0] are NULL.
 *
 * @param window  The @c RollingWindowOfTruth to snapshot.
 */
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

/**
 * @brief Publish a new snapshot only if the @c snapshot_pending flag is set.
 *
 * Atomically exchanges @c window->snapshot_pending with 0 using
 * @c __ATOMIC_ACQ_REL. If the exchanged value was non-zero (meaning
 * @c rolling_window_record_execution() has written new data since the last
 * publish), calls @c rolling_window_publish_snapshot() to make the current
 * state visible to readers. Otherwise returns without copying.
 *
 * This is the lazy-publish counterpart to @c rolling_window_record_execution()'s
 * eager flag set. By batching publish work to read-side call sites (
 * @c rolling_window_get_recent_sequence(), @c rolling_window_find_hottest_word(),
 * etc.), unnecessary @c memcpy of the full @c ROLLING_WINDOW_SIZE buffer is
 * avoided when no new data has arrived.
 *
 * @param window  The @c RollingWindowOfTruth to conditionally snapshot.
 *                No-op if NULL.
 */
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

/**
 * @brief Initialise a @c RollingWindowOfTruth, allocating its three heap buffers.
 *
 * Allocates and zero-initialises three @c uint32_t arrays, each of
 * @c ROLLING_WINDOW_SIZE elements, via @c sf_calloc:
 * - @c window->execution_history — the live circular write buffer.
 * - @c window->snapshot_buffers[0] — first double-buffer read slot.
 * - @c window->snapshot_buffers[1] — second double-buffer read slot.
 *
 * If any allocation fails, all previously allocated buffers are freed and
 * nulled before returning −1. Initialises scalar state:
 * - @c window_pos = 0, @c total_executions = 0, @c is_warm = 0 (cold start).
 * - @c effective_window_size = @c ROLLING_WINDOW_SIZE (start at maximum).
 * - @c snapshot_index = 0, @c snapshot_pending = 1.
 * - All @c snapshot_window_pos[] / @c snapshot_total_executions[] / etc. = 0.
 * - @c adaptive_check_accumulator = 0, @c adaptive_pending = 0.
 *
 * Immediately calls @c rolling_window_publish_snapshot() to push the zeroed
 * state into both snapshot slots so that any early read-side call before the
 * first @c rolling_window_record_execution() gets a coherent (empty) view.
 *
 * @param window  The @c RollingWindowOfTruth to initialise; must not be NULL.
 * @return        0 on success; −1 on NULL @p window or allocation failure.
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

/**
 * @brief Record one word execution in the circular history buffer.
 *
 * Writes @p word_id into @c window->execution_history[window_pos], advances
 * @c window_pos modulo @c ROLLING_WINDOW_SIZE (circular wrap), increments
 * @c total_executions, and checks the warm threshold:
 * - Once @c total_executions ≥ 1024 and @c is_warm is still 0, sets
 *   @c is_warm = 1. The 1024-execution threshold is calibrated to the POST
 *   harness which generates approximately 1450 executions; reaching 1024
 *   guarantees a statistically meaningful execution distribution before
 *   diversity or percentile analysis begins.
 *
 * Sets @c snapshot_pending = 1 to signal that a new snapshot should be
 * published on the next read-side access. When the window is warm and
 * @c adaptive_check_accumulator reaches @c ADAPTIVE_CHECK_FREQUENCY, also sets
 * @c adaptive_pending = 1 to request an @c effective_window_size update.
 *
 * Called from the interpreter hot path on every word dispatch. All operations
 * are O(1) with no locks and no allocations.
 *
 * @param window   The @c RollingWindowOfTruth to record into.
 * @param word_id  Stable numeric ID of the word just executed.
 * @return         0 on success; −1 if @p window or @c execution_history are NULL.
 */
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

/**
 * @brief Copy the most recent @p depth word IDs from the rolling window into @p out_sequence.
 *
 * Calls @c rolling_window_publish_snapshot_if_needed() then reads the current
 * double-buffer snapshot via @c rolling_window_snapshot_view(). Copies at most
 * @c min(@p depth, available) entries from the most recent portion of the
 * circular buffer into @p out_sequence in chronological order (oldest entry at
 * index 0, most recent at index @c actual_depth - 1).
 *
 * The available count is capped at @c min(@c total_executions, @c ROLLING_WINDOW_SIZE)
 * so callers requesting more history than has been recorded get what is available
 * rather than stale zeros.
 *
 * Entries are addressed via:
 * @code
 *   idx = (window_pos - actual_depth + i + ROLLING_WINDOW_SIZE) % ROLLING_WINDOW_SIZE
 * @endcode
 *
 * Returns 0 immediately if @p window, @p out_sequence are NULL or @p depth is 0.
 *
 * @param window       The @c RollingWindowOfTruth to sample.
 * @param depth        Maximum number of recent word IDs to copy.
 * @param out_sequence Caller-allocated output buffer of at least @p depth
 *                     @c uint32_t entries.
 * @return             Actual number of entries written to @p out_sequence;
 *                     ≤ @p depth.
 */
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

/**
 * @brief Return the word ID that appears most frequently in the rolling window.
 *
 * Obtains the current snapshot, allocates a zero-filled frequency array of
 * @p dict_size @c uint32_t counters via @c sf_calloc, and scans all
 * @c ROLLING_WINDOW_SIZE history slots to tally occurrences. Then performs a
 * linear scan for the maximum and frees the frequency array before returning.
 *
 * Returns 0 immediately (without allocating) if:
 * - @p window is NULL.
 * - @c window->is_warm is 0 (not enough history to be meaningful).
 * - The snapshot's @c is_warm flag is 0.
 * - @c sf_calloc fails.
 *
 * The returned word ID is the zero-based index into the dictionary's word-ID
 * space. Word ID 0 may be returned both as a legitimate hottest word and as
 * the not-warm sentinel; callers should check @c rolling_window_is_warm()
 * before relying on the result.
 *
 * @param window     The @c RollingWindowOfTruth to analyse.
 * @param dict_size  Size of the dictionary's word-ID space; bounds the
 *                   frequency array and the scan of the history.
 * @return           Word ID of the most-frequently-executed word; 0 if
 *                   the window is not warm or allocation fails.
 */
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

/**
 * @brief Count how many times the transition (word_a → word_b) occurs in the history.
 *
 * Scans all @c ROLLING_WINDOW_SIZE consecutive adjacent pairs in the current
 * snapshot (wrapping at the buffer end: pair @c i is
 * @c (history[i], history[(i+1) % ROLLING_WINDOW_SIZE])) and counts how many
 * pairs match @p word_a followed by @p word_b.
 *
 * Returns 0 if:
 * - @p window is NULL.
 * - @c window->is_warm is 0 (the history is too short to be meaningful).
 * - The snapshot's @c is_warm flag is 0.
 *
 * Note: because the history is circular and the scan is linear across all
 * slots regardless of @c effective_window_size, the count includes transitions
 * that span the buffer wrap-around boundary. This is intentional: the full
 * @c ROLLING_WINDOW_SIZE is used for transition counting.
 *
 * @param window  The @c RollingWindowOfTruth to query.
 * @param word_a  ID of the predecessor word.
 * @param word_b  ID of the successor word.
 * @return        Count of (word_a → word_b) transitions; 0 if not warm.
 */
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

/**
 * @brief Return whether the rolling window has accumulated enough history to be useful.
 *
 * Returns @c window->is_warm, which is set to 1 by
 * @c rolling_window_record_execution() once @c total_executions ≥ 1024.
 * Before that threshold the window is considered "cold" — its contents do not
 * yet represent a stable distribution of the program's word usage, and
 * analysis functions (@c rolling_window_find_hottest_word(),
 * @c rolling_window_count_transition(), adaptive sizing) return early with
 * conservative defaults when @c is_warm is 0.
 *
 * @param window  The @c RollingWindowOfTruth to query; NULL → returns 0.
 * @return        1 if the window is warm (≥ 1024 executions); 0 otherwise.
 */
int rolling_window_is_warm(const RollingWindowOfTruth* window)
{
    if (!window) return 0;
    return window->is_warm;
}

/**
 * @brief Allocate and return a one-page status summary for a @c RollingWindowOfTruth.
 *
 * Allocates a 1024-byte buffer via @c sf_malloc and fills it with a
 * multi-line report covering:
 * - @c total_executions — lifetime word-execution count.
 * - @c ROLLING_WINDOW_SIZE — compile-time capacity constant.
 * - Warm/cold status: "WARM (representative)" or "COLD (warming up)".
 * - @c window_pos / @c ROLLING_WINDOW_SIZE — current write-cursor position.
 *
 * Values are read from the current double-buffer snapshot obtained via
 * @c rolling_window_snapshot_view(); a lazy publish is triggered first by
 * @c rolling_window_publish_snapshot_if_needed().
 *
 * The caller is responsible for calling @c sf_free() on the returned pointer.
 * Returns NULL if @p window is NULL or @c sf_malloc fails.
 *
 * @param window  The @c RollingWindowOfTruth to describe.
 * @return        Heap-allocated NUL-terminated summary string; caller must
 *                @c sf_free() it. NULL on failure.
 */
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

/**
 * @brief Reset the rolling window to an empty cold state without freeing heap memory.
 *
 * Zeros @c execution_history via @c memset and resets the scalar fields:
 * @c window_pos = 0, @c total_executions = 0, @c is_warm = 0. Sets
 * @c snapshot_pending = 1 so that the next read will publish an empty snapshot.
 *
 * The three heap buffers (@c execution_history, @c snapshot_buffers[0/1]) are
 * preserved; only their contents are zeroed. Intended for benchmark harnesses
 * that need a clean baseline between test runs without the overhead of
 * @c rolling_window_cleanup() + @c rolling_window_init().
 *
 * @param window  The @c RollingWindowOfTruth to reset; no-op if NULL or
 *                @c execution_history is NULL.
 */
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

/**
 * @brief Free all heap memory held by a @c RollingWindowOfTruth.
 *
 * Calls @c sf_free() on @c execution_history and both @c snapshot_buffers[],
 * setting each to NULL after freeing. Does not zero the scalar fields; the
 * caller must not use @p window after this call without a subsequent
 * @c rolling_window_init().
 *
 * No-op if @p window is NULL. Each buffer pointer is individually NULL-checked
 * before freeing (defensive against partial-init failures from
 * @c rolling_window_init()).
 *
 * @param window  The @c RollingWindowOfTruth to clean up; no-op if NULL.
 */
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
 * @brief Warm-start the hot-words cache from POST execution history.
 *
 * Called exactly once after POST completes and before the REPL begins, so
 * that the first interactive command benefits from cache entries derived from
 * real POST execution data rather than starting cold. The algorithm:
 *
 * 1. Allocates a zero-filled frequency array of @c DICTIONARY_SIZE @c uint32_t
 *    counters via @c sf_calloc.
 * 2. Obtains the current snapshot and tallies occurrences of each word ID
 *    over all @c ROLLING_WINDOW_SIZE history slots.
 * 3. Computes a promotion threshold: words appearing more than the average
 *    frequency across all non-zero-frequency words are considered "hot".
 * 4. Iterates over the frequency array; for each word above the threshold,
 *    looks up the live @c DictEntry via @c vm_dictionary_lookup_by_word_id()
 *    (under @c vm->dict_lock), updates @c entry->execution_heat to the
 *    observed frequency count, and calls @c hotwords_cache_promote() to insert
 *    it into the cache.
 * 5. Logs the count of promoted words at @c LOG_INFO.
 *
 * No-op if @p window is NULL, not warm, @p vm is NULL, or @c vm->hotwords_cache
 * is NULL. Also no-op if the snapshot's @c is_warm flag is 0.
 *
 * @param window  Warm @c RollingWindowOfTruth containing POST execution data.
 * @param vm      The VM whose @c hotwords_cache is seeded and whose
 *                @c dict_lock is held during @c DictEntry updates.
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
 * @brief Warm-start pipelining context windows from POST execution history.
 *
 * Called exactly once after POST completes and before the REPL begins,
 * to populate each word's @c transition_metrics context window with real
 * predecessor data so that speculative prefetch can begin making informed
 * decisions on the first interactive command. The algorithm:
 *
 * 1. Allocates a sliding @p context[] buffer of @c context_window_size
 *    @c uint32_t entries (max(@c TRANSITION_WINDOW_SIZE, 2)) and initialises
 *    it with the first @c context_window_size entries from the history.
 * 2. Scans all @c ROLLING_WINDOW_SIZE consecutive adjacent pairs in the
 *    snapshot:
 *    - Looks up @c current_entry = @c vm_dictionary_lookup_by_word_id(current_id)
 *      (under @c vm->dict_lock, released immediately).
 *    - If @c current_entry->transition_metrics is set, calls
 *      @c transition_metrics_record_context() with the current @p context[]
 *      and @c next_id to populate the Phase 1 context-transition counter.
 *    - Shifts the @p context[] buffer left by one, appending @c next_id at
 *      the right end (sliding window advance).
 * 3. Logs seeded-words and seeded-transitions counts at @c LOG_INFO.
 *
 * No-op if @p window is NULL, not warm, @p vm is NULL, @c ENABLE_PIPELINING
 * is 0, or the context buffer allocation fails.
 *
 * @param window  Warm @c RollingWindowOfTruth containing POST execution data.
 * @param vm      The VM whose per-word @c transition_metrics structures are
 *                seeded; @c dict_lock is briefly held per lookup.
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

/**
 * @brief Export up to @p max_count entries from the execution history in chronological order.
 *
 * Linearises the internal circular buffer so callers get entries from oldest
 * to newest without knowing the internal @c window_pos cursor. Two cases:
 *
 * - **Pre-wrap** (@c total_executions < @c ROLLING_WINDOW_SIZE): the buffer
 *   has not yet wrapped; entries 0..@c total_executions-1 are valid and are
 *   copied contiguously from @c history[0].
 *
 * - **Post-wrap** (@c total_executions ≥ @c ROLLING_WINDOW_SIZE): the oldest
 *   entry is at @c history[window_pos]. Two @c memcpy calls stitch the
 *   two halves together:
 *   1. @c history[window_pos .. ROLLING_WINDOW_SIZE-1] (first part)
 *   2. @c history[0 .. window_pos-1] (second part, if @p export_count >
 *      @c first_part)
 *
 * @p export_count is @c min(@c total_executions, @p max_count). Returns 0 if
 * @p window, @p out_sequence are NULL, @p max_count is 0, the snapshot has no
 * history, or @c export_count is 0.
 *
 * @param window       The @c RollingWindowOfTruth to export.
 * @param out_sequence Caller-allocated output buffer of at least @p max_count
 *                     @c uint32_t entries.
 * @param max_count    Maximum number of entries to export.
 * @return             Actual number of entries written to @p out_sequence.
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

/**
 * @brief Estimate the fraction of transitions that carry unique contextual information.
 *
 * Measures how many of the recorded adjacent-pair transitions produce a non-zero
 * hash when the preceding @p test_window_size word IDs are incorporated:
 * @code
 *   pattern_hash = (... ((word[i-ws] × 31 + word[i-ws+1]) × 31 + ...) × 31 + word[i+1])
 *                  mod 0x7FFFFFFF
 * @endcode
 * A non-zero hash is counted as a "captured" pattern. The capture rate is:
 * @code
 *   rate = patterns_seen / total_transitions × 100  (clamped to [0, 100])
 * @endcode
 *
 * In practice, hash collisions with the zero sentinel are extraordinarily rare
 * (probability ~1/2^31), so nearly all transitions contribute to @c patterns_seen.
 * The function thus measures whether @p test_window_size is wide enough to produce
 * a non-trivial hash — a proxy for whether the window introduces meaningful context
 * beyond a single predecessor.
 *
 * Linearises the circular buffer via @c rolling_window_export_execution_history()
 * before scanning. Returns 0.0 if @p window or @p test_window_size < 1, if the
 * linearised history has fewer than 2 entries, or if @c sf_malloc fails.
 *
 * @param window           The @c RollingWindowOfTruth to analyse.
 * @param test_window_size Number of preceding word IDs to fold into each pattern hash.
 * @return                 Capture rate as a percentage in [0.0, 100.0].
 */
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
 * @brief Return the current pattern diversity score for the rolling window.
 *
 * Triggers a lazy snapshot publish via @c rolling_window_publish_snapshot_if_needed(),
 * acquires a read-side view via @c rolling_window_snapshot_view(), and delegates
 * to the internal @c rolling_window_measure_diversity_view() to count the number
 * of unique adjacent-pair transitions in the active window.
 *
 * Higher diversity means the window is observing a wide variety of word sequences,
 * indicating that the execution pattern is volatile and heat-stratified lookup is
 * more beneficial than stable-pattern nearest-first lookup. Lower diversity means
 * the program is executing a narrow, repetitive sequence — pattern knowledge is
 * concentrated and reliable.
 *
 * Used by @c dict_adaptive_optimization_pass() to select lookup strategy
 * (threshold = 70 unique transitions) and by @c rolling_window_run_adaptive_pass()
 * to calibrate @c effective_window_size.
 *
 * @param window  The @c RollingWindowOfTruth to measure.
 * @return        Count of unique adjacent-pair transitions in the active window;
 *                0 if the window has fewer than 2 executions or @p window is NULL.
 */
uint64_t rolling_window_measure_diversity(const RollingWindowOfTruth* window)
{
    rolling_window_publish_snapshot_if_needed((RollingWindowOfTruth*)window);
    rolling_window_view_t view;
    rolling_window_snapshot_view(window, &view);
    return rolling_window_measure_diversity_view(&view);
}

/**
 * @brief Adjust @c effective_window_size based on pattern diversity growth rate.
 *
 * Runs the core adaptive window logic using the snapshot @p view. Measures the
 * current diversity via @c rolling_window_measure_diversity_view(), computes the
 * fractional growth rate relative to @c window->last_pattern_diversity in Q16.16
 * fixed-point:
 * @code
 *   growth_rate_q48 = (diversity_delta << 16) / last_pattern_diversity
 * @endcode
 * and compares it against a threshold:
 * @code
 *   threshold_q48 = (ADAPTIVE_GROWTH_THRESHOLD << 16) / 100
 * @endcode
 *
 * **Shrink path** (@c growth_rate_q48 < @c threshold_q48): diversity is
 * plateauing; reducing the window saves scan work. The new size is:
 * @code
 *   new_size = max(effective_window_size × ADAPTIVE_SHRINK_RATE / 100,
 *                  ADAPTIVE_MIN_WINDOW_SIZE)
 * @endcode
 * Already at @c ADAPTIVE_MIN_WINDOW_SIZE → no change (logs "Floor Reached").
 *
 * **Grow path** (@c growth_rate_q48 ≥ @c threshold_q48): diversity is still
 * increasing; the larger window is capturing new patterns. The new size is:
 * @code
 *   new_size = min(effective_window_size × 100 / ADAPTIVE_SHRINK_RATE,
 *                  ROLLING_WINDOW_SIZE)
 * @endcode
 * Already at @c ROLLING_WINDOW_SIZE → no change (logs "Ceiling Reached").
 *
 * On the very first call after init, @c last_pattern_diversity == 0; only the
 * baseline is recorded and the function returns without resizing. All decisions
 * are logged at @c LOG_DEBUG for offline analysis.
 *
 * No-op if @p window or @p view are NULL or the view is not warm.
 *
 * @param window  The @c RollingWindowOfTruth whose @c effective_window_size
 *                and @c last_pattern_diversity are updated.
 * @param view    Current immutable snapshot; must be warm.
 */
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

/**
 * @brief Drain both pending flags — snapshot and adaptive — in one call.
 *
 * Called from both @c rolling_window_check_adaptive_shrink() and
 * @c rolling_window_service() as the single internal dispatch point:
 * 1. Calls @c rolling_window_publish_snapshot_if_needed() to flush any pending
 *    execution history into the double-buffer snapshot.
 * 2. Checks @c window->adaptive_pending; if set, acquires a view, runs
 *    @c rolling_window_run_adaptive_pass(), then clears @c adaptive_pending.
 *
 * No-op if @p window is NULL.
 *
 * @param window  The @c RollingWindowOfTruth to service.
 */
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

/**
 * @brief Force an immediate adaptive window check, bypassing the accumulator gate.
 *
 * Sets @c window->adaptive_pending = 1 unconditionally and then calls
 * @c rolling_window_service_internal() to drain it immediately. This allows
 * external callers (e.g. the heartbeat timer or a FORTH word) to trigger a
 * window-size evaluation outside the normal @c ADAPTIVE_CHECK_FREQUENCY cadence
 * — for example, after a major workload transition or at benchmark checkpoints.
 *
 * If the window is not warm the adaptive pass is a no-op internally.
 *
 * @param window  The @c RollingWindowOfTruth to check; no-op if NULL.
 */
void rolling_window_check_adaptive_shrink(RollingWindowOfTruth* window)
{
    if (!window)
        return;

    window->adaptive_pending = 1;
    rolling_window_service_internal(window);
}

/**
 * @brief Service the rolling window — publish pending snapshots and run adaptive sizing.
 *
 * The public entry point for the rolling window's periodic maintenance. Intended
 * to be called from the heartbeat tick handler or the REPL idle loop at a rate
 * determined by the caller (typically 100 Hz from the APIC timer ISR via
 * @c heartbeat_tick()). Delegates entirely to @c rolling_window_service_internal():
 * - Flushes the pending snapshot if @c snapshot_pending is set.
 * - Runs @c rolling_window_run_adaptive_pass() if @c adaptive_pending is set.
 *
 * Unlike @c rolling_window_check_adaptive_shrink(), this function does NOT force
 * @c adaptive_pending = 1; it only services flags that the record path has already
 * set. The normal adaptive-pass cadence is therefore governed entirely by
 * @c ADAPTIVE_CHECK_FREQUENCY executions between checks.
 *
 * @param window  The @c RollingWindowOfTruth to service; no-op if NULL.
 */
void rolling_window_service(RollingWindowOfTruth* window)
{
    rolling_window_service_internal(window);
}
