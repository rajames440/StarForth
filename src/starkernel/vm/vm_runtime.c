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

#include "../include/vm.h"
#include "../include/log.h"
#include "../include/rolling_window_of_truth.h"
#include "../include/dictionary_heat_optimization.h"
#include "../include/inference_engine.h"
#include "../include/physics_metadata.h"
#include "../include/physics_hotwords_cache.h"
#include "../include/ssm_jacquard.h"
#include "vm_internal.h"

#include <errno.h>
#include <string.h>
#include <time.h>

uint32_t heartbeat_snapshot_index_load(const volatile uint32_t *ptr)
{
#if defined(__GNUC__)
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
#else
    return *ptr;
#endif
}

void heartbeat_snapshot_index_store(volatile uint32_t *ptr, uint32_t value)
{
#if defined(__GNUC__)
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
#else
    *ptr = value;
#endif
}

void heartbeat_publish_snapshot(VM *vm)
{
    if (!vm)
        return;

    uint32_t current = heartbeat_snapshot_index_load(&vm->heartbeat.snapshot_index) & 1u;
    uint32_t next = current ^ 1u;
    HeartbeatSnapshot *snapshot = &vm->heartbeat.snapshots[next];

    snapshot->published_tick = vm->heartbeat.tick_count;
    snapshot->published_ns = vm_monotonic_ns(vm);
    snapshot->window_width = vm->rolling_window.effective_window_size;
    snapshot->decay_slope_q48 = vm->decay_slope_q48;
    snapshot->hot_word_count = vm->hot_word_count_at_check;
    snapshot->stale_word_count = vm->stale_word_count_at_check;
    snapshot->total_heat = vm->total_heat_at_last_check;

    heartbeat_snapshot_index_store(&vm->heartbeat.snapshot_index, next);
}

/* ====================== VM Heartbeat (Time-Driven Tuning) ======================= */

/**
 * @brief Central heartbeat dispatcher for time-driven tuning operations
 *
 * Aggregates all periodic optimization tasks (Loop #3 and Loop #5) into one place.
 * Designed as plugin architecture - new tuning operations can be added as plugins.
 *
 * Options for integration:
 * - Synchronous (now): Called from main execution loop, every N executions
 * - Background thread (future): Runs in separate thread, decoupled from VM execution
 *
 * @param vm Pointer to VM instance
 */
void vm_tick(VM* vm)
{
    if (!vm || !vm->heartbeat.heartbeat_enabled)
        return;

    vm->heartbeat.tick_count++;

    /* Unified Inference Engine (Phase 2: Replaces Loops #3 & #5)
     * Runs every HEARTBEAT_INFERENCE_FREQUENCY ticks to infer optimal:
     * - Window width (via variance inflection detection)
     * - Decay slope (via exponential regression on heat trajectory)
     */
    if ((vm->heartbeat.tick_count - vm->heartbeat.last_inference_tick) >= HEARTBEAT_INFERENCE_FREQUENCY)
    {
        vm_tick_inference_engine(vm);
    }

    /* Plugin 2: System State Monitoring (Future) */
    /* vm_tick_system_monitor(vm); */

    /* Plugin 3: Formal Verification State Update (Future) */
    /* vm_tick_formal_state_sync(vm); */
}

/**
 * @brief Loop #5: Context-aware window tuning via binary chop search
 *
 * Uses prefetch accuracy to guide window size adaptation.
 * Binary search converges on optimal effective_window_size for current workload.
 *
 * @param vm Pointer to VM instance
 */
