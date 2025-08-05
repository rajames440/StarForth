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

/* defining_words.c - FORTH-79 Defining Words */
#include "include/defining_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <string.h>

/* Runtime function for CONSTANT - pushes the stored value */
static void defining_runtime_constant(VM *vm) {
    DictEntry *entry;
    cell_t *data_field;
    cell_t value;

    /* Find the dictionary entry for this word */
    entry = vm_dictionary_find_latest_by_func(vm, defining_runtime_constant);
    if (entry == NULL) {
        log_message(LOG_ERROR, "CONSTANT runtime: Cannot find dictionary entry");
        vm->error = 1;
        return;
    }

    /* Get the data field containing the constant value */
    data_field = vm_dictionary_get_data_field(entry);
    if (data_field == NULL) {
        log_message(LOG_ERROR, "CONSTANT runtime: Cannot access data field");
        vm->error = 1;
        return;
    }

    value = *data_field;
    vm_push(vm, value);

    log_message(LOG_DEBUG, "CONSTANT runtime: Pushed value %ld from %s",
                (long)value, entry->name);
}

/* Runtime function for VARIABLE - pushes the VM memory offset */
static void defining_runtime_variable(VM *vm) {
    DictEntry *entry;
    cell_t *data_field;
    cell_t vm_offset;

    /* Find the dictionary entry for this word */
    entry = vm_dictionary_find_latest_by_func(vm, defining_runtime_variable);
    if (entry == NULL) {
        log_message(LOG_ERROR, "VARIABLE runtime: Cannot find dictionary entry");
        vm->error = 1;
        return;
    }

    /* Get the data field containing the VM memory offset */
    data_field = vm_dictionary_get_data_field(entry);
    if (data_field == NULL) {
        log_message(LOG_ERROR, "VARIABLE runtime: Cannot access data field");
        vm->error = 1;
        return;
    }

    vm_offset = *data_field;
    vm_push(vm, vm_offset);

    log_message(LOG_DEBUG, "VARIABLE runtime: Pushed VM offset %ld for %s",
                (long)vm_offset, entry->name);
}

/* Runtime function for CREATE - pushes the VM memory offset */
static void defining_runtime_create(VM *vm) {
    DictEntry *entry;
    cell_t *data_field;
    cell_t vm_offset;

    /* Find the dictionary entry for this word */
    entry = vm_dictionary_find_latest_by_func(vm, defining_runtime_create);
    if (entry == NULL) {
        log_message(LOG_ERROR, "CREATE runtime: Cannot find dictionary entry");
        vm->error = 1;
        return;
    }

    /* Get the data field containing the VM memory offset */
    data_field = vm_dictionary_get_data_field(entry);
    if (data_field == NULL) {
        log_message(LOG_ERROR, "CREATE runtime: Cannot access data field");
        vm->error = 1;
        return;
    }

    vm_offset = *data_field;
    vm_push(vm, vm_offset);

    log_message(LOG_DEBUG, "CREATE runtime: Pushed VM offset %ld for %s",
                (long)vm_offset, entry->name);
}

/* : ( -- ) Start colon definition */
void defining_word_colon(VM *vm) {
    char word_name[64];
    int name_len;

    /* Parse the word name */
    name_len = vm_parse_word(vm, word_name, sizeof(word_name));
    if (name_len <= 0) {
        log_message(LOG_ERROR, ": Missing word name");
        vm->error = 1;
        return;
    }

    /* Enter compile mode */
    vm_enter_compile_mode(vm, word_name, (size_t)name_len);

    log_message(LOG_DEBUG, ": Started definition of '%.*s'", name_len, word_name);
}

/* ; ( -- ) End colon definition (immediate) */
void defining_word_semicolon(VM *vm) {
    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "; Not in compile mode");
        vm->error = 1;
        return;
    }

    /* Compile EXIT and exit compile mode */
    vm_compile_exit(vm);
    vm_exit_compile_mode(vm);

    log_message(LOG_DEBUG, "; Completed definition");
}

