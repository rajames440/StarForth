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
 *  StarForth dictionary_management.c  —  FORTH-79 + ANSI C99
 *  Optimized: incremental FC index + capacity reuse + newest-first search
 */
#include "../include/dictionary_management.h"
#include "../include/vm.h"
#include "../include/vm_host.h"
#include "../include/io.h"
#include "../include/log.h"
#include "../include/physics_metadata.h"
#include "../include/physics_hotwords_cache.h"
#include "../include/physics_pipelining_metrics.h"
#include "../include/dictionary_heat_optimization.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
/* LIKELY/UNLIKELY macros are defined in vm.h */

#ifdef __STARKERNEL__
#include "starkernel/vm/vm_internal.h"
#include "starkernel/console.h"
#include "starkernel/hal/hal.h"
#endif

#ifndef SF_FC_BUCKETS
#define SF_FC_BUCKETS 256
#endif

/* Buckets of entry pointers, sizes, and capacities (we reuse memory). */
/* Note: sf_fc_list, sf_fc_count, and sf_fc_cap are not static to allow access from
 * dictionary_heat_optimization.c for heat-aware bucket reorganization with proper
 * capacity checks (ASan fix: 2025-12-09). */
DictEntry **sf_fc_list[SF_FC_BUCKETS];
size_t sf_fc_count[SF_FC_BUCKETS];
size_t sf_fc_cap[SF_FC_BUCKETS];

/* Forward declaration for debug printing */
static void sf_debug_print_hex(uint64_t value);

#ifdef __STARKERNEL__
static void sf_debug_dump_types(void) {
    console_println("[VM_TYPES] dumping sizes and offsets:");
    console_puts("sizeof(cell_t)="); sf_debug_print_hex(sizeof(cell_t));
    console_puts(" sizeof(void*)="); sf_debug_print_hex(sizeof(void*));
    console_puts(" sizeof(size_t)="); sf_debug_print_hex(sizeof(size_t));
    console_puts(" sizeof(DictEntry)="); sf_debug_print_hex(sizeof(DictEntry));
    console_puts(" sizeof(DictEntry*)="); sf_debug_print_hex(sizeof(DictEntry*));
    console_puts(" offsetof(link)="); sf_debug_print_hex(offsetof(DictEntry, link));
    console_puts(" offsetof(func)="); sf_debug_print_hex(offsetof(DictEntry, func));
    console_puts(" offsetof(flags)="); sf_debug_print_hex(offsetof(DictEntry, flags));
    console_puts(" offsetof(name_len)="); sf_debug_print_hex(offsetof(DictEntry, name_len));
    console_puts(" offsetof(execution_heat)="); sf_debug_print_hex(offsetof(DictEntry, execution_heat));
    console_puts(" offsetof(word_id)="); sf_debug_print_hex(offsetof(DictEntry, word_id));
    console_puts(" offsetof(name)="); sf_debug_print_hex(offsetof(DictEntry, name));
    
    console_puts(" offsetof(VM, memory)="); sf_debug_print_hex(offsetof(VM, memory));
    console_puts(" offsetof(VM, here)="); sf_debug_print_hex(offsetof(VM, here));
    console_puts(" offsetof(VM, latest)="); sf_debug_print_hex(offsetof(VM, latest));
    console_println("");
}
#endif

static DictEntry *sf_cached_latest = NULL; /* last head we indexed */

static vm_last_word_record_t vm_last_registered_word;
static VM *vm_last_registered_vm = NULL;
static size_t vm_last_here = 0;
static DictEntry *vm_last_latest = NULL;

static void vm_xt_fatal(VM *vm, const char *reason) {
    const VMHostServices *host = vm ? vm->host : NULL;
    if (host && host->panic) {
        host->panic(reason);
    }
}

