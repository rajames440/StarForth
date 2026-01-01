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
#include "../include/inference_engine.h"
#include "../include/log.h"
#include "../include/word_registry.h"
#include "../include/vm_debug.h"
#include "../include/profiler.h"
#include "../include/platform_time.h"
#include "../include/physics_metadata.h"
#include "../include/physics_hotwords_cache.h"
#include "../include/physics_pipelining_metrics.h"
#include "../include/rolling_window_of_truth.h"
#include "../include/dictionary_heat_optimization.h"
#include "../include/ssm_jacquard.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>

#if HEARTBEAT_THREAD_ENABLED && !defined(L4RE_TARGET)
#include <pthread.h>
#define HEARTBEAT_HAS_THREADS 1
#else
#define HEARTBEAT_HAS_THREADS 0
#endif

#define HEARTBEAT_DECAY_BATCH 64u

/** @name Forward Declarations
 * @{
 */
void execute_colon_word(VM * vm); /* Non-static for SEE decompiler */

static void vm_bootstrap_scr(VM * vm);

static unsigned vm_get_base(const VM* vm);

static void vm_set_base(VM* vm, unsigned b);

typedef struct HeartbeatWorker
{
#if HEARTBEAT_HAS_THREADS
    pthread_t thread;
#endif
    uint64_t tick_ns;
    int running;
    int stop_requested;
} HeartbeatWorker;

static void vm_heartbeat_run_cycle(VM *vm);
static void heartbeat_publish_snapshot(VM *vm);
static void vm_tick_apply_background_decay(VM *vm, uint64_t now_ns);
#if HEARTBEAT_THREAD_ENABLED
static void* heartbeat_thread_main(void *arg);
#endif

static inline uint32_t heartbeat_snapshot_index_load(const volatile uint32_t *ptr)
{
#if defined(__GNUC__)
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
#else
    return *ptr;
#endif
}

static inline void heartbeat_snapshot_index_store(volatile uint32_t *ptr, uint32_t value)
{
#if defined(__GNUC__)
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
#else
    *ptr = value;
#endif
}

/* ====================== Base helpers ======================= */

static unsigned vm_get_base(const VM* vm)
{
    if (!vm) return 10u;
    /* Prefer VM cell if valid */
    vaddr_t a = vm->base_addr;
    if ((a % sizeof(cell_t)) == 0 && (size_t)a + sizeof(cell_t) <= VM_MEMORY_SIZE)
    {
        cell_t v = vm_load_cell((VM*)vm, a); /* cast-away const for accessor */
        if (v >= 2 && v <= 36) return (unsigned)v;
    }
    /* Fallback to host mirror */
    if (vm->base >= 2 && vm->base <= 36) return (unsigned)vm->base;
    return 10u;
}

static void vm_set_base(VM* vm, unsigned b)
{
    if (!vm) return;
    if (b < 2 || b > 36) b = 10;
    vm_store_cell(vm, vm->base_addr, (cell_t)b);
    vm->base = (cell_t)b; /* host mirror */
}

static void heartbeat_publish_snapshot(VM *vm)
{
    if (!vm)
        return;

    uint32_t current = heartbeat_snapshot_index_load(&vm->heartbeat.snapshot_index) & 1u;
    uint32_t next = current ^ 1u;
    HeartbeatSnapshot *snapshot = &vm->heartbeat.snapshots[next];

    snapshot->published_tick = vm->heartbeat.tick_count;
    snapshot->published_ns = sf_monotonic_ns();
    snapshot->window_width = vm->rolling_window.effective_window_size;
    snapshot->decay_slope_q48 = vm->decay_slope_q48;
    snapshot->hot_word_count = vm->hot_word_count_at_check;
    snapshot->stale_word_count = vm->stale_word_count_at_check;
    snapshot->total_heat = vm->total_heat_at_last_check;

    heartbeat_snapshot_index_store(&vm->heartbeat.snapshot_index, next);
}

/* ====================== VM init / teardown ======================= */

/**
 * @brief Initialize a new virtual machine instance
 *
 * Allocates memory and initializes all VM structures including:
 * - Memory array
 * - Data and return stacks
 * - System variables (SCR, STATE, BASE)
 * - Dictionary
 * - Forth-79 wordset
 *
 * @param vm Pointer to VM structure to initialize
 */
