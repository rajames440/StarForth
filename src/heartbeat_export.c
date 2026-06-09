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

#include "vm.h"
#include "log.h"
#include "platform_time.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Capture current VM metrics into a heartbeat tick snapshot
 *
 * Collects 11 metrics per tick for multivariate dynamics analysis.
 * Called every heartbeat tick to record VM state.
 *
 * @param vm Pointer to the VM instance
 * @param snapshot Pointer to snapshot structure to populate
 */
void heartbeat_capture_tick_snapshot(VM* vm, HeartbeatTickSnapshot* snapshot)
{
    if (!vm || !snapshot)
        return;

    HeartbeatState* hb = &vm->heartbeat;

    /* Monotonic tick counter — use the real heartbeat counter, not tick_count_total */
    snapshot->tick_number = (uint32_t)hb->tick_count;

    /* Timing: heartbeats ARE the clock ("we count heartbeats not timer ticks").
     * sf_monotonic_ns() is unreliable in freestanding kernel (returns 0 on amd64,
     * raw counter on riscv64). Use nominal tick period instead. */
    snapshot->elapsed_ns = hb->tick_count * (uint64_t)HEARTBEAT_TICK_NS;

    static uint64_t last_tick_count = 0;
    uint64_t delta_ticks = hb->tick_count - last_tick_count;
    snapshot->tick_interval_ns = delta_ticks * (uint64_t)HEARTBEAT_TICK_NS;
    last_tick_count = hb->tick_count;

    /* Delta metrics — computed from per-VM counters using static last-value tracking.
     * On VM reset (current < last), treat current value as the full delta. */

    /* Cache metrics from hotwords cache */
    snapshot->cache_hits_delta = 0;  /* TODO: wire hotwords cache hit counter */
    snapshot->bucket_hits_delta = 0; /* TODO: wire bucket hit counter */

    /* Word execution delta — words executed since the previous heartbeat tick.
     * vm->heartbeat.words_executed is incremented by vm_core.c on every dispatch. */
    {
        static uint64_t last_words_executed = 0;
        uint64_t cur = hb->words_executed;
        snapshot->word_executions_delta = (uint32_t)(cur >= last_words_executed
                                                     ? cur - last_words_executed
                                                     : cur);
        last_words_executed = cur;
    }

    /* Hot word count - walk dictionary and count words above threshold */
    snapshot->hot_word_count = 0;
    uint64_t total_heat = 0;
    uint32_t word_count = 0;

    for (DictEntry* entry = vm->latest; entry; entry = entry->link)
    {
        if (entry->execution_heat > 0)
        {
            total_heat += entry->execution_heat;
            word_count++;

            /* Count as "hot" if above cache promotion threshold */
            if (entry->execution_heat >= 10) /* HEAT_CACHE_DEMOTION_THRESHOLD */
            {
                snapshot->hot_word_count++;
            }
        }
    }

    /* Average word heat (convert Q48.16 to double) */
    if (word_count > 0)
    {
        snapshot->avg_word_heat = (double)total_heat / (double)word_count / 65536.0;
    }
    else
    {
        snapshot->avg_word_heat = 0.0;
    }

    /* Rolling window size.
     * window_width  = L8's inferred effective size (the target).
     * actual_window_size = min(total_executions, effective_window_size):
     *   how many entries are ACTUALLY being analysed right now — grows toward
     *   window_width as data accumulates, then tracks it as L8 narrows/widens. */
    snapshot->window_width = vm->rolling_window.effective_window_size;
    {
        uint64_t fill = vm->rolling_window.total_executions;
        uint32_t eff  = vm->rolling_window.effective_window_size;
        snapshot->actual_window_size = (uint32_t)(fill < (uint64_t)eff ? fill : eff);
    }

    /* predicted_label_hits: ANOVA early-exit confirmations per tick.
     * Each early exit means the inference engine validated the current L8 config
     * as stable — the selector's choice correlated with subsequent execution patterns.
     * High rate = selector chose well and the system settled.
     * Low rate  = selector is still searching. */
    {
        static uint64_t last_early_exit_count = 0;
        uint64_t cur = hb->early_exit_count;
        snapshot->predicted_label_hits = (uint32_t)(cur >= last_early_exit_count
                                                     ? cur - last_early_exit_count
                                                     : cur);
        last_early_exit_count = cur;
    }

    /* Jitter: tick-based timing has no wall-clock deviation to measure.
     * Encode delta_ticks as a deviation: 0 means exactly on schedule (1 tick/call). */
    snapshot->estimated_jitter_ns = (delta_ticks > 1)
        ? (double)((delta_ticks - 1) * (uint64_t)HEARTBEAT_TICK_NS)
        : 0.0;
}

/**
 * @brief Emit a heartbeat tick snapshot as CSV row to stderr
 *
 * Outputs 11 comma-separated values (no header) for real-time streaming.
 * Called every heartbeat tick to emit metrics immediately.
 *
 * Format: tick_number,elapsed_ns,tick_interval_ns,cache_hits_delta,
 *         bucket_hits_delta,word_executions_delta,hot_word_count,
 *         avg_word_heat,window_width,actual_window_size,predicted_label_hits,estimated_jitter_ns
 *
 * @param vm Pointer to the VM instance (unused, for API consistency)
 * @param snapshot Pointer to the snapshot to emit
 */
void heartbeat_emit_tick_row(VM* vm, HeartbeatTickSnapshot* snapshot)
{
    (void)vm; /* Unused parameter */

    if (!snapshot)
        return;

    /* Emit CSV row to stderr - no header, just data */
    fprintf(stderr, "%u,%lu,%lu,%u,%u,%u,%lu,%.6f,%u,%u,%u,%.2f\n",
            snapshot->tick_number,
            snapshot->elapsed_ns,
            snapshot->tick_interval_ns,
            snapshot->cache_hits_delta,
            snapshot->bucket_hits_delta,
            snapshot->word_executions_delta,
            snapshot->hot_word_count,
            snapshot->avg_word_heat,
            snapshot->window_width,
            snapshot->actual_window_size,
            snapshot->predicted_label_hits,
            snapshot->estimated_jitter_ns);

    /* Ensure immediate output to stderr (unbuffered) */
    fflush(stderr);
}