#ifdef __STARKERNEL__
static void vm_dictionary_verify_xt_chain(VM *vm) {
    if (!vm || !vm->host || !vm->host->is_executable_ptr) {
        return;
    }
    for (DictEntry *entry = vm->latest; entry; entry = entry->link) {
        if (!entry->func) {
            log_message(LOG_ERROR,
                        "VM XT corruption: '%.*s' missing func pointer",
                        (int)(entry->name_len),
                        entry->name);
            vm_xt_fatal(vm, "VM XT missing func pointer");
        } else if (!vm->host->is_executable_ptr((const void *)entry->func)) {
            log_message(LOG_ERROR,
                        "VM XT corruption: '%.*s' func=%p",
                        (int)(entry->name_len),
                        entry->name,
                        (void *)entry->func);
            vm_xt_fatal(vm, "VM XT executable violation");
        }
    }
}
#endif

static int vm_xt_is_valid(VM *vm, const char *name, size_t len,
                          DictEntry *entry, size_t entry_bytes) {
    if (!vm || !entry) {
        return 0;
    }

    const VMHostServices *host = vm->host;
    if (host && host->owns_xt_entry) {
        if (!host->owns_xt_entry(entry, entry_bytes)) {
            log_message(LOG_ERROR,
                        "vm_create_word: XT for '%.*s' outside kernel dictionary heap (xt=%p size=%zu)",
                        (int)len,
                        name,
                        (void *)entry,
                        entry_bytes);
            vm_xt_fatal(vm, "VM XT ownership violation");
            return 0;
        }
    }

    if (!entry->func) {
        log_message(LOG_ERROR,
                    "vm_create_word: NULL func pointer for '%.*s'",
                    (int)len,
                    name);
        vm_xt_fatal(vm, "VM XT missing func pointer");
        return 0;
    }

    if (host && host->is_executable_ptr) {
        if (!host->is_executable_ptr((const void *)entry->func)) {
            log_message(LOG_ERROR,
                        "vm_create_word: HAL rejected func for '%.*s' (xt=%p func=%p)",
                        (int)len,
                        name,
                        (void *)entry,
                        (const void *)entry->func);
            vm_xt_fatal(vm, "VM XT executable violation");
            return 0;
        }
    }

    return 1;
}

static uint32_t vm_dictionary_acquire_word_id(VM *vm) {
    if (!vm) {
        return WORD_ID_INVALID;
    }

    if (vm->recycled_word_id_count > 0) {
        return vm->recycled_word_ids[--vm->recycled_word_id_count];
    }

    if (vm->next_word_id < DICTIONARY_SIZE) {
        return vm->next_word_id++;
    }

    return WORD_ID_INVALID;
}

void vm_dictionary_track_entry(VM *vm, DictEntry *entry) {
    if (!vm || !entry) {
        return;
    }

    uint32_t word_id = vm_dictionary_acquire_word_id(vm);
    entry->word_id = word_id;

    if (word_id == WORD_ID_INVALID || word_id >= DICTIONARY_SIZE) {
        log_message(LOG_WARN,
                    "vm_dictionary_track_entry: out of stable IDs, disabling metrics for '%.*s'",
                    (int)entry->name_len, entry->name);
        return;
    }

    vm->word_id_map[word_id] = entry;
}

void vm_dictionary_untrack_entry(VM *vm, DictEntry *entry) {
    if (!vm || !entry) {
        return;
    }

    uint32_t word_id = entry->word_id;
    if (word_id == WORD_ID_INVALID || word_id >= DICTIONARY_SIZE) {
        return;
    }

    if (vm->word_id_map[word_id] == entry) {
        vm->word_id_map[word_id] = NULL;
    }

    if (vm->recycled_word_id_count < DICTIONARY_SIZE) {
        vm->recycled_word_ids[vm->recycled_word_id_count++] = word_id;
    }

    entry->word_id = WORD_ID_INVALID;
}

DictEntry *vm_dictionary_lookup_by_word_id(VM *vm, uint32_t word_id) {
    if (!vm || word_id >= DICTIONARY_SIZE) {
        return NULL;
    }

    return vm->word_id_map[word_id];
}

const vm_last_word_record_t *vm_dictionary_last_word_record(void) {
    return &vm_last_registered_word;
}