void vm_init(VM* vm)
{
    if (!vm) return;
    memset(vm, 0, sizeof(*vm));
    vm->next_word_id = 0;
    vm->recycled_word_id_count = 0;

    if (sf_mutex_init(&vm->dict_lock) != 0)
    {
        log_message(LOG_ERROR, "vm_init: dict_lock init failed");
        vm->error = 1;
        return;
    }

    if (sf_mutex_init(&vm->tuning_lock) != 0)
    {
        log_message(LOG_ERROR, "vm_init: tuning_lock init failed");
        sf_mutex_destroy(&vm->dict_lock);
        vm->error = 1;
        return;
    }

    vm->memory = (uint8_t*)malloc(VM_MEMORY_SIZE);
    if (!vm->memory)
    {
        log_message(LOG_ERROR, "vm_init: out of host memory");
        vm->error = 1;
        return;
    }

    vm->dsp = -1;
    vm->rsp = -1;
    vm->here = 0;
    vm->exit_colon = 0;
    vm->abort_requested = 0;

    vm_align(vm);

    /* SCR */
    {
        void* p = vm_allot(vm, sizeof(cell_t));
        if (!p)
        {
            vm->error = 1;
            log_message(LOG_ERROR, "vm_init: SCR allot failed");
            return;
        }
        vm->scr_addr = (vaddr_t)((uint8_t*)p - vm->memory);
        vm_store_cell(vm, vm->scr_addr, 0);
    }

    /* STATE (0=interpret, -1=compile) */
    {
        void* p = vm_allot(vm, sizeof(cell_t));
        if (!p)
        {
            vm->error = 1;
            log_message(LOG_ERROR, "vm_init: STATE allot failed");
            return;
        }
        vm->state_addr = (vaddr_t)((uint8_t*)p - vm->memory);
        vm_store_cell(vm, vm->state_addr, 0);
        vm->state_var = 0;
    }

    /* BASE (default 10) */
    {
        void* p = vm_allot(vm, sizeof(cell_t));
        if (!p)
        {
            vm->error = 1;
            log_message(LOG_ERROR, "vm_init: BASE allot failed");
            return;
        }
        vm->base_addr = (vaddr_t)((uint8_t*)p - vm->memory);
        vm_set_base(vm, 10);
    }

    vm_bootstrap_scr(vm);

    vm->mode = MODE_INTERPRET;
    vm->compiling_word = NULL;
    vm->latest = NULL;
    vm->error = 0;
    vm->halted = 0;

    vm->input_length = 0;
    vm->input_pos = 0;
    vm->current_executing_entry = NULL;

    vm_debug_set_current_vm(vm);
    vm_debug_install_signal_handlers();

    /* Register Forth-79 wordset */
    register_forth79_words(vm);

    /* Set FORGET fence to post-boot */
    vm->dict_fence_latest = vm->latest;
    vm->dict_fence_here = vm->here;

    /* Initialize hot-words cache (physics frequency-driven acceleration) */
    vm->hotwords_cache = (HotwordsCache*)malloc(sizeof(HotwordsCache));
    if (!vm->hotwords_cache)
    {
        log_message(LOG_ERROR, "vm_init: hotwords cache malloc failed");
        vm->error = 1;
        return;
    }
    hotwords_cache_init(vm->hotwords_cache);

    /* Initialize rolling window of truth (deterministic execution history) */
    if (rolling_window_init(&vm->rolling_window) != 0)
    {
        log_message(LOG_ERROR, "vm_init: rolling window malloc failed");
        vm->error = 1;
        return;
    }

    /* Initialize VM heartbeat (centralized time-driven tuning) */
    vm->heartbeat.tick_count = 0;
    vm->heartbeat.last_inference_tick = 0;
    vm->heartbeat.check_counter = 0;
    vm->heartbeat.heartbeat_enabled = 1;  /* Enabled by default */
    vm->heartbeat.tick_target_ns = HEARTBEAT_TICK_NS;
    vm->heartbeat.snapshot_index = 0;
    vm->heartbeat.worker = NULL;
    vm->heartbeat_decay_cursor_id = WORD_ID_INVALID;

    /* Initialize pipelining global metrics (aggregated prefetch tracking) */
    vm->pipeline_metrics.prefetch_attempts = 0;
    vm->pipeline_metrics.prefetch_hits = 0;
    vm->pipeline_metrics.window_tuning_checks = 0;
    vm->pipeline_metrics.last_checked_window_size = vm->rolling_window.effective_window_size;
    vm->pipeline_metrics.last_checked_accuracy = 0.0;
    vm->pipeline_metrics.suggested_next_size = vm->rolling_window.effective_window_size;

    heartbeat_publish_snapshot(vm);

#if HEARTBEAT_HAS_THREADS
    vm->heartbeat.worker = calloc(1, sizeof(HeartbeatWorker));
    if (vm->heartbeat.worker)
    {
        vm->heartbeat.worker->tick_ns = HEARTBEAT_TICK_NS;
        if (pthread_create(&vm->heartbeat.worker->thread, NULL, heartbeat_thread_main, vm) != 0)
        {
            log_message(LOG_WARN, "heartbeat: pthread_create failed (%d), falling back to inline mode", errno);
            free(vm->heartbeat.worker);
            vm->heartbeat.worker = NULL;
        }
    }
    else
    {
        log_message(LOG_WARN, "heartbeat: worker allocation failed, using inline heartbeat");
    }
#endif

    /* Initialize adaptive heat decay tuning (Loop #3) */
    /* Start with 2:1 ratio in Q48.16 format: 2.0 << 16 = 131072 */
    vm->decay_slope_q48 = (1ULL << 16) / 3;  /* 1/3 starting slope (Q48.16) */
    vm->last_decay_check_ns = 0;
    vm->total_heat_at_last_check = 0;
    vm->stale_word_count_at_check = 0;
    vm->decay_slope_direction = 0;  /* Start neutral */

    /* Phase 2: Initialize heat-aware dictionary optimization */
    vm->lookup_strategy = 0;  /* Start with naive lookup, will adapt based on patterns */
    vm->last_bucket_reorg_ns = 0;  /* Force first reorg quickly */
    dict_update_heat_percentiles(vm);  /* Calculate initial percentiles */

    /* SSM L8: Jacquard Mode Selector initialization */
    vm->ssm_l8_state = malloc(sizeof(ssm_l8_state_t));
    if (!vm->ssm_l8_state)
    {
        log_message(LOG_ERROR, "vm_init: SSM L8 state malloc failed");
        vm->error = 1;
        return;
    }
    ssm_l8_init((ssm_l8_state_t*)vm->ssm_l8_state, SSM_MODE_C0);

    vm->ssm_config = malloc(sizeof(ssm_config_t));
    if (!vm->ssm_config)
    {
        log_message(LOG_ERROR, "vm_init: SSM config malloc failed");
        vm->error = 1;
        return;
    }
    /* Initialize with C0 (minimal) mode: L2=0, L3=0, L5=0, L6=0 */
    ((ssm_config_t*)vm->ssm_config)->L2_rolling_window = 0;
    ((ssm_config_t*)vm->ssm_config)->L3_linear_decay = 0;
    ((ssm_config_t*)vm->ssm_config)->L5_window_inference = 0;
    ((ssm_config_t*)vm->ssm_config)->L6_decay_inference = 0;
}

