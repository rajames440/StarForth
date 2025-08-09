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

/**
 * @file vm.c
 * @brief Implementation of the StarForth virtual machine
 *
 * This file contains the implementation of the core virtual machine functionality
 * for the StarForth Forth interpreter, including memory management, word compilation,
 * and execution of Forth words.
 */

#include "../include/vm.h"
#include "../include/log.h"
#include "../include/io.h"
#include "../include/word_registry.h"
#include <string.h>
#include <stdlib.h>

static void execute_colon_word(VM *vm);

/**
 * @brief Initialize a new Forth virtual machine instance
 *
 * Allocates memory for the VM, initializes stacks, dictionary space and registers
 * the standard Forth-79 word set.
 *
 * @param vm Pointer to VM structure to initialize
 */
void vm_init(VM *vm) {
    /* Clear all memory */
    memset(vm, 0, sizeof(VM));

    /* Allocate unified block storage - this IS our VM memory */
    vm->memory = malloc(VM_MEMORY_SIZE);  /* 1MB = 1024 blocks */
    if (vm->memory == NULL) {
        log_message(LOG_ERROR, "Failed to allocate VM memory");
        vm->error = 1;
        return;
    }

    /* Initialize stack pointers */
    vm->dsp = -1;   /* Data stack empty */
    vm->rsp = -1;   /* Return stack empty */

    /* Initialize dictionary space (first 64 blocks) */
    vm->here = 0;   /* Start at beginning of allocated memory */
    vm_align(vm);   /* Ensure initial alignment */

    /* Point blocks to the same memory - UNIFIED! */
    vm->blocks = vm->memory;  /* Blocks and heap share same space */

    /* Initialize other fields */
    vm->latest = NULL;
    vm->mode = MODE_INTERPRET;
    vm->compiling_word = NULL;
    vm->state_var = 0;  /* Interpret mode */
    vm->error = 0;
    vm->halted = 0;

    /* Initialize input system */
    vm->input_length = 0;
    vm->input_pos = 0;

    /* Initialize current executing entry */
    vm->current_executing_entry = NULL;

    log_message(LOG_DEBUG, "VM initialized - unified memory=%p, blocks=%p, here=%zu (dict_blocks=%d)",
                (void*)vm->memory, (void*)vm->blocks, vm->here, DICTIONARY_BLOCKS);

    /* Register all FORTH-79 standard words at the end of initialization */
    register_forth79_words(vm);
}

/**
 * @brief Clean up VM resources
 *
 * Frees all allocated memory associated with the VM.
 *
 * @param vm Pointer to VM structure to clean up
 */
void vm_cleanup(VM *vm) {
    if (vm->memory != NULL) {
        free(vm->memory);
        vm->memory = NULL;
    }
    vm->here = 0;
}

/**
 * @brief Parse next word from input buffer
 *
 * Extracts the next space-delimited word from the VM's input buffer.
 *
 * @param vm Pointer to VM structure
 * @param word Buffer to store parsed word
 * @param max_len Maximum length of word buffer
 * @return Length of parsed word, or 0 if no word found
 */
int vm_parse_word(VM *vm, char *word, size_t max_len) {
    size_t len = 0;
    char ch;

    if (word == NULL || max_len == 0) {
        return 0;
    }

    /* Skip leading whitespace */
    while (vm->input_pos < vm->input_length) {
        ch = vm->input_buffer[vm->input_pos];
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            break;
        }
        vm->input_pos++;
    }

    /* Check if we've reached end of input */
    if (vm->input_pos >= vm->input_length) {
        return 0;
    }

    /* Read word characters until whitespace or end of input */
    while (vm->input_pos < vm->input_length && len < max_len - 1) {
        ch = vm->input_buffer[vm->input_pos];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            break;
        }
        word[len++] = ch;
        vm->input_pos++;
    }

    /* Null terminate */
    word[len] = '\0';

    return (int)len;
}

/**
 * @brief Parse string as number
 *
 * Attempts to parse a string as a decimal number.
 *
 * @param str String to parse
 * @param value Pointer to store parsed value
 * @return 1 if successful, 0 if parsing failed
 */
int vm_parse_number(const char *str, cell_t *value) {
    char *endptr;
    long val = strtol(str, &endptr, 10);

    if (*endptr == '\0') {
        *value = (cell_t)val;
        return 1;
    }

    return 0;
}

/**
 * @brief Enter compilation mode for a new word
 *
 * Sets up the VM for compiling a new colon definition.
 *
 * @param vm Pointer to VM structure
 * @param name Name of word being defined
 * @param len Length of word name
 */