void vm_dictionary_log_last_word(VM *vm, const char *tag) {
#ifdef __STARKERNEL__
    const vm_last_word_record_t *info = vm_dictionary_last_word_record();
    if (!info) return;
    VM *ctx = vm ? vm : vm_last_registered_vm;
    size_t here = ctx ? ctx->here : vm_last_here;
    DictEntry *latest = ctx ? ctx->latest : vm_last_latest;
    log_message(LOG_ERROR,
                "[dict][%s] last='%s' entry=%p func=%p here=0x%zx latest=%p latest='%s'",
                tag ? tag : "stage",
                info->name[0] ? info->name : "(none)",
                (const void *)info->entry,
                info->func,
                here,
                (void *)latest,
                latest ? latest->name : "(null)");
#else
    (void)vm;
    (void)tag;
#endif
}

/* --- helpers ------------------------------------------------------------- */

static inline void *sf_xrealloc(void *p, size_t nbytes) {
    void *r = realloc(p, nbytes);
    if (!r) {
        free(p);
        return NULL;
    }
    return r;
}

static void sf_fc_clear_all(void) {
    for (size_t i = 0; i < SF_FC_BUCKETS; ++i) {
        free(sf_fc_list[i]);
        sf_fc_list[i] = NULL;
        sf_fc_count[i] = 0;
        sf_fc_cap[i] = 0;
    }
}

/* Ensure bucket i has at least `need` capacity; grow geometrically. */
static int sf_fc_reserve(size_t i, size_t need) {
    if (sf_fc_cap[i] >= need) return 1;
    size_t newcap = sf_fc_cap[i] ? sf_fc_cap[i] : 8;
    while (newcap < need) newcap <<= 1;
    DictEntry **newv = (DictEntry **) sf_xrealloc(sf_fc_list[i], newcap * sizeof(*newv));
    if (!newv) return 0;
    sf_fc_list[i] = newv;
    sf_fc_cap[i] = newcap;
    return 1;
}

/* Full rebuild: count → reserve → fill oldest→newest (so backward scan = newest-first). */
static void sf_rebuild_fc_index(VM *vm) {
    /* Reset counts, keep capacity to avoid churn. */
    for (size_t i = 0; i < SF_FC_BUCKETS; ++i) sf_fc_count[i] = 0;

    /* Count first chars. */
    for (DictEntry *e = vm->latest; e; e = e->link) {
        unsigned fc = (unsigned char) e->name[0];
        sf_fc_count[fc]++;
    }

    /* Reserve memory once per bucket. */
    for (size_t i = 0; i < SF_FC_BUCKETS; ++i) {
        if (sf_fc_count[i] && !sf_fc_reserve(i, sf_fc_count[i])) {
            /* OOM for this bucket → treat as empty; we’ll just miss the fast path. */
            sf_fc_count[i] = 0;
        }
    }

    /* Back-fill so [0]=oldest, [count-1]=newest. */
    size_t fill_at[SF_FC_BUCKETS];
    for (size_t i = 0; i < SF_FC_BUCKETS; ++i) fill_at[i] = sf_fc_count[i];

    for (DictEntry *e = vm->latest; e; e = e->link) {
        unsigned fc = (unsigned char) e->name[0];
        if (sf_fc_count[fc] == 0 || !sf_fc_list[fc]) continue;
        sf_fc_list[fc][--fill_at[fc]] = e;
    }

    sf_cached_latest = vm->latest;
}

