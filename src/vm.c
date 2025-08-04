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

/* vm.c - Core VM implementation with compilation support */
#include "../include/vm.h"
#include "../include/log.h"
#include "../include/io.h"
#include "../include/word_registry.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* Bytecode opcodes for compiled words */
typedef enum {
    OP_LITERAL = 1,     /* Push literal value */
    OP_CALL = 2,        /* Call word function */
    OP_EXIT = 3,        /* Exit from word */

    /* Control flow opcodes - placeholders for control_words.c */
    OP_BRANCH = 4,      /* Unconditional branch */
    OP_0BRANCH = 5,     /* Branch if top of stack is zero */
    OP_IF = 6,          /* IF - compile-time marker (NOOP at runtime) */
    OP_THEN = 7,        /* THEN - compile-time marker (NOOP at runtime) */
    OP_ELSE = 8,        /* ELSE - compile-time marker (NOOP at runtime) */

    /* Loop opcodes */
    OP_DO = 9,          /* DO - initialize loop */
    OP_LOOP = 10,       /* LOOP - increment and test loop */
    OP_PLUS_LOOP = 11,  /* +LOOP - add increment and test loop */
    OP_LEAVE = 12,      /* LEAVE - exit loop early */
    OP_I = 13,          /* I - current loop index */
    OP_J = 14,          /* J - outer loop index */

    /* Additional control structures */
    OP_BEGIN = 15,      /* BEGIN - loop start marker */
    OP_WHILE = 16,      /* WHILE - conditional in BEGIN/WHILE/REPEAT */
    OP_REPEAT = 17,     /* REPEAT - end of BEGIN/WHILE/REPEAT loop */
    OP_UNTIL = 18,      /* UNTIL - end of BEGIN/UNTIL loop */
    OP_AGAIN = 19,      /* AGAIN - end of BEGIN/AGAIN infinite loop */

    /* Future extension opcodes */
    OP_CASE = 20,       /* CASE - start of CASE/OF/ENDOF/ENDCASE structure */
    OP_OF = 21,         /* OF - case branch */
    OP_ENDOF = 22,      /* ENDOF - end of case branch */
    OP_ENDCASE = 23     /* ENDCASE - end of case structure */
} opcode_type_t;

/* Forward declarations */
static void execute_compiled_word(VM *vm, DictEntry *entry);

/* Actual compiled word runtime that executes bytecode */
void vm_compiled_word_runtime(VM *vm) {
    /* This function should not be called directly - it's a marker.
     * The actual execution happens in execute_compiled_word() */
    log_message(LOG_ERROR, "vm_compiled_word_runtime called directly - using execute_compiled_word instead");

    /* Try to find the current entry being executed and call execute_compiled_word */
    if (vm->compiling_word != NULL) {
        execute_compiled_word(vm, vm->compiling_word);
    } else {
        vm->error = 1;
    }
}

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

    /* Initialize memory contents to zero */
    memset(vm->memory, 0, VM_MEMORY_SIZE);

    /* Initialize stack pointers */
    vm->dsp = -1;   /* Data stack empty */
    vm->rsp = -1;   /* Return stack empty */

    /* Initialize other fields */
    vm->here = 0;   /* Start at beginning of allocated memory */
    vm->latest = NULL;
    vm->mode = MODE_INTERPRET;
    vm->compiling_word = NULL;
    vm->compile_pos = 0;
    vm->compile_size = COMPILE_BUFFER_SIZE;
    vm->state_var = 0;  /* Interpret mode */
    vm->error = 0;
    vm->halted = 0;

    /* Initialize input system */
    vm->input_length = 0;
    vm->input_pos = 0;

    /* Initialize blocks pointer */
    vm->blocks = NULL;

    /* Initialize compilation buffer */
    memset(vm->compile_buffer, 0, sizeof(vm->compile_buffer));
    memset(vm->current_word_name, 0, sizeof(vm->current_word_name));

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
    log_message(LOG_DEBUG, "PUSH: %ld (dsp=%d)", (long)value, vm->dsp);
}

cell_t vm_pop(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "Stack underflow (dsp=%d)", vm->dsp);
        vm->error = 1;
        return 0;
    }
    cell_t value = vm->data_stack[vm->dsp--];
    log_message(LOG_DEBUG, "POP: %ld (dsp=%d)", (long)value, vm->dsp);
    return value;
}

/* Return stack operations */
void vm_rpush(VM *vm, cell_t value) {
    if (vm->rsp >= STACK_SIZE - 1) {
        vm->error = 1;  /* Return stack overflow */
        return;
    }

    vm->rsp++;
    vm->return_stack[vm->rsp] = value;
}