/**
 * @brief Clean up and free VM resources
 *
 * Frees allocated memory and resets VM state.
 *
 * @param vm Pointer to VM structure to clean up
 */
void vm_cleanup(VM* vm)
{
    if (!vm) return;

#if HEARTBEAT_HAS_THREADS
    if (vm->heartbeat.worker)
    {
        vm->heartbeat.worker->stop_requested = 1;
        pthread_join(vm->heartbeat.worker->thread, NULL);
        free(vm->heartbeat.worker);
        vm->heartbeat.worker = NULL;
    }
#endif

    /* Clean up hot-words cache */
    if (vm->hotwords_cache)
    {
        hotwords_cache_cleanup(vm->hotwords_cache);
        free(vm->hotwords_cache);
        vm->hotwords_cache = NULL;
    }

    /* Clean up rolling window of truth */
    rolling_window_cleanup(&vm->rolling_window);

    /* Clean up SSM L8 state */
    if (vm->ssm_l8_state)
    {
        free(vm->ssm_l8_state);
        vm->ssm_l8_state = NULL;
    }
    if (vm->ssm_config)
    {
        free(vm->ssm_config);
        vm->ssm_config = NULL;
    }

    if (vm->memory)
    {
        free(vm->memory);
        vm->memory = NULL;
    }
    vm->here = 0;

    sf_mutex_destroy(&vm->tuning_lock);
    sf_mutex_destroy(&vm->dict_lock);
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

static void vm_heartbeat_run_cycle(VM *vm)
{
    if (!vm || !vm->heartbeat.heartbeat_enabled)
        return;

    /* L8 FINAL INTEGRATION: Core heartbeat operations */
    vm_tick(vm);
    vm_tick_apply_background_decay(vm, sf_monotonic_ns());
    rolling_window_service(&vm->rolling_window);
    dict_adaptive_optimization_pass(vm);  /* Adaptive dictionary optimization */

    /* L8 FINAL INTEGRATION: Jacquard mode selector - the sole policy engine */
    vm_heartbeat_update_l8(vm);

    heartbeat_publish_snapshot(vm);

    /* Phase 2: Real-time heartbeat metrics emission (DISABLED)
     * Re-enable with --heartbeat-log=full or add a runtime flag.
     */
#if 0
    HeartbeatTickSnapshot tick_snapshot;
    heartbeat_capture_tick_snapshot(vm, &tick_snapshot);
    heartbeat_emit_tick_row(vm, &tick_snapshot);
#endif
}

#if HEARTBEAT_THREAD_ENABLED
static void* heartbeat_thread_main(void *arg)
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
        vm->last_inference_outputs = malloc(sizeof(InferenceOutputs));
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

/* ====================== Parser / number ======================= */

/**
 * @brief Parse next word from input buffer
 *
 * Skips leading whitespace and extracts next word delimited by whitespace.
 *
 * @param vm Pointer to VM instance
 * @param word Buffer to store parsed word
 * @param max_len Maximum length of word buffer
 * @return Length of parsed word or 0 if no word found
 */
int vm_parse_word(VM* vm, char* word, size_t max_len)
{
    if (!vm || !word || max_len == 0) return 0;

    /* Skip whitespace */
    while (vm->input_pos < vm->input_length)
    {
        char c = vm->input_buffer[vm->input_pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
        vm->input_pos++;
    }
    if (vm->input_pos >= vm->input_length) return 0;

    size_t len = 0;
    while (vm->input_pos < vm->input_length && len < max_len - 1)
    {
        char c = vm->input_buffer[vm->input_pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        word[len++] = c;
        vm->input_pos++;
    }
    word[len] = '\0';
    return (int)len;
}

/**
 * @brief Parse string as number in current base
 *
 * Attempts to parse string as number using VM's current number base.
 * Handles optional sign prefix.
 *
 * @param vm Pointer to VM instance 
 * @param s String to parse
 * @param out Pointer to store parsed value
 * @return 1 on success, 0 on parse failure
 */
int vm_parse_number(VM* vm, const char* s, cell_t* out)
{
    if (!s || !*s || !out) return 0;

    unsigned base = vm_get_base(vm);
    int neg = 0;
    if (*s == '+' || *s == '-')
    {
        neg = (*s == '-');
        s++;
        if (!*s) return 0;
    }

    unsigned long long acc = 0;
    int any = 0;
    for (const char* p = s; *p; ++p)
    {
        unsigned d;
        unsigned char c = (unsigned char)*p;
        if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
        else if (c >= 'A' && c <= 'Z') d = 10u + (unsigned)(c - 'A');
        else if (c >= 'a' && c <= 'z') d = 10u + (unsigned)(c - 'a');
        else return 0;
        if (d >= base) return 0;
        acc = acc * base + d;
        any = 1;
    }
    if (!any) return 0;
    cell_t v = (cell_t)acc;
    if (neg) v = (cell_t)(-v);
    *out = v;
    return 1;
}

/* ====================== Compile state ======================= */

void vm_enter_compile_mode(VM* vm, const char* name, size_t len)
{
    if (!vm) return;
    vm->mode = MODE_COMPILE;
    vm->state_var = -1;
    vm_store_cell(vm, vm->state_addr, vm->state_var);

    if (len > WORD_NAME_MAX) len = WORD_NAME_MAX;
    memcpy(vm->current_word_name, name, len);
    vm->current_word_name[len] = '\0';

    /* Create colon word header with code pointer = execute_colon_word */
    DictEntry* de = vm_create_word(vm, name, len, execute_colon_word);
    vm->compiling_word = de;
    if (!de)
    {
        vm->error = 1;
        return;
    }
    de->flags |= WORD_SMUDGED;

    /* DF (first data cell) will hold the VM-relative address of threaded body */
    vm_align(vm);
    cell_t* df = vm_dictionary_get_data_field(de);
    if (!df)
    {
        vm->error = 1;
        return;
    }
    *df = (cell_t)(int64_t)((vaddr_t)vm->here);

    log_message(LOG_DEBUG, ": started '%s' at HERE=%zu", vm->current_word_name, vm->here);
}

void vm_compile_word(VM* vm, DictEntry* entry)
{
    if (!vm || vm->mode != MODE_COMPILE) return;
    if (!entry)
    {
        vm->error = 1;
        return;
    }
    vm_align(vm);
    cell_t* slot = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (!slot)
    {
        vm->error = 1;
        return;
    }
    *slot = (cell_t)(uintptr_t)
    entry; /* threaded code stores DictEntry* as cell */
}

void vm_compile_literal(VM* vm, cell_t value)
{
    if (!vm) return;
    if (vm->mode != MODE_COMPILE)
    {
        vm_push(vm, value);
        return;
    }

    DictEntry* LIT = vm_find_word(vm, "LIT", 3);
    if (!LIT)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "LIT not found");
        return;
    }
    vm_compile_word(vm, LIT);

    vm_align(vm);
    cell_t* val = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (!val)
    {
        vm->error = 1;
        return;
    }
    *val = value;
}

void vm_compile_call(VM* vm, word_func_t func)
{
    if (!vm || vm->mode != MODE_COMPILE)
    {
        vm->error = 1;
        return;
    }
    DictEntry* entry = vm_dictionary_find_by_func(vm, func);
    if (!entry)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "vm_compile_call: entry not found");
        return;
    }
    vm_compile_word(vm, entry);
}

