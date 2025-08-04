/*
 * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
 * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 */

#include "include/defining_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <string.h>
#include <stdlib.h>

/* Runtime function pointers for different word types */
static void constant_runtime(VM *vm);
static void variable_runtime(VM *vm);
static void create_runtime(VM *vm);

/* Global pointer to track currently executing word */
static DictEntry *current_executing_entry = NULL;

/* FIXED: Proper data field calculation using VM memory layout */
static cell_t* get_data_field(VM *vm, DictEntry *entry) {
    if (entry == NULL) {
        log_message(LOG_ERROR, "get_data_field: NULL entry");
        return NULL;
    }

    /* Calculate data field address: after the dictionary entry structure */
    uintptr_t addr = (uintptr_t)entry + sizeof(DictEntry);

    /* Align to cell boundary */
    addr = (addr + sizeof(cell_t) - 1) & ~(sizeof(cell_t) - 1);

    /* Verify address is within VM memory bounds */
    uintptr_t vm_start = (uintptr_t)vm->memory;
    uintptr_t vm_end = vm_start + VM_MEMORY_SIZE;

    /* Fix: Use <= for upper bound check to allow addresses exactly at the boundary */
    if (addr < vm_start || addr > vm_end) {
        log_message(LOG_ERROR, "Data field address %p outside VM memory range [%p, %p]",
                    (void*)addr, (void*)vm_start, (void*)vm_end);
        return NULL;
    }

    log_message(LOG_DEBUG, "get_data_field: entry=%p, data_field=%p", entry, (void*)addr);
    return (cell_t*)addr;
}

/* : ( -- ) Start colon definition */
static void forth_colon(VM *vm) {
    char name_buffer[WORD_NAME_MAX + 1];
    int name_len;

    /* Parse word name using VM's parser */
    name_len = vm_parse_word(vm, name_buffer, sizeof(name_buffer) - 1);
    if (name_len <= 0) {
        log_message(LOG_ERROR, ": Missing word name");
        vm->error = 1;
        return;
    }
    name_buffer[name_len] = '\0';

    /* Use VM's built-in compilation system */
    vm_enter_compile_mode(vm, name_buffer, (size_t)name_len);

    log_message(LOG_DEBUG, ": Started compiling '%s'", name_buffer);
}

/* ; ( -- ) End colon definition - IMMEDIATE */
static void forth_semicolon(VM *vm) {
    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "; Not compiling");
        vm->error = 1;
        return;
    }

    /* Add EXIT instruction and finish compilation */
    vm_compile_exit(vm);
    vm_exit_compile_mode(vm);

    log_message(LOG_DEBUG, "; Finished compiling");
}

/* CREATE ( -- ) Create dictionary entry */
static void forth_create(VM *vm) {
    char name_buffer[WORD_NAME_MAX + 1];
    int name_len;
    DictEntry *entry;
    cell_t *data_field;

    /* Parse word name using VM's parser */
    name_len = vm_parse_word(vm, name_buffer, sizeof(name_buffer) - 1);
    if (name_len <= 0) {
        log_message(LOG_ERROR, "CREATE: Missing word name");
        vm->error = 1;
        return;
    }
    name_buffer[name_len] = '\0';

    /* Create dictionary entry */
    entry = vm_create_word(vm, name_buffer, (size_t)name_len, create_runtime);
    if (entry == NULL) {
        vm->error = 1;
        return;
    }

    /* Allocate data field immediately after the entry */
    vm_align(vm);
    data_field = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (data_field == NULL) {
        vm->error = 1;
        return;
    }

    /* Store address of data field in itself (CREATE default behavior) */
    *data_field = (cell_t)(uintptr_t)data_field;

    log_message(LOG_DEBUG, "CREATE: Created '%s' with data field at %p",
                name_buffer, (void*)data_field);
}

/* CONSTANT ( n -- ) Define constant */
static void forth_constant(VM *vm) {
    cell_t value;
    char name_buffer[WORD_NAME_MAX + 1];
    int name_len;
    DictEntry *entry;
    cell_t *data_field;

    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "CONSTANT: Data stack underflow");
        vm->error = 1;
        return;
    }

    value = vm_pop(vm);

    /* Parse word name using VM's parser */
    name_len = vm_parse_word(vm, name_buffer, sizeof(name_buffer) - 1);
    if (name_len <= 0) {
        log_message(LOG_ERROR, "CONSTANT: Missing constant name");
        vm->error = 1;
        return;
    }
    name_buffer[name_len] = '\0';

    /* Create the constant with the parsed name */
    entry = vm_create_word(vm, name_buffer, (size_t)name_len, constant_runtime);
    if (!entry) {
        log_message(LOG_ERROR, "CONSTANT: Failed to create word '%s'", name_buffer);
        vm->error = 1;
        return;
    }

    /* Store the constant value in the data field */
    data_field = get_data_field(vm, entry);
    if (!data_field) {
        log_message(LOG_ERROR, "CONSTANT: Cannot get data field for '%s'", name_buffer);
        vm->error = 1;
        return;
    }

    *data_field = value;

    log_message(LOG_DEBUG, "CONSTANT: Created '%s' = %ld with data at %p",
                name_buffer, (long)value, (void*)data_field);
}