cell_t vm_rpop(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;  /* Return stack underflow */
        return 0;
    }

    cell_t value = vm->return_stack[vm->rsp];
    vm->rsp--;
    return value;
}

/* Dictionary operations */
DictEntry* vm_find_word(VM *vm, const char *name, size_t len) {
    DictEntry *entry = vm->latest;

    while (entry) {
        if (!(entry->flags & WORD_HIDDEN) &&
            !(entry->flags & WORD_SMUDGED) &&  /* Don't find smudged words */
            entry->name_len == len &&
            memcmp(entry->name, name, len) == 0) {
            return entry;
        }
        entry = entry->link;
    }

    return NULL;
}

/* Enhanced vm_create_word to properly handle func field */
DictEntry* vm_create_word(VM *vm, const char *name, size_t len, word_func_t func) {
    size_t total_size;
    DictEntry *entry;

    if (len > WORD_NAME_MAX) {
        len = WORD_NAME_MAX;
    }

    /* Calculate total size: just the DictEntry structure now (name is fixed-size) */
    total_size = sizeof(DictEntry);

    /* Align to cell boundary */
    vm_align(vm);

    /* Check if we have enough memory */
    if (vm->here + total_size >= VM_MEMORY_SIZE) {
        log_message(LOG_ERROR, "vm_create_word: Out of memory");
        return NULL;
    }

    /* Allocate space */
    entry = (DictEntry*)vm_allot(vm, total_size);
    if (entry == NULL) {
        vm->error = 1;
        return NULL;
    }

    /* Initialize entry */
    entry->link = vm->latest;
    entry->func = func;           /* Store function pointer */
    entry->flags = 0;
    entry->name_len = (uint8_t)len;

    /* Copy name to fixed-size buffer and null-terminate */
    if (len > 0) {
        memcpy(entry->name, name, len);
    }
    entry->name[len] = '\0';  /* Null-terminate the name */

    /* Update latest */
    vm->latest = entry;

    log_message(LOG_DEBUG, "vm_create_word: Created '%.*s' at %p with func %p",
                (int)len, name, entry, func);

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
        log_message(LOG_ERROR, "Out of memory");
        return NULL;
    }

    void *ptr = vm->memory + vm->here;
    vm->here += bytes;
    return ptr;
}

void vm_align(VM *vm) {
    vm->here = (vm->here + sizeof(cell_t) - 1) & ~(sizeof(cell_t) - 1);
}

/* Enhanced input parsing with better bounds checking */
int vm_parse_word(VM *vm, char *word, size_t max_len) {
    if (word == NULL || max_len == 0) {
        return 0;
    }

    /* Skip whitespace */
    while (vm->input_pos < vm->input_length &&
           (vm->input_buffer[vm->input_pos] == ' ' ||
            vm->input_buffer[vm->input_pos] == '\t' ||
            vm->input_buffer[vm->input_pos] == '\n' ||
            vm->input_buffer[vm->input_pos] == '\r')) {
        vm->input_pos++;
    }

    if (vm->input_pos >= vm->input_length) {
        word[0] = '\0';
        return 0;
    }

    /* Parse word with strict bounds checking */
    size_t len = 0;
    while (vm->input_pos < vm->input_length &&
           len < max_len - 1 &&  /* Leave space for null terminator */
           vm->input_buffer[vm->input_pos] != ' ' &&
           vm->input_buffer[vm->input_pos] != '\t' &&
           vm->input_buffer[vm->input_pos] != '\n' &&
           vm->input_buffer[vm->input_pos] != '\r' &&
           vm->input_buffer[vm->input_pos] != '\0') {
        word[len++] = vm->input_buffer[vm->input_pos++];
    }

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

/* Get function pointer for compiled word runtime */
word_func_t vm_get_compiled_word_runtime(void) {
    return vm_compiled_word_runtime;
}

/* Enhanced compilation functions with better error handling */
void vm_enter_compile_mode(VM *vm, const char *name, size_t len) {
    if (vm == NULL || name == NULL || len == 0) {
        log_message(LOG_ERROR, "vm_enter_compile_mode: Invalid parameters");
        if (vm) vm->error = 1;
        return;
    }

    /* Check if already in compile mode */
    if (vm->mode == MODE_COMPILE) {
        log_message(LOG_ERROR, "Already in compile mode");
        vm->error = 1;
        return;
    }

    /* Set compile mode */
    vm->mode = MODE_COMPILE;
    vm->state_var = -1;  /* FORTH-79 uses -1 for true/compile mode */

    /* Store word name being compiled with bounds checking */
    size_t safe_len = (len > WORD_NAME_MAX) ? WORD_NAME_MAX : len;
    memcpy(vm->current_word_name, name, safe_len);
    vm->current_word_name[safe_len] = '\0';

    log_message(LOG_DEBUG, ": Started compiling '%s'", vm->current_word_name);

    /* Initialize compilation buffer BEFORE creating dictionary entry */
    vm->compile_pos = 0;
    memset(vm->compile_buffer, 0, sizeof(vm->compile_buffer));

    /* Create dictionary entry with compiled word runtime */
    vm->compiling_word = vm_create_word(vm, name, safe_len, vm_get_compiled_word_runtime());
    if (vm->compiling_word == NULL) {
        log_message(LOG_ERROR, "Failed to create dictionary entry for '%s'", vm->current_word_name);
        vm->mode = MODE_INTERPRET;
        vm->state_var = 0;
        vm->error = 1;
        return;
    }

    /* Mark as being compiled (smudged) and as compiled word */
    vm->compiling_word->flags |= WORD_SMUDGED | WORD_COMPILED;

    log_message(LOG_DEBUG, "Dictionary entry created for '%s' at %p", vm->current_word_name, vm->compiling_word);
}

void vm_compile_literal(VM *vm, cell_t value) {
    if (vm->mode != MODE_COMPILE) {
        vm_push(vm, value);
        return;
    }

    if (vm->compile_pos + 2 >= vm->compile_size) {
        log_message(LOG_ERROR, "Compile buffer overflow");
        vm->error = 1;
        return;
    }

    vm->compile_buffer[vm->compile_pos++] = OP_LITERAL;
    vm->compile_buffer[vm->compile_pos++] = value;

    log_message(LOG_DEBUG, "Compiled literal: %ld", (long)value);
}

void vm_compile_call(VM *vm, word_func_t func) {
    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "vm_compile_call called outside compile mode");
        vm->error = 1;
        return;
    }

    if (vm->compile_pos + 2 >= vm->compile_size) {
        log_message(LOG_ERROR, "Compile buffer overflow");
        vm->error = 1;
        return;
    }

    vm->compile_buffer[vm->compile_pos++] = OP_CALL;
    vm->compile_buffer[vm->compile_pos++] = (cell_t)(uintptr_t)func;

    log_message(LOG_DEBUG, "Compiled call to function %p", func);
}