void vm_compile_exit(VM* vm)
{
    if (!vm || vm->mode != MODE_COMPILE) return;
    DictEntry* EXIT = vm_find_word(vm, "EXIT", 4);
    if (!EXIT)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "EXIT not found");
        return;
    }
    vm_compile_word(vm, EXIT);
}

void vm_exit_compile_mode(VM* vm)
{
    if (!vm || !vm->compiling_word)
    {
        vm->error = 1;
        return;
    }

    DictEntry* EXIT = vm_find_word(vm, "EXIT", 4);
    if (!EXIT)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "EXIT not found");
        return;
    }
    vm_compile_word(vm, EXIT);

    vm->compiling_word->flags &= ~WORD_SMUDGED;
    vm->compiling_word->flags |= WORD_COMPILED;

    cell_t* df = vm_dictionary_get_data_field(vm->compiling_word);
    if (df)
    {
        uint64_t header_bytes = (uint64_t)(((uint8_t*)df + sizeof(cell_t)) - (uint8_t*)vm->compiling_word);
        uint64_t body_start = (uint64_t)(vaddr_t)(uint64_t)(*df);
        uint64_t here_bytes = (uint64_t)vm->here;
        uint64_t body_bytes = (here_bytes >= body_start) ? (here_bytes - body_start) : 0;
        uint64_t total = header_bytes + body_bytes;
        uint32_t mass = (total > UINT32_MAX) ? UINT32_MAX : (uint32_t)total;
        physics_metadata_set_mass(vm->compiling_word, mass);
    }
    physics_metadata_refresh_state(vm->compiling_word);

    vm->mode = MODE_INTERPRET;
    vm->state_var = 0;
    vm_store_cell(vm, vm->state_addr, vm->state_var);
    vm->compiling_word = NULL;

    log_message(LOG_DEBUG, "; end definition");
}

