/*

                                 ***   StarForth   ***
  dictionary_management.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
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
    size_t total_size;
    DictEntry *entry;

    if (vm == NULL || name == NULL) {
        log_message(LOG_ERROR, "vm_create_word: NULL parameters");
        if (vm) vm->error = 1;
        return NULL;
    }

    if (len > WORD_NAME_MAX) {
        len = WORD_NAME_MAX;
    }

    /* Calculate total size: header + name */
    total_size = sizeof(DictEntry) + len;

    log_message(LOG_DEBUG, "vm_create_word: Creating '%.*s' (len=%zu, total_size=%zu)",
                (int)len, name, len, total_size);

    /* Align to cell boundary */
    vm_align(vm);

    /* Allocate space */
    entry = (DictEntry*)vm_allot(vm, total_size);
    if (entry == NULL) {
        log_message(LOG_ERROR, "vm_create_word: Failed to allocate dictionary entry");
        vm->error = 1;
        return NULL;
    }

    log_message(LOG_DEBUG, "vm_create_word: Allocated entry at %p", (void*)entry);

    /* Initialize entry */
    entry->link = vm->latest;
    entry->func = func;           /* Store function pointer */
    entry->flags = 0;
    entry->name_len = (uint8_t)len;

    /* Copy name safely */
    if (len > 0) {
        log_message(LOG_DEBUG, "vm_create_word: Copying name '%.*s'", (int)len, name);
        memcpy((void*)entry->name, name, len);
        log_message(LOG_DEBUG, "vm_create_word: Name copied successfully");
    }

    /* Update latest */
    vm->latest = entry;

    log_message(LOG_DEBUG, "vm_create_word: Successfully created '%.*s' at %p",
                (int)len, name, (void*)entry);

    vm_align(vm);  /* Align for future data field access */

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
    if (entry == NULL) {
        return NULL;
    }

    /* Data field comes right after the entry structure and name */
    /* vm_create_word() ensures proper alignment after the name */
    /* vm_create_word() ensures proper alignment after the name */
    uintptr_t entry_end = (uintptr_t)entry + sizeof(DictEntry) + entry->name_len;

    return (cell_t*)entry_end;
}

/**
 * @brief Marks the most recently defined word as immediate
 *
 * @param vm Pointer to the VM instance
 */
void vm_make_immediate(VM *vm) {
    if (vm->latest) {
        vm->latest->flags |= WORD_IMMEDIATE;
    }
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
