/*

                                 ***   StarForth   ***
  vocabulary_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/12/25, 6:05 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/* vocabulary_words.c - FORTH-79 Vocabulary System Words */
#include "include/vocabulary_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"
#include <string.h>
#include <stdio.h>

/* FORTH-79 Vocabulary System Implementation */

/* Maximum vocabulary search order depth */
#define MAX_VOCAB_SEARCH_ORDER 8

/* Vocabulary system state (host-side) */
static DictEntry *current_vocab = NULL;                           /* CURRENT - where new words go */
static DictEntry *context_vocabs[MAX_VOCAB_SEARCH_ORDER];         /* CONTEXT - search order (top is highest index) */
static int        search_order_depth = 1;                         /* Current search order depth */

/* VM-visible variables (VM addresses / offsets) */
static vaddr_t context_var_addr = 0;   /* VM cell holding pointer to top-of-search vocabulary (DictEntry*) */
static vaddr_t current_var_addr = 0;   /* VM cell holding pointer to CURRENT vocabulary (DictEntry*) */

/* Sync host state -> VM cells */
static inline void vocab_sync_vm_vars(VM *vm) {
    if (!context_var_addr || !current_var_addr) return;
    DictEntry *top = (search_order_depth > 0) ? context_vocabs[search_order_depth - 1] : NULL;
    vm_store_cell(vm, context_var_addr, (cell_t)(uintptr_t)top);
    vm_store_cell(vm, current_var_addr, (cell_t)(uintptr_t)current_vocab);
}

/* Initialize vocabulary system and VM cells (once) */
static void init_vocabulary_system(VM *vm) {
    static int initialized = 0;
    if (!initialized) {
        current_vocab = vm->latest;         /* Start with FORTH vocabulary */
        context_vocabs[0] = vm->latest;     /* FORTH is default search vocabulary */
        search_order_depth = 1;

        /* Allocate VM cells for CONTEXT and CURRENT (variables return their VM addresses) */
        void *p1 = vm_allot(vm, sizeof(cell_t));
        if (!p1) { log_message(LOG_ERROR, "VOCAB: failed to allot CONTEXT cell"); vm->error = 1; return; }
        context_var_addr = (vaddr_t)((uint8_t*)p1 - vm->memory);

        void *p2 = vm_allot(vm, sizeof(cell_t));
        if (!p2) { log_message(LOG_ERROR, "VOCAB: failed to allot CURRENT cell"); vm->error = 1; return; }
        current_var_addr = (vaddr_t)((uint8_t*)p2 - vm->memory);

        vocab_sync_vm_vars(vm);
        initialized = 1;
    }
}

/* Find word in vocabulary search order */
static DictEntry* vocab_find_word(VM *vm, const char *name, size_t len) {
    init_vocabulary_system(vm);

    /* Search through vocabulary search order (top of stack first) */
    for (int i = search_order_depth - 1; i >= 0; i--) {
        DictEntry *entry = context_vocabs[i];

        /* Search this vocabulary chain */
        while (entry) {
            if (!(entry->flags & WORD_HIDDEN) &&
                entry->name_len == len &&
                memcmp(entry->name, name, len) == 0) {
                return entry;
            }
            entry = entry->link;
        }
    }

    return NULL;
}

/* Vocabulary word execution function */
static void vocabulary_select_runtime(VM *vm) {
    /* In a full system we'd select the executed vocabulary as context.
       For now, keep FORTH default. */
    init_vocabulary_system(vm);
    context_vocabs[0] = vm->latest;
    search_order_depth = 1;
    vocab_sync_vm_vars(vm);

    log_message(LOG_DEBUG, "Vocabulary selected for search order");
}

/* VOCABULARY ( -- )  Create new vocabulary */
void vocabulary_word_vocabulary(VM *vm) {
    char vocab_name[64];

    if (!vm_parse_word(vm, vocab_name, sizeof(vocab_name))) {
        log_message(LOG_ERROR, "VOCABULARY: Expected vocabulary name");
        vm->error = 1;
        return;
    }

    /* Create a header that executes vocabulary_select_runtime when invoked */
    DictEntry *vocab_entry =
        vm_create_word(vm, vocab_name, strlen(vocab_name), vocabulary_select_runtime);
    if (vocab_entry == NULL) {
        vm->error = 1;
        return;
    }

    /* --- REVEAL: make the header visible to the interpreter immediately --- */
#ifdef WORD_SMUDGED
    vocab_entry->flags &= ~WORD_SMUDGED;   /* clear smudge bit (what ';' would do) */
#endif
#ifdef WORD_HIDDEN
    vocab_entry->flags &= ~WORD_HIDDEN;    /* clear hidden bit if you also use it */
#endif

    /* Internal marker (keep your existing semantics) */
    vocab_entry->flags |= 0x40;

    /* Ensure top-of-search sees newest entry (if using vocab-based lookup) */
    init_vocabulary_system(vm);
    context_vocabs[search_order_depth - 1] = vm->latest;
    vocab_sync_vm_vars(vm);

    log_message(LOG_DEBUG, "VOCABULARY: Created vocabulary '%s'", vocab_name);
}

/* DEFINITIONS ( -- )  Set current vocabulary for new definitions */
void vocabulary_word_definitions(VM *vm) {
    init_vocabulary_system(vm);
    if (search_order_depth > 0) {
        current_vocab = context_vocabs[search_order_depth - 1];
    }
    vocab_sync_vm_vars(vm);
    log_message(LOG_DEBUG, "DEFINITIONS: Current vocabulary set");
}