/* ====================== Inner interpreter ======================= */
/*
   Threaded code layout (compiled by vm_compile_word / vm_compile_literal):

   DF cell (in DictEntry) holds a VM address (vaddr_t) of the first code cell.
   Each code cell is a cell_t that encodes a DictEntry* (for a word to call),
   or is a literal payload following a compiled LIT word.

   Control-flow runtime words (e.g., (BRANCH), (0BRANCH), (DO), loops) are
   responsible for *modifying the IP stored at the top of the return stack*.
   The inner interpreter saves the "next ip" on the return stack before
   calling the word; after the word returns, we pop the possibly-modified IP
   and continue. This matches your runtime branch helpers' contract.

   IMPORTANT: EXIT behavior —
     Words implement EXIT by setting vm->exit_colon = 1 (one-shot).
     We honor that flag here to unwind the *current* colon only,
     without disturbing the caller’s R-stack frame.
*/

/* vm.c */

/* Executes the threaded code of a colon definition.
 * Uses a return-stack IP: each call saves the next IP on RS and pops it on return.
 * When vm->exit_colon is set, it discards the saved IP and returns early.
 */
/* Executes a colon-defined word (direct/threaded).
 * Contract: before each call we push the resume IP on RS; after the call we pop
 * the (possibly modified) IP. If a word sets vm->exit_colon, we discard the
 * saved resume IP and return to the caller (one-shot).
 */
