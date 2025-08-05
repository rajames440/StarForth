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

/* vm.c - FORTH-79 compliant VM implementation with direct threading */
#include "../include/vm.h"
#include "../include/log.h"
#include "../include/io.h"
#include "../include/word_registry.h"
#include <string.h>
#include <stdlib.h>

/* Forward declarations */
static void execute_colon_word(VM *vm);

/* Core VM functions */
void vm_init(VM *vm) {
    /* Clear all memory */
    memset(vm, 0, sizeof(VM));

    /* Allocate dynamic memory */
    vm->memory = malloc(VM_MEMORY_SIZE);
    if (vm->memory == NULL) {
        log_message(LOG_ERROR, "Failed to allocate VM memory");
        vm->error = 1;
        return;
    }

    /* Initialize stack pointers */
    vm->dsp = -1;   /* Data stack empty */
    vm->rsp = -1;   /* Return stack empty */

    /* Initialize other fields */
    vm->here = 0;   /* Start at beginning of allocated memory */
    vm->latest = NULL;
    vm->mode = MODE_INTERPRET;
    vm->compiling_word = NULL;
    vm->state_var = 0;  /* Interpret mode */
    vm->error = 0;
    vm->halted = 0;

    /* Initialize input system */
    vm->input_length = 0;
    vm->input_pos = 0;

    /* Initialize blocks pointer */
    vm->blocks = NULL;

    /* Initialize current executing entry */
    vm->current_executing_entry = NULL;

    log_message(LOG_DEBUG, "VM initialized - memory=%p", (void*)vm->memory);

    /* Register all FORTH-79 standard words at the end of initialization */
    register_forth79_words(vm);
}

/* Cleanup function */
void vm_cleanup(VM *vm) {
    if (vm->memory != NULL) {
        free(vm->memory);
        vm->memory = NULL;
    }
    vm->here = 0;
}

/* Stack operations */
void vm_push(VM *vm, cell_t value) {
    if (vm->dsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "Stack overflow");
        vm->error = 1;
        return;
    }
    vm->data_stack[++vm->dsp] = value;
    log_message(LOG_DEBUG, "PUSH: %d (dsp=%d)", (int)value, vm->dsp);
}

cell_t vm_pop(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "Stack underflow (dsp=%d)", vm->dsp);
        vm->error = 1;
        return 0;
    }
    cell_t value = vm->data_stack[vm->dsp--];
    log_message(LOG_DEBUG, "POP: %d (dsp=%d)", (int)value, vm->dsp);
    return value;
}

/* Return stack operations */
void vm_rpush(VM *vm, cell_t value) {
    if (vm->rsp >= STACK_SIZE - 1) {
        vm->error = 1;  /* Return stack overflow */
        return;
    }
    vm->return_stack[++vm->rsp] = value;
}

cell_t vm_rpop(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;  /* Return stack underflow */
        return 0;
    }
    cell_t value = vm->return_stack[vm->rsp--];
    return value;
}

/* Dictionary operations */
DictEntry* vm_find_word(VM *vm, const char *name, size_t len) {
    DictEntry *entry = vm->latest;

    while (entry) {
        if (!(entry->flags & WORD_HIDDEN) &&
            !(entry->flags & WORD_SMUDGED) &&
            entry->name_len == len &&
            memcmp(entry->name, name, len) == 0) {
            return entry;
        }
        entry = entry->link;
    }

    return NULL;
}