/* VARIABLE ( -- ) Define variable */
static void forth_variable(VM *vm) {
    char name_buffer[WORD_NAME_MAX + 1];
    int name_len;
    DictEntry *entry;
    cell_t *data_field;

    /* Parse word name using VM's parser */
    name_len = vm_parse_word(vm, name_buffer, sizeof(name_buffer) - 1);
    if (name_len <= 0) {
        log_message(LOG_ERROR, "VARIABLE: Missing word name");
        vm->error = 1;
        return;
    }
    name_buffer[name_len] = '\0';

    /* Create dictionary entry */
    entry = vm_create_word(vm, name_buffer, (size_t)name_len, variable_runtime);
    if (entry == NULL) {
        vm->error = 1;
        return;
    }

    /* Allocate data field immediately after the entry */
    vm_align(vm);
    data_field = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (data_field == NULL) {
        vm->error = 1;
        return;
    }

    *data_field = 0; /* Initialize to zero */

    log_message(LOG_DEBUG, "VARIABLE: Created '%s' with data at %p",
                name_buffer, (void*)data_field);
}

/* 2CONSTANT ( d -- ) Define double constant */
static void forth_2constant(VM *vm) {
    char name_buffer[WORD_NAME_MAX + 1];
    int name_len;
    cell_t dlow, dhigh;
    DictEntry *entry;
    cell_t *data_field;

    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "2CONSTANT: Stack underflow");
        vm->error = 1;
        return;
    }

    /* Note: Forth double numbers are typically stored as (dhigh dlow) */
    dlow = vm_pop(vm);   /* Low part on top */
    dhigh = vm_pop(vm);  /* High part below */

    /* Parse word name using VM's parser */
    name_len = vm_parse_word(vm, name_buffer, sizeof(name_buffer) - 1);
    if (name_len <= 0) {
        log_message(LOG_ERROR, "2CONSTANT: Missing word name");
        vm->error = 1;
        return;
    }
    name_buffer[name_len] = '\0';

    /* Create dictionary entry with special flag */
    entry = vm_create_word(vm, name_buffer, (size_t)name_len, constant_runtime);
    if (entry == NULL) {
        vm->error = 1;
        return;
    }

    entry->flags |= 0x08; /* Mark as double constant */

    /* Allocate data field immediately after the entry */
    vm_align(vm);
    data_field = (cell_t*)vm_allot(vm, 2 * sizeof(cell_t));
    if (data_field == NULL) {
        vm->error = 1;
        return;
    }

    data_field[0] = dhigh;  /* Store high part first */
    data_field[1] = dlow;   /* Store low part second */

    log_message(LOG_DEBUG, "2CONSTANT: Created '%s' with double data at %p",
                name_buffer, (void*)data_field);
}

/* 2VARIABLE ( -- ) Define double variable */
static void forth_2variable(VM *vm) {
    char name_buffer[WORD_NAME_MAX + 1];
    int name_len;
    DictEntry *entry;
    cell_t *data_field;

    /* Parse word name using VM's parser */
    name_len = vm_parse_word(vm, name_buffer, sizeof(name_buffer) - 1);
    if (name_len <= 0) {
        log_message(LOG_ERROR, "2VARIABLE: Missing word name");
        vm->error = 1;
        return;
    }
    name_buffer[name_len] = '\0';

    /* Create dictionary entry with special flag */
    entry = vm_create_word(vm, name_buffer, (size_t)name_len, variable_runtime);
    if (entry == NULL) {
        vm->error = 1;
        return;
    }

    entry->flags |= 0x08; /* Mark as double variable */

    /* Allocate data field immediately after the entry */
    vm_align(vm);
    data_field = (cell_t*)vm_allot(vm, 2 * sizeof(cell_t));
    if (data_field == NULL) {
        vm->error = 1;
        return;
    }

    data_field[0] = 0;  /* Initialize high part */
    data_field[1] = 0;  /* Initialize low part */

    log_message(LOG_DEBUG, "2VARIABLE: Created '%s' with double data at %p",
                name_buffer, (void*)data_field);
}

/* IMMEDIATE ( -- ) Make most recent word immediate */
static void forth_immediate(VM *vm) {
    if (vm->latest == NULL) {
        log_message(LOG_ERROR, "IMMEDIATE: No word to make immediate");
        vm->error = 1;
        return;
    }

    vm->latest->flags |= WORD_IMMEDIATE;

    log_message(LOG_DEBUG, "IMMEDIATE: Made '%.*s' immediate",
                (int)vm->latest->name_len, vm->latest->name);
}