void execute_colon_word(VM* vm)
{
    if (!vm || !vm->current_executing_entry) return;

    /* Fetch threaded body address from the DictEntry's data field (DF) */
    DictEntry* entry = vm->current_executing_entry;
    cell_t* df = vm_dictionary_get_data_field(entry);
    if (!df)
    {
        vm->error = 1;
        return;
    }

    /* DF holds a VM virtual address (byte offset) of the first code cell */
    vaddr_t body_addr = (vaddr_t)(uint64_t)(*df);
    cell_t* ip = (cell_t*)vm_ptr(vm, body_addr);
    if (!ip)
    {
        vm->error = 1;
        return;
    }

    /* Phase 1: Track word-to-word transitions for pipelining metrics */
    /* L8 FINAL INTEGRATION: Loop always-on */
    DictEntry* prev_word = NULL;

    for (;;)
    {
        /* Each code cell stores a DictEntry* (called word) */
        DictEntry* w = (DictEntry*)(uintptr_t)(*ip);
        if (w)
        {
            /* Phase 2: Apply linear decay before accumulating new heat */
            uint64_t now_ns = sf_monotonic_ns();
            uint64_t elapsed_ns = now_ns - w->physics.last_active_ns;
            physics_metadata_apply_linear_decay(w, elapsed_ns, vm);
            w->physics.last_active_ns = now_ns;
            w->physics.last_decay_ns = now_ns;

            physics_execution_heat_increment(w);

            uint32_t word_id = w->word_id;
            if (word_id < DICTIONARY_SIZE)
            {
                /* L8 FINAL INTEGRATION: Loop #2 always-on - Rolling Window of Truth */
                rolling_window_record_execution(&vm->rolling_window, word_id);

/* L8 FINAL INTEGRATION: Loop always-on */                /* Loop #4: Pipelining Transition Metrics - WIRED & UTILIZED */
                if (prev_word && prev_word->transition_metrics && ENABLE_PIPELINING)
                {
                    /* Check if current word was speculatively promoted by previous word */
                    uint32_t prev_speculation_target = prev_word->transition_metrics->most_likely_next_word_id;
                    if (prev_speculation_target == word_id && prev_word->transition_metrics->prefetch_attempts > 0)
                    {
                        /* PREFETCH HIT: Current word matches previous word's speculation! */
                        transition_metrics_record_prefetch_hit(prev_word->transition_metrics, 0);
                        vm->pipeline_metrics.prefetch_hits++;
                    }

                    /* Record transition from previous word to current word */
                    transition_metrics_record(prev_word->transition_metrics, word_id, DICTIONARY_SIZE);

                    /* Update probability cache to find most likely next word */
                    transition_metrics_update_cache(prev_word->transition_metrics, DICTIONARY_SIZE);

                    uint32_t speculated_word_id = prev_word->transition_metrics->most_likely_next_word_id;

                    /* Check if we should speculatively prefetch the most likely next word */
                    if (speculated_word_id < DICTIONARY_SIZE &&
                        transition_metrics_should_speculate(prev_word->transition_metrics, speculated_word_id))
                    {
                        /* Speculation decision: the most likely next word has high probability
                         * Action: Promote it to hotwords cache now (speculative pre-caching)
                         * This way when we actually look up that word, it's cache-warm */
                        DictEntry *spec_entry = vm_dictionary_lookup_by_word_id(vm, speculated_word_id);

                        /* If we found the entry and have a cache, promote it speculatively */
                        if (spec_entry && vm->hotwords_cache && ENABLE_HOTWORDS_CACHE)
                        {
                            spec_entry->execution_heat = HOTWORDS_EXECUTION_HEAT_THRESHOLD + 1;  /* Ensure promotion */
                            hotwords_cache_promote(vm->hotwords_cache, spec_entry);

                            /* Record the prefetch attempt (both per-word and global) */
                            prev_word->transition_metrics->prefetch_attempts++;
                            vm->pipeline_metrics.prefetch_attempts++;
                        }
                    }
                }
/* End always-on loop */            }
        }

        /* Advance IP to next cell and save resume IP on return stack */
        ip = ip + 1;
        vm_rpush(vm, (cell_t)(uintptr_t)ip);
        if (vm->error) { return; }

        /* Execute the word */
        vm->current_executing_entry = w;

        /* Track word execution for profiling */
        profiler_word_count(w);

        if (w && w->func)
        {
            profiler_word_enter(w);
            w->func(vm);
            physics_metadata_touch(w, w->execution_heat, sf_monotonic_ns());
            profiler_word_exit(w);
            vm->heartbeat.words_executed++;  /* DoE counter */
        }
        else
        {
            log_message(LOG_ERROR, "execute_colon_word: null word func");
            vm->error = 1;
        }
        vm->current_executing_entry = entry; /* restore current colon */

        /* L8 FINAL INTEGRATION: Loop #4 always-on - Pipelining */
        if (ENABLE_PIPELINING && w)
        {
            prev_word = w;
        }

        /* Heartbeat: Periodic time-driven tuning (Loop #3 & #5) */
        if (!vm->heartbeat.worker && ++vm->heartbeat.check_counter >= HEARTBEAT_CHECK_FREQUENCY)
        {
            vm_heartbeat_run_cycle(vm);
            vm->heartbeat.check_counter = 0;
        }

        if (vm->error) { return; }

        /* Check for ABORT request (clears both stacks, immediate termination) */
        if (vm->abort_requested)
        {
            vm->abort_requested = 0;
            return;
        }

        /* One-shot early return? (EXIT) */
        if (vm->exit_colon)
        {
            vm->exit_colon = 0;

            /* CRITICAL: discard the per-step resume IP */
            (void)vm_rpop(vm);

            return;
        }

        /* Normal path: resume at IP popped from RS (possibly patched by runtime) */
        ip = (cell_t*)(uintptr_t)vm_rpop(vm);
        if (vm->error) { return; }
    }
}