/* Create word in dictionary */
DictEntry* vm_create_word(VM *vm, const char *name, size_t len, word_func_t func) {
    size_t total_size;
    DictEntry *entry;

    if (vm == NULL || name == NULL) {
        log_message(LOG_ERROR, "vm_create_word: NULL parameters");
        if (vm) vm->error = 1;
        return NULL;
    }

    if (len > WORD_NAME_MAX) {
        len = WORD_NAME_MAX;
    }

    /* Calculate total size: header + name */
    total_size = sizeof(DictEntry) + len;

    log_message(LOG_DEBUG, "vm_create_word: Creating '%.*s' (len=%zu, total_size=%zu)",
                (int)len, name, len, total_size);

    /* Align to cell boundary */
    vm_align(vm);

    /* Allocate space */
    entry = (DictEntry*)vm_allot(vm, total_size);
    if (entry == NULL) {
        log_message(LOG_ERROR, "vm_create_word: Failed to allocate dictionary entry");
        vm->error = 1;
        return NULL;
    }

    log_message(LOG_DEBUG, "vm_create_word: Allocated entry at %p", (void*)entry);

    /* Initialize entry */
    entry->link = vm->latest;
    entry->func = func;           /* Store function pointer */
    entry->flags = 0;
    entry->name_len = (uint8_t)len;

    /* Copy name safely */
    if (len > 0) {
        log_message(LOG_DEBUG, "vm_create_word: Copying name '%.*s'", (int)len, name);
        memcpy((void*)entry->name, name, len);
        log_message(LOG_DEBUG, "vm_create_word: Name copied successfully");
    }

    /* Update latest */
    vm->latest = entry;

    log_message(LOG_DEBUG, "vm_create_word: Successfully created '%.*s' at %p",
                (int)len, name, (void*)entry);

    return entry;
}

void vm_make_immediate(VM *vm) {
    if (vm->latest) {
        vm->latest->flags |= WORD_IMMEDIATE;
    }
}

void vm_hide_word(VM *vm) {
    if (vm->latest) {
        vm->latest->flags |= WORD_HIDDEN;
    }
}

/* FORTH-79 SMUDGE word support */
void vm_smudge_word(VM *vm) {
    if (vm->latest == NULL) {
        vm->error = 1;
        return;
    }

    /* Toggle the smudge bit */
    vm->latest->flags ^= WORD_SMUDGED;
}

/* Memory management */
void* vm_allot(VM *vm, size_t bytes) {
    if (vm->here + bytes >= VM_MEMORY_SIZE) {
        log_message(LOG_ERROR, "Out of memory (here=%zu, bytes=%zu, limit=%d)",
                    vm->here, bytes, VM_MEMORY_SIZE);
        vm->error = 1;
        return NULL;
    }

    void *ptr = vm->memory + vm->here;
    vm->here += bytes;

    log_message(LOG_DEBUG, "ALLOT: Allocated %zu bytes at offset %zu",
                bytes, vm->here - bytes);

    return ptr;
}

void vm_align(VM *vm) {
    vm->here = (vm->here + sizeof(cell_t) - 1) & ~(sizeof(cell_t) - 1);
}

/* Input parsing */
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

int vm_parse_number(const char *str, cell_t *value) {
    char *endptr;
    long val = strtol(str, &endptr, 10);

    if (*endptr == '\0') {
        *value = (cell_t)val;
        return 1;
    }

    return 0;
}

/* Compilation functions - FORTH-79 style */
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

/* Compile a word reference into the dictionary */
void vm_compile_word(VM *vm, DictEntry *entry) {
    cell_t *addr;

    if (vm->mode != MODE_COMPILE) {
        return;
    }

    /* Allocate space for a cell to hold the word's execution token */
    vm_align(vm);
    addr = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (addr == NULL) {
        return;
    }

    /* Store pointer to the word entry */
    *addr = (cell_t)(uintptr_t)entry;

    log_message(LOG_DEBUG, "vm_compile_word: Compiled reference to '%.*s'",
                (int)entry->name_len, entry->name);
}

/* Compile a literal value into the dictionary */
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

/* Compile a function call - for backward compatibility */
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

/* Compile EXIT word - for backward compatibility */
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

/* Execute a colon definition (threaded code) */
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

