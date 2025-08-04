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

/* Bytecode opcodes for compiled words */
typedef enum {
    OP_LITERAL = 1,     /* Push literal value */
    OP_CALL = 2,        /* Call word function */
    OP_EXIT = 3         /* Exit from word */
} opcode_type_t;

/* REMOVE THIS HACK - Global context for compiled word execution */
/* static DictEntry *current_executing_word = NULL; */

/* Forward declaration */
static void compiled_word_runtime(VM *vm);

/* Core VM functions */
void vm_init(VM *vm) {
    /* Clear all memory */
    memset(vm, 0, sizeof(VM));
    
    /* Allocate dynamic memory - ADD THIS */
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

    /* Register all FORTH-79 standard words at the end of initialization */
    register_forth79_words(vm);
}

/* Add cleanup function - ADD THIS NEW FUNCTION */
void vm_cleanup(VM *vm) {
    if (vm->memory != NULL) {
        free(vm->memory);
        vm->memory = NULL;
    }
    vm->here = 0;
}

/* Clean stack operations - no changes needed */
void vm_push(VM *vm, cell_t value) {
    if (vm->dsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "Stack overflow");
        vm->error = 1;
        return;
    }
    vm->data_stack[++vm->dsp] = value;
    log_message(LOG_DEBUG, "PUSH: %d (dsp=%d)", (int)value, vm->dsp);  // FIX: Added missing format and closing quote
}

cell_t vm_pop(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "Stack underflow (dsp=%d)", vm->dsp);
        vm->error = 1;
        return 0;
    }
    cell_t value = vm->data_stack[vm->dsp--];
    log_message(LOG_DEBUG, "POP: %d (dsp=%d)", (int)value, vm->dsp);   // FIX: Added missing format and closing quote
    return value;
}

/* Return stack operations - FIXED VERSION */
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

/* Dictionary operations - clean up the hacks */
DictEntry* vm_find_word(VM *vm, const char *name, size_t len) {
    DictEntry *entry = vm->latest;
    
    while (entry) {
        if (!(entry->flags & WORD_HIDDEN) && 
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
    
    /* Calculate total size: header + name */
    total_size = sizeof(DictEntry) + len;
    
    /* Align to cell boundary */
    vm_align(vm);
    
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
    
    /* Copy name */
    if (len > 0) {
        memcpy((void*)entry->name, name, len);
    }
    
    /* Update latest */
    vm->latest = entry;
    
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

/* Add this function to vm.c - it should go with the other dictionary operations */

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

/* CLEAN INPUT PARSING - remove excessive debug logging */
int vm_parse_word(VM *vm, char *word, size_t max_len) {
    /* Skip whitespace */
    while (vm->input_pos < vm->input_length && 
           (vm->input_buffer[vm->input_pos] == ' ' || 
            vm->input_buffer[vm->input_pos] == '\t' ||
            vm->input_buffer[vm->input_pos] == '\n')) {
        vm->input_pos++;
    }
    
    if (vm->input_pos >= vm->input_length) {
        word[0] = '\0';
        return 0;
    }
    
    /* Parse word */
    size_t len = 0;
    while (vm->input_pos < vm->input_length && len < max_len - 1 &&
           vm->input_buffer[vm->input_pos] != ' ' &&
           vm->input_buffer[vm->input_pos] != '\t' &&
           vm->input_buffer[vm->input_pos] != '\n') {
        word[len++] = vm->input_buffer[vm->input_pos++];
    }
    
    word[len] = '\0';
    return len > 0;
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

/* CLEAN compilation functions */
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
    vm->compiling_word = vm_create_word(vm, name, len, NULL);
    if (vm->compiling_word != NULL) {
        vm->compiling_word->flags |= WORD_SMUDGED;  /* Mark as being compiled */
    }
    
    /* Initialize compilation buffer */
    vm->compile_pos = 0;
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
}

/* SIMPLIFIED compiled word creation - remove all the complex alignment hacks */
void vm_exit_compile_mode(VM *vm) {
    /* Clear smudge bit */
    if (vm->compiling_word != NULL) {
        vm->compiling_word->flags &= ~WORD_SMUDGED;
        
        /* Set the compiled word's function pointer */
        vm->compiling_word->func = compiled_word_runtime;
    }
    
    /* Return to interpret mode */
    vm->mode = MODE_INTERPRET;
    vm->state_var = 0;  /* 0 = interpret mode */
    
    vm->compiling_word = NULL;
}

/* Add bytecode execution function */
static void execute_compiled_word(VM *vm, DictEntry *entry) {
    /* Get bytecode area (after function pointer) */
    word_func_t *func_ptr = (word_func_t*)(entry->name + entry->name_len);
    cell_t *bytecode = (cell_t*)(func_ptr + 1);
    
    /* Execute bytecode */
    size_t pc = 0;  /* Program counter */
    
    while (1) {
        opcode_type_t opcode = (opcode_type_t)bytecode[pc++];
        
        switch (opcode) {
            case OP_LITERAL: {
                cell_t value = bytecode[pc++];
                vm_push(vm, value);
                break;
            }
            
            case OP_CALL: {
                word_func_t func = (word_func_t)(uintptr_t)bytecode[pc++];
                func(vm);
                if (vm->error) return;
                break;
            }
            
            case OP_EXIT:
                return;  /* Exit from compiled word */
            
            default:
                log_message(LOG_ERROR, "Unknown opcode: %d", opcode);
                vm->error = 1;
                return;
        }
        
        if (vm->error) return;
    }
}

void vm_interpret_word(VM *vm, const char *word_str, size_t len) {
    /* Try to find word in dictionary */
    DictEntry *entry = vm_find_word(vm, word_str, len);
    
    if (entry) {
        if (entry->flags & WORD_COMPILED) {
            /* Compiled word: execute bytecode */
            if (vm->mode == MODE_COMPILE && !(entry->flags & WORD_IMMEDIATE)) {
                /* Compile a call to this compiled word */
                vm_compile_call(vm, entry->func);  // Use entry->func directly
            } else {
                /* Execute the compiled word immediately */
                execute_compiled_word(vm, entry);
            }
        } else {
            /* Built-in word: use function pointer directly from entry */
            word_func_t func = entry->func;  // FIX: Use entry->func directly!
            
            if (vm->mode == MODE_COMPILE && !(entry->flags & WORD_IMMEDIATE)) {
                /* Compile the word */
                vm_compile_call(vm, func);
            } else {
                /* Execute immediately */
                if (func != NULL) {  // Safety check
                    func(vm);
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

/* Remove the placeholder - it's no longer needed */
static void compiled_word_runtime(VM *vm) {
    /* This should never be called now */
    log_message(LOG_ERROR, "compiled_word_runtime called - this should not happen");
    vm->error = 1;
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