/* CREATE ( -- ) Create a new dictionary entry */
void defining_word_create(VM *vm) {
    char word_name[64];
    int name_len;
    DictEntry *entry;
    cell_t *data_field;
    cell_t vm_offset;

    /* Parse the word name */
    name_len = vm_parse_word(vm, word_name, sizeof(word_name));
    if (name_len <= 0) {
        log_message(LOG_ERROR, "CREATE: Missing word name");
        vm->error = 1;
        return;
    }

    /* Allocate space in VM memory for the data field */
    vm_offset = vm->here;
    void *allocated = vm_allot(vm, sizeof(cell_t));
    if (allocated == NULL) {
        log_message(LOG_ERROR, "CREATE: Out of memory");
        vm->error = 1;
        return;
    }

    /* Initialize the allocated space to zero */
    *(cell_t*)allocated = 0;

    /* Create dictionary entry */
    entry = vm_create_word(vm, word_name, (size_t)name_len, defining_runtime_create);
    if (entry == NULL) {
        log_message(LOG_ERROR, "CREATE: Failed to create dictionary entry");
        vm->error = 1;
        return;
    }

    /* Store the VM memory offset in the data field */
    data_field = vm_dictionary_get_data_field(entry);
    if (data_field == NULL) {
        log_message(LOG_ERROR, "CREATE: Cannot access data field");
        vm->error = 1;
        return;
    }

    *data_field = vm_offset;

    log_message(LOG_DEBUG, "CREATE: Created '%.*s' with VM offset %ld",
                name_len, word_name, (long)vm_offset);
}

/* CONSTANT ( n -- ) Create a constant */
void defining_word_constant(VM *vm) {
    char word_name[64];
    int name_len;
    cell_t value;
    DictEntry *entry;
    cell_t *data_field;

    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "CONSTANT: Data stack underflow");
        vm->error = 1;
        return;
    }

    /* Get the constant value */
    value = vm_pop(vm);

    /* Parse the word name */
    name_len = vm_parse_word(vm, word_name, sizeof(word_name));
    if (name_len <= 0) {
        log_message(LOG_ERROR, "CONSTANT: Missing word name");
        vm->error = 1;
        return;
    }

    log_message(LOG_DEBUG, "CONSTANT: Creating '%.*s' = %ld",
                name_len, word_name, (long)value);

    /* Create dictionary entry */
    entry = vm_create_word(vm, word_name, (size_t)name_len, defining_runtime_constant);
    if (entry == NULL) {
        log_message(LOG_ERROR, "CONSTANT: Failed to create dictionary entry");
        vm->error = 1;
        return;
    }

    /* Store the constant value in the data field */
    data_field = vm_dictionary_get_data_field(entry);
    if (data_field == NULL) {
        log_message(LOG_ERROR, "CONSTANT: Cannot access data field");
        vm->error = 1;
        return;
    }

    *data_field = value;

    log_message(LOG_DEBUG, "CONSTANT: Created '%.*s' with value %ld at %p",
                name_len, word_name, (long)value, (void*)data_field);
}

/* VARIABLE ( -- ) Create a variable */
void defining_word_variable(VM *vm) {
    char word_name[64];
    int name_len;
    DictEntry *entry;
    cell_t *data_field;
    cell_t vm_offset;

    /* Parse the word name */
    name_len = vm_parse_word(vm, word_name, sizeof(word_name));
    if (name_len <= 0) {
        log_message(LOG_ERROR, "VARIABLE: Missing word name");
        vm->error = 1;
        return;
    }

    log_message(LOG_DEBUG, "VARIABLE: Creating '%.*s'", name_len, word_name);

    /* Allocate space in VM memory for the variable */
    vm_offset = vm->here;
    void *allocated = vm_allot(vm, sizeof(cell_t));
    if (allocated == NULL) {
        log_message(LOG_ERROR, "VARIABLE: Out of memory");
        vm->error = 1;
        return;
    }

    /* Initialize the variable to zero */
    *(cell_t*)allocated = 0;

    /* Create dictionary entry */
    entry = vm_create_word(vm, word_name, (size_t)name_len, defining_runtime_variable);
    if (entry == NULL) {
        log_message(LOG_ERROR, "VARIABLE: Failed to create dictionary entry");
        vm->error = 1;
        return;
    }

    /* Store the VM memory offset in the data field */
    data_field = vm_dictionary_get_data_field(entry);
    if (data_field == NULL) {
        log_message(LOG_ERROR, "VARIABLE: Cannot access data field");
        vm->error = 1;
        return;
    }

    *data_field = vm_offset;

    log_message(LOG_DEBUG, "VARIABLE: Created '%.*s' at VM offset %ld",
                name_len, word_name, (long)vm_offset);
}

/* IMMEDIATE ( -- ) Mark last word as immediate */
void defining_word_immediate(VM *vm) {
    vm_make_immediate(vm);
    log_message(LOG_DEBUG, "IMMEDIATE: Marked last word as immediate");
}