void vm_enter_compile_mode(VM *vm, const char *name, size_t len) {
    /* Set compile mode */
    vm->mode = MODE_COMPILE;
    vm->state_var = -1;  /* FORTH-79 uses -1 for true/compile mode */

    /* Store word name being compiled */
    if (len > WORD_NAME_MAX) {
        len = WORD_NAME_MAX;
    }
    memcpy(vm->current_word_name, name, len);
    vm->current_word_name[len] = '\0';

    /* Create dictionary entry (initially smudged) */
    vm->compiling_word = vm_create_word(vm, name, len, execute_colon_word);
    if (vm->compiling_word != NULL) {
        vm->compiling_word->flags |= WORD_SMUDGED;  /* Mark as being compiled */
    }

    log_message(LOG_DEBUG, ": Started definition of '%s'", vm->current_word_name);
}

/**
 * @brief Compile a word reference
 *
 * Compiles a reference to an existing word into the current definition.
 *
 * @param vm Pointer to VM structure
 * @param entry Dictionary entry of word to compile
 */
void vm_compile_word(VM *vm, DictEntry *entry) {
    cell_t *addr;

    if (vm->mode != MODE_COMPILE) {
        return;
    }

    vm_align(vm);
    addr = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (addr == NULL) {
        return;
    }

    // 🔥 CORRECT: Store the word’s function pointer
    *addr = (cell_t)(uintptr_t)(entry->func);

    log_message(LOG_DEBUG, "vm_compile_word: Compiled function pointer for '%.*s'",
                (int)entry->name_len, entry->name);
}

/**
 * @brief Compile a literal value
 *
 * Compiles a literal number into the current definition.
 *
 * @param vm Pointer to VM structure
 * @param value Value to compile as literal
 */
void vm_compile_literal(VM *vm, cell_t value) {
    cell_t *addr;
    DictEntry *lit_entry;

    if (vm->mode != MODE_COMPILE) {
        vm_push(vm, value);
        return;
    }

    /* Find the LIT word */
    lit_entry = vm_find_word(vm, "LIT", 3);
    if (lit_entry == NULL) {
        log_message(LOG_ERROR, "LIT word not found");
        vm->error = 1;
        return;
    }

    /* Compile call to LIT */
    vm_compile_word(vm, lit_entry);

    /* Compile the literal value */
    vm_align(vm);
    addr = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (addr == NULL) {
        return;
    }
    *addr = value;

    log_message(LOG_DEBUG, "vm_compile_literal: Compiled literal %ld", (long)value);
}

/**
 * @brief Compile a function call
 *
 * Compiles a reference to a C function into the current definition.
 *
 * @param vm Pointer to VM structure
 * @param func Function pointer to compile
 */
void vm_compile_call(VM *vm, word_func_t func) {
    DictEntry *entry;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "vm_compile_call called outside compile mode");
        vm->error = 1;
        return;
    }

    /* Find the dictionary entry for this function */
    entry = vm_dictionary_find_by_func(vm, func);
    if (entry == NULL) {
        log_message(LOG_ERROR, "vm_compile_call: Could not find dictionary entry for function");
        vm->error = 1;
        return;
    }

    /* Compile reference to the word */
    vm_compile_word(vm, entry);
}

/**
 * @brief Compile EXIT word
 *
 * Compiles an EXIT instruction into the current definition.
 *
 * @param vm Pointer to VM structure
 */
void vm_compile_exit(VM *vm) {
    DictEntry *exit_entry;

    if (vm->mode != MODE_COMPILE) {
        return;
    }

    /* Find the EXIT word */
    exit_entry = vm_find_word(vm, "EXIT", 4);
    if (exit_entry == NULL) {
        log_message(LOG_ERROR, "EXIT word not found");
        vm->error = 1;
        return;
    }

    /* Compile call to EXIT */
    vm_compile_word(vm, exit_entry);
}

/**
 * @brief Exit compilation mode
 *
 * Finalizes the current word definition and returns to interpretation mode.
 *
 * @param vm Pointer to VM structure
 */
void vm_exit_compile_mode(VM *vm) {
    DictEntry *exit_entry;

    if (vm->compiling_word == NULL) {
        vm->error = 1;
        return;
    }

    /* Find the EXIT word */
    exit_entry = vm_find_word(vm, "EXIT", 4);
    if (exit_entry == NULL) {
        log_message(LOG_ERROR, "EXIT word not found");
        vm->error = 1;
        return;
    }

    /* Compile call to EXIT */
    vm_compile_word(vm, exit_entry);

    /* Clear smudge bit */
    vm->compiling_word->flags &= ~WORD_SMUDGED;

    log_message(LOG_DEBUG, "; Completed definition");

    /* Return to interpret mode */
    vm->mode = MODE_INTERPRET;
    vm->state_var = 0;  /* 0 = interpret mode */

    vm->compiling_word = NULL;
}

