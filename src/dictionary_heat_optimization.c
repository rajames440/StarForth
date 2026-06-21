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

  dictionary_heat_optimization.c - Phase 2: Heat-Aware Dictionary Lookup Optimization

  Implements inference-driven dictionary lookup optimizations based on execution
  heat patterns. Uses rolling window to understand which words are hot and how they
  co-execute, then reorganizes the dictionary to optimize lookup performance.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "vm.h"
#include "rolling_window_of_truth.h"
#include "dictionary_heat_optimization.h"
#include "log.h"
#include "platform_time.h"

/* sf_fc_list/sf_fc_count/sf_fc_cap are declared in dictionary_heat_optimization.h */

/* ============================================================================
 * Heat Percentile Calculation
 * ============================================================================
 */

/**
 * @brief qsort comparator for @c cell_t execution-heat values in ascending order.
 *
 * Compares two @c cell_t values passed as @c const @c void* pointers (the
 * signature required by @c qsort). Returns −1, 0, or +1 following C standard
 * three-way comparison convention. Used exclusively by
 * @c dict_update_heat_percentiles() to sort the scratch @c heats[] array
 * before computing the 25th, 50th, and 75th percentile thresholds.
 *
 * @param a  Pointer to the first @c cell_t value.
 * @param b  Pointer to the second @c cell_t value.
 * @return   −1 if @p *a < @p *b; +1 if @p *a > @p *b; 0 if equal.
 */
static int compare_heat_ascending(const void *a, const void *b) {
    cell_t heat_a = *(const cell_t *)a;
    cell_t heat_b = *(const cell_t *)b;
    if (heat_a < heat_b) return -1;
    if (heat_a > heat_b) return 1;
    return 0;
}

/**
 * @brief Collect dictionary execution heats and compute the 25th, 50th, and 75th percentiles.
 *
 * Traverses the full linked-list dictionary under @c vm->dict_lock to snapshot
 * every word's @c execution_heat into a stack-allocated scratch array (up to
 * @c DICTIONARY_SIZE entries). After releasing the lock it sorts the snapshot
 * with @c qsort + @c compare_heat_ascending and computes percentile indices
 * using integer arithmetic:
 * @code
 *   vm->heat_threshold_25th = heats[(count * 25) / 100];
 *   vm->heat_threshold_50th = heats[(count * 50) / 100];
 *   vm->heat_threshold_75th = heats[(count * 75) / 100];
 * @endcode
 *
 * If the dictionary contains no words all three thresholds are set to 0.
 * The lock is held only during traversal; sorting is performed outside the
 * critical section to minimise contention with the concurrent heartbeat decay
 * sweep that also walks the word list.
 *
 * The resulting thresholds are consumed by @c dict_find_word_heat_aware()
 * (which stratifies its search by heat tier) and by
 * @c dict_adaptive_optimization_pass() (which calls this function as its
 * first step every ~1 Hz).
 *
 * @param vm  The VM whose dictionary is sampled and whose
 *            @c heat_threshold_25th / @c heat_threshold_50th /
 *            @c heat_threshold_75th fields are updated.  No-op if NULL.
 */
void dict_update_heat_percentiles(VM *vm) {
    if (!vm) return;

    /* Collect all heats from dictionary */
    cell_t heats[DICTIONARY_SIZE];
    int count = 0;

    /* Protect traversal against concurrent heartbeat decay sweeps */
    sf_mutex_lock(&vm->dict_lock);
    for (DictEntry *w = vm->latest; w && count < DICTIONARY_SIZE; w = w->link) {
        heats[count++] = w->execution_heat;
    }
    sf_mutex_unlock(&vm->dict_lock);

    if (count == 0) {
        vm->heat_threshold_25th = 0;
        vm->heat_threshold_50th = 0;
        vm->heat_threshold_75th = 0;
        return;
    }

    /* Sort heats in ascending order */
    qsort(heats, count, sizeof(cell_t), compare_heat_ascending);

    /* Calculate percentiles */
    vm->heat_threshold_25th = heats[(count * 25) / 100];
    vm->heat_threshold_50th = heats[(count * 50) / 100];
    vm->heat_threshold_75th = heats[(count * 75) / 100];

    log_message(LOG_DEBUG,
                "Heat percentiles: 25th=%ld, 50th=%ld, 75th=%ld (from %d words)",
                vm->heat_threshold_25th,
                vm->heat_threshold_50th,
                vm->heat_threshold_75th,
                count);
}

