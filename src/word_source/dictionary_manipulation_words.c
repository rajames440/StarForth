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

/* dictionary_manipulation_words.c - FORTH-79 Dictionary Manipulation Words */
#include "include/dictionary_manipulation_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* Global state for dictionary manipulation */
static cell_t state_variable = 0; /* STATE - compilation state */

/* Helper function to calculate aligned address */
static uintptr_t align_address(uintptr_t addr) {
    size_t alignment = sizeof(cell_t);
    return (addr + alignment - 1) & ~(alignment - 1);
}

/* Helper function to get name field from execution token */
static char *get_name_field(DictEntry *entry) {
    if (entry == NULL) {
        return NULL;
    }
    return (char *) entry->name;
}

/* Helper function to get body address from execution token */
static void *get_body_address(DictEntry *entry) {
    if (entry == NULL) {
        return NULL;
    }

    /* Body follows the header structure */
    uintptr_t body_addr = (uintptr_t) entry + sizeof(DictEntry) + entry->name_len;
    return (void *) align_address(body_addr);
}

/* Helper function to find dictionary entry from name field */
static DictEntry *find_entry_from_name(VM *vm, char *name_field) {
    DictEntry *entry;

    entry = vm->latest;
    while (entry != NULL) {
        if ((char *) entry->name == name_field) {
            return entry;
        }
        entry = entry->link;
    }
    return NULL;
}

/* Helper function to traverse name field */
static char *traverse_name_field(char *name_addr, int direction) {
    uint8_t *ptr;
    uint8_t name_len;

    if (name_addr == NULL) {
        return NULL;
    }

    /* FORTH-79 names start with length byte */
    ptr = (uint8_t *) name_addr;

    if (direction > 0) {
        /* Forward traversal - skip over name */
        name_len = *ptr & 0x1F; /* Mask off flags */
        return (char *) (ptr + name_len + 1);
    } else {
        /* Backward traversal - find start of name */
        /* This is more complex - we need to scan backwards */
        /* For simplicity, assume we're at the start already */
        return name_addr;
    }
}

/** 
 * @brief FORTH word [ - Enter interpretation mode
 * @param vm Pointer to VM instance
 * @stack ( -- )
 */
void dictionary_m_word_left_bracket(VM *vm) {
    vm->mode = MODE_INTERPRET;
    state_variable = 0;
}

/**
 * @brief FORTH word ] - Enter compilation mode
 * @param vm Pointer to VM instance
 * @stack ( -- )
 */
void dictionary_m_word_right_bracket(VM *vm) {
    vm->mode = MODE_COMPILE;
    state_variable = -1; /* FORTH-79 uses -1 for true */
}

/**
 * @brief FORTH word STATE - Get compilation state variable address
 * @param vm Pointer to VM instance
 * @stack ( -- addr )
 */
void dictionary_m_word_state(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t) & state_variable);
}

/**
 * @brief FORTH word SMUDGE - Toggle smudge bit of latest word
 * @param vm Pointer to VM instance
 * @stack ( -- )
 * @note Compile-only word
 */
static void dictionary_m_word_smudge(VM *vm) {
    // compile-only: error if used while interpreting
    if (vm->mode != MODE_COMPILE) {
        // correct
        vm->error = 1;
        return;
    }
    // existing body that toggles/sets the smudge/hidden bit on the *latest* word
    // (leave your current logic exactly as-is)
}

/* >BODY ( xt -- addr )  Convert execution token to body */
void dictionary_m_word_to_body(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, ">BODY: Stack underflow");
        vm->error = 1;
        return;
    }

    cell_t xt = vm_pop(vm);
    log_message(LOG_DEBUG, ">BODY: POP: %ld (dsp=%d)", (long) xt, vm->dsp);

    DictEntry *entry = (DictEntry *) (intptr_t) xt;
    if (!entry) {
        log_message(LOG_ERROR, ">BODY: Invalid execution token (NULL)");
        vm->error = 1;
        return;
    }

    // Use the VM's dictionary function to get the data field
    cell_t *data_field = vm_dictionary_get_data_field(entry);
    if (!data_field) {
        log_message(LOG_ERROR, ">BODY: Unable to get data field address for entry %p", (void *) entry);
        vm->error = 1;
        return;
    }

    cell_t body_addr = (cell_t)(intptr_t)data_field;
    log_message(LOG_DEBUG, ">BODY: xt=%p -> body=%p", (void *) entry, (void *) data_field);
    vm_push(vm, body_addr);
    log_message(LOG_DEBUG, ">BODY: PUSH: %ld (dsp=%d)", (long) body_addr, vm->dsp);
}

