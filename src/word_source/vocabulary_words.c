/*

                                 ***   StarForth   ***
  vocabulary_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/12/25, 8:27 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/*
                                 ***   StarForth   ***
  vocabulary_words.c - FORTH-79 Standard and ANSI C99 ONLY
  Last modified - 2025-08-12
  CC0-1.0
*/

#include "include/vocabulary_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"
#include <string.h>
#include <stdio.h>

/* ===== FORTH-79 vocabulary model =====
   - CONTEXT: vocabulary searched first
   - then FORTH (system vocabulary) is searched
   - CURRENT: where new words are entered (DEFINITIONS: CURRENT := CONTEXT)
   - No ALSO/ONLY/PREVIOUS here. Period.
*/

/* Host-side state */
static DictEntry *forth_vocab    = NULL;   /* FORTH vocabulary head (root of system) */
static DictEntry *context_vocab  = NULL;   /* CONTEXT vocabulary head */
static DictEntry *current_vocab  = NULL;   /* CURRENT vocabulary head */

/* VM-visible variables (addresses in VM space) */
static vaddr_t context_var_addr = 0;       /* cell containing (DictEntry*) CONTEXT */
static vaddr_t current_var_addr = 0;       /* cell containing (DictEntry*) CURRENT */

/* Sync host state -> VM cells */
static inline void vocab_sync_vm_vars(VM *vm) {
    if (!context_var_addr || !current_var_addr) return;
    vm_store_cell(vm, context_var_addr, (cell_t)(uintptr_t)context_vocab);
    vm_store_cell(vm, current_var_addr, (cell_t)(uintptr_t)current_vocab);
}

/* One-time init */
static void init_vocabulary_system(VM *vm) {
    static int initialized = 0;
    if (initialized) return;

    /* Treat vm->latest as the FORTH vocabulary head */
    forth_vocab   = vm->latest;
    context_vocab = forth_vocab;
    current_vocab = forth_vocab;

    /* Allocate VM cells for CONTEXT and CURRENT */
    void *p1 = vm_allot(vm, sizeof(cell_t));
    if (!p1) { vm->error = 1; log_message(LOG_ERROR, "VOCAB: failed CONTEXT cell"); return; }
    context_var_addr = (vaddr_t)((uint8_t*)p1 - vm->memory);

    void *p2 = vm_allot(vm, sizeof(cell_t));
    if (!p2) { vm->error = 1; log_message(LOG_ERROR, "VOCAB: failed CURRENT cell"); return; }
    current_var_addr = (vaddr_t)((uint8_t*)p2 - vm->memory);

    vocab_sync_vm_vars(vm);
    initialized = 1;
}

/* Finder: search CONTEXT chain first, then FORTH chain; skip hidden/smudged */
static DictEntry* vocab_find_word(VM *vm, const char *name, size_t len) {
    init_vocabulary_system(vm);

    /* search CONTEXT chain */
    for (DictEntry *e = context_vocab; e; e = e->link) {
#ifdef WORD_HIDDEN
        if (e->flags & WORD_HIDDEN) continue;
#endif
#ifdef WORD_SMUDGED
        if (e->flags & WORD_SMUDGED) continue;
#endif
        if (e->name_len == (int)len && memcmp(e->name, name, len) == 0) {
            return e;
        }
    }

    /* then FORTH chain (if different) */
    if (context_vocab != forth_vocab) {
        for (DictEntry *e = forth_vocab; e; e = e->link) {
#ifdef WORD_HIDDEN
            if (e->flags & WORD_HIDDEN) continue;
#endif
#ifdef WORD_SMUDGED
            if (e->flags & WORD_SMUDGED) continue;
#endif
            if (e->name_len == (int)len && memcmp(e->name, name, len) == 0) {
                return e;
            }
        }
    }

    return NULL;
}

/* Executing a vocabulary word makes it the CONTEXT (FORTH-79) */
static void vocabulary_select_runtime(VM *vm) {
    init_vocabulary_system(vm);

    /* We don’t have a “currently executing entry” field; use newest header. */
    context_vocab = vm->latest;
    vocab_sync_vm_vars(vm);

    log_message(LOG_DEBUG, "Vocabulary selected (CONTEXT updated)");
}

/* ===== Words ===== */

