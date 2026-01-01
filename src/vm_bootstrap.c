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
 * vm_bootstrap.c - How a VM comes into existence
 *
 * Contains:
 *   - dictionary seeding
 *   - primitive registration
 *   - initial vocabularies
 *   - host vs kernel bootstrap branching
 *   - early init code paths
 *
 * Rule: If it runs before the VM executes a single word, it belongs here.
 */

#include "vm_internal.h"
#include "../include/log.h"
#include "../include/word_registry.h"
#include "../include/vm_debug.h"
#include "../include/physics_hotwords_cache.h"
#include "../include/rolling_window_of_truth.h"
#include "../include/dictionary_heat_optimization.h"
#include "../include/ssm_jacquard.h"
#include "../include/platform_alloc.h"

#include <string.h>

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

    vm->memory = (uint8_t*)sf_malloc(VM_MEMORY_SIZE);
    if (!vm->memory)
    {
        log_message(LOG_ERROR, "vm_init: out of memory");
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
    vm->hotwords_cache = (HotwordsCache*)sf_malloc(sizeof(HotwordsCache));
    if (!vm->hotwords_cache)
    {
        log_message(LOG_ERROR, "vm_init: hotwords cache alloc failed");
        vm->error = 1;
        return;
    }
    hotwords_cache_init(vm->hotwords_cache);

    /* Initialize rolling window of truth (deterministic execution history) */
    if (rolling_window_init(&vm->rolling_window) != 0)
    {
        log_message(LOG_ERROR, "vm_init: rolling window alloc failed");
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

    vm_heartbeat_publish_snapshot(vm);

    /* Start heartbeat thread (if enabled) */
    vm_heartbeat_start_thread(vm);

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
    vm->ssm_l8_state = sf_malloc(sizeof(ssm_l8_state_t));
    if (!vm->ssm_l8_state)
    {
        log_message(LOG_ERROR, "vm_init: SSM L8 state alloc failed");
        vm->error = 1;
        return;
    }
    ssm_l8_init((ssm_l8_state_t*)vm->ssm_l8_state, SSM_MODE_C0);

    vm->ssm_config = sf_malloc(sizeof(ssm_config_t));
    if (!vm->ssm_config)
    {
        log_message(LOG_ERROR, "vm_init: SSM config alloc failed");
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

    /* Stop heartbeat thread (if running) */
    vm_heartbeat_stop_thread(vm);

    /* Clean up hot-words cache */
    if (vm->hotwords_cache)
    {
        hotwords_cache_cleanup(vm->hotwords_cache);
        sf_free(vm->hotwords_cache);
        vm->hotwords_cache = NULL;
    }

    /* Clean up rolling window of truth */
    rolling_window_cleanup(&vm->rolling_window);

    /* Clean up SSM L8 state */
    if (vm->ssm_l8_state)
    {
        sf_free(vm->ssm_l8_state);
        vm->ssm_l8_state = NULL;
    }
    if (vm->ssm_config)
    {
        sf_free(vm->ssm_config);
        vm->ssm_config = NULL;
    }

    if (vm->memory)
    {
        sf_free(vm->memory);
        vm->memory = NULL;
    }
    vm->here = 0;

    sf_mutex_destroy(&vm->tuning_lock);
    sf_mutex_destroy(&vm->dict_lock);
}