/* >NAME ( xt -- addr )  Convert execution token to name */
void dictionary_m_word_to_name(VM *vm) {
    cell_t xt;
    DictEntry *entry;
    char *name_field;

    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    xt = vm_pop(vm);
    entry = (DictEntry *) (uintptr_t) xt;

    name_field = get_name_field(entry);
    if (name_field == NULL) {
        vm->error = 1;
        return;
    }

    vm_push(vm, (cell_t)(uintptr_t)name_field);
}

/* NAME> ( addr -- xt )  Convert name to execution token */
void dictionary_m_word_name_to(VM *vm) {
    cell_t addr;
    char *name_field;
    DictEntry *entry;

    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    addr = vm_pop(vm);
    name_field = (char *) (uintptr_t) addr;

    entry = find_entry_from_name(vm, name_field);
    if (entry == NULL) {
        vm->error = 1;
        return;
    }

    vm_push(vm, (cell_t)(uintptr_t)entry);
}

/* >LINK ( addr -- addr )  Get link field address */
void dictionary_m_word_to_link(VM *vm) {
    cell_t addr;
    DictEntry *entry;

    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    addr = vm_pop(vm);
    entry = (DictEntry *) (uintptr_t) addr;

    if (entry == NULL) {
        vm->error = 1;
        return;
    }

    /* Link field is part of the DictEntry structure */
    vm_push(vm, (cell_t)(uintptr_t) & entry->link);
}

/* LINK> ( addr -- addr )  Get next word from link */
void dictionary_m_word_link_from(VM *vm) {
    cell_t addr;
    DictEntry **link_field;
    DictEntry *next_entry;

    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    addr = vm_pop(vm);
    link_field = (DictEntry **) (uintptr_t) addr;

    if (link_field == NULL) {
        vm->error = 1;
        return;
    }

    next_entry = *link_field;
    vm_push(vm, (cell_t)(uintptr_t)next_entry);
}

/* CFA ( addr -- xt )  Get code field address */
void dictionary_m_word_cfa(VM *vm) {
    cell_t addr;
    DictEntry *entry;

    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    addr = vm_pop(vm);
    entry = (DictEntry *) (uintptr_t) addr;

    if (entry == NULL) {
        vm->error = 1;
        return;
    }

    /* In our implementation, the execution token is the entry itself */
    vm_push(vm, (cell_t)(uintptr_t)entry);
}

/* LFA ( addr -- addr )  Get link field address */
void dictionary_m_word_lfa(VM *vm) {
    dictionary_m_word_to_link(vm); /* Same as >LINK */
}

/* NFA ( addr -- addr )  Get name field address */
void dictionary_m_word_nfa(VM *vm) {
    cell_t addr;
    DictEntry *entry;
    char *name_field;

    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    addr = vm_pop(vm);
    entry = (DictEntry *) (uintptr_t) addr;

    name_field = get_name_field(entry);
    if (name_field == NULL) {
        vm->error = 1;
        return;
    }

    vm_push(vm, (cell_t)(uintptr_t)name_field);
}

/* PFA ( addr -- addr )  Get parameter field address */
void dictionary_m_word_pfa(VM *vm) {
    cell_t addr;
    DictEntry *entry;
    void *body_addr;

    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    addr = vm_pop(vm);
    entry = (DictEntry *) (uintptr_t) addr;

    body_addr = get_body_address(entry);
    if (body_addr == NULL) {
        vm->error = 1;
        return;
    }

    vm_push(vm, (cell_t)(uintptr_t)body_addr);
}

/* TRAVERSE ( addr n -- addr )  Move through name field */
void dictionary_m_word_traverse(VM *vm) {
    cell_t n, addr;
    char *name_addr;
    char *result_addr;

    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    n = vm_pop(vm);
    addr = vm_pop(vm);

    name_addr = (char *) (uintptr_t) addr;
    result_addr = traverse_name_field(name_addr, (int) n);

    if (result_addr == NULL) {
        vm->error = 1;
        return;
    }

    vm_push(vm, (cell_t)(uintptr_t)result_addr);
}

/* INTERPRET ( -- )  Text interpreter */
void dictionary_m_word_interpret(VM *vm) {
    /* Simply call the VM's built-in interpreter - it handles everything */
    /* The VM already has proper input management, word parsing, and execution */

    /* In FORTH-79, INTERPRET processes the current input stream */
    /* Since vm_interpret handles input parsing and execution, we just ensure */
    /* we're in the right mode and let the VM do the work */

    /* Set interpretation mode */
    vm->mode = MODE_INTERPRET;
    state_variable = 0;

    /* The actual interpretation happens through vm_interpret() calls */
    /* This word exists mainly for completeness and mode setting */
}