void vm_compile_exit(VM *vm) {
    if (vm->mode != MODE_COMPILE) {
        return;
    }

    if (vm->compile_pos + 1 >= vm->compile_size) {
        log_message(LOG_ERROR, "Compile buffer overflow");
        vm->error = 1;
        return;
    }

    vm->compile_buffer[vm->compile_pos++] = OP_EXIT;
    log_message(LOG_DEBUG, "Compiled EXIT");
}

/* Store the compiled bytecode in the dictionary entry's data field */
static void store_compiled_bytecode(VM *vm, DictEntry *entry) {
    if (vm->compile_pos == 0) {
        log_message(LOG_DEBUG, "No bytecode to store");
        return;
    }

    /* Align to cell boundary for bytecode storage */
    vm_align(vm);

    /* Allocate space for bytecode after the dictionary entry */
    cell_t *bytecode_area = (cell_t*)vm_allot(vm, vm->compile_pos * sizeof(cell_t));
    if (bytecode_area == NULL) {
        log_message(LOG_ERROR, "Failed to allocate bytecode storage");
        vm->error = 1;
        return;
    }

    /* Copy compiled bytecode */
    memcpy(bytecode_area, vm->compile_buffer, vm->compile_pos * sizeof(cell_t));

    log_message(LOG_DEBUG, "Stored %d cells of bytecode at %p", vm->compile_pos, bytecode_area);
}

/* Enhanced vm_exit_compile_mode */
void vm_exit_compile_mode(VM *vm) {
    if (vm == NULL) {
        return;
    }

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_WARN, "vm_exit_compile_mode called but not in compile mode");
        return;
    }

    if (vm->compiling_word == NULL) {
        log_message(LOG_ERROR, "vm_exit_compile_mode: No word being compiled");
        vm->error = 1;
        return;
    }

    log_message(LOG_DEBUG, "Finishing compilation of '%s' (compiled %d bytes)",
                vm->current_word_name, vm->compile_pos);

    /* Automatically add EXIT if not present */
    if (vm->compile_pos == 0 || vm->compile_buffer[vm->compile_pos - 1] != OP_EXIT) {
        vm_compile_exit(vm);
    }

    /* Store the compiled bytecode in dictionary */
    store_compiled_bytecode(vm, vm->compiling_word);

    /* Clear smudge bit to make word visible */
    vm->compiling_word->flags &= ~WORD_SMUDGED;

    /* Return to interpret mode */
    vm->mode = MODE_INTERPRET;
    vm->state_var = 0;  /* 0 = interpret mode */

    /* Clear compilation state */
    vm->compiling_word = NULL;
    vm->compile_pos = 0;

    log_message(LOG_DEBUG, "Compilation completed successfully");
}