void vm_tick_window_tuner(VM* vm)
{
    if (!vm || !vm->rolling_window.is_warm || !ENABLE_PIPELINING)
        return;

    RollingWindowOfTruth *window = &vm->rolling_window;
    PipelineGlobalMetrics *metrics = &vm->pipeline_metrics;

    /* Calculate current prefetch accuracy */
    if (metrics->prefetch_attempts == 0)
        return;  /* Not enough data yet */

    double current_accuracy = (double)metrics->prefetch_hits / (double)metrics->prefetch_attempts;

    /* Binary chop suggests next window size to try */
    uint32_t suggested_size = window->effective_window_size;  /* Default: no change */

    if (metrics->window_tuning_checks == 0)
    {
        /* First check: try shrinking by 25% */
        suggested_size = (window->effective_window_size * 75) / 100;
    }
    else
    {
        /* Compare current accuracy to last check */
        double accuracy_delta = current_accuracy - metrics->last_checked_accuracy;

        if (accuracy_delta > 0.01)  /* Improvement threshold: 1% */
        {
            /* Accuracy improved! Try shrinking more */
            uint32_t smaller = (window->effective_window_size * 75) / 100;
            suggested_size = (smaller > ADAPTIVE_MIN_WINDOW_SIZE) ? smaller : ADAPTIVE_MIN_WINDOW_SIZE;
        }
        else if (accuracy_delta < -0.01)
        {
            /* Accuracy degraded. Try growing instead */
            uint32_t larger = (window->effective_window_size * 133) / 100;  /* Grow by ~33% */
            suggested_size = (larger < ROLLING_WINDOW_SIZE) ? larger : ROLLING_WINDOW_SIZE;
        }
        /* else: Plateau, stick with current size */
    }

    /* Apply if different */
    if (suggested_size != window->effective_window_size)
    {
        log_message(LOG_INFO,
                   "HEARTBEAT[window]: %u → %u (accuracy %.2f%%, %lu/%lu prefetch hits)",
                   window->effective_window_size,
                   suggested_size,
                   current_accuracy * 100.0,
                   metrics->prefetch_hits,
                   metrics->prefetch_attempts);

        window->effective_window_size = suggested_size;
    }

    /* Record for next iteration */
    metrics->last_checked_window_size = window->effective_window_size;
    metrics->last_checked_accuracy = current_accuracy;
    metrics->window_tuning_checks++;
}

/**
 * @brief Loop #3: Heat decay slope validation via periodic measurement
 *
 * Validates that linear decay is actually helping optimize dictionary caching.
 * Measures stale word ratio, hot word count, and average heat distribution.
 *
 * @param vm Pointer to VM instance
 */
void vm_tick_slope_validator(VM* vm)
{
    if (!vm)
        return;

    /* Collect snapshot of current state */
    uint64_t hot_word_count = 0;
    uint64_t stale_word_count = 0;
    uint64_t total_heat = 0;
    uint32_t word_count = 0;

    /* Scan dictionary and categorize words by heat level */
    sf_mutex_lock(&vm->dict_lock);
    for (DictEntry *e = vm->latest; e != NULL; e = e->link)
    {
        if (e->execution_heat > HOTWORDS_EXECUTION_HEAT_THRESHOLD)
            hot_word_count++;
        else if (e->execution_heat > 0 && e->execution_heat < 10)
            stale_word_count++;

        total_heat += e->execution_heat;
        word_count++;
    }
    sf_mutex_unlock(&vm->dict_lock);

    double avg_heat = (word_count > 0) ? (double)total_heat / (double)word_count : 0.0;
    double stale_ratio = (word_count > 0) ? (double)stale_word_count / (double)word_count : 0.0;

    /* === LOOP #3: INFERENCE ENGINE ===
     * Compare current measurements to baseline from last check
     * Decide whether decay is too fast, too slow, or optimal
     */
    int new_slope_direction = 0; /* -1: decrease slope, 0: stable, +1: increase slope */

    sf_mutex_lock(&vm->tuning_lock);

    if (vm->word_count_at_check > 0)
    {
        /* Calculate trend in stale words: absolute count delta indicates accumulation/clearing */
        int64_t stale_delta = (int64_t)stale_word_count - (int64_t)vm->stale_word_count_at_check;

        /* INFERENCE: If stale words INCREASING, decay is too slow → increase slope */
        /* If stale words DECREASING, decay is working (or too fast) → monitor */
        if (stale_delta > 5)  /* Threshold: 5+ additional stale words signals problem */
        {
            /* Stale words accumulating: decay is insufficient */
            new_slope_direction = 1;
            log_message(LOG_INFO,
                       "HEARTBEAT[slope]: stale_delta=%ld, decay TOO SLOW, increase slope",
                       (long)stale_delta);
        }
        else if (stale_delta < -5)  /* Threshold: 5+ fewer stale words signals clearing */
        {
            /* Stale words clearing: decay is aggressive (potentially too fast) */
            /* Only decrease slope if avg_heat is dropping below target */
            if (avg_heat < 5.0)
            {
                new_slope_direction = -1;
                log_message(LOG_INFO,
                           "HEARTBEAT[slope]: stale_delta=%ld, avg_heat=%.1f, decay TOO FAST, decrease slope",
                           (long)stale_delta, avg_heat);
            }
            else
            {
                log_message(LOG_INFO,
                           "HEARTBEAT[slope]: stale_delta=%ld, decay working, hold slope",
                           (long)stale_delta);
            }
        }
        else
        {
            log_message(LOG_INFO,
                       "HEARTBEAT[slope]: stale_delta=%ld (stable), hold slope",
                       (long)stale_delta);
        }
    }
    else
    {
        log_message(LOG_INFO,
                   "HEARTBEAT[slope]: baseline measurement - hot_words=%lu, stale_ratio=%.2f%%, avg_heat=%.1f",
                   hot_word_count,
                   stale_ratio * 100.0,
                   avg_heat);
    }

    /* === APPLY SLOPE ADJUSTMENT ===
     * Only adjust if direction changed (hysteresis to prevent oscillation)
     */
    if (new_slope_direction != vm->decay_slope_direction && new_slope_direction != 0)
    {
        vm->decay_slope_direction = new_slope_direction;

        /* Calculate adjustment in Q48.16: 5% change per cycle */
        uint64_t adjustment = (vm->decay_slope_q48 * 5) / 100;
        if (adjustment < 1) adjustment = 1; /* Minimum increment */

        uint64_t old_slope = vm->decay_slope_q48;
        if (new_slope_direction > 0)
        {
            vm->decay_slope_q48 += adjustment;
        }
        else if (new_slope_direction < 0)
        {
            vm->decay_slope_q48 = (vm->decay_slope_q48 > adjustment)
                                  ? (vm->decay_slope_q48 - adjustment)
                                  : 1; /* Floor at 1 */
        }

        /* Log the adjustment as human-readable double */
        double old_slope_dbl = (double)old_slope / 65536.0;
        double new_slope_dbl = (double)vm->decay_slope_q48 / 65536.0;
        log_message(LOG_INFO,
                   "HEARTBEAT[slope]: ADJUSTED slope from %.3f to %.3f (direction=%d)",
                   old_slope_dbl,
                   new_slope_dbl,
                   new_slope_direction);
    }

    /* Store baseline for next comparison */
    vm->hot_word_count_at_check = hot_word_count;
    vm->total_heat_at_last_check = total_heat;
    vm->stale_word_count_at_check = stale_word_count;
    vm->word_count_at_check = word_count;

    sf_mutex_unlock(&vm->tuning_lock);
}

