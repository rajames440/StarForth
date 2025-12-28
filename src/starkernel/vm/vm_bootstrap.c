/*
 * vm_bootstrap.c - VM initialization and bootstrap logic
 */

#include "../include/vm.h"
#include "../include/log.h"
#include "../include/word_registry.h"
#include "../include/vm_debug.h"
#include "../include/physics_hotwords_cache.h"
#include "../include/rolling_window_of_truth.h"
#include "../include/dictionary_heat_optimization.h"
#include "../include/ssm_jacquard.h"
#include "word_source/include/vocabulary_words.h"
#include "vm_internal.h"
#ifdef __STARKERNEL__
#include "starkernel/vm/arena.h"
#include "starkernel/console.h"
#endif

#include <string.h>
#include <errno.h>

#if defined(__STARKERNEL__) && SK_PARITY_DEBUG
static void vm_bootstrap_print_hex(uint64_t value)
{
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    buf[18] = '\0';
    for (int i = 0; i < 16; ++i) {
        uint8_t nibble = (uint8_t)((value >> ((15 - i) * 4)) & 0xF);
        buf[i + 2] = (nibble < 10) ? (char)('0' + nibble) : (char)('a' + nibble - 10);
    }
    console_puts(buf);
}
#endif

static void vm_bootstrap_scr(VM *vm)
{
    vm_align(vm);

    void* p = vm_allot(vm, sizeof(cell_t));
    if (!p)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "vm_bootstrap_scr: SCR allot failed");
        return;
    }
    vm->scr_addr = (vaddr_t)((uint8_t*)p - vm->memory);
    vm_store_cell(vm, vm->scr_addr, 0);
}

void vm_bootstrap_root_vocabulary(VM *vm, const char *name)
{
    if (!vm || !name) {
        return;
    }
#if defined(__STARKERNEL__) && SK_PARITY_DEBUG
    console_println("[VM_BOOTSTRAP] root vocabulary entry");
#endif
#ifdef __STARKERNEL__
    sk_vm_arena_assert_guards("vm_bootstrap_root_vocabulary:entry");
#endif
    vocabulary_create_vocabulary_direct(vm, name);
    size_t len = strlen(name);
#if defined(__STARKERNEL__) && SK_PARITY_DEBUG
    console_println("[VM_BOOTSTRAP] before vm_find_word");
#endif
    DictEntry *entry = vm_find_word(vm, name, len);
#if defined(__STARKERNEL__) && SK_PARITY_DEBUG
    console_println("[VM_BOOTSTRAP] after vm_find_word");
#endif
    if (!entry || !entry->func) {
        vm->error = 1;
        log_message(LOG_ERROR, "bootstrap: missing vocabulary '%s'", name);
        return;
    }
#if defined(__STARKERNEL__) && SK_PARITY_DEBUG
    console_puts("[VOC] ");
    console_puts(name);
    console_puts(" entry=");
    vm_bootstrap_print_hex((uint64_t)(uintptr_t)entry);
    console_puts(" xt=");
    vm_bootstrap_print_hex((uint64_t)(uintptr_t)entry->func);
    console_println("");
    const VMHostServices *host = vm_host(vm);
    if (host && host->is_executable_ptr &&
        !host->is_executable_ptr((const void *)entry->func)) {
        console_puts("[VOC] invalid XT for ");
        console_puts(name);
        console_puts(" func=");
        vm_bootstrap_print_hex((uint64_t)(uintptr_t)entry->func);
        console_println("");
        if (host->panic) {
            host->panic("bootstrap vocabulary XT invalid");
        }
    }
#endif
    entry->func(vm);
    vocabulary_word_definitions(vm);
#ifdef __STARKERNEL__
    sk_vm_arena_assert_guards("vm_bootstrap_root_vocabulary:exit");
#endif
}

void vm_init(VM* vm)
{
    vm_init_with_host(vm, NULL);
}

