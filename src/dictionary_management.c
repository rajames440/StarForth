/*

                                 ***   StarForth   ***
  dictionary_management.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/14/25, 10:56 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../include/vm.h"
#include "../include/io.h"
#include "../include/log.h"
#include <string.h> /* ISO C99: memcpy, memcmp, etc. */
#include <stddef.h>   /* offsetof */
#include <stdlib.h>   /* malloc, free */

/* ---- simple first-character index (lazy) ---- */
#ifndef SF_FC_BUCKETS
#define SF_FC_BUCKETS 256
#endif

static DictEntry   **sf_fc_list[SF_FC_BUCKETS];   /* arrays of entry pointers */
static size_t        sf_fc_count[SF_FC_BUCKETS];  /* sizes for each bucket */
static DictEntry    *sf_cached_latest = NULL;     /* detect when dict changes */

static void sf_free_fc_index(void) {
    for (size_t i = 0; i < SF_FC_BUCKETS; ++i) {
        free(sf_fc_list[i]);
        sf_fc_list[i] = NULL;
        sf_fc_count[i] = 0;
    }
}

static void sf_rebuild_fc_index(VM *vm) {
    /* two-pass: count, alloc, then fill in oldest→newest order */
    sf_free_fc_index();

    /* count by first char */
    for (DictEntry *e = vm->latest; e; e = e->link) {
        unsigned fc = (unsigned char)e->name[0];
        sf_fc_count[fc]++;
    }
    /* allocate arrays */
    for (size_t i = 0; i < SF_FC_BUCKETS; ++i) {
        if (sf_fc_count[i]) {
            sf_fc_list[i] = (DictEntry **)malloc(sf_fc_count[i] * sizeof(DictEntry*));
        }
    }
    /* fill from oldest to newest so we can search newest-first later */
    size_t filled[SF_FC_BUCKETS] = {0};
    /* walk to the tail to preserve order: collect into a temporary stack */
    /* simpler: collect forward and then reverse when searching */
    for (DictEntry *e = vm->latest; e; e = e->link) {
        /* nothing else needed; we’ll reverse on search by iterating backwards */
        unsigned fc = (unsigned char)e->name[0];
        sf_fc_list[fc][filled[fc]++] = e;
    }

    sf_cached_latest = vm->latest;
}

/**
 * @file dictionary_management.c
 * @brief Implementation of Forth dictionary management functions
 *
 * This file contains the implementation of dictionary operations for the StarForth
 * virtual machine, including word lookup, creation, and flag manipulation.
 */

/**
 * @brief Searches for a word in the dictionary by name
 *
 * @param vm Pointer to the VM instance
 * @param name Name of the word to find
 * @param len Length of the name string
 * @return DictEntry* Pointer to the found dictionary entry, or NULL if not found
 */
DictEntry* vm_find_word(VM *vm, const char *name, size_t len) {
    if (!vm || !name || len == 0) return NULL;

    /* rebuild index if dictionary head changed */
    if (sf_cached_latest != vm->latest) {
        sf_rebuild_fc_index(vm);
    }

    const unsigned char first = (unsigned char)name[0];
    const unsigned char last  = (unsigned char)name[len - 1];

    DictEntry **bucket = sf_fc_list[first];
    size_t      n      = sf_fc_count[first];

    /* newest-first: iterate backward through the bucket */
    for (size_t i = n; i-- > 0;) {
        DictEntry *e = bucket[i];

        /* skip hidden/smudged only after cheap rejects */
        if ((size_t)e->name_len != len) continue;

        const char *en = e->name;
        if ((unsigned char)en[len - 1] != last) continue;

#ifdef WORD_HIDDEN
        if (e->flags & WORD_HIDDEN) continue;
#endif
#ifdef WORD_SMUDGED
        if (e->flags & WORD_SMUDGED) continue;
#endif
        if (memcmp(en, name, len) == 0) return e;
    }
    return NULL;
}




/**
 * @brief Creates a new word in the dictionary
 *
 * @param vm Pointer to the VM instance
 * @param name Name of the word to create
 * @param len Length of the name string
 * @param func Function pointer for the word's behavior
 * @return DictEntry* Pointer to the created dictionary entry, or NULL on failure
 */
