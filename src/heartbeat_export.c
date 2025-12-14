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

    /* Monotonic tick counter */
    snapshot->tick_number = (uint32_t)hb->tick_count_total;

    /* Timing metrics */
    uint64_t now_ns = sf_monotonic_ns();
    snapshot->elapsed_ns = now_ns - hb->run_start_ns;

    /* Calculate actual tick interval from previous tick */
    static uint64_t last_tick_ns = 0;
    if (last_tick_ns == 0) {
        last_tick_ns = hb->run_start_ns;
    }
    snapshot->tick_interval_ns = now_ns - last_tick_ns;
    last_tick_ns = now_ns;

    /* Delta metrics - compute from VM counters
     * NOTE: These require tracking "last tick" values to compute deltas.
     * For now, we'll use placeholder logic - the actual delta tracking
     * should be added to vm_tick() or similar. */

    /* Cache metrics from hotwords cache */
    snapshot->cache_hits_delta = 0;  /* TODO: Track delta in vm_tick() */
    snapshot->bucket_hits_delta = 0; /* TODO: Track delta in vm_tick() */

    /* Word execution delta */
    snapshot->word_executions_delta = 0; /* TODO: Track delta in vm_tick() */

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

    /* Rolling window size */
    snapshot->window_width = vm->rolling_window.effective_window_size;

    /* Pipelining metrics */
    snapshot->predicted_label_hits = 0; /* TODO: Extract from pipelining metrics */

    /* Jitter estimation - deviation from nominal tick interval */
    uint64_t nominal_tick_ns = hb->tick_target_ns ? hb->tick_target_ns : HEARTBEAT_TICK_NS;
    if (snapshot->tick_interval_ns > nominal_tick_ns)
    {
        snapshot->estimated_jitter_ns = (double)(snapshot->tick_interval_ns - nominal_tick_ns);
    }
    else
    {
        snapshot->estimated_jitter_ns = (double)(nominal_tick_ns - snapshot->tick_interval_ns);
    }
}

/**
 * @brief Emit a heartbeat tick snapshot as CSV row to stderr
 *
 * Outputs 11 comma-separated values (no header) for real-time streaming.
 * Called every heartbeat tick to emit metrics immediately.
 *
 * Format: tick_number,elapsed_ns,tick_interval_ns,cache_hits_delta,
 *         bucket_hits_delta,word_executions_delta,hot_word_count,
 *         avg_word_heat,window_width,predicted_label_hits,estimated_jitter_ns
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
    fprintf(stderr, "%u,%lu,%lu,%u,%u,%u,%lu,%.6f,%u,%u,%.2f\n",
            snapshot->tick_number,
            snapshot->elapsed_ns,
            snapshot->tick_interval_ns,
            snapshot->cache_hits_delta,
            snapshot->bucket_hits_delta,
            snapshot->word_executions_delta,
            snapshot->hot_word_count,
            snapshot->avg_word_heat,
            snapshot->window_width,
            snapshot->predicted_label_hits,
            snapshot->estimated_jitter_ns);

    /* Ensure immediate output to stderr (unbuffered) */
    fflush(stderr);
}