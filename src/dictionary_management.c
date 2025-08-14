/*

                                 ***   StarForth   ***
  dictionary_management.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/13/25, 5:16 PM
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
    DictEntry *entry = vm->latest;

    while (entry) {
        if (!(entry->flags & WORD_HIDDEN) &&
            !(entry->flags & WORD_SMUDGED) &&
            entry->name_len == len &&
            memcmp(entry->name, name, len) == 0) {
            return entry;
        }
        entry = entry->link;
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