/* Fast path: if exactly one new word was pushed, append it without rebuilding. */
static void sf_try_fast_append(VM *vm) {
    if (!vm || vm->latest == sf_cached_latest) return;
#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
    console_puts("[FAST_APPEND] vm=");
    sf_debug_print_hex((uintptr_t)vm);
    console_puts(" newest=");
    sf_debug_print_hex((uintptr_t)vm->latest);
    console_puts(" cached=");
    sf_debug_print_hex((uintptr_t)sf_cached_latest);
    console_println("");
#endif
#endif
    DictEntry *newest = vm->latest;
    /* single push if the new head links to the previously cached head */
    if (newest && newest->link == sf_cached_latest) {
        unsigned fc = (unsigned char) newest->name[0];
        size_t n = sf_fc_count[fc];
#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
        console_puts("[FAST_APPEND] fc=");
        sf_debug_print_hex(fc);
        console_puts(" n=");
        sf_debug_print_hex(n);
        console_println("");
#endif
#endif
        if (sf_fc_reserve(fc, n + 1)) {
            /* Keep oldest→…→newest ordering: append at the end. */
#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
            console_puts("[BUCKET_STORE] bucket=");
            sf_debug_print_hex(fc);
            console_puts(" index=");
            sf_debug_print_hex(n);
            console_puts(" dest=");
            sf_debug_print_hex((uintptr_t)&sf_fc_list[fc][n]);
            console_puts(" value=");
            sf_debug_print_hex((uintptr_t)newest);
            console_println("");
#endif
#endif
#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
    {
        uintptr_t val = (uintptr_t)newest;
        if (val != 0 && vm_last_registered_vm && (val < (uintptr_t)vm_last_registered_vm->memory || val >= (uintptr_t)vm_last_registered_vm->memory + VM_MEMORY_SIZE)) {
             console_puts("[BUCKET_STORE] PANIC: newest outside arena: ");
             sf_debug_print_hex(val);
             console_println("");
        }
    }
#endif
#endif
            sf_fc_list[fc][n] = newest;
            sf_fc_count[fc] = n + 1;
            sf_cached_latest = newest;
            return;
        }
    }
    /* Otherwise fall back to a full rebuild (multiple changes, deletes, etc.). */
    sf_rebuild_fc_index(vm);
}

/* --- API ----------------------------------------------------------------- */

/* Find by name: newest-first within the first-character bucket. */
/**
 * @brief Finds a word in the dictionary by name
 *
 * Searches for a word in the dictionary using an optimized first-character index.
 * The search is performed newest-first within the matching first-character bucket.
 *
 * @param vm The virtual machine context
 * @param name The name to search for
 * @param len Length of the name string
 * @return DictEntry* Pointer to found dictionary entry or NULL if not found
 */
#ifdef __STARKERNEL__
#include "starkernel/console.h"
#include "starkernel/hal/hal.h"
/* Forward declaration for canonical check */
static inline int sf_is_canonical(uint64_t addr) {
    int64_t saddr = (int64_t)addr;
    return (saddr >> 47) == 0 || (saddr >> 47) == -1;
}
#endif

/* Helper for debug printing in kernel mode */
static void sf_debug_print_hex(uint64_t value) __attribute__((unused));
static void sf_debug_print_hex(uint64_t value) {
#ifdef __STARKERNEL__
    char buf[19];
    buf[0] = '0'; buf[1] = 'x'; buf[18] = '\0';
    for (int i = 0; i < 16; ++i) {
        uint8_t nibble = (uint8_t)((value >> ((15 - i) * 4)) & 0xF);
        buf[i + 2] = (nibble < 10) ? (char)('0' + nibble) : (char)('a' + nibble - 10);
    }
    console_puts(buf);
#else
    (void)value;
#endif
}