/* ============================================================================
 * Heat-Aware Lookup Strategy
 * ============================================================================
 */

/**
 * @brief Three-pass heat-stratified dictionary lookup, hottest tier first.
 *
 * Searches the first-character bucket @c sf_fc_list[(unsigned char)name[0]]
 * for a word whose @c name and @c name_len match @p name / @p len, scanning
 * in three priority tiers ordered by execution heat:
 *
 * -# **Hot pass** (heat ≥ @c heat_threshold_75th):  words in the top 25%
 *    by execution frequency. Most FORTH programs spend the vast majority of
 *    ticks in a handful of hot primitives; checking this tier first yields a
 *    near-constant-time hit on steady-state workloads.
 * -# **Medium pass** (heat in [threshold_25th, threshold_75th)):  the middle
 *    50% of words. These are occasionally-called words that still appear often
 *    enough to warrant a dedicated pass before touching the long tail.
 * -# **Cold pass** (heat < @c heat_threshold_25th):  the rarely-executed
 *    bottom quartile. Words that have never been called also live here.
 *
 * Within each pass, matching uses the same cheap-first filter chain as the
 * standard lookup: @c name_len comparison → last-character byte check →
 * full @c memcmp. The last-character pre-filter is skipped for single-character
 * names.
 *
 * Returns @c NULL only if the word is absent from the bucket; all three passes
 * exhausted without a hit. Also returns @c NULL immediately if @p vm, @p name,
 * or @p len are invalid.
 *
 * Heat thresholds are updated lazily by @c dict_adaptive_optimization_pass()
 * (≈1 Hz); stale thresholds degrade to a full three-pass scan without
 * affecting correctness.
 *
 * @param vm    The VM; provides @c heat_threshold_* fields and the bucket table.
 * @param name  Word name to search for (not NUL-terminated; length in @p len).
 * @param len   Length of @p name in bytes; must be ≥ 1.
 * @return      Pointer to the matching @c DictEntry, or @c NULL if not found.
 */
DictEntry *dict_find_word_heat_aware(VM *vm, const char *name, size_t len) {
    if (!vm || !name || len == 0) return NULL;

    const unsigned char first = (unsigned char)name[0];
    DictEntry **bucket = sf_fc_list[first];
    size_t n = sf_fc_count[first];

    if (!bucket || n == 0) return NULL;

    const unsigned char last = (len > 1) ? (unsigned char)name[len - 1] : 0;

    /* Strategy: Search in order of heat percentiles (hot first) */

    /* First pass: Top 25% by heat (likely hits) */
    for (size_t i = 0; i < n; i++) {
        DictEntry *e = bucket[i];
        if (!e) continue;
        if (e->execution_heat < vm->heat_threshold_75th) continue;

        if (e->name_len != len) continue;
        if (len > 1 && (unsigned char)e->name[len - 1] != last) continue;
        if (memcmp(e->name, name, len) == 0) return e;
    }

    /* Second pass: Middle 50% by heat (25th-75th) */
    for (size_t i = 0; i < n; i++) {
        DictEntry *e = bucket[i];
        if (!e) continue;
        if (e->execution_heat < vm->heat_threshold_25th) continue;
        if (e->execution_heat >= vm->heat_threshold_75th) continue;

        if (e->name_len != len) continue;
        if (len > 1 && (unsigned char)e->name[len - 1] != last) continue;
        if (memcmp(e->name, name, len) == 0) return e;
    }

    /* Third pass: Bottom 25% (rarely executed) */
    for (size_t i = 0; i < n; i++) {
        DictEntry *e = bucket[i];
        if (!e) continue;
        if (e->execution_heat >= vm->heat_threshold_25th) continue;

        if (e->name_len != len) continue;
        if (len > 1 && (unsigned char)e->name[len - 1] != last) continue;
        if (memcmp(e->name, name, len) == 0) return e;
    }

    return NULL;
}