/* VOCABULARY ( -- )  Create a new vocabulary; executing it selects itself as CONTEXT. */
void vocabulary_word_vocabulary(VM *vm) {
    char name[64];

    if (!vm_parse_word(vm, name, sizeof name)) {
        vm->error = 1;
        log_message(LOG_ERROR, "VOCABULARY: missing name");
        return;
    }

    /* Reject duplicate by scanning the ENTIRE dictionary from vm->latest. */
    {
        size_t nlen = strlen(name);
        for (DictEntry *e = vm->latest; e; e = e->link) {
            if (e->name_len == (int)nlen && memcmp(e->name, name, nlen) == 0) {
                vm->error = 1;
                log_message(LOG_ERROR, "VOCABULARY: duplicate '%s'", name);
                return;
            }
            if (e->link == vm->latest) break; /* safety if dictionary ever becomes circular */
        }
    }

    /* Create header that, when executed, selects this vocabulary as CONTEXT */
    DictEntry *entry = vm_create_word(vm, name, (int)strlen(name), vocabulary_select_runtime);
    if (!entry) { vm->error = 1; return; }

    /* Reveal immediately so it’s discoverable by FIND/INTERPRET */
#ifdef WORD_SMUDGED
    entry->flags &= ~WORD_SMUDGED;
#endif
#ifdef WORD_HIDDEN
    entry->flags &= ~WORD_HIDDEN;
#endif

    /* Give the vocabulary a body cell (anchor); surfaces OOM deterministically */
    void *body = vm_allot(vm, sizeof(cell_t));
    if (!body) { vm->error = 1; log_message(LOG_ERROR, "VOCABULARY: out of dictionary space"); return; }
    vaddr_t body_addr = (vaddr_t)((uint8_t*)body - vm->memory);
    vm_store_cell(vm, body_addr, 0);

    vocab_sync_vm_vars(vm);
    log_message(LOG_DEBUG, "VOCABULARY: created '%s'", name);
}

/* DEFINITIONS ( -- )  CURRENT := CONTEXT */
void vocabulary_word_definitions(VM *vm) {
    init_vocabulary_system(vm);
    current_vocab = context_vocab;
    vocab_sync_vm_vars(vm);
    log_message(LOG_DEBUG, "DEFINITIONS: CURRENT := CONTEXT");
}

/* CONTEXT ( -- addr )  Return VM address of CONTEXT cell */
void vocabulary_word_context(VM *vm) {
    init_vocabulary_system(vm);
    vm_push(vm, CELL(context_var_addr));
}

/* CURRENT ( -- addr )  Return VM address of CURRENT cell */
void vocabulary_word_current(VM *vm) {
    init_vocabulary_system(vm);
    vm_push(vm, CELL(current_var_addr));
}

/* FORTH ( -- )  Make FORTH the CONTEXT vocabulary (and nothing else) */
void vocabulary_word_forth(VM *vm) {
    init_vocabulary_system(vm);
    context_vocab = forth_vocab;
    vocab_sync_vm_vars(vm);
    log_message(LOG_DEBUG, "FORTH selected (CONTEXT := FORTH)");
}

/* (FIND) ( addr -- addr flag )  primitive finder on counted string at addr */
void vocabulary_word_paren_find(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }

    cell_t addr = vm->data_stack[vm->dsp]; /* keep addr */
    vaddr_t a = VM_ADDR(addr);
    if (!vm_addr_ok(vm, a, 1)) { vm_push(vm, 0); return; }

    uint8_t *s = vm_ptr(vm, a);
    if (!s) { vm_push(vm, 0); return; }

    uint8_t n = s[0];
    if (!vm_addr_ok(vm, a + 1, n)) { vm_push(vm, 0); return; }
    const char *name = (const char *)&s[1];

    DictEntry *e = vocab_find_word(vm, name, (size_t)n);
    if (e) {
        vm->data_stack[vm->dsp] = (cell_t)(uintptr_t)e;
        vm_push(vm, (e->flags & WORD_IMMEDIATE) ? 1 : -1);
    } else {
        vm_push(vm, 0);
    }
}

/* ORDER ( -- )  Display search order (CONTEXT then FORTH) and CURRENT */
void vocabulary_word_order(VM *vm) {
    init_vocabulary_system(vm);

    printf("Search order: ");
    if (context_vocab && context_vocab->name_len > 0) {
        fwrite(context_vocab->name, 1, (size_t)context_vocab->name_len, stdout);
        if (context_vocab != forth_vocab) printf(" ");
    }
    if (context_vocab != forth_vocab && forth_vocab && forth_vocab->name_len > 0) {
        fwrite(forth_vocab->name, 1, (size_t)forth_vocab->name_len, stdout);
    }
    if (!context_vocab && forth_vocab && forth_vocab->name_len > 0) {
        fwrite(forth_vocab->name, 1, (size_t)forth_vocab->name_len, stdout);
    }
    printf("\n");

    printf("Current: ");
    if (current_vocab && current_vocab->name_len > 0) {
        fwrite(current_vocab->name, 1, (size_t)current_vocab->name_len, stdout);
    } else {
        printf("(none)");
    }
    printf("\n");
}

/* Vocabulary-aware lookup for the interpreter */
DictEntry* vm_vocabulary_find_word(VM *vm, const char *name, size_t len) {
    return vocab_find_word(vm, name, len);
}

/* Registration: ONLY standard words */
void register_vocabulary_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 vocabulary words...");

    register_word(vm, "VOCABULARY", vocabulary_word_vocabulary);
    register_word(vm, "DEFINITIONS", vocabulary_word_definitions);
    register_word(vm, "CONTEXT",     vocabulary_word_context);
    register_word(vm, "CURRENT",     vocabulary_word_current);
    register_word(vm, "FORTH",       vocabulary_word_forth);
    register_word(vm, "ORDER",       vocabulary_word_order);
    register_word(vm, "(FIND)",      vocabulary_word_paren_find);

    init_vocabulary_system(vm);
    log_message(LOG_INFO, "FORTH-79 vocabulary registered");
}
