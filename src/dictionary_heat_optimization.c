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

/* External dictionary bucket structures (from dictionary_management.c) */
#ifndef SF_FC_BUCKETS
#define SF_FC_BUCKETS 256
#endif
extern DictEntry** sf_fc_list[SF_FC_BUCKETS];
extern size_t sf_fc_count[SF_FC_BUCKETS];
extern size_t sf_fc_cap[SF_FC_BUCKETS];  /* ASan fix: capacity check (2025-12-09) */

/* ============================================================================
 * Heat Percentile Calculation
 * ============================================================================
 */

/**
 * Compare function for qsort (ascending order)
 */
static int compare_heat_ascending(const void *a, const void *b) {
    cell_t heat_a = *(const cell_t *)a;
    cell_t heat_b = *(const cell_t *)b;
    if (heat_a < heat_b) return -1;
    if (heat_a > heat_b) return 1;
    return 0;
}

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
        if (e->execution_heat < vm->heat_threshold_75th) continue;

        if (e->name_len != len) continue;
        if (len > 1 && (unsigned char)e->name[len - 1] != last) continue;
        if (memcmp(e->name, name, len) == 0) return e;
    }

    /* Second pass: Middle 50% by heat (25th-75th) */
    for (size_t i = 0; i < n; i++) {
        DictEntry *e = bucket[i];
        if (e->execution_heat < vm->heat_threshold_25th) continue;
        if (e->execution_heat >= vm->heat_threshold_75th) continue;

        if (e->name_len != len) continue;
        if (len > 1 && (unsigned char)e->name[len - 1] != last) continue;
        if (memcmp(e->name, name, len) == 0) return e;
    }

    /* Third pass: Bottom 25% (rarely executed) */
    for (size_t i = 0; i < n; i++) {
        DictEntry *e = bucket[i];
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
 * Compare function for bucket reordering (descending heat order)
 */
static int compare_dict_entry_heat_desc(const void *a, const void *b) {
    DictEntry *entry_a = *(DictEntry * const *)a;
    DictEntry *entry_b = *(DictEntry * const *)b;

    if (entry_a->execution_heat > entry_b->execution_heat) return -1;
    if (entry_a->execution_heat < entry_b->execution_heat) return 1;
    return 0;
}

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