/* ============================================================================
 * Bucket Reorganization by Heat Patterns
 * ============================================================================
 */

/**
 * @brief qsort comparator for @c DictEntry* pointers in descending execution-heat order.
 *
 * Dereferences two @c DictEntry** pointers and compares their
 * @c execution_heat fields, returning −1 if the first entry is hotter,
 * +1 if the second is hotter, or 0 on a tie. Used exclusively by
 * @c dict_reorganize_buckets_by_heat() to sort each of the 256 first-character
 * hash buckets so that hot words appear at lower array indices, reducing the
 * average scan depth during the medium and cold passes of
 * @c dict_find_word_heat_aware().
 *
 * @param a  Pointer to a @c DictEntry* (first element).
 * @param b  Pointer to a @c DictEntry* (second element).
 * @return   −1 if @p a is hotter; +1 if @p b is hotter; 0 if equal.
 */
static int compare_dict_entry_heat_desc(const void *a, const void *b) {
    DictEntry *entry_a = *(DictEntry * const *)a;
    DictEntry *entry_b = *(DictEntry * const *)b;

    if (entry_a->execution_heat > entry_b->execution_heat) return -1;
    if (entry_a->execution_heat < entry_b->execution_heat) return 1;
    return 0;
}

/**
 * @brief Sort all 256 first-character hash buckets by descending execution heat.
 *
 * Holds @c vm->dict_lock for the entire operation to prevent concurrent
 * @c DictEntry frees (from word redefinition or GC) from invalidating the
 * pointer array while it is being sorted. For each non-empty bucket in
 * @c sf_fc_list[0..255]:
 *
 * 1. Reads @c sf_fc_count[bucket_id] and @c sf_fc_cap[bucket_id]; skips the
 *    bucket if @c count > @c cap (indicates a TOCTOU race — logs a warning).
 * 2. Allocates a temporary copy of the bucket pointer array.
 * 3. Re-verifies @c count has not changed (TOCTOU double-check); if it has,
 *    frees the copy and skips.
 * 4. Sorts the copy with @c qsort + @c compare_dict_entry_heat_desc so that
 *    the hottest @c DictEntry* comes first.
 * 5. Copies the sorted array back over the original bucket.
 * 6. Frees the temporary copy.
 *
 * After returning, @c vm->last_bucket_reorg_ns is updated to the completion
 * timestamp so that @c dict_adaptive_optimization_pass() can enforce its 1 Hz
 * throttle. The elapsed reorganization time is emitted at @c LOG_DEBUG.
 *
 * @param vm  The VM whose first-character hash buckets are reorganized.
 *            No-op if NULL. @c dict_lock is held during the entire scan.
 */
void dict_reorganize_buckets_by_heat(VM *vm) {
    if (!vm) return;

    uint64_t reorg_start = sf_monotonic_ns();

    /* Race fix: Hold dict_lock during entire reorganization (2025-12-09)
     * Without this, main thread can free DictEntry objects while we're sorting pointers */
    sf_mutex_lock(&vm->dict_lock);

    /* Reorganize each bucket by heat (hottest words first) */
    for (int bucket_id = 0; bucket_id < 256; bucket_id++) {
        if (!sf_fc_list[bucket_id] || sf_fc_count[bucket_id] == 0) continue;

        /* ASan fix: Verify count doesn't exceed capacity before accessing (2025-12-09)
         * Race condition: Main thread may have changed count/capacity while we read it */
        size_t count = sf_fc_count[bucket_id];
        size_t cap = sf_fc_cap[bucket_id];
        if (count > cap) {
            log_message(LOG_WARN,
                        "dict_reorganize_buckets_by_heat: skipping bucket %d - count=%zu exceeds capacity=%zu (race condition)",
                        bucket_id, count, cap);
            continue;
        }

        /* Make a copy of bucket pointers */
        DictEntry **bucket_copy = malloc(count * sizeof(DictEntry *));
        if (!bucket_copy) continue;

        /* Re-verify count hasn't changed (TOCTOU protection) */
        size_t count_recheck = sf_fc_count[bucket_id];
        if (count_recheck != count || count_recheck > cap) {
            log_message(LOG_WARN,
                        "dict_reorganize_buckets_by_heat: bucket %d count changed (%zu -> %zu), aborting",
                        bucket_id, count, count_recheck);
            free(bucket_copy);
            continue;
        }

        memcpy(bucket_copy, sf_fc_list[bucket_id], count * sizeof(DictEntry *));

        /* Sort by descending heat (use cached count to avoid TOCTOU race) */
        qsort(bucket_copy, count, sizeof(DictEntry *),
              compare_dict_entry_heat_desc);

        /* Copy back to original bucket (still use cached count) */
        memcpy(sf_fc_list[bucket_id], bucket_copy, count * sizeof(DictEntry *));

        free(bucket_copy);
    }

    sf_mutex_unlock(&vm->dict_lock);

    uint64_t reorg_end = sf_monotonic_ns();
    vm->last_bucket_reorg_ns = reorg_end;

    log_message(LOG_DEBUG,
                "Dictionary buckets reorganized by heat in %.1f μs",
                (double)(reorg_end - reorg_start) / 1000.0);
}

