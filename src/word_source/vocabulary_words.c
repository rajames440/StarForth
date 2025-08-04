/*
 * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
 *
 * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 * To the extent possible under law, the author(s) have dedicated all copyright and related
 * and neighboring rights to this software to the public domain worldwide.
 * This software is distributed without any warranty.
 *
 * See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
 */

/* vocabulary_words.c - FORTH-79 Vocabulary System Words */
#include "include/vocabulary_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <string.h>

/* Simple vocabulary system state */
static DictEntry *current_vocab = NULL;      /* CURRENT - where new words go */
static DictEntry *context_vocab = NULL;      /* CONTEXT - search vocabulary */

/* VOCABULARY ( -- )  Create new vocabulary */
void word_vocabulary(VM *vm) {
    /* For now, just log that vocabulary was called */
    log_message(LOG_DEBUG, "VOCABULARY called - simplified implementation");
    /* TODO: Full vocabulary creation in future iteration */
}

/* DEFINITIONS ( -- )  Set current vocabulary */
void word_definitions(VM *vm) {
    current_vocab = vm->latest;
    log_message(LOG_DEBUG, "DEFINITIONS called");
}

/* CONTEXT ( -- addr )  Variable: search vocabulary pointer */
void word_context(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)&context_vocab);
}

/* CURRENT ( -- addr )  Variable: definition vocabulary pointer */
void word_current(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)&current_vocab);
}

/* FORTH ( -- )  Select FORTH vocabulary */
void word_forth(VM *vm) {
    context_vocab = vm->latest;
    log_message(LOG_DEBUG, "FORTH vocabulary selected");
}

/* ONLY ( -- )  Set minimal search order */
void word_only(VM *vm) {
    context_vocab = vm->latest;
    log_message(LOG_DEBUG, "ONLY - minimal search order set");
}

/* ALSO ( -- )  Duplicate top of search order */
void word_also(VM *vm) {
    log_message(LOG_DEBUG, "ALSO - search order duplicated");
    /* Simplified - no action needed for basic implementation */
}

/* ORDER ( -- )  Display search order */
void word_order(VM *vm) {
    printf("Search order: FORTH\n");
    printf("Current: FORTH\n");
}

/* PREVIOUS ( -- )  Remove top of search order */
void word_previous(VM *vm) {
    log_message(LOG_DEBUG, "PREVIOUS - removed top of search order");
    /* Simplified - no action needed for basic implementation */
}

/* (FIND) ( addr -- addr flag )  Find word in dictionary (primitive) */
void word_paren_find(VM *vm) {
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
    
    /* Search dictionary */
    entry = vm_find_word(vm, name, (size_t)name_len);
    
    if (entry != NULL) {
        /* Found - replace addr with execution token and push 1 */
        vm->data_stack[vm->dsp] = (cell_t)(uintptr_t)entry;
        vm_push(vm, 1);
    } else {
        /* Not found - keep addr and push 0 */
        vm_push(vm, 0);
    }
}

/* FORTH-79 Vocabulary Word Registration */
void register_vocabulary_words(VM *vm) {
    log_message(LOG_INFO, "Registering vocabulary system words...");
    
    /* Register all vocabulary system words */
    register_word(vm, "VOCABULARY", word_vocabulary);
    register_word(vm, "DEFINITIONS", word_definitions);
    register_word(vm, "CONTEXT", word_context);
    register_word(vm, "CURRENT", word_current);
    register_word(vm, "FORTH", word_forth);
    register_word(vm, "ONLY", word_only);
    register_word(vm, "ALSO", word_also);
    register_word(vm, "ORDER", word_order);
    register_word(vm, "PREVIOUS", word_previous);
    register_word(vm, "(FIND)", word_paren_find);
    
    log_message(LOG_INFO, "Basic vocabulary system registered");
}