/* [ ( -- ) Enter interpret mode (immediate) */
void defining_word_left_bracket(VM *vm) {
    vm->mode = MODE_INTERPRET;
    log_message(LOG_DEBUG, "[: Entered interpret mode");
}

/* ] ( -- ) Enter compile mode */
void defining_word_right_bracket(VM *vm) {
    vm->mode = MODE_COMPILE;
    log_message(LOG_DEBUG, "]: Entered compile mode");
}

/* STATE ( -- addr ) Variable containing compile/interpret state */
void defining_word_state(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)&vm->mode);
}

/* COMPILE ( -- ) Compile next word (obsolete) */
void defining_word_compile(VM *vm) {
    char word_name[64];
    int name_len;
    DictEntry *entry;

    /* Parse the next word */
    name_len = vm_parse_word(vm, word_name, sizeof(word_name));
    if (name_len <= 0) {
        log_message(LOG_ERROR, "COMPILE: Missing word name");
        vm->error = 1;
        return;
    }

    /* Find the word */
    entry = vm_find_word(vm, word_name, (size_t)name_len);
    if (entry == NULL) {
        log_message(LOG_ERROR, "COMPILE: Word '%.*s' not found", name_len, word_name);
        vm->error = 1;
        return;
    }

    /* Compile a call to the word */
    vm_compile_call(vm, entry->func);

    log_message(LOG_DEBUG, "COMPILE: Compiled call to '%.*s'", name_len, word_name);
}

/* [COMPILE] ( -- ) Compile next word even if immediate */
void defining_word_bracket_compile(VM *vm) {
    defining_word_compile(vm);  /* Same implementation for now */
}

/* LITERAL ( n -- ) Compile literal value */
void defining_word_literal(VM *vm) {
    cell_t value;

    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "LITERAL: Data stack underflow");
        vm->error = 1;
        return;
    }

    value = vm_pop(vm);
    vm_compile_literal(vm, value);

    log_message(LOG_DEBUG, "LITERAL: Compiled literal %ld", (long)value);
}

/* FORGET ( -- ) Remove word and all subsequent definitions */
void defining_word_forget(VM *vm) {
    char word_name[64];
    int name_len;
    DictEntry *entry;

    /* Parse the word name */
    name_len = vm_parse_word(vm, word_name, sizeof(word_name));
    if (name_len <= 0) {
        log_message(LOG_ERROR, "FORGET: Missing word name");
        vm->error = 1;
        return;
    }

    /* Find the word */
    entry = vm_find_word(vm, word_name, (size_t)name_len);
    if (entry == NULL) {
        log_message(LOG_ERROR, "FORGET: Word '%.*s' not found", name_len, word_name);
        vm->error = 1;
        return;
    }

    /* Reset latest to the previous entry */
    vm->latest = entry->link;

    log_message(LOG_DEBUG, "FORGET: Removed '%.*s' and subsequent definitions",
                name_len, word_name);
}

/* DOES> ( -- ) Modify behavior of most recent CREATE */
void defining_word_does(VM *vm) {
    /* This is a complex word that requires runtime code generation */
    /* For now, just log that it's not implemented */
    log_message(LOG_WARN, "DOES>: Not yet implemented");
    vm->error = 1;
}

/* Register all defining words */
void register_defining_words(VM *vm) {
    log_message(LOG_INFO, "Registering defining words...");

    /* Register defining words */
    register_word(vm, ":", defining_word_colon);
    register_word(vm, ";", defining_word_semicolon);
    register_word(vm, "CREATE", defining_word_create);
    register_word(vm, "CONSTANT", defining_word_constant);
    register_word(vm, "VARIABLE", defining_word_variable);
    register_word(vm, "IMMEDIATE", defining_word_immediate);
    register_word(vm, "[", defining_word_left_bracket);
    register_word(vm, "]", defining_word_right_bracket);
    register_word(vm, "STATE", defining_word_state);
    register_word(vm, "COMPILE", defining_word_compile);
    register_word(vm, "[COMPILE]", defining_word_bracket_compile);
    register_word(vm, "LITERAL", defining_word_literal);
    register_word(vm, "FORGET", defining_word_forget);
    register_word(vm, "DOES>", defining_word_does);

    /* Make ; and [ immediate words */
    DictEntry *semicolon = vm_find_word(vm, ";", 1);
    if (semicolon) semicolon->flags |= WORD_IMMEDIATE;

    DictEntry *left_bracket = vm_find_word(vm, "[", 1);
    if (left_bracket) left_bracket->flags |= WORD_IMMEDIATE;

    log_message(LOG_INFO, "Defining words registered successfully");
}