DictEntry *vm_find_word(VM *vm, const char *name, size_t len) {
#ifdef __STARKERNEL__
    static int types_dumped = 0;
    if (!types_dumped) { sf_debug_dump_types(); types_dumped = 1; }
    console_puts("[FIND] ENTRY name=");
    console_puts(name);
    console_println("");
#endif
    if (UNLIKELY(!vm || !name || len == 0)) return NULL;

#if defined(__STARKERNEL__) && defined(SK_PARITY_DEBUG) && SK_PARITY_DEBUG
    console_puts("[FIND] name=");
    console_puts(name);
    console_puts(" len=");
    sf_debug_print_hex(len);
    console_println("");
#endif

    /* DoE counter: dictionary lookups */
    vm->heartbeat.dictionary_lookups++;

    /* Thread safety: Acquire dict_lock for read access to dictionary */
#ifdef __STARKERNEL__
    console_println("[FIND] BEFORE LOCK");
#endif
    sf_mutex_lock(&vm->dict_lock);
#ifdef __STARKERNEL__
    console_println("[FIND] AFTER LOCK");
#endif

    /* Keep the index in sync with dictionary head, cheaply if possible. */
#ifdef __STARKERNEL__
    console_println("[FIND] BEFORE FAST APPEND");
#endif
    if (UNLIKELY(sf_cached_latest != vm->latest)) sf_try_fast_append(vm);
#ifdef __STARKERNEL__
    console_println("[FIND] AFTER FAST APPEND");
#endif

    const unsigned char first = (unsigned char) name[0];
#ifdef __STARKERNEL__
    console_println("[FIND] BEFORE BUCKET ACCESS");
#endif
    DictEntry **bucket = sf_fc_list[first];
    size_t n = sf_fc_count[first];
#ifdef __STARKERNEL__
    console_println("[FIND] AFTER BUCKET ACCESS");
    if (n > 0 && bucket) {
        DictEntry *e0 = bucket[n-1];
        uintptr_t base = (uintptr_t)vm->memory;
        uintptr_t end = base + VM_MEMORY_SIZE;
        console_puts("[FIND] bucket_ptr=");
        sf_debug_print_hex((uintptr_t)bucket);
        console_puts(" entry0=");
        sf_debug_print_hex((uintptr_t)e0);
        console_puts(" base=");
        sf_debug_print_hex(base);
        if (e0) {
            console_puts(" link=");
            sf_debug_print_hex((uintptr_t)e0->link);
            console_puts(" func=");
            sf_debug_print_hex((uintptr_t)e0->func);
        }
        console_println("");
        
        /* Canonical & Arena checks for bucket and entry */
        if (!sf_is_canonical((uintptr_t)bucket)) { console_println("PANIC: bucket non-canonical"); sk_hal_panic("bucket non-canonical"); }
        if (e0 && !sf_is_canonical((uintptr_t)e0)) { console_println("PANIC: entry non-canonical"); sk_hal_panic("entry non-canonical"); }
        
        /* DictEntry pointers should live in the arena */
        if (e0 && ((uintptr_t)e0 < base || (uintptr_t)e0 >= end)) {
            console_println("PANIC: entry outside arena");
            sk_hal_panic("entry outside arena");
        }
    }
#endif
    if (UNLIKELY(!bucket || n == 0)) {
        sf_mutex_unlock(&vm->dict_lock);
        return NULL;
    }

#if defined(__STARKERNEL__) && defined(SK_PARITY_DEBUG) && SK_PARITY_DEBUG
    console_puts("[FIND] bucket=");
    sf_debug_print_hex((uint64_t)(uintptr_t)bucket);
    console_puts(" count=");
    sf_debug_print_hex(n);
    console_println("");
#endif

    /* Use hot-words cache for physics-driven frequency-based acceleration */
#ifdef __STARKERNEL__
    console_println("[FIND] BEFORE HOTWORDS");
#endif
    DictEntry *found = hotwords_cache_lookup(vm->hotwords_cache, bucket, n, name, len);
#ifdef __STARKERNEL__
    console_println("[FIND] AFTER HOTWORDS");
#endif
    if (found) {
        sf_mutex_unlock(&vm->dict_lock);
        return found;
    }

    /* Phase 2: Choose lookup strategy based on pattern diversity */
    if (vm->lookup_strategy == 1) {
        /* Heat-aware lookup: search by heat percentiles (hot first) */
        DictEntry *result = dict_find_word_heat_aware(vm, name, len);
        sf_mutex_unlock(&vm->dict_lock);
        return result;
    }

    /* Fallback/Default: Manual search if cache didn't find it or is disabled
     * Pre-calculate last char only if needed */
    const unsigned char last = (len > 1) ? (unsigned char) name[len - 1] : 0;

    /* NEWEST-first: scan backwards so recent defs win and we bail early. */
    for (size_t i = n; i-- > 0;) {
        DictEntry *e = bucket[i];

#if defined(__STARKERNEL__) && defined(SK_PARITY_DEBUG) && SK_PARITY_DEBUG
        if (i % 10 == 0 || i == n - 1) {
            console_puts("[FIND] idx=");
            sf_debug_print_hex(i);
            console_puts(" entry=");
            sf_debug_print_hex((uint64_t)(uintptr_t)e);
            console_println("");
        }
#endif

        if (UNLIKELY(!e)) continue;

        /* Length check first - most discriminating and cheapest */
        if ((size_t) e->name_len != len) continue;

#ifdef WORD_HIDDEN
        if (UNLIKELY(e->flags & WORD_HIDDEN)) continue;
#endif
#ifdef WORD_SMUDGED
        if (UNLIKELY(e->flags & WORD_SMUDGED)) continue;
#endif

        const char *en = e->name;

#if defined(__STARKERNEL__) && defined(SK_PARITY_DEBUG) && SK_PARITY_DEBUG
        /* Sanity check name pointer before dereference */
        uint64_t name_addr = (uint64_t)(uintptr_t)en;
        if (name_addr < 0x1000 || (name_addr >= 0xa0000 && name_addr < 0x100000)) {
             console_puts("[FIND] SUSPICIOUS name ptr=");
             sf_debug_print_hex(name_addr);
             console_println("");
        }
#endif

        /* Last char check only for multi-char names - avoids many memcmp calls */
        if (len > 1 && (unsigned char) en[len - 1] != last) continue;

        /* Only call memcmp after all cheap filters pass */
        if (memcmp(en, name, len) == 0) {
            sf_mutex_unlock(&vm->dict_lock);
            return e;
        }
    }

    sf_mutex_unlock(&vm->dict_lock);
    return NULL;
}