/* Get bytecode area for a compiled word */
static cell_t* get_bytecode_area(DictEntry *entry) {
    /* Bytecode is stored after the dictionary entry structure */
    /* Since name is now fixed-size, calculate address simply */
    uintptr_t addr = (uintptr_t)entry + sizeof(DictEntry);

    /* Align to cell boundary */
    addr = (addr + sizeof(cell_t) - 1) & ~(sizeof(cell_t) - 1);

    return (cell_t*)addr;
}

/* Actual bytecode execution function */
static void execute_compiled_word(VM *vm, DictEntry *entry) {
    if (!(entry->flags & WORD_COMPILED)) {
        log_message(LOG_ERROR, "execute_compiled_word called on non-compiled word");
        vm->error = 1;
        return;
    }

    /* Get bytecode area */
    cell_t *bytecode = get_bytecode_area(entry);

    log_message(LOG_DEBUG, "Executing compiled word bytecode at %p", bytecode);

    /* Execute bytecode */
    size_t pc = 0;  /* Program counter */

    while (!vm->error) {
        opcode_type_t opcode = (opcode_type_t)bytecode[pc++];

        log_message(LOG_DEBUG, "PC=%zu: Executing opcode %d", pc-1, opcode);

        switch (opcode) {
            case OP_LITERAL: {
                cell_t value = bytecode[pc++];
                vm_push(vm, value);
                log_message(LOG_DEBUG, "OP_LITERAL: pushed %ld", (long)value);
                break;
            }

            case OP_CALL: {
                word_func_t func = (word_func_t)(uintptr_t)bytecode[pc++];
                log_message(LOG_DEBUG, "OP_CALL: calling function %p", func);
                if (func != NULL) {
                    func(vm);
                } else {
                    log_message(LOG_ERROR, "OP_CALL: NULL function pointer");
                    vm->error = 1;
                }
                break;
            }

            case OP_EXIT:
                log_message(LOG_DEBUG, "OP_EXIT: exiting compiled word");
                return;  /* Exit from compiled word */

            /* Control flow opcodes - NOOPs for now */
            case OP_BRANCH:
            case OP_0BRANCH:
            case OP_IF:
            case OP_THEN:
            case OP_ELSE:
            case OP_DO:
            case OP_LOOP:
            case OP_PLUS_LOOP:
            case OP_LEAVE:
            case OP_I:
            case OP_J:
            case OP_BEGIN:
            case OP_WHILE:
            case OP_REPEAT:
            case OP_UNTIL:
            case OP_AGAIN:
            case OP_CASE:
            case OP_OF:
            case OP_ENDOF:
            case OP_ENDCASE:
                /* These are NOOPs until control_words.c is implemented */
                log_message(LOG_DEBUG, "Control flow opcode %d not yet implemented", opcode);
                break;

            default:
                log_message(LOG_ERROR, "Unknown opcode: %d", opcode);
                vm->error = 1;
                return;
        }
    }
}

void vm_interpret_word(VM *vm, const char *word_str, size_t len) {
    /* Try to find word in dictionary */
    DictEntry *entry = vm_find_word(vm, word_str, len);

    if (entry) {
        log_message(LOG_DEBUG, "Found word: %.*s", (int)len, word_str);

        if (entry->flags & WORD_COMPILED) {
            /* Compiled word: execute bytecode */
            if (vm->mode == MODE_COMPILE && !(entry->flags & WORD_IMMEDIATE)) {
                /* Compile a call to this compiled word */
                vm_compile_call(vm, entry->func);
            } else {
                /* Execute the compiled word immediately */
                execute_compiled_word(vm, entry);
            }
        } else {
            /* Built-in word: use function pointer directly from entry */
            word_func_t func = entry->func;

            if (vm->mode == MODE_COMPILE && !(entry->flags & WORD_IMMEDIATE)) {
                /* Compile the word */
                vm_compile_call(vm, func);
                log_message(LOG_DEBUG, "Compiled call to %.*s", (int)len, word_str);
            } else {
                /* Execute immediately */
                if (func != NULL) {
                    log_message(LOG_DEBUG, "Executing %.*s immediately", (int)len, word_str);
                    execute_defining_word(vm, entry);
                } else {
                    log_message(LOG_ERROR, "NULL function pointer for word: %.*s", (int)len, word_str);
                    vm->error = 1;
                }
            }
        }
    } else {
        /* Try to parse as number */
        cell_t value;
        if (vm_parse_number(word_str, &value)) {
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

        /* Exit early if error occurred */
        if (vm->error) {
            break;
        }
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