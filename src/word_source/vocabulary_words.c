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

#include "include/vocabulary_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static size_t vocab_safe_len(const char *text, size_t max_len)
{
    size_t len = 0;
    if (!text) return 0;
    while (len < max_len && text[len] != '\0') {
        len++;
    }
    return len;
}


/* ---- first-character index for vocab chains (lazy rebuild) ---- */
#define SF_FC_BUCKETS 256

static DictEntry **ctx_fc[SF_FC_BUCKETS]; /* arrays of entry pointers */
static size_t ctx_n[SF_FC_BUCKETS];
static DictEntry *ctx_cached_head = NULL;

static DictEntry **forth_fc[SF_FC_BUCKETS];
static size_t forth_n[SF_FC_BUCKETS];
static DictEntry *forth_cached_head = NULL;

static void fc_free(DictEntry ***lists, size_t *counts) {
    for (size_t i = 0; i < SF_FC_BUCKETS; ++i) {
        free(lists[i]);
        lists[i] = NULL;
        counts[i] = 0;
    }
}

static void fc_rebuild(DictEntry *head, DictEntry ***lists, size_t *counts, DictEntry **cached_head) {
    fc_free(lists, counts);

    /* first pass: counts */
    for (DictEntry *e = head; e; e = e->link) {
        unsigned c = (unsigned char) e->name[0];
        counts[c]++;
    }
    /* alloc */
    for (size_t i = 0; i < SF_FC_BUCKETS; ++i) {
        if (counts[i]) {
            lists[i] = (DictEntry **) malloc(counts[i] * sizeof(DictEntry *));
        }
    }
    /* second pass: fill oldest→newest (we’ll search newest-first by iterating backwards) */
    size_t filled[SF_FC_BUCKETS] = {0};
    for (DictEntry *e = head; e; e = e->link) {
        unsigned c = (unsigned char) e->name[0];
        lists[c][filled[c]++] = e;
    }
    *cached_head = head;
}

/* ===== FORTH-79 vocabulary model =====
   - CONTEXT: vocabulary searched first
   - then FORTH (system vocabulary) is searched
   - CURRENT: where new words are entered (DEFINITIONS: CURRENT := CONTEXT)
   - No ALSO/ONLY/PREVIOUS here. Period.
*/

/* Host-side state */
static DictEntry *forth_vocab = NULL; /* FORTH vocabulary head (root of system) */
static DictEntry *context_vocab = NULL; /* CONTEXT vocabulary head */
static DictEntry *current_vocab = NULL; /* CURRENT vocabulary head */

/* VM-visible variables (addresses in VM space) */
static vaddr_t context_var_addr = 0; /* cell containing (DictEntry*) CONTEXT */
static vaddr_t current_var_addr = 0; /* cell containing (DictEntry*) CURRENT */

/* Sync host state -> VM cells */
static inline void vocab_sync_vm_vars(VM *vm) {
    if (!context_var_addr || !current_var_addr) return;
    vm_store_cell(vm, context_var_addr, (cell_t)(uintptr_t)context_vocab);
    vm_store_cell(vm, current_var_addr, (cell_t)(uintptr_t)current_vocab);
}

/**
 * @brief Initialize the FORTH vocabulary system
 * @details Sets up FORTH as root vocabulary and allocates VM cells for CONTEXT/CURRENT
 * @param vm Pointer to VM instance
 */
static void init_vocabulary_system(VM *vm) {
    static int initialized = 0;
    if (initialized) return;

    /* Treat vm->latest as the FORTH vocabulary head */
    forth_vocab = vm->latest;
    context_vocab = forth_vocab;
    current_vocab = forth_vocab;

    /* Allocate VM cells for CONTEXT and CURRENT */
    void *p1 = vm_allot(vm, sizeof(cell_t));
    if (!p1) {
        vm->error = 1;
        log_message(LOG_ERROR, "VOCAB: failed CONTEXT cell");
        return;
    }
    context_var_addr = (vaddr_t)((uint8_t *) p1 - vm->memory);

    void *p2 = vm_allot(vm, sizeof(cell_t));
    if (!p2) {
        vm->error = 1;
        log_message(LOG_ERROR, "VOCAB: failed CURRENT cell");
        return;
    }
    current_var_addr = (vaddr_t)((uint8_t *) p2 - vm->memory);

    vocab_sync_vm_vars(vm);
    initialized = 1;
}