/* Create a new word (unchanged layout); index append happens lazily on next lookup. */
/**
 * @brief Creates a new word in the dictionary
 *
 * Allocates and initializes a new dictionary entry with the given name and function.
 * The word is added to the dictionary but index update is deferred until next lookup.
 *
 * @param vm The virtual machine context
 * @param name Name for the new word
 * @param len Length of the name string
 * @param func Function pointer for the word's behavior
 * @return DictEntry* Pointer to new dictionary entry or NULL on failure
 */
#ifdef __STARKERNEL__
#include "starkernel/hal/hal.h"
#include "starkernel/console.h"
#include "starkernel/vm/arena.h"
#endif

DictEntry *vm_create_word(VM *vm, const char *name, size_t len, word_func_t func) {
#ifdef __STARKERNEL__
    static int types_dumped = 0;
    if (!types_dumped) {
        sf_debug_dump_types();
        types_dumped = 1;
    }
#endif
    if (!vm || !name || len == 0 || len > WORD_NAME_MAX || !func) {
        if (vm) {
            vm->error = 1;
            log_message(LOG_ERROR, "vm_create_word: invalid parameters (vm=%p, name=%p, len=%zu, max=%d)",
                        (void *) vm, (const void *) name, len, WORD_NAME_MAX);
        }
        return NULL;
    }
#ifdef __STARKERNEL__
    vm_dictionary_verify_xt_chain(vm);
#endif

    DictEntry *entry = NULL;
    int lock_held = 0;
    sf_mutex_lock(&vm->dict_lock);
    lock_held = 1;

    size_t base = offsetof(DictEntry, name);
    size_t name_bytes = len + 1;
    size_t align = sizeof(cell_t);
    size_t df_off = (base + name_bytes + (align - 1)) & ~(align - 1);
    size_t total = df_off + sizeof(cell_t);

#ifdef __STARKERNEL__
    /* In StarKernel, dictionary headers MUST be in the arena. 
       We use vm_allot to allocate from the arena instead of malloc. */
    entry = (DictEntry *) vm_allot(vm, total);
#else
    entry = (DictEntry *) malloc(total);
#endif
    if (!entry) {
        vm->error = 1;
        log_message(LOG_ERROR, "vm_create_word: malloc failed for '%.*s' (%zu bytes)",
                    (int) len, name, total);
        entry = NULL;
        goto create_cleanup;
    }

#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
    {
        uintptr_t base_m = (uintptr_t)vm->memory;
        uintptr_t end_m = base_m + VM_MEMORY_SIZE;
        if ((uintptr_t)entry < base_m || (uintptr_t)entry >= end_m) {
            console_puts("PANIC: vm_create_word: entry outside arena: ");
            sf_debug_print_hex((uintptr_t)entry);
            console_puts(" arena=");
            sf_debug_print_hex(base_m);
            console_println("");
            sk_hal_panic("entry outside arena");
        }
    }
#endif
#endif
#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
    {
        uintptr_t val = (uintptr_t)vm->latest;
        if (val != 0 && (val < (uintptr_t)vm->memory || val >= (uintptr_t)vm->memory + VM_MEMORY_SIZE)) {
             console_puts("[STORE_LINK] PANIC: vm->latest outside arena: ");
             sf_debug_print_hex(val);
             console_println("");
        }
    }
#endif
#endif
    entry->link = vm->latest;
#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
    console_puts("[STORE_LINK] entry=");
    sf_debug_print_hex((uintptr_t)entry);
    console_puts(" link_addr=");
    sf_debug_print_hex((uintptr_t)&entry->link);
    console_puts(" value=");
    sf_debug_print_hex((uintptr_t)vm->latest);
    console_println("");
#endif
#endif
    entry->func = func;
    entry->flags = 0;
    entry->name_len = (uint8_t) len;
    entry->execution_heat = 0; /* Initialize execution heat counter */
    entry->acl_default = ACL_USER_DEFAULT;
    entry->word_id = WORD_ID_INVALID;

    uint32_t header_bytes = (total > UINT32_MAX) ? UINT32_MAX : (uint32_t) total;
    physics_metadata_init(entry, header_bytes);
    physics_metadata_apply_seed(entry);

    /* Initialize word transition metrics for pipelining (Phase 1) */
    entry->transition_metrics = (WordTransitionMetrics *)malloc(sizeof(WordTransitionMetrics));
    if (entry->transition_metrics) {
        transition_metrics_init(entry->transition_metrics);
    } else {
        log_message(LOG_WARN, "vm_create_word: transition metrics malloc failed for '%.*s'", (int)len, name);
    }

    memcpy(entry->name, name, len);
    entry->name[len] = '\0';

    if (df_off > base + name_bytes) {
        memset(((uint8_t *) entry) + base + name_bytes, 0, df_off - (base + name_bytes));
    }
    *(cell_t *) (((uint8_t *) entry) + df_off) = 0;

    if (!vm_xt_is_valid(vm, name, len, entry, total)) {
        vm->error = 1;
        entry = NULL;
        goto create_cleanup;
    }

    size_t copy = (len < WORD_NAME_MAX) ? len : WORD_NAME_MAX;
    memcpy(vm_last_registered_word.name, name, copy);
    vm_last_registered_word.name[copy] = '\0';
    vm_last_registered_word.entry = entry;
    vm_last_registered_word.func = entry->func;
    vm_last_registered_vm = vm;
    vm_last_here = vm->here;
    vm_last_latest = vm->latest;

#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
    {
        uintptr_t val = (uintptr_t)entry;
        if (val != 0 && (val < (uintptr_t)vm->memory || val >= (uintptr_t)vm->memory + VM_MEMORY_SIZE)) {
             console_puts("[UPDATE_LATEST] PANIC: entry outside arena: ");
             sf_debug_print_hex(val);
             console_println("");
        }
    }
#endif
#endif
    vm->latest = entry;
#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
    console_puts("[UPDATE_LATEST] vm=");
    sf_debug_print_hex((uintptr_t)vm);
    console_puts(" latest_addr=");
    sf_debug_print_hex((uintptr_t)&vm->latest);
    console_puts(" value=");
    sf_debug_print_hex((uintptr_t)entry);
    console_println("");
    
    uintptr_t base_m = (uintptr_t)vm->memory;
    uintptr_t end_m = base_m + VM_MEMORY_SIZE;
    if ((uintptr_t)vm->latest < base_m || (uintptr_t)vm->latest >= end_m) {
        console_puts("PANIC: vm->latest outside arena after update: value=");
        sf_debug_print_hex((uintptr_t)vm->latest);
        console_println("");
        sk_hal_panic("vm->latest outside arena");
    }
#endif
#endif
    vm_dictionary_track_entry(vm, entry);

    log_message(LOG_DEBUG, "vm_create_word: '%.*s' len=%zu total=%zu @%p",
                (int) len, name, len, total, (void *) entry);

create_cleanup:
    if (lock_held)
        sf_mutex_unlock(&vm->dict_lock);
    return entry;
}