/* Main word interpreter */
void vm_interpret_word(VM *vm, const char *word_str, size_t len) {
    log_message(LOG_DEBUG, "vm_interpret_word: Processing '%.*s' (mode=%s)",
                (int)len, word_str,
                vm->mode == MODE_COMPILE ? "COMPILE" : "INTERPRET");

    /* Try to find word in dictionary */
    DictEntry *entry = vm_find_word(vm, word_str, len);

    if (entry) {
        log_message(LOG_DEBUG, "vm_interpret_word: Found word '%.*s' at %p",
                    (int)len, word_str, (void*)entry);

        if (vm->mode == MODE_COMPILE && !(entry->flags & WORD_IMMEDIATE)) {
            /* Compile the word */
            log_message(LOG_DEBUG, "vm_interpret_word: Compiling call to '%.*s'",
                        (int)len, word_str);
            vm_compile_word(vm, entry);
        } else {
            /* Execute immediately */
            log_message(LOG_DEBUG, "vm_interpret_word: Executing '%.*s'",
                        (int)len, word_str);
            vm->current_executing_entry = entry;
            if (entry->func != NULL) {
                entry->func(vm);
            } else {
                log_message(LOG_ERROR, "NULL function pointer for word: %.*s", (int)len, word_str);
                vm->error = 1;
            }
            vm->current_executing_entry = NULL;
        }
    } else {
        /* Try to parse as number */
        cell_t value;
        log_message(LOG_DEBUG, "vm_interpret_word: Word '%.*s' not found, trying as number",
                    (int)len, word_str);

        if (vm_parse_number(word_str, &value)) {
            log_message(LOG_DEBUG, "vm_interpret_word: Parsed '%.*s' as number %ld",
                        (int)len, word_str, (long)value);
            if (vm->mode == MODE_COMPILE) {
                vm_compile_literal(vm, value);
            } else {
                vm_push(vm, value);
            }
        } else {
            log_message(LOG_ERROR, "Unknown word: %.*s", (int)len, word_str);
            vm->error = 1;
        }
    }

    log_message(LOG_DEBUG, "vm_interpret_word: Finished processing '%.*s'", (int)len, word_str);
}

/* Main interpreter */
void vm_interpret(VM *vm, const char *input) {
    /* Copy input to buffer */
    size_t len = strlen(input);
    if (len >= INPUT_BUFFER_SIZE) {
        len = INPUT_BUFFER_SIZE - 1;
    }

    memcpy(vm->input_buffer, input, len);
    vm->input_buffer[len] = '\0';
    vm->input_length = len;
    vm->input_pos = 0;

    /* Parse and interpret words */
    char word[64];
    while (vm_parse_word(vm, word, sizeof(word)) && !vm->error) {
        vm_interpret_word(vm, word, strlen(word));
    }
}

/* REPL */
void vm_repl(VM *vm) {
    log_message(LOG_INFO, "Starting Forth REPL");

    char input[256];
    while (!vm->halted && !vm->error) {
        printf("ok> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        vm_interpret(vm, input);

        if (!vm->error) {
            printf(" ok\n");
        } else {
            printf(" ERROR\n");
            vm->error = 0; /* Reset error for next input */
        }
    }
}

/* Dictionary search functions */
DictEntry* vm_dictionary_find_by_func(VM *vm, word_func_t func) {
    DictEntry *entry = vm->latest;

    while (entry != NULL) {
        if (entry->func == func) {
            return entry;
        }
        entry = entry->link;
    }
    return NULL;
}

DictEntry* vm_dictionary_find_latest_by_func(VM *vm, word_func_t func) {
    /* Since dictionary is ordered newest-first, first match is latest */
    return vm_dictionary_find_by_func(vm, func);
}

/* Get data field address from dictionary entry */
cell_t* vm_dictionary_get_data_field(DictEntry *entry) {
    if (entry == NULL) {
        return NULL;
    }

    /* Data field comes right after the entry structure and name */
    uintptr_t entry_end = (uintptr_t)entry + sizeof(DictEntry) + entry->name_len;

    /* Align to cell boundary */
    entry_end = (entry_end + sizeof(cell_t) - 1) & ~(sizeof(cell_t) - 1);

    return (cell_t*)entry_end;
}