void vm_init_with_host(VM* vm, const VMHostServices *host)
{
    if (!vm) return;

    *vm = (VM){0};
    vm->host = host ? host : vm_default_host_services();
#ifndef __STARKERNEL__
    vm_reset_hosted_fake_ns(vm);
#endif
    vm->interpreter_enabled = 0;
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

    vm->memory = (uint8_t*)vm_host_alloc(vm, VM_MEMORY_SIZE, sizeof(void*));
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

    log_message(LOG_DEBUG, "vm_init: memory=%p here=%zu", (void*)vm->memory, vm->here);

    vm_bootstrap_scr(vm);

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
    
#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
    console_println("[VM_BOOTSTRAP] register_forth79_words complete");
#endif
#endif
    
    /* Interpreter may be enabled only after parity check passes (in sk_vm_bootstrap.c) */

    /* Set FORGET fence to post-boot */
    vm->dict_fence_latest = vm->latest;
    vm->dict_fence_here = vm->here;

    /* Initialize hot-words cache */
    vm->hotwords_cache = (HotwordsCache*)vm_host_alloc(vm, sizeof(HotwordsCache), sizeof(void*));
    if (!vm->hotwords_cache)
    {
        log_message(LOG_ERROR, "vm_init: hotwords cache malloc failed");
        vm->error = 1;
        return;
    }
    hotwords_cache_init(vm->hotwords_cache);

    /* Initialize rolling window */
    if (rolling_window_init(&vm->rolling_window) != 0)
    {
        log_message(LOG_ERROR, "vm_init: rolling window malloc failed");
        vm->error = 1;
        return;
    }

    /* Initialize VM heartbeat */
    vm->heartbeat.tick_count = 0;
    vm->heartbeat.last_inference_tick = 0;
    vm->heartbeat.check_counter = 0;
    vm->heartbeat.heartbeat_enabled = 1;
    vm->heartbeat.tick_target_ns = HEARTBEAT_TICK_NS;
    vm->heartbeat.snapshot_index = 0;
    vm->heartbeat.worker = NULL;
    vm->heartbeat_decay_cursor_id = WORD_ID_INVALID;

    vm->pipeline_metrics.prefetch_attempts = 0;
    vm->pipeline_metrics.prefetch_hits = 0;
    vm->pipeline_metrics.window_tuning_checks = 0;
    vm->pipeline_metrics.last_checked_window_size = vm->rolling_window.effective_window_size;
    vm->pipeline_metrics.last_checked_accuracy = 0.0;
    vm->pipeline_metrics.suggested_next_size = vm->rolling_window.effective_window_size;

    heartbeat_publish_snapshot(vm);

#if HEARTBEAT_HAS_THREADS
    vm->heartbeat.worker = vm_host_calloc(vm, 1, sizeof(HeartbeatWorker));
    if (vm->heartbeat.worker)
    {
        vm->heartbeat.worker->tick_ns = HEARTBEAT_TICK_NS;
        if (pthread_create(&vm->heartbeat.worker->thread, NULL, heartbeat_thread_main, vm) != 0)
        {
            log_message(LOG_WARN, "heartbeat: pthread_create failed (%d), falling back to inline mode", errno);
            vm_host_free(vm, vm->heartbeat.worker);
            vm->heartbeat.worker = NULL;
        }
    }
    else
    {
        log_message(LOG_WARN, "heartbeat: worker allocation failed, using inline heartbeat");
    }
#endif

    vm->decay_slope_q48 = (1ULL << 16) / 3;
    vm->last_decay_check_ns = 0;
    vm->total_heat_at_last_check = 0;
    vm->stale_word_count_at_check = 0;
    vm->decay_slope_direction = 0;

    vm->lookup_strategy = 0;
    vm->last_bucket_reorg_ns = 0;
    dict_update_heat_percentiles(vm);

    vm->ssm_l8_state = vm_host_alloc(vm, sizeof(ssm_l8_state_t), sizeof(void*));
    if (!vm->ssm_l8_state)
    {
        log_message(LOG_ERROR, "vm_init: SSM L8 state malloc failed");
        vm->error = 1;
        return;
    }
    ssm_l8_init((ssm_l8_state_t*)vm->ssm_l8_state, SSM_MODE_C0);

    vm->ssm_config = vm_host_alloc(vm, sizeof(ssm_config_t), sizeof(void*));
    if (!vm->ssm_config)
    {
        log_message(LOG_ERROR, "vm_init: SSM config malloc failed");
        vm->error = 1;
        return;
    }
    ((ssm_config_t*)vm->ssm_config)->L2_rolling_window = 0;
    ((ssm_config_t*)vm->ssm_config)->L3_linear_decay = 0;
    ((ssm_config_t*)vm->ssm_config)->L5_window_inference = 0;
    ((ssm_config_t*)vm->ssm_config)->L6_decay_inference = 0;
}