/* Linear scan by func (left as-is; usually cold path). */
/**
 * @brief Finds a word by its function pointer
 *
 * Performs a linear scan of the dictionary to find an entry with matching function.
 *
 * @param vm The virtual machine context
 * @param func Function pointer to search for
 * @return DictEntry* Pointer to found dictionary entry or NULL if not found
 */
DictEntry *vm_dictionary_find_by_func(VM *vm, word_func_t func) {
    for (DictEntry *e = vm ? vm->latest : NULL; e; e = e->link) {
        if (e->func == func) return e;
    }
    return NULL;
}

DictEntry *vm_dictionary_find_latest_by_func(VM *vm, word_func_t func) {
    return vm_dictionary_find_by_func(vm, func);
}

/**
 * @brief Gets pointer to a word's data field
 *
 * Calculates and returns pointer to the aligned data field following the name field.
 *
 * @param entry Dictionary entry to get data field from
 * @return cell_t* Pointer to data field or NULL if entry is NULL
 */
cell_t *vm_dictionary_get_data_field(DictEntry *entry) {
    if (!entry) return NULL;
    size_t base = offsetof(DictEntry, name);
    size_t name_bytes = (size_t) entry->name_len + 1;
    size_t align = sizeof(cell_t);
    size_t df_off = (base + name_bytes + (align - 1)) & ~(align - 1);
    return (cell_t *) (((uint8_t *) entry) + df_off);
}

