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
#include "../include/vm_host.h"
#ifdef __STARKERNEL__
#include "starkernel/vm/arena.h"
#endif
#include "word_source/include/vocabulary_words.h"
#include "vm_internal.h"

#ifndef __STARKERNEL__
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#endif

#include <string.h>
#include <stdint.h>

/* ====================== Host services helpers ======================= */

#ifndef __STARKERNEL__
static uint64_t hosted_fake_ns = 0;

static void* hosted_alloc(size_t size, size_t align)
{
    (void)align;
    return malloc(size);
}

static void hosted_free(void *ptr)
{
    free(ptr);
}

static uint64_t hosted_time_ns(void)
{
#if PARITY_MODE
    hosted_fake_ns += 1000; /* Deterministic 1us increments */
    return hosted_fake_ns;
#else
    return sf_monotonic_ns();
#endif
}

static int hosted_mutex_init(void *mutex) { return sf_mutex_init((sf_mutex_t *)mutex); }
static int hosted_mutex_lock(void *mutex) { sf_mutex_lock((sf_mutex_t *)mutex); return 0; }
static int hosted_mutex_unlock(void *mutex) { sf_mutex_unlock((sf_mutex_t *)mutex); return 0; }
static void hosted_mutex_destroy(void *mutex) { sf_mutex_destroy((sf_mutex_t *)mutex); }

static int hosted_puts(const char *str)
{
    if (!str) return -1;
    if (fputs(str, stdout) < 0) return -1;
    return (int)strlen(str);
}

static int hosted_putc(int c)
{
    return (fputc(c, stdout) == EOF) ? -1 : 1;
}

static void hosted_panic(const char *message)
{
    if (message) {
        fprintf(stderr, "[StarForth panic] %s\n", message);
    } else {
        fprintf(stderr, "[StarForth panic] unknown\n");
    }
    fflush(stderr);
    abort();
}

static bool hosted_xt_is_executable(const void *ptr)
{
    (void)ptr;
    return true;
}

static bool hosted_xt_entry_owned(const void *ptr, size_t bytes)
{
    (void)ptr;
    (void)bytes;
    return true;
}

static const VMHostServices hosted_services = {
    .alloc = hosted_alloc,
    .free = hosted_free,
    .monotonic_ns = hosted_time_ns,
    .mutex_init = hosted_mutex_init,
    .mutex_lock = hosted_mutex_lock,
    .mutex_unlock = hosted_mutex_unlock,
    .mutex_destroy = hosted_mutex_destroy,
    .puts = hosted_puts,
    .putc = hosted_putc,
    .is_executable_ptr = hosted_xt_is_executable,
    .owns_xt_entry = hosted_xt_entry_owned,
    .panic = hosted_panic,
    .parity_mode = PARITY_MODE,
    .verbose = 0
};

#ifndef __STARKERNEL__
void vm_reset_hosted_fake_ns(VM *vm)
{
    if (vm && vm->host == &hosted_services && PARITY_MODE) {
        hosted_fake_ns = 0;
    }
}
#endif
#endif /* !__STARKERNEL__ */

static void vm_assert_interpreter_enabled(VM *vm, const char *caller)
{
    if (vm && vm->interpreter_enabled) {
        return;
    }
    if (vm) {
        vm->error = 1;
    }
    log_message(LOG_ERROR, "[vm] interpreter disabled (caller=%s)", caller ? caller : "unknown");
    const VMHostServices *host = vm_host(vm);
    if (host && host->panic) {
        host->panic("interpreter invoked before bootstrap completion");
    }
}

void vm_enable_interpreter(VM *vm)
{
    if (vm) {
        vm->interpreter_enabled = 1;
    }
}

const VMHostServices* vm_default_host_services(void)
{
#ifdef __STARKERNEL__
    return sk_host_services();
#else
    return &hosted_services;
#endif
}

const VMHostServices* vm_host(const VM *vm)
{
    const VMHostServices *host = vm ? vm->host : NULL;
    return host ? host : vm_default_host_services();
}

void* vm_host_alloc(VM *vm, size_t size, size_t align)
{
    const VMHostServices *host = vm_host(vm);
    if (!host || !host->alloc || size == 0) {
        return NULL;
    }
    if (align == 0) {
        align = sizeof(void*);
    }
    return host->alloc(size, align);
}

void* vm_host_calloc(VM *vm, size_t n, size_t size)
{
    size_t total = n * size;
    void *p = vm_host_alloc(vm, total, sizeof(void*));
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

void vm_host_free(VM *vm, void *ptr)
{
    const VMHostServices *host = vm_host(vm);
    if (ptr && host && host->free) {
        host->free(ptr);
    }
}

uint64_t vm_monotonic_ns(VM *vm)
{
    const VMHostServices *host = vm_host(vm);
    if (host && host->monotonic_ns) {
        return host->monotonic_ns();
    }
    return 0;
}

unsigned vm_get_base(const VM* vm)
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

void vm_set_base(VM* vm, unsigned b)
{
    if (!vm) return;
    if (b < 2 || b > 36) b = 10;
    vm_store_cell(vm, vm->base_addr, (cell_t)b);
    vm->base = (cell_t)b; /* host mirror */
}

/* ====================== VM init / teardown ======================= */
void vm_cleanup(VM* vm)
{
    if (!vm) return;

#if HEARTBEAT_HAS_THREADS
    if (vm->heartbeat.worker)
    {
        vm->heartbeat.worker->stop_requested = 1;
        pthread_join(vm->heartbeat.worker->thread, NULL);
        vm_host_free(vm, vm->heartbeat.worker);
        vm->heartbeat.worker = NULL;
    }
#endif

    /* Clean up hot-words cache */
    if (vm->hotwords_cache)
    {
        hotwords_cache_cleanup(vm->hotwords_cache);
        vm_host_free(vm, vm->hotwords_cache);
        vm->hotwords_cache = NULL;
    }

    /* Clean up rolling window of truth */
    rolling_window_cleanup(&vm->rolling_window);

    /* Clean up SSM L8 state */
    if (vm->ssm_l8_state)
    {
        vm_host_free(vm, vm->ssm_l8_state);
        vm->ssm_l8_state = NULL;
    }
    if (vm->ssm_config)
    {
        vm_host_free(vm, vm->ssm_config);
        vm->ssm_config = NULL;
    }

    if (vm->memory)
    {
        vm_host_free(vm, vm->memory);
        vm->memory = NULL;
    }
    vm->here = 0;

    sf_mutex_destroy(&vm->tuning_lock);
    sf_mutex_destroy(&vm->dict_lock);
}

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
    vm_assert_interpreter_enabled(vm, "vm_parse_word");

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
    vm_assert_interpreter_enabled(vm, "vm_parse_number");

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
    vm_assert_interpreter_enabled(vm, "execute_colon_word");

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
            uint64_t now_ns = vm_monotonic_ns(vm);
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
            physics_metadata_touch(w, w->execution_heat, vm_monotonic_ns(vm));
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
    vm_assert_interpreter_enabled(vm, "vm_interpret_word");

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
        uint64_t lookup_ns = vm_monotonic_ns(vm);

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
            physics_metadata_touch(entry, entry->execution_heat, vm_monotonic_ns(vm));
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
    vm_assert_interpreter_enabled(vm, "vm_interpret");

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
#ifdef __STARKERNEL__
#define vm_check_arena(tag) sk_vm_arena_assert_guards(tag)
#else
#define vm_check_arena(tag) ((void)0)
#endif