DictEntry* vm_create_word(VM *vm, const char *name, size_t len, word_func_t func) {
    if (!vm || !name || len == 0 || len > WORD_NAME_MAX) {
        if (vm) vm->error = 1;
        return NULL;
    }

    /* Layout: [link|func|flags|name_len|name...|NUL|pad→cell align|cell_t DFA] */
    size_t base       = offsetof(DictEntry, name);
    size_t name_bytes = len + 1;

    size_t align  = sizeof(cell_t);
    size_t df_off = (base + name_bytes + (align - 1)) & ~(align - 1);
    size_t total  = df_off + sizeof(cell_t);

    DictEntry *entry = (DictEntry*)malloc(total);
    if (!entry) { vm->error = 1; return NULL; }

    entry->link     = vm->latest;
    entry->func     = func;
    entry->flags    = 0;
    entry->name_len = (uint8_t)len;

    memcpy(entry->name, name, len);
    entry->name[len] = '\0';

    /* Zero padding and DFA cell */
    if (df_off > base + name_bytes) {
        memset(((uint8_t*)entry) + base + name_bytes, 0, df_off - (base + name_bytes));
    }
    cell_t *df_cell = (cell_t*)(((uint8_t*)entry) + df_off);
    *df_cell = 0;

    vm->latest = entry;

    log_message(LOG_DEBUG, "vm_create_word: Creating '%.*s' (len=%zu, total_size=%zu)",
                (int)len, name, len, total);
    log_message(LOG_DEBUG, "vm_create_word: Allocated entry at %p", (void*)entry);
    log_message(LOG_DEBUG, "vm_create_word: Successfully created '%.*s' at %p",
                (int)len, name, (void*)entry);

    return entry;
}

/**
 * @brief Searches for a word in the dictionary by its function pointer
 *
 * @param vm Pointer to the VM instance
 * @param func Function pointer to search for
 * @return DictEntry* Pointer to the found dictionary entry, or NULL if not found
 */
DictEntry* vm_dictionary_find_by_func(VM *vm, word_func_t func) {
    DictEntry *entry = vm->latest;

    while (entry != NULL) {
        if (entry->func == func) {
            return entry;
        }
        entry = entry->link;
    }
    return NULL;
}

/**
 * @brief Finds the most recent word definition with the given function
 *
 * @param vm Pointer to the VM instance
 * @param func Function pointer to search for
 * @return DictEntry* Pointer to the most recent matching entry, or NULL if not found
 */
DictEntry* vm_dictionary_find_latest_by_func(VM *vm, word_func_t func) {
    /* Since dictionary is ordered newest-first, first match is latest */
    return vm_dictionary_find_by_func(vm, func);
}

/**
 * @brief Gets the data field pointer for a dictionary entry
 *
 * @param entry Pointer to the dictionary entry
 * @return cell_t* Pointer to the entry's data field, or NULL if entry is NULL
 */
cell_t* vm_dictionary_get_data_field(DictEntry *entry) {
    if (!entry) return NULL;

    size_t base       = offsetof(DictEntry, name);
    size_t name_bytes = (size_t)entry->name_len + 1;
    size_t align      = sizeof(cell_t);
    size_t df_off     = (base + name_bytes + (align - 1)) & ~(align - 1);

    return (cell_t*)(((uint8_t*)entry) + df_off);
}


/**
 * @brief Hides the most recently defined word from dictionary searches
 *
 * @param vm Pointer to the VM instance
 */
void vm_hide_word(VM *vm) {
    if (vm->latest) {
        vm->latest->flags |= WORD_HIDDEN;
    }
}

/**
 * @brief Toggles the smudge bit of the most recently defined word
 *
 * @param vm Pointer to the VM instance
 */
void vm_smudge_word(VM *vm) {
    if (vm->latest == NULL) {
        vm->error = 1;
        return;
    }

    /* Toggle the smudge bit */
    vm->latest->flags ^= WORD_SMUDGED;
}