/**
 * @brief Execute a colon definition
 *
 * Executes a compiled Forth word (colon definition).
 *
 * @param vm Pointer to VM structure
 */
static void execute_colon_word(VM *vm) {
    DictEntry *entry = vm->current_executing_entry;
    cell_t *body, *ip, *old_ip;

    if (entry == NULL) {
        log_message(LOG_ERROR, "execute_colon_word: no current entry");
        vm->error = 1;
        return;
    }

    /* Get body of word (after the dictionary entry) */
    body = vm_dictionary_get_data_field(entry);
    if (body == NULL) {
        log_message(LOG_ERROR, "execute_colon_word: no body");
        vm->error = 1;
        return;
    }

    log_message(LOG_DEBUG, "execute_colon_word: Executing '%.*s' body at %p",
                (int)entry->name_len, entry->name, (void*)body);

    /* Save current IP on return stack */
    if (vm->rsp >= 0) {
        old_ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];
        vm_rpush(vm, (cell_t)(uintptr_t)old_ip);
    }

    /* Set IP to start of word body */
    ip = body;

    /* Execute threaded code */
    while (ip != NULL && !vm->error) {
        DictEntry *word_to_execute = (DictEntry*)(uintptr_t)*ip;

        if (word_to_execute == NULL) {
            break;
        }

        log_message(LOG_DEBUG, "execute_colon_word: Executing '%.*s'",
                    (int)word_to_execute->name_len, word_to_execute->name);

        /* Save IP for next instruction */
        ip++;
        vm_rpush(vm, (cell_t)(uintptr_t)ip);

        /* Set current executing entry and execute */
        vm->current_executing_entry = word_to_execute;
        if (word_to_execute->func != NULL) {
            word_to_execute->func(vm);
        }

        /* Restore IP */
        ip = (cell_t*)(uintptr_t)vm_rpop(vm);
    }

    /* Restore previous IP */
    if (vm->rsp >= 0) {
        old_ip = (cell_t*)(uintptr_t)vm_rpop(vm);
    }
}

/**
 * @brief Interpret or compile a word
 *
 * Processes a single word either by executing it immediately or compiling it
 * into the current definition.
 *
 * @param vm Pointer to VM structure
 * @param word_str Word to interpret
 * @param len Length of word string
 */
void vm_interpret_word(VM *vm, const char *word_str, size_t len) {
    log_message(LOG_DEBUG, "INTERPRET: '%.*s' (mode=%s)", (int)len, word_str,
                vm->mode == MODE_COMPILE ? "COMPILE" : "INTERPRET");

    DictEntry *entry = vm_find_word(vm, word_str, len);
    if (entry) {
        if (vm->mode == MODE_COMPILE && !(entry->flags & WORD_IMMEDIATE)) {
            log_message(LOG_DEBUG, "COMPILE: '%.*s'", (int)len, word_str);
            vm_compile_word(vm, entry);
        } else {
            log_message(LOG_DEBUG, "EXECUTE: '%.*s'", (int)len, word_str);
            vm->current_executing_entry = entry;
            if (entry->func) {
                entry->func(vm);
            } else {
                log_message(LOG_ERROR, "NULL func for '%.*s'", (int)len, word_str);
                vm->error = 1;
            }
            vm->current_executing_entry = NULL;
        }
        return;
    }

    /* Not found — try as number */
    cell_t value;
    log_message(LOG_DEBUG, "NOT FOUND: '%.*s' — try number", (int)len, word_str);
    if (vm_parse_number(word_str, &value)) {
        log_message(LOG_DEBUG, "NUMBER: '%.*s' = %ld", (int)len, word_str, (long)value);
        if (vm->mode == MODE_COMPILE) {
            vm_compile_literal(vm, value);
        } else {
            vm_push(vm, value);
        }
    } else {
        log_message(LOG_ERROR, "UNKNOWN WORD: '%.*s'", (int)len, word_str);
        vm->error = 1;
    }
}

/**
 * @brief Interpret a string of Forth code
 *
 * Processes a string containing Forth words, executing or compiling them
 * as appropriate.
 *
 * @param vm Pointer to VM structure
 * @param input String containing Forth code to interpret
 */
void vm_interpret(VM *vm, const char *input) {
    size_t len = strlen(input);
    if (len >= INPUT_BUFFER_SIZE) len = INPUT_BUFFER_SIZE - 1;

    memcpy(vm->input_buffer, input, len);
    vm->input_buffer[len] = '\0';
    vm->input_length = len;
    vm->input_pos = 0;

    char word[64];
    size_t word_len;

    while ((word_len = vm_parse_word(vm, word, sizeof(word))) > 0 && !vm->error) {
        vm_interpret_word(vm, word, word_len);
    }
}