/* Finder: search CONTEXT chain first, then FORTH chain; skip hidden/smudged */
/* src/word_source/vocabulary_words.c */
static DictEntry *vocab_find_word(VM *vm, const char *name, size_t len) {
    init_vocabulary_system(vm);
    if (!name || len == 0) return NULL;

    /* rebuild per-vocab indices if heads changed */
    if (ctx_cached_head != context_vocab) fc_rebuild(context_vocab, ctx_fc, ctx_n, &ctx_cached_head);
    if (forth_cached_head != forth_vocab) fc_rebuild(forth_vocab, forth_fc, forth_n, &forth_cached_head);

    const unsigned char first = (unsigned char) name[0];
    const unsigned char last = (unsigned char) name[len - 1];

    /* search CONTEXT bucket (newest-first) */
    {
        DictEntry **bucket = ctx_fc[first];
        size_t n = ctx_n[first];
        for (size_t i = n; i-- > 0;) {
            DictEntry *e = bucket[i];
            if ((size_t) e->name_len != len) continue;
            const char *en = e->name;
            if ((unsigned char) en[len - 1] != last) continue;
#ifdef WORD_HIDDEN
            if (e->flags & WORD_HIDDEN) continue;
#endif
#ifdef WORD_SMUDGED
            if (e->flags & WORD_SMUDGED) continue;
#endif
            if (memcmp(en, name, len) == 0) return e;
        }
    }

    /* then FORTH bucket (if different) */
    if (context_vocab != forth_vocab) {
        DictEntry **bucket = forth_fc[first];
        size_t n = forth_n[first];
        for (size_t i = n; i-- > 0;) {
            DictEntry *e = bucket[i];
            if ((size_t) e->name_len != len) continue;
            const char *en = e->name;
            if ((unsigned char) en[len - 1] != last) continue;
#ifdef WORD_HIDDEN
            if (e->flags & WORD_HIDDEN) continue;
#endif
#ifdef WORD_SMUDGED
            if (e->flags & WORD_SMUDGED) continue;
#endif
            if (memcmp(en, name, len) == 0) return e;
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
/**
 * @brief VOCABULARY ( -- ) Create a new vocabulary
 * @details Creates a vocabulary that when executed makes itself the CONTEXT
 * @param vm Pointer to VM instance
 */
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
            if (e->name_len == (int) nlen && memcmp(e->name, name, nlen) == 0) {
                vm->error = 1;
                log_message(LOG_ERROR, "VOCABULARY: duplicate '%s'", name);
                return;
            }
            if (e->link == vm->latest) break; /* safety if dictionary ever becomes circular */
        }
    }

    /* Create header that, when executed, selects this vocabulary as CONTEXT */
    DictEntry *entry = vm_create_word(vm, name, (int) strlen(name), vocabulary_select_runtime);
    if (!entry) {
        vm->error = 1;
        return;
    }

    /* Reveal immediately so it’s discoverable by FIND/INTERPRET */
#ifdef WORD_SMUDGED
    entry->flags &= ~WORD_SMUDGED;
#endif
#ifdef WORD_HIDDEN
    entry->flags &= ~WORD_HIDDEN;
#endif

    /* Give the vocabulary a body cell (anchor); surfaces OOM deterministically */
    void *body = vm_allot(vm, sizeof(cell_t));
    if (!body) {
        vm->error = 1;
        log_message(LOG_ERROR, "VOCABULARY: out of dictionary space");
        return;
    }
    vaddr_t body_addr = (vaddr_t)((uint8_t *) body - vm->memory);
    vm_store_cell(vm, body_addr, 0);

    vocab_sync_vm_vars(vm);
    log_message(LOG_DEBUG, "VOCABULARY: created '%s'", name);
}

void vocabulary_create_vocabulary_direct(VM *vm, const char *name)
{
    if (!vm || !name) {
        return;
    }
    char local[64];
    size_t len = vocab_safe_len(name, sizeof local - 1);
    if (len == 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "VOCABULARY: missing name");
        return;
    }
    memcpy(local, name, len);
    local[len] = '\0';

    size_t nlen = len;
    for (DictEntry *e = vm->latest; e; e = e->link) {
        if (e->name_len == (int)nlen && memcmp(e->name, local, nlen) == 0) {
            vm->error = 1;
            log_message(LOG_ERROR, "VOCABULARY: duplicate '%s'", local);
            return;
        }
        if (e->link == vm->latest) break;
    }

    DictEntry *entry = vm_create_word(vm, local, (int)nlen, vocabulary_select_runtime);
    if (!entry) {
        vm->error = 1;
        return;
    }

#ifdef WORD_SMUDGED
    entry->flags &= ~WORD_SMUDGED;
#endif
#ifdef WORD_HIDDEN
    entry->flags &= ~WORD_HIDDEN;
#endif

    void *body = vm_allot(vm, sizeof(cell_t));
    if (!body) {
        vm->error = 1;
        log_message(LOG_ERROR, "VOCABULARY: out of dictionary space");
        return;
    }
    vaddr_t body_addr = (vaddr_t)((uint8_t *) body - vm->memory);
    vm_store_cell(vm, body_addr, 0);

    vocab_sync_vm_vars(vm);
    log_message(LOG_DEBUG, "VOCABULARY: created '%s'", local);
}

/* DEFINITIONS ( -- )  CURRENT := CONTEXT */
/**
 * @brief DEFINITIONS ( -- ) Set CURRENT to CONTEXT
 * @details Makes new words be defined in the CONTEXT vocabulary
 * @param vm Pointer to VM instance
 */
void vocabulary_word_definitions(VM *vm) {
    init_vocabulary_system(vm);
    current_vocab = context_vocab;
    vocab_sync_vm_vars(vm);
    log_message(LOG_DEBUG, "DEFINITIONS: CURRENT := CONTEXT");
}