/* FORGET ( -- ) Remove word and all after it */
static void forth_forget(VM *vm) {
    char name_buffer[WORD_NAME_MAX + 1];
    int name_len;
    DictEntry *entry;
    DictEntry *current;
    uintptr_t forget_addr;

    /* Parse word name using VM's parser */
    name_len = vm_parse_word(vm, name_buffer, sizeof(name_buffer) - 1);
    if (name_len <= 0) {
        log_message(LOG_ERROR, "FORGET: Missing word name");
        vm->error = 1;
        return;
    }
    name_buffer[name_len] = '\0';

    /* Find the word */
    entry = vm_find_word(vm, name_buffer, (size_t)name_len);
    if (entry == NULL) {
        log_message(LOG_ERROR, "FORGET: Word '%s' not found", name_buffer);
        vm->error = 1;
        return;
    }

    /* Calculate the address to reset HERE to */
    forget_addr = (uintptr_t)entry;

    /* Find the entry that comes before the word to forget */
    current = vm->latest;
    while (current && current != entry) {
        if (current->link == entry) {
            /* This entry points to the word we want to forget */
            vm->latest = current;
            break;
        }
        current = current->link;
    }

    /* If we're forgetting the latest word */
    if (current == entry) {
        vm->latest = entry->link;
    }

    /* Reset HERE pointer to reclaim memory */
    if (forget_addr >= (uintptr_t)vm->memory &&
        forget_addr < (uintptr_t)vm->memory + vm->here) {
        vm->here = forget_addr - (uintptr_t)vm->memory;
    }

    log_message(LOG_DEBUG, "FORGET: Removed '%s' and all after it", name_buffer);
}

/* Runtime functions for different word types */

/* FIXED: Runtime for constants */
static void constant_runtime(VM *vm) {
    if (current_executing_entry == NULL) {
        log_message(LOG_ERROR, "constant_runtime: No current entry");
        vm->error = 1;
        return;
    }

    cell_t *data_field = get_data_field(vm, current_executing_entry);
    if (data_field == NULL) {
        log_message(LOG_ERROR, "constant_runtime: Cannot get data field");
        vm->error = 1;
        return;
    }

    if (current_executing_entry->flags & 0x08) {
        /* Double constant - push high then low */
        vm_push(vm, data_field[0]); /* dhigh */
        vm_push(vm, data_field[1]); /* dlow */
    } else {
        /* Single constant */
        vm_push(vm, *data_field);
    }
}

/* FIXED: Runtime for variables */
static void variable_runtime(VM *vm) {
    if (current_executing_entry == NULL) {
        log_message(LOG_ERROR, "variable_runtime: No current entry");
        vm->error = 1;
        return;
    }

    cell_t *data_field = get_data_field(vm, current_executing_entry);
    if (data_field == NULL) {
        log_message(LOG_ERROR, "variable_runtime: Cannot get data field");
        vm->error = 1;
        return;
    }

    /* Push address of data field */
    vm_push(vm, (cell_t)(uintptr_t)data_field);
}

/* FIXED: Runtime for CREATE words */
static void create_runtime(VM *vm) {
    if (current_executing_entry == NULL) {
        log_message(LOG_ERROR, "create_runtime: No current entry");
        vm->error = 1;
        return;
    }

    cell_t *data_field = get_data_field(vm, current_executing_entry);
    if (data_field == NULL) {
        log_message(LOG_ERROR, "create_runtime: Cannot get data field");
        vm->error = 1;
        return;
    }

    /* Push the value stored in data field (address of data area) */
    vm_push(vm, *data_field);
}

/* Helper function to execute a defining word with proper context */
void execute_defining_word(VM *vm, DictEntry *entry) {
    DictEntry *saved_entry = current_executing_entry;
    current_executing_entry = entry;

    if (entry->func) {
        entry->func(vm);
    }

    current_executing_entry = saved_entry;
}

void register_defining_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 defining words...");

    /* Core defining words */
    register_word(vm, ":", forth_colon);

    register_word(vm, ";", forth_semicolon);
    vm_make_immediate(vm); /* ; is immediate */

    register_word(vm, "CREATE", forth_create);
    register_word(vm, "CONSTANT", forth_constant);
    register_word(vm, "VARIABLE", forth_variable);
    register_word(vm, "2CONSTANT", forth_2constant);
    register_word(vm, "2VARIABLE", forth_2variable);
    register_word(vm, "IMMEDIATE", forth_immediate);
    register_word(vm, "FORGET", forth_forget);

    log_message(LOG_INFO, "FORTH-79 defining words registered - 9 words");
}