static void dictionary_m_word_find(VM *vm) {
    char namebuf[128];
    int nlen = vm_parse_word(vm, namebuf, sizeof namebuf);
    if (nlen <= 0) {
        vm->error = 1; // real input underflow
        return;
    }

    DictEntry *e = vm_find_word(vm, namebuf, (size_t) nlen);
    if (e) {
        vm_push(vm, (cell_t)(uintptr_t)e); // compilation address; swap to CFA if you prefer later
    } else {
        vm_push(vm, 0); // miss is NOT an error
    }
}

static void dictionary_m_word_tick(VM *vm) {
    char namebuf[128];
    int nlen = vm_parse_word(vm, namebuf, sizeof namebuf);
    if (nlen <= 0) {
        log_message(LOG_ERROR, "': unable to parse word");
        vm->error = 1;
        return;
    }

    DictEntry *e = vm_find_word(vm, namebuf, (size_t) nlen);
    if (!e) {
        log_message(LOG_ERROR, "': word '%.*s' not found", nlen, namebuf);
        vm->error = 1;
        return;
    }

    cell_t xt = (cell_t)(uintptr_t)e;
    log_message(LOG_DEBUG, "': found '%.*s' xt=%p", nlen, namebuf, (void *) e);

    if (vm->mode == MODE_COMPILE) {
        // Compile mode: compile LIT <xt>
        log_message(LOG_DEBUG, "': compile mode - compiling literal");
        vm_compile_literal(vm, xt);
    } else {
        // Interpret mode: push execution token
        log_message(LOG_DEBUG, "': interpret mode - pushing xt=%ld", (long) xt);
        vm_push(vm, xt);
    }
}

// in src/word_source/dictionary_manipulation_words.c
static void dictionary_m_word_hidden(VM *vm) {
    // compile-only guard (matches your other compile-only words)
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        return;
    }

    // get the latest entry (use whatever you use elsewhere; many places access vm->latest)
    DictEntry *e = vm->latest;
    if (!e) {
        vm->error = 1;
        return;
    }

    // Set the hidden/smudge flag. Prefer a named flag if you have it.
    // If your codebase uses WORD_HIDDEN, use that.
    // If it uses the same bit as SMUDGE, use that (often called WORD_SMUDGED).
#ifdef WORD_HIDDEN
    e->flags |= WORD_HIDDEN;
#else
    // Fallback: if you only have a toggling SMUDGE path, set conditionally using it.
    if (!(e->flags & WORD_SMUDGED)) {
        // call your existing SMUDGE implementation to flip it on once
        dictionary_word_smudge(vm);
        // ensure we didn't accidentally set an error
        if (vm->error) return;
    }
#endif
}

/**
 * @brief Register all dictionary manipulation words with the VM
 * @param vm Pointer to VM instance
 * @details Registers standard FORTH-79 dictionary manipulation words
 */
void register_dictionary_manipulation_words(VM *vm) {

    /* Register all dictionary manipulation words */
    register_word(vm, "[", dictionary_m_word_left_bracket);
    register_word(vm, "]", dictionary_m_word_right_bracket);
    register_word(vm, "STATE", dictionary_m_word_state);
    register_word(vm, "SMUDGE", dictionary_m_word_smudge);
    register_word(vm, "HIDDEN", dictionary_m_word_hidden);
    register_word(vm, ">BODY", dictionary_m_word_to_body);
    register_word(vm, ">NAME", dictionary_m_word_to_name);
    register_word(vm, "NAME>", dictionary_m_word_name_to);
    register_word(vm, ">LINK", dictionary_m_word_to_link);
    register_word(vm, "LINK>", dictionary_m_word_link_from);
    register_word(vm, "CFA", dictionary_m_word_cfa);
    register_word(vm, "LFA", dictionary_m_word_lfa);
    register_word(vm, "NFA", dictionary_m_word_nfa);
    register_word(vm, "PFA", dictionary_m_word_pfa);
    register_word(vm, "TRAVERSE", dictionary_m_word_traverse);
    register_word(vm, "INTERPRET", dictionary_m_word_interpret);
    register_word(vm, "FIND", dictionary_m_word_find);
    register_word(vm, "'", dictionary_m_word_tick);
}