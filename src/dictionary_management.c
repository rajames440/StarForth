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

static DictEntry *sf_cached_latest = NULL; /* last head we indexed */

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
    DictEntry *newest = vm->latest;
    /* single push if the new head links to the previously cached head */
    if (newest && newest->link == sf_cached_latest) {
        unsigned fc = (unsigned char) newest->name[0];
        size_t n = sf_fc_count[fc];
        if (sf_fc_reserve(fc, n + 1)) {
            /* Keep oldest→…→newest ordering: append at the end. */
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
DictEntry *vm_find_word(VM *vm, const char *name, size_t len) {
    if (UNLIKELY(!vm || !name || len == 0)) return NULL;

    /* DoE counter: dictionary lookups */
    vm->heartbeat.dictionary_lookups++;

    /* Thread safety: Acquire dict_lock for read access to dictionary */
    sf_mutex_lock(&vm->dict_lock);

    /* Keep the index in sync with dictionary head, cheaply if possible. */
    if (UNLIKELY(sf_cached_latest != vm->latest)) sf_try_fast_append(vm);

    const unsigned char first = (unsigned char) name[0];
    DictEntry **bucket = sf_fc_list[first];
    size_t n = sf_fc_count[first];
    if (UNLIKELY(!bucket || n == 0)) {
        sf_mutex_unlock(&vm->dict_lock);
        return NULL;
    }

    /* Use hot-words cache for physics-driven frequency-based acceleration */
    DictEntry *found = hotwords_cache_lookup(vm->hotwords_cache, bucket, n, name, len);
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

        /* Length check first - most discriminating and cheapest */
        if ((size_t) e->name_len != len) continue;

#ifdef WORD_HIDDEN
        if (UNLIKELY(e->flags & WORD_HIDDEN)) continue;
#endif
#ifdef WORD_SMUDGED
        if (UNLIKELY(e->flags & WORD_SMUDGED)) continue;
#endif

        const char *en = e->name;
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
DictEntry *vm_create_word(VM *vm, const char *name, size_t len, word_func_t func) {
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

    entry = (DictEntry *) malloc(total);
    if (!entry) {
        vm->error = 1;
        log_message(LOG_ERROR, "vm_create_word: malloc failed for '%.*s' (%zu bytes)",
                    (int) len, name, total);
        entry = NULL;
        goto create_cleanup;
    }

    entry->link = vm->latest;
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

    vm->latest = entry;
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