static void vm_tick_apply_background_decay(VM *vm, uint64_t now_ns)
{
    if (!vm)
        return;

    sf_mutex_lock(&vm->dict_lock);

    DictEntry *cursor = NULL;
    if (vm->heartbeat_decay_cursor_id != WORD_ID_INVALID)
        cursor = vm_dictionary_lookup_by_word_id(vm, vm->heartbeat_decay_cursor_id);
    if (!cursor)
        cursor = vm->latest;

    size_t processed = 0;
    while (cursor && processed < HEARTBEAT_DECAY_BATCH)
    {
        uint64_t last_decay = cursor->physics.last_decay_ns;
        if (last_decay == 0)
            last_decay = cursor->physics.last_active_ns;

        if (last_decay > 0 && now_ns > last_decay)
        {
            uint64_t elapsed_ns = now_ns - last_decay;
            physics_metadata_apply_linear_decay(cursor, elapsed_ns, vm);
            cursor->physics.last_decay_ns = now_ns;
        }

        cursor = cursor->link;
        processed++;
    }

    vm->heartbeat_decay_cursor_id = (cursor && cursor->word_id != WORD_ID_INVALID)
                                    ? cursor->word_id
                                    : WORD_ID_INVALID;

    sf_mutex_unlock(&vm->dict_lock);
}

/**
 * @brief L8 FINAL INTEGRATION: Jacquard mode selector heartbeat update
 *
 * Collects metrics from L1-L7 physics layers and feeds them to the L8 Jacquard
 * mode selector. L8 then chooses the optimal configuration mode based on workload
 * characteristics and applies it to the runtime.
 *
 * This is the SOLE policy engine - all adaptive decisions flow through L8.
 *
 * @param vm Pointer to VM instance
 */
