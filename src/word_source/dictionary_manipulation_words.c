/*

                                 ***   StarForth ***
  dictionary_manipulation_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/* dictionary_manipulation_words.c - FORTH-79 Dictionary Manipulation Words */
#include "include/dictionary_manipulation_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Global state for dictionary manipulation */
static cell_t state_variable = 0;        /* STATE - compilation state */

/* Helper function to calculate aligned address */
static uintptr_t align_address(uintptr_t addr) {
    size_t alignment = sizeof(cell_t);
    return (addr + alignment - 1) & ~(alignment - 1);
}

/* Helper function to get name field from execution token */
static char* get_name_field(DictEntry *entry) {
    if (entry == NULL) {
        return NULL;
    }
    return (char*)entry->name;
}

/* Helper function to get body address from execution token */
static void* get_body_address(DictEntry *entry) {
    if (entry == NULL) {
        return NULL;
    }
    
    /* Body follows the header structure */
    uintptr_t body_addr = (uintptr_t)entry + sizeof(DictEntry) + entry->name_len;
    return (void*)align_address(body_addr);
}

/* Helper function to find dictionary entry from name field */
static DictEntry* find_entry_from_name(VM *vm, char *name_field) {
    DictEntry *entry;
    
    entry = vm->latest;
    while (entry != NULL) {
        if ((char*)entry->name == name_field) {
            return entry;
        }
        entry = entry->link;
    }
    return NULL;
}

/* Helper function to traverse name field */
static char* traverse_name_field(char *name_addr, int direction) {
    uint8_t *ptr;
    uint8_t name_len;
    
    if (name_addr == NULL) {
        return NULL;
    }
    
    /* FORTH-79 names start with length byte */
    ptr = (uint8_t*)name_addr;
    
    if (direction > 0) {
        /* Forward traversal - skip over name */
        name_len = *ptr & 0x1F;  /* Mask off flags */
        return (char*)(ptr + name_len + 1);
    } else {
        /* Backward traversal - find start of name */
        /* This is more complex - we need to scan backwards */
        /* For simplicity, assume we're at the start already */
        return name_addr;
    }
}

/* [ ( -- )  Enter interpretation mode */
void dictionary_m_word_left_bracket(VM *vm) {
    vm->mode = MODE_INTERPRET;
    state_variable = 0;
}

/* ] ( -- )  Enter compilation mode */
void dictionary_m_word_right_bracket(VM *vm) {
    vm->mode = MODE_COMPILE;
    state_variable = -1;  /* FORTH-79 uses -1 for true */
}

/* STATE ( -- addr )  Variable: compilation state */
void dictionary_m_word_state(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)&state_variable);
}

/* SMUDGE ( -- )  Toggle smudge bit of latest word */
void dictionary_m_word_smudge(VM *vm) {
    if (vm->latest == NULL) {
        vm->error = 1;
        return;
    }
    
    /* Toggle the smudge bit (bit 5 in FORTH-79) */
    vm->latest->flags ^= WORD_SMUDGED;
}

/* >BODY ( xt -- addr )  Convert execution token to body */
void dictionary_m_word_to_body(VM *vm) {
    cell_t xt;
    DictEntry *entry;
    void *body_addr;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    xt = vm_pop(vm);
    entry = (DictEntry*)(uintptr_t)xt;
    
    body_addr = get_body_address(entry);
    if (body_addr == NULL) {
        vm->error = 1;
        return;
    }
    
    vm_push(vm, (cell_t)(uintptr_t)body_addr);
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
    entry = (DictEntry*)(uintptr_t)xt;
    
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
    name_field = (char*)(uintptr_t)addr;
    
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
    entry = (DictEntry*)(uintptr_t)addr;
    
    if (entry == NULL) {
        vm->error = 1;
        return;
    }
    
    /* Link field is part of the DictEntry structure */
    vm_push(vm, (cell_t)(uintptr_t)&entry->link);
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
    link_field = (DictEntry**)(uintptr_t)addr;
    
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
    entry = (DictEntry*)(uintptr_t)addr;
    
    if (entry == NULL) {
        vm->error = 1;
        return;
    }
    
    /* In our implementation, the execution token is the entry itself */
    vm_push(vm, (cell_t)(uintptr_t)entry);
}

/* LFA ( addr -- addr )  Get link field address */
void dictionary_m_word_lfa(VM *vm) {
    dictionary_m_word_to_link(vm);  /* Same as >LINK */
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
    entry = (DictEntry*)(uintptr_t)addr;
    
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
    entry = (DictEntry*)(uintptr_t)addr;
    
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
    
    name_addr = (char*)(uintptr_t)addr;
    result_addr = traverse_name_field(name_addr, (int)n);
    
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

/* FORTH-79 Dictionary Manipulation Word Registration and Testing */
void register_dictionary_manipulation_words(VM *vm) {
    log_message(LOG_INFO, "Registering dictionary manipulation words...");
    
    /* Register all dictionary manipulation words */
    register_word(vm, "[", dictionary_m_word_left_bracket);
    register_word(vm, "]", dictionary_m_word_right_bracket);
    register_word(vm, "STATE", dictionary_m_word_state);
    register_word(vm, "SMUDGE", dictionary_m_word_smudge);
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

    log_message(LOG_INFO, "Note: These are low-level words for dictionary introspection");
}