/* ====================== Outer interpreter ======================= */

void vm_interpret_word(VM* vm, const char* word_str, size_t len)
{
    if (!vm || !word_str) return;

    log_message(LOG_DEBUG, "INTERPRET: '%.*s' (mode=%s)",
                (int)len, word_str,
                vm->mode == MODE_COMPILE ? "COMPILE" : "INTERPRET");

    /* Prefer vocabulary-aware lookup; fall back to canonical dictionary */
    extern DictEntry*vm_vocabulary_find_word(VM* vm, const char* name, size_t nlen);
    DictEntry* entry = vm_vocabulary_find_word(vm, word_str, len);
    DictEntry* canon = vm_find_word(vm, word_str, len);
    if (!entry) entry = canon;

    if (entry)
    {
        /* Bump usage counters - Thread safety: lock dict for heat modifications */
        uint64_t lookup_ns = sf_monotonic_ns();

        sf_mutex_lock(&vm->dict_lock);
        /* Phase 2: Apply linear decay before accumulating heat */
        uint64_t elapsed_entry = lookup_ns - entry->physics.last_active_ns;
        physics_metadata_apply_linear_decay(entry, elapsed_entry, vm);
        entry->physics.last_active_ns = lookup_ns;
        entry->physics.last_decay_ns = lookup_ns;

        physics_execution_heat_increment(entry);
        if (canon && canon != entry)
        {
            /* Apply decay to canonical entry as well */
            uint64_t elapsed_canon = lookup_ns - canon->physics.last_active_ns;
            physics_metadata_apply_linear_decay(canon, elapsed_canon, vm);
            canon->physics.last_active_ns = lookup_ns;
            canon->physics.last_decay_ns = lookup_ns;

            physics_execution_heat_increment(canon);
            physics_metadata_touch(canon, canon->execution_heat, lookup_ns);
        }
        sf_mutex_unlock(&vm->dict_lock);

        /* Immediate if either entry or canonical is flagged immediate */
        int is_immediate =
        ((entry && (entry->flags & WORD_IMMEDIATE)) ||
            (canon && (canon->flags & WORD_IMMEDIATE)));

        if (vm->mode == MODE_COMPILE && !is_immediate)
        {
            log_message(LOG_DEBUG, "COMPILE: '%.*s'", (int)len, word_str);
            vm_compile_word(vm, entry);
            return;
        }

        log_message(LOG_DEBUG, "EXECUTE: '%.*s'", (int)len, word_str);
        vm->current_executing_entry = entry;

        /* Track word execution for profiling */
        profiler_word_count(entry);

        if (entry->func)
        {
            profiler_word_enter(entry);
            entry->func(vm);
            physics_metadata_touch(entry, entry->execution_heat, sf_monotonic_ns());
            profiler_word_exit(entry);
            vm->heartbeat.words_executed++;  /* DoE counter */
        }
        else
        {
            log_message(LOG_ERROR, "NULL func for '%.*s'", (int)len, word_str);
            vm->error = 1;
        }
        vm->current_executing_entry = NULL;
        return;
    }

    /* Not found: try to parse a number in the current BASE */
    cell_t value;
    if (vm_parse_number(vm, word_str, &value))
    {
        log_message(LOG_DEBUG, "NUMBER: '%.*s' = %ld", (int)len, word_str, (long)value);
        if (vm->mode == MODE_COMPILE)
        {
            vm_compile_literal(vm, value);
        }
        else
        {
            vm_push(vm, value);
        }
        return;
    }

    /* Unknown word */
    log_message(LOG_ERROR, "UNKNOWN WORD: '%.*s'", (int)len, word_str);
    vm->error = 1;
}

/**
 * @brief Interpret a string of Forth code
 *
 * Main interpretation loop that:
 * - Loads input into VM buffer
 * - Parses words
 * - Executes or compiles each word
 * - Handles numbers
 *
 * @param vm Pointer to VM instance
 * @param input String containing Forth code to interpret
 */