static void vm_heartbeat_update_l8(VM *vm)
{
    if (!vm || !vm->ssm_l8_state)
        return;

    ssm_l8_state_t *l8 = (ssm_l8_state_t*)vm->ssm_l8_state;

    /* === Collect L1-L7 Metrics and Convert to L8 Format === */
    ssm_l8_metrics_t metrics = {0};

    /* L2: Rolling window entropy (normalized diversity) */
    /* Use unique word count / total executions as entropy proxy */
    uint32_t unique_words = (vm->rolling_window.total_executions > 0) ?
                            (uint32_t)(vm->rolling_window.effective_window_size) : 0;
    metrics.entropy = (double)unique_words / (double)(ROLLING_WINDOW_SIZE > 0 ? ROLLING_WINDOW_SIZE : 1);

    /* L4: Pipelining metrics → CV (coefficient of variation) */
    if (vm->pipeline_metrics.prefetch_attempts > 0) {
        double accuracy = (double)vm->pipeline_metrics.prefetch_hits / (double)vm->pipeline_metrics.prefetch_attempts;
        metrics.cv = 1.0 - accuracy; /* Invert: high accuracy = low CV (stability) */
    } else {
        metrics.cv = 0.5; /* Default moderate CV if no data */
    }

    /* L3: Decay slope → temporal locality signal (Q48.16 → double) */
    double decay_slope = (double)vm->decay_slope_q48 / 65536.0;
    metrics.temporal_decay = (decay_slope > 0.0) ? (1.0 / decay_slope) : 0.0; /* Higher slope = stronger temporal locality */
    if (metrics.temporal_decay > 1.0) metrics.temporal_decay = 1.0; /* Clamp to [0,1] */

    /* L5/L6: Inference stability for hysteresis */
    sf_mutex_lock(&vm->tuning_lock);
    int early_exited = (vm->last_inference_outputs && vm->last_inference_outputs->early_exited);
    sf_mutex_unlock(&vm->tuning_lock);
    metrics.stability_score = early_exited ? 0.9 : 0.1; /* High stability if inference early-exited */

    /* === Run L8 Mode Selection === */
    ssm_l8_mode_t old_mode = l8->current_mode;
    ssm_l8_update(&metrics, l8);

    /* === Apply Mode Configuration (if changed) === */
    if (l8->current_mode != old_mode) {
        ssm_config_t *config = (ssm_config_t*)vm->ssm_config;
        ssm_apply_mode(l8, config);

        log_message(LOG_INFO,
                   "L8[JACQUARD]: Mode %s → %s (entropy=%.2f, cv=%.2f, temporal=%.2f)",
                   ssm_l8_mode_name(old_mode),
                   ssm_l8_mode_name(l8->current_mode),
                   metrics.entropy,
                   metrics.cv,
                   metrics.temporal_decay);
    }
}

void vm_heartbeat_run_cycle(VM *vm)
{
    if (!vm || !vm->heartbeat.heartbeat_enabled)
        return;

    /* L8 FINAL INTEGRATION: Core heartbeat operations */
    vm_tick(vm);
    vm_tick_apply_background_decay(vm, vm_monotonic_ns(vm));
    rolling_window_service(&vm->rolling_window);
    dict_adaptive_optimization_pass(vm);  /* Adaptive dictionary optimization */

    /* L8 FINAL INTEGRATION: Jacquard mode selector - the sole policy engine */
    vm_heartbeat_update_l8(vm);

    heartbeat_publish_snapshot(vm);

    /* Phase 2: Real-time heartbeat metrics emission */
    HeartbeatTickSnapshot tick_snapshot;
    heartbeat_capture_tick_snapshot(vm, &tick_snapshot);
    heartbeat_emit_tick_row(vm, &tick_snapshot);
}

#if HEARTBEAT_THREAD_ENABLED
void* heartbeat_thread_main(void *arg)
{
    VM *vm = (VM*)arg;
    if (!vm || !vm->heartbeat.worker)
        return NULL;

    HeartbeatWorker *worker = vm->heartbeat.worker;
    worker->running = 1;

    /* IMPORTANT: No startup delay to allow heartbeat to emit during short DoE runs.
     * Original 50ms delay avoided race conditions during word registration, but prevented
     * real-time metrics emission in fast-completing tests. */

    while (!worker->stop_requested)
    {
        vm_heartbeat_run_cycle(vm);

        uint64_t tick_ns = worker->tick_ns ? worker->tick_ns : HEARTBEAT_TICK_NS;
        struct timespec req;
        req.tv_sec = (time_t)(tick_ns / 1000000000ULL);
        req.tv_nsec = (long)(tick_ns % 1000000000ULL);

        while (!worker->stop_requested && nanosleep(&req, &req) == -1 && errno == EINTR)
        {
            /* Retry with remaining time */
        }
    }

    worker->running = 0;
    return NULL;
}
#endif