/* CONTEXT ( -- addr )  Variable: returns VM address (offset) of CONTEXT cell */
void vocabulary_word_context(VM *vm) {
    init_vocabulary_system(vm);
    vm_push(vm, CELL(context_var_addr));
}

/* CURRENT ( -- addr )  Variable: returns VM address (offset) of CURRENT cell */
void vocabulary_word_current(VM *vm) {
    init_vocabulary_system(vm);
    vm_push(vm, CELL(current_var_addr));
}

/* FORTH ( -- )  Select FORTH vocabulary */
void vocabulary_word_forth(VM *vm) {
    init_vocabulary_system(vm);

    /* Set FORTH as single vocabulary in search order */
    context_vocabs[0] = vm->latest;  /* FORTH vocabulary root */
    search_order_depth = 1;
    current_vocab = vm->latest;
    vocab_sync_vm_vars(vm);

    log_message(LOG_DEBUG, "FORTH vocabulary selected");
}

/* ONLY ( -- )  Set minimal search order (FORTH only) */
void vocabulary_word_only(VM *vm) {
    init_vocabulary_system(vm);

    context_vocabs[0] = vm->latest;
    search_order_depth = 1;
    current_vocab = vm->latest;
    vocab_sync_vm_vars(vm);

    log_message(LOG_DEBUG, "ONLY: Minimal search order set (FORTH only)");
}

/* ALSO ( -- )  Duplicate top of search order */
void vocabulary_word_also(VM *vm) {
    init_vocabulary_system(vm);

    if (search_order_depth >= MAX_VOCAB_SEARCH_ORDER) {
        log_message(LOG_ERROR, "ALSO: Search order stack overflow");
        vm->error = 1;
        return;
    }

    /* Duplicate top of search order */
    context_vocabs[search_order_depth] = context_vocabs[search_order_depth - 1];
    search_order_depth++;
    vocab_sync_vm_vars(vm);

    log_message(LOG_DEBUG, "ALSO: Search order duplicated (depth=%d)", search_order_depth);
}

/* ORDER ( -- )  Display search order */
void vocabulary_word_order(VM *vm) {
    init_vocabulary_system(vm);

    printf("Search order: ");
    for (int i = search_order_depth - 1; i >= 0; i--) {
        /* In a full implementation, we would print the actual vocabulary name.
           Placeholder: assume FORTH. */
        printf("FORTH ");
    }
    printf("\n");

    printf("Current: FORTH\n");
}

/* PREVIOUS ( -- )  Remove top of search order */
void vocabulary_word_previous(VM *vm) {
    init_vocabulary_system(vm);

    if (search_order_depth <= 1) {
        log_message(LOG_ERROR, "PREVIOUS: Cannot remove last vocabulary from search order");
        vm->error = 1;
        return;
    }

    search_order_depth--;
    vocab_sync_vm_vars(vm);

    log_message(LOG_DEBUG, "PREVIOUS: Removed top of search order (depth=%d)", search_order_depth);
}

/* (FIND) ( addr -- addr flag )  Find word in dictionary (primitive)
   NOTE: addr is a VM address (offset) to a COUNTED string. */
void vocabulary_word_paren_find(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }

    /* Keep addr on stack */
    cell_t addr = vm->data_stack[vm->dsp];
    vaddr_t a   = VM_ADDR(addr);

    if (!vm_addr_ok(vm, a, 1)) {
        vm_push(vm, 0);  /* Not found */
        return;
    }

    uint8_t *counted_str = vm_ptr(vm, a);
    if (!counted_str) {
        vm_push(vm, 0);
        return;
    }

    uint8_t name_len = counted_str[0];
    if (!vm_addr_ok(vm, a + 1, name_len)) {
        vm_push(vm, 0);
        return;
    }
    const char *name = (const char*)&counted_str[1];

    /* Search using vocabulary system */
    DictEntry *entry = vocab_find_word(vm, name, (size_t)name_len);

    if (entry != NULL) {
        /* Replace addr with execution token, push flag: 1 if IMMEDIATE, else -1 (Forth-79) */
        vm->data_stack[vm->dsp] = (cell_t)(uintptr_t)entry;
        vm_push(vm, (entry->flags & WORD_IMMEDIATE) ? 1 : -1);
    } else {
        vm_push(vm, 0);
    }
}

/* Enhanced word lookup for the main interpreter */
DictEntry* vm_vocabulary_find_word(VM *vm, const char *name, size_t len) {
    return vocab_find_word(vm, name, len);
}

/* FORTH-79 Vocabulary Word Registration */
void register_vocabulary_words(VM *vm) {
    log_message(LOG_INFO, "Registering vocabulary system words...");

    register_word(vm, "VOCABULARY", vocabulary_word_vocabulary);
    register_word(vm, "DEFINITIONS", vocabulary_word_definitions);
    register_word(vm, "CONTEXT", vocabulary_word_context);
    register_word(vm, "CURRENT", vocabulary_word_current);
    register_word(vm, "FORTH", vocabulary_word_forth);
    register_word(vm, "ONLY", vocabulary_word_only);
    register_word(vm, "ALSO", vocabulary_word_also);
    register_word(vm, "ORDER", vocabulary_word_order);
    register_word(vm, "PREVIOUS", vocabulary_word_previous);
    register_word(vm, "(FIND)", vocabulary_word_paren_find);

    /* Initialize and publish VM variable addresses */
    init_vocabulary_system(vm);

    log_message(LOG_INFO, "Full FORTH-79 vocabulary system registered");
}