/* ============================================================================
 * Adaptive Optimization Pass
 * ============================================================================
 */

/**
 * @brief Throttled adaptive optimization pass — runs at most once per second.
 *
 * Called from the interpreter loop (or heartbeat) on every word dispatch; the
 * 1 Hz gate (@c now_ns - @c vm->last_bucket_reorg_ns < 1,000,000,000) ensures
 * the three expensive sub-steps run at a rate that is imperceptible to FORTH
 * execution while still keeping the dictionary layout fresh:
 *
 * 1. **Update heat percentiles** — calls @c dict_update_heat_percentiles() to
 *    re-sample @c execution_heat across all words and compute the 25th / 50th /
 *    75th thresholds used by the stratified lookup.
 *
 * 2. **Reorganize buckets** — calls @c dict_reorganize_buckets_by_heat() to
 *    sort each of the 256 first-character hash buckets so that the hottest
 *    words sit at the lowest array indices, shortening the average scan depth
 *    during the hot-tier pass.
 *
 * 3. **Select lookup strategy** — queries the rolling window for the current
 *    pattern-diversity score via @c rolling_window_measure_diversity():
 *    - Diversity > 70 (high churn in which words are called) → strategy 1
 *      (@c heat-aware lookup, @c dict_find_word_heat_aware()), where the heat
 *      stratification most pays off.
 *    - Diversity ≤ 70 (stable repetitive pattern) → strategy 0 (naive
 *      newest-first bucket scan), which is already nearly O(1) for programs
 *      that call the same small word set repeatedly.
 *    The chosen strategy is stored in @c vm->lookup_strategy and read by the
 *    interpreter's dictionary lookup dispatch.
 *
 * @param vm  The VM to optimise. No-op if NULL or if less than 1 second has
 *            elapsed since the last pass (@c vm->last_bucket_reorg_ns).
 */
void dict_adaptive_optimization_pass(VM *vm) {
    if (!vm) return;

    uint64_t now_ns = sf_monotonic_ns();

    /* Run optimization every 1 second */
    if (now_ns - vm->last_bucket_reorg_ns < 1000000000ULL) {
        return; /* Too soon, skip */
    }

    /* Step 1: Update heat percentiles based on current dictionary state */
    dict_update_heat_percentiles(vm);

    /* Step 2: Reorganize buckets by heat order (hot words first) */
    dict_reorganize_buckets_by_heat(vm);

    /* Step 3: Select lookup strategy based on pattern diversity */
    uint64_t pattern_diversity = rolling_window_measure_diversity(&vm->rolling_window);

    /* High diversity (0.7+): Use heat-aware lookup (patterns change frequently) */
    if (pattern_diversity > 70) {
        vm->lookup_strategy = 1; /* Heat-aware */
        log_message(LOG_DEBUG,
                    "High pattern diversity (%.0f%%), using heat-aware lookup",
                    (double)pattern_diversity);
    }
    /* Medium diversity: Use naive newest-first (patterns reasonably stable) */
    else {
        vm->lookup_strategy = 0; /* Naive */
        log_message(LOG_DEBUG,
                    "Stable patterns (%.0f%% diversity), using naive lookup",
                    (double)pattern_diversity);
    }
}