void vm_snapshot_read(const VM* vm, HeartbeatSnapshot* out_snapshot)
{
    if (!vm || !out_snapshot)
        return;

    uint32_t index = heartbeat_snapshot_index_load(&vm->heartbeat.snapshot_index) & 1u;
    *out_snapshot = vm->heartbeat.snapshots[index];
}

/**
 * @brief Phase 2: Unified Inference Engine - Adaptive Window & Decay Slope Tuning
 *
 * Coordinates inference on rolling window of truth to determine:
 * - Optimal adaptive window width (via variance inflection detection)
 * - Optimal decay slope (via exponential regression on heat trajectory)
 *
 * Uses ANOVA early-exit to skip full inference when variance is stable (<5% change).
 * All math uses Q48.16 fixed-point (integer-only, no floating-point).
 *
 * Replaces legacy vm_tick_window_tuner() and vm_tick_slope_validator().
 *
 * @param vm Pointer to VM instance
 */
void vm_tick_inference_engine(VM* vm)
{
    if (!vm || !vm->heartbeat.heartbeat_enabled || !vm->rolling_window.is_warm)
        return;

    /* DoE counter: inference engine invocations */
    vm->heartbeat.inference_run_count++;

    rolling_window_service(&vm->rolling_window);

    /* Allocate InferenceOutputs if needed - protect with tuning_lock against race with doe_metrics */
    sf_mutex_lock(&vm->tuning_lock);
    if (!vm->last_inference_outputs)
    {
        vm->last_inference_outputs = vm_host_alloc(vm, sizeof(InferenceOutputs), sizeof(void*));
        if (!vm->last_inference_outputs)
        {
            sf_mutex_unlock(&vm->tuning_lock);
            log_message(LOG_ERROR, "INFERENCE: Failed to allocate InferenceOutputs");
            return;
        }
        memset(vm->last_inference_outputs, 0, sizeof(InferenceOutputs));
    }
    sf_mutex_unlock(&vm->tuning_lock);

    /* === Collect Current Dictionary Metrics === */
    uint64_t hot_word_count = 0;
    uint64_t stale_word_count = 0;
    uint64_t total_heat = 0;
    uint32_t word_count = 0;

    sf_mutex_lock(&vm->dict_lock);
    for (DictEntry *e = vm->latest; e != NULL; e = e->link)
    {
        if (e->execution_heat > HOTWORDS_EXECUTION_HEAT_THRESHOLD)
            hot_word_count++;
        else if (e->execution_heat > 0 && e->execution_heat < 10)
            stale_word_count++;

        total_heat += e->execution_heat;
        word_count++;
    }
    sf_mutex_unlock(&vm->dict_lock);

    /* === Populate InferenceInputs === */
    InferenceInputs inference_inputs = {
        .window = &vm->rolling_window,
        .vm = vm,  /* Required for dictionary lookups in extract_heat_trajectory */
        .trajectory_length = (vm->rolling_window.window_pos > 0)
                             ? vm->rolling_window.window_pos
                             : vm->rolling_window.total_executions,
        .prefetch_hits = vm->pipeline_metrics.prefetch_hits,
        .prefetch_attempts = vm->pipeline_metrics.prefetch_attempts,
        .hot_word_count = hot_word_count,
        .stale_word_count = stale_word_count,
        .total_heat = total_heat,
        .word_count = word_count,
        .last_total_heat = vm->total_heat_at_last_check,
        .last_stale_count = vm->stale_word_count_at_check
    };

    /* === Run Unified Inference Engine === */
    inference_engine_run(&inference_inputs, vm->last_inference_outputs);

    /* === Apply Inferred Tuning Parameters === */
    if (!vm->last_inference_outputs->early_exited)
    {
        /* Full inference was executed (not cached by ANOVA early-exit) */

        /* 1. Apply adaptive window width */
        if (vm->last_inference_outputs->adaptive_window_width > 0 &&
            vm->last_inference_outputs->adaptive_window_width != vm->rolling_window.effective_window_size)
        {
            log_message(LOG_INFO,
                       "INFERENCE[window]: %u → %u (variance=%.6f Q48.16)",
                       vm->rolling_window.effective_window_size,
                       vm->last_inference_outputs->adaptive_window_width,
                       (double)vm->last_inference_outputs->window_variance_q48 / 65536.0);
            vm->rolling_window.effective_window_size = vm->last_inference_outputs->adaptive_window_width;
        }

        /* 2. Apply adaptive decay slope */
        sf_mutex_lock(&vm->tuning_lock);
        if (vm->last_inference_outputs->adaptive_decay_slope > 0 &&
            vm->last_inference_outputs->adaptive_decay_slope != vm->decay_slope_q48)
        {
            double old_slope_dbl = (double)vm->decay_slope_q48 / 65536.0;
            double new_slope_dbl = (double)vm->last_inference_outputs->adaptive_decay_slope / 65536.0;

            log_message(LOG_INFO,
                       "INFERENCE[slope]: %.3f → %.3f (fit_quality=%.6f Q48.16)",
                       old_slope_dbl,
                       new_slope_dbl,
                       (double)vm->last_inference_outputs->slope_fit_quality_q48 / 65536.0);
            vm->decay_slope_q48 = vm->last_inference_outputs->adaptive_decay_slope;
        }
        sf_mutex_unlock(&vm->tuning_lock);

        /* 3. Validate outputs */
        if (!inference_outputs_validate(vm->last_inference_outputs))
        {
            log_message(LOG_WARN,
                       "INFERENCE: Output validation failed, ignoring results");
        }

        vm->heartbeat.last_inference_tick = vm->heartbeat.tick_count;
    }
    else
    {
        /* ANOVA early-exit: variance stable, using cached outputs */
        vm->heartbeat.early_exit_count++;
        log_message(LOG_DEBUG,
                   "INFERENCE: Early-exit (variance stable <5%%), using cached outputs");
    }

    /* Store baseline for next inference comparison */
    sf_mutex_lock(&vm->tuning_lock);
    vm->total_heat_at_last_check = total_heat;
    vm->stale_word_count_at_check = stale_word_count;
    vm->word_count_at_check = word_count;
    sf_mutex_unlock(&vm->tuning_lock);

/* L8 FINAL INTEGRATION: Loop always-on */    /* === Loop #7: Adaptive Heartrate ===
     * Adjust tick frequency based on system stability:
     * - Variance stable (early_exited) → increase tick interval (less frequent)
     * - Variance volatile (full inference) → decrease tick interval (more frequent)
     *
     * Bounds: [HEARTBEAT_TICK_NS / 4, HEARTBEAT_TICK_NS * 4]
     */
    {
        uint64_t current_tick_ns = vm->heartbeat.tick_target_ns;
        uint64_t min_tick_ns = HEARTBEAT_TICK_NS / 4;   /* 4x faster minimum */
        uint64_t max_tick_ns = HEARTBEAT_TICK_NS * 4;   /* 4x slower maximum */

        if (vm->last_inference_outputs && vm->last_inference_outputs->early_exited)
        {
            /* System stable: slow down heartbeat by 25% */
            uint64_t new_tick_ns = (current_tick_ns * 125) / 100;
            if (new_tick_ns > max_tick_ns) new_tick_ns = max_tick_ns;

            if (new_tick_ns != current_tick_ns)
            {
                vm->heartbeat.tick_target_ns = new_tick_ns;
                if (vm->heartbeat.worker)
                    vm->heartbeat.worker->tick_ns = new_tick_ns;

                log_message(LOG_DEBUG,
                           "HEARTBEAT[rate]: stable → slower tick %lu → %lu ns",
                           (unsigned long)current_tick_ns,
                           (unsigned long)new_tick_ns);
            }
        }
        else
        {
            /* System volatile: speed up heartbeat by 25% */
            uint64_t new_tick_ns = (current_tick_ns * 80) / 100;
            if (new_tick_ns < min_tick_ns) new_tick_ns = min_tick_ns;

            if (new_tick_ns != current_tick_ns)
            {
                vm->heartbeat.tick_target_ns = new_tick_ns;
                if (vm->heartbeat.worker)
                    vm->heartbeat.worker->tick_ns = new_tick_ns;

                log_message(LOG_DEBUG,
                           "HEARTBEAT[rate]: volatile → faster tick %lu → %lu ns",
                           (unsigned long)current_tick_ns,
                           (unsigned long)new_tick_ns);
            }
        }
    }
/* End always-on loop */}
