/*
                                  ***   StarForth   ***

  dictionary_management.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T08:13:23.631-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/dictionary_management.c
 */

/*
 *  StarForth dictionary_management.c  —  FORTH-79 + ANSI C99
 *  Optimized: incremental FC index + capacity reuse + newest-first search
 */
#include "../include/vm.h"
#include "../include/io.h"
#include "../include/log.h"
#include "../include/physics_metadata.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

/* Branch prediction hints if not already defined */
#ifndef LIKELY
#define LIKELY(x) __builtin_expect(!!(x), 1)
#endif
#ifndef UNLIKELY
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

#ifndef SF_FC_BUCKETS
#define SF_FC_BUCKETS 256
#endif

/* Buckets of entry pointers, sizes, and capacities (we reuse memory). */
static DictEntry **sf_fc_list[SF_FC_BUCKETS];
static size_t sf_fc_count[SF_FC_BUCKETS];
static size_t sf_fc_cap[SF_FC_BUCKETS];

static DictEntry *sf_cached_latest = NULL; /* last head we indexed */

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

    /* Keep the index in sync with dictionary head, cheaply if possible. */
    if (UNLIKELY(sf_cached_latest != vm->latest)) sf_try_fast_append(vm);

    const unsigned char first = (unsigned char) name[0];
    DictEntry **bucket = sf_fc_list[first];
    size_t n = sf_fc_count[first];
    if (UNLIKELY(!bucket || n == 0)) return NULL;

    /* Pre-calculate last char only if needed */
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
        if (memcmp(en, name, len) == 0) return e;
    }
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
    if (!vm || !name || len == 0 || len > WORD_NAME_MAX) {
        if (vm) {
            vm->error = 1;
            log_message(LOG_ERROR, "vm_create_word: invalid parameters (vm=%p, name=%p, len=%zu, max=%d)",
                        (void *) vm, (const void *) name, len, WORD_NAME_MAX);
        }
        return NULL;
    }

    size_t base = offsetof(DictEntry, name);
    size_t name_bytes = len + 1;
    size_t align = sizeof(cell_t);
    size_t df_off = (base + name_bytes + (align - 1)) & ~(align - 1);
    size_t total = df_off + sizeof(cell_t);

    DictEntry *entry = (DictEntry *) malloc(total);
    if (!entry) {
        vm->error = 1;
        log_message(LOG_ERROR, "vm_create_word: malloc failed for '%.*s' (%zu bytes)",
                    (int) len, name, total);
        return NULL;
    }

    entry->link = vm->latest;
    entry->func = func;
    entry->flags = 0;
    entry->name_len = (uint8_t) len;
    entry->entropy = 0; /* Initialize usage counter */
    entry->acl_default = ACL_USER_DEFAULT;

    uint32_t header_bytes = (total > UINT32_MAX) ? UINT32_MAX : (uint32_t) total;
    physics_metadata_init(entry, header_bytes);
    physics_metadata_apply_seed(entry);

    memcpy(entry->name, name, len);
    entry->name[len] = '\0';

    if (df_off > base + name_bytes) {
        memset(((uint8_t *) entry) + base + name_bytes, 0, df_off - (base + name_bytes));
    }
    *(cell_t *) (((uint8_t *) entry) + df_off) = 0;

    vm->latest = entry;

    log_message(LOG_DEBUG, "vm_create_word: '%.*s' len=%zu total=%zu @%p",
                (int) len, name, len, total, (void *) entry);
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

void vm_pin_entropy(VM *vm) {
    if (!vm || !vm->latest) {
        if (vm) {
            vm->error = 1;
            log_message(LOG_ERROR, "vm_pin_entropy: invalid vm or no latest word");
        }
        return;
    }
    vm->latest->flags |= WORD_PINNED;
    physics_metadata_refresh_state(vm->latest);
}

void vm_unpin_entropy(VM *vm) {
    if (!vm || !vm->latest) {
        if (vm) {
            vm->error = 1;
            log_message(LOG_ERROR, "vm_unpin_entropy: invalid vm or no latest word");
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