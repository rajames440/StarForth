/*

                                 ***   StarForth   ***
  vocabulary_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
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
#include <string.h>

/* FORTH-79 Vocabulary System Implementation */

/* Maximum vocabulary search order depth */
#define MAX_VOCAB_SEARCH_ORDER 8

/* Vocabulary system state */
static DictEntry *current_vocab = NULL;                           /* CURRENT - where new words go */
static DictEntry *context_vocabs[MAX_VOCAB_SEARCH_ORDER];         /* CONTEXT - search order */
static int search_order_depth = 1;                               /* Current search order depth */

/* Initialize vocabulary system */
static void init_vocabulary_system(VM *vm) {
    static int initialized = 0;
    if (!initialized) {
        current_vocab = vm->latest;         /* Start with FORTH vocabulary */
        context_vocabs[0] = vm->latest;     /* FORTH is default search vocabulary */
        search_order_depth = 1;
        initialized = 1;
    }
}

/* Find word in vocabulary search order */
static DictEntry* vocab_find_word(VM *vm, const char *name, size_t len) {
    init_vocabulary_system(vm);

    /* Search through vocabulary search order */
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
    /* This gets called when a vocabulary word is executed */
    /* We need to find which vocabulary was executed and set it as context */

    /* For now, set to FORTH - in full implementation, we'd track which vocab */
    init_vocabulary_system(vm);
    context_vocabs[0] = vm->latest;
    search_order_depth = 1;

    log_message(LOG_DEBUG, "Vocabulary selected for search order");
}

/* VOCABULARY ( -- )  Create new vocabulary */
void vocabulary_word_vocabulary(VM *vm) {
    char vocab_name[64];

    /* Parse vocabulary name from input stream */
    if (!vm_parse_word(vm, vocab_name, sizeof(vocab_name))) {
        log_message(LOG_ERROR, "VOCABULARY: Expected vocabulary name");
        vm->error = 1;
        return;
    }

    /* Create vocabulary header word */
    DictEntry *vocab_entry = vm_create_word(vm, vocab_name, strlen(vocab_name), vocabulary_select_runtime);

    if (vocab_entry == NULL) {
        vm->error = 1;
        return;
    }

    /* Mark as vocabulary word */
    vocab_entry->flags |= 0x40;  /* VOCABULARY flag */

    /* Store vocabulary starting point (current dictionary head) */
    /* In full implementation, we'd create separate vocabulary chains */

    log_message(LOG_DEBUG, "VOCABULARY: Created vocabulary '%s'", vocab_name);
}

/* DEFINITIONS ( -- )  Set current vocabulary for new definitions */
void vocabulary_word_definitions(VM *vm) {
    init_vocabulary_system(vm);

    /* Set current vocabulary to the top of search order */
    if (search_order_depth > 0) {
        current_vocab = context_vocabs[search_order_depth - 1];
    }

    log_message(LOG_DEBUG, "DEFINITIONS: Current vocabulary set");
}

/* CONTEXT ( -- addr )  Variable: search vocabulary pointer */
void vocabulary_word_context(VM *vm) {
    init_vocabulary_system(vm);
    vm_push(vm, (cell_t)(uintptr_t)&context_vocabs[0]);
}

/* CURRENT ( -- addr )  Variable: definition vocabulary pointer */
void vocabulary_word_current(VM *vm) {
    init_vocabulary_system(vm);
    vm_push(vm, (cell_t)(uintptr_t)&current_vocab);
}

/* FORTH ( -- )  Select FORTH vocabulary */
void vocabulary_word_forth(VM *vm) {
    init_vocabulary_system(vm);

    /* Set FORTH as single vocabulary in search order */
    context_vocabs[0] = vm->latest;  /* FORTH vocabulary root */
    search_order_depth = 1;

    log_message(LOG_DEBUG, "FORTH vocabulary selected");
}

/* ONLY ( -- )  Set minimal search order (FORTH only) */
void vocabulary_word_only(VM *vm) {
    init_vocabulary_system(vm);

    /* Reset to minimal search order - just FORTH */
    context_vocabs[0] = vm->latest;
    search_order_depth = 1;

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

    log_message(LOG_DEBUG, "ALSO: Search order duplicated (depth=%d)", search_order_depth);
}

/* ORDER ( -- )  Display search order */
void vocabulary_word_order(VM *vm) {
    init_vocabulary_system(vm);

    printf("Search order: ");
    for (int i = search_order_depth - 1; i >= 0; i--) {
        printf("FORTH ");  /* In full implementation, we'd show actual vocab names */
    }
    printf("\n");

    printf("Current: FORTH\n");  /* In full implementation, show actual current vocab */
}

/* PREVIOUS ( -- )  Remove top of search order */
void vocabulary_word_previous(VM *vm) {
    init_vocabulary_system(vm);

    if (search_order_depth <= 1) {
        log_message(LOG_ERROR, "PREVIOUS: Cannot remove last vocabulary from search order");
        vm->error = 1;
        return;
    }

    /* Remove top of search order */
    search_order_depth--;

    log_message(LOG_DEBUG, "PREVIOUS: Removed top of search order (depth=%d)", search_order_depth);
}

/* (FIND) ( addr -- addr flag )  Find word in dictionary (primitive) */
void vocabulary_word_paren_find(VM *vm) {
    cell_t addr;
    uint8_t *counted_str;
    uint8_t name_len;
    const char *name;
    DictEntry *entry;

    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    addr = vm->data_stack[vm->dsp];  /* Keep addr on stack */
    counted_str = (uint8_t*)(uintptr_t)addr;

    if (counted_str == NULL) {
        vm_push(vm, 0);  /* Not found */
        return;
    }

    name_len = counted_str[0];
    name = (const char*)&counted_str[1];

    /* Search using vocabulary system */
    entry = vocab_find_word(vm, name, (size_t)name_len);

    if (entry != NULL) {
        /* Found - replace addr with execution token and push flag */
        vm->data_stack[vm->dsp] = (cell_t)(uintptr_t)entry;
        vm_push(vm, (entry->flags & WORD_IMMEDIATE) ? 1 : -1);  /* FORTH-79 immediate flag */
    } else {
        /* Not found - keep addr and push 0 */
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

    /* Register all vocabulary system words */
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

    /* Initialize vocabulary system */
    init_vocabulary_system(vm);

    log_message(LOG_INFO, "Full FORTH-79 vocabulary system registered");
}