/**
 * @brief Marks the latest word as hidden
 *
 * Sets the WORD_HIDDEN flag on the most recently defined word.
 *
 * @param vm The virtual machine context
 */
void vm_hide_word(VM *vm) {
    if (vm && vm->latest) {
        vm->latest->flags |= WORD_HIDDEN;
        physics_metadata_refresh_state(vm->latest);
    }
}

void vm_smudge_word(VM *vm) {
    if (!vm || !vm->latest) {
        if (vm) {
            vm->error = 1;
            log_message(LOG_ERROR, "vm_smudge_word: invalid vm or no latest word");
        }
        return;
    }
    vm->latest->flags ^= WORD_SMUDGED;
    physics_metadata_refresh_state(vm->latest);
}

void vm_pin_execution_heat(VM *vm) {
    if (!vm || !vm->latest) {
        if (vm) {
            vm->error = 1;
            log_message(LOG_ERROR, "vm_pin_execution_heat: invalid vm or no latest word");
        }
        return;
    }
    vm->latest->flags |= WORD_PINNED;
    physics_metadata_refresh_state(vm->latest);
}

void vm_unpin_execution_heat(VM *vm) {
    if (!vm || !vm->latest) {
        if (vm) {
            vm->error = 1;
            log_message(LOG_ERROR, "vm_unpin_execution_heat: invalid vm or no latest word");
        }
        return;
    }
    vm->latest->flags &= ~WORD_PINNED;
    physics_metadata_refresh_state(vm->latest);
}

/* If you ever need to hard-reset (e.g., switching vocabularies wholesale) */
/**
 * @brief Resets the dictionary index
 *
 * Clears all index data and cached state. Used when switching vocabularies.
 */
void vm_dictionary_index_reset(void) {
    sf_cached_latest = NULL;
    sf_fc_clear_all();
}