void vm_interpret(VM* vm, const char* input)
{
    if (!vm || !input) return;

    /* Load into input buffer (cap + NUL) */
    size_t n = 0, cap = INPUT_BUFFER_SIZE ? INPUT_BUFFER_SIZE - 1 : 0;
    while (n < cap)
    {
        char c = input[n];
        vm->input_buffer[n] = c;
        if (c == '\0') break;
        ++n;
    }
    if (n == cap) vm->input_buffer[n] = '\0';

    vm->input_length = n;
    vm->input_pos = 0;

    char word[64];
    size_t wlen;
    while (!vm->error && (wlen = (size_t)vm_parse_word(vm, word, sizeof(word))) > 0)
    {
        vm_interpret_word(vm, word, wlen);

        /* Heartbeat: Periodic time-driven tuning (Loop #3 & #5) */
        if (!vm->heartbeat.worker && ++vm->heartbeat.check_counter >= HEARTBEAT_CHECK_FREQUENCY)
        {
            vm_heartbeat_run_cycle(vm);
            vm->heartbeat.check_counter = 0;
        }
    }
}

/* ====================== VM memory helpers ======================= */

/**
 * @brief Check if memory address range is valid
 *
 * Validates that an address range falls within VM memory bounds.
 *
 * @param vm Pointer to VM instance
 * @param addr Virtual address to check
 * @param len Length of memory range
 * @return 1 if range is valid, 0 if invalid
 */
int vm_addr_ok(struct VM* vm, vaddr_t addr, size_t len)
{
    if (!vm || !vm->memory) return 0;
    if (len > VM_MEMORY_SIZE) return 0;
    return addr <= (vaddr_t)(VM_MEMORY_SIZE - len);
}

uint8_t* vm_ptr(struct VM* vm, vaddr_t addr)
{
    if (!vm || !vm->memory) return NULL;
    if (!vm_addr_ok(vm, addr, 1)) return NULL;
    return vm->memory + (size_t)addr;
}

uint8_t vm_load_u8(struct VM* vm, vaddr_t addr)
{
    uint8_t* p = vm_ptr(vm, addr);
    if (!p)
    {
        vm->error = 1;
        return 0;
    }
    return *p;
}

void vm_store_u8(struct VM* vm, vaddr_t addr, uint8_t v)
{
    uint8_t* p = vm_ptr(vm, addr);
    if (!p)
    {
        vm->error = 1;
        return;
    }
    *p = v;
}

cell_t vm_load_cell(struct VM* vm, vaddr_t addr)
{
    if (!vm_addr_ok(vm, addr, sizeof(cell_t)) || (addr % sizeof(cell_t)) != 0)
    {
        vm->error = 1;
        return 0;
    }
    cell_t out = 0;
    memcpy(&out, vm->memory + (size_t)addr, sizeof(cell_t));
    return out;
}

void vm_store_cell(struct VM* vm, vaddr_t addr, cell_t v)
{
    if (!vm_addr_ok(vm, addr, sizeof(cell_t)) || (addr % sizeof(cell_t)) != 0)
    {
        vm->error = 1;
        return;
    }
    memcpy(vm->memory + (size_t)addr, &v, sizeof(cell_t));
}

/* ====================== Bootstrap helpers ======================= */

static void vm_bootstrap_scr(VM* vm)
{
    if (!vm) return;
    /* Ensure SCR cell exists and is zero */
    if (!vm_addr_ok(vm, vm->scr_addr, sizeof(cell_t)) ||
        (vm->scr_addr % sizeof(cell_t)) != 0)
    {
        vm_align(vm);
        void* p = vm_allot(vm, sizeof(cell_t));
        if (!p)
        {
            vm->error = 1;
            log_message(LOG_ERROR, "bootstrap SCR allot failed");
            return;
        }
        vm->scr_addr = (vaddr_t)((uint8_t*)p - vm->memory);
    }
    vm_store_cell(vm, vm->scr_addr, 0);
}

/* Make the most recently created word immediate (FORTH-79) */
void vm_make_immediate(VM* vm)
{
    if (!vm) return;
    if (!vm->latest)
    {
        log_message(LOG_ERROR, "vm_make_immediate: no latest word to mark IMMEDIATE");
        vm->error = 1;
        return;
    }
    vm->latest->flags |= WORD_IMMEDIATE;
    physics_metadata_refresh_state(vm->latest);
    log_message(LOG_DEBUG, "IMMEDIATE: '%.*s'",
                (int)vm->latest->name_len, vm->latest->name);
}