/* CONTEXT ( -- addr )  Return VM address of CONTEXT cell */
/**
 * @brief CONTEXT ( -- addr ) Get CONTEXT variable address
 * @details Returns VM address of cell containing CONTEXT vocabulary pointer
 * @param vm Pointer to VM instance
 */
void vocabulary_word_context(VM *vm) {
    init_vocabulary_system(vm);
    vm_push(vm, CELL(context_var_addr));
}

/* CURRENT ( -- addr )  Return VM address of CURRENT cell */
/**
 * @brief CURRENT ( -- addr ) Get CURRENT variable address
 * @details Returns VM address of cell containing CURRENT vocabulary pointer
 * @param vm Pointer to VM instance
 */
void vocabulary_word_current(VM *vm) {
    init_vocabulary_system(vm);
    vm_push(vm, CELL(current_var_addr));
}

/* FORTH ( -- )  Make FORTH the CONTEXT vocabulary (and nothing else) */
/**
 * @brief FORTH ( -- ) Select FORTH vocabulary
 * @details Makes FORTH the CONTEXT vocabulary
 * @param vm Pointer to VM instance
 */
void vocabulary_word_forth(VM *vm) {
    init_vocabulary_system(vm);
    context_vocab = forth_vocab;
    vocab_sync_vm_vars(vm);
    log_message(LOG_DEBUG, "FORTH selected (CONTEXT := FORTH)");
}

/* (FIND) ( addr -- addr flag )  primitive finder on counted string at addr */
/**
 * @brief (FIND) ( addr -- addr flag ) Find word in dictionary
 * @details Searches for counted string at addr in CONTEXT then FORTH vocabularies
 * @param vm Pointer to VM instance
 */
void vocabulary_word_paren_find(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t addr = vm->data_stack[vm->dsp]; /* keep addr */
    vaddr_t a = VM_ADDR(addr);
    if (!vm_addr_ok(vm, a, 1)) {
        vm_push(vm, 0);
        return;
    }

    uint8_t *s = vm_ptr(vm, a);
    if (!s) {
        vm_push(vm, 0);
        return;
    }

    uint8_t n = s[0];
    if (!vm_addr_ok(vm, a + 1, n)) {
        vm_push(vm, 0);
        return;
    }
    const char *name = (const char *) &s[1];

    DictEntry *e = vocab_find_word(vm, name, (size_t) n);
    if (e) {
        vm->data_stack[vm->dsp] = (cell_t)(uintptr_t)
        e;
        vm_push(vm, (e->flags & WORD_IMMEDIATE) ? 1 : -1);
    } else {
        vm_push(vm, 0);
    }
}

/* ORDER ( -- )  Display search order (CONTEXT then FORTH) and CURRENT */
/**
 * @brief ORDER ( -- ) Display search order
 * @details Shows CONTEXT and FORTH vocabularies plus CURRENT
 * @param vm Pointer to VM instance
 */
void vocabulary_word_order(VM *vm) {
    init_vocabulary_system(vm);

    printf("Search order: ");
    if (context_vocab && context_vocab->name_len > 0) {
        fwrite(context_vocab->name, 1, (size_t) context_vocab->name_len, stdout);
        if (context_vocab != forth_vocab) printf(" ");
    }
    if (context_vocab != forth_vocab && forth_vocab && forth_vocab->name_len > 0) {
        fwrite(forth_vocab->name, 1, (size_t) forth_vocab->name_len, stdout);
    }
    if (!context_vocab && forth_vocab && forth_vocab->name_len > 0) {
        fwrite(forth_vocab->name, 1, (size_t) forth_vocab->name_len, stdout);
    }
    printf("\n");

    printf("Current: ");
    if (current_vocab && current_vocab->name_len > 0) {
        fwrite(current_vocab->name, 1, (size_t) current_vocab->name_len, stdout);
    } else {
        printf("(none)");
    }
    printf("\n");
}

/* Vocabulary-aware lookup for the interpreter */
/**
 * @brief Find word in vocabulary-aware dictionary
 * @details Searches CONTEXT then FORTH vocabularies
 * @param vm Pointer to VM instance
 * @param name Word name to find
 * @param len Length of word name
 * @return Pointer to dictionary entry if found, NULL if not found
 */
DictEntry *vm_vocabulary_find_word(VM *vm, const char *name, size_t len) {
    return vocab_find_word(vm, name, len);
}

/**
 * @brief Register all FORTH-79 vocabulary words
 * @details Registers VOCABULARY, DEFINITIONS, CONTEXT, CURRENT, FORTH, ORDER, and (FIND)
 * @param vm Pointer to VM instance
 */
void register_vocabulary_words(VM *vm) {
    register_word(vm, "VOCABULARY", vocabulary_word_vocabulary);
    register_word(vm, "DEFINITIONS", vocabulary_word_definitions);
    register_word(vm, "CONTEXT", vocabulary_word_context);
    register_word(vm, "CURRENT", vocabulary_word_current);
    register_word(vm, "FORTH", vocabulary_word_forth);
    register_word(vm, "ORDER", vocabulary_word_order);
    register_word(vm, "(FIND)", vocabulary_word_paren_find);
    init_vocabulary_system(vm);
}
