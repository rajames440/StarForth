/* vm.c */

#include "vm.h"
#include "io.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>  /* for uintptr_t */

#ifndef HAVE_STRDUP
/* Provide strdup if not available */
char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if (p) {
        memcpy(p, s, len);
    }
    return p;
}
#endif

#define STACK_SIZE 1024
#define DICTIONARY_SIZE 512
#define OPCODE_TABLE_SIZE 256

/* Forward declarations for special VM opcode handlers */
static void op_lit(VM *vm);
static void op_branch(VM *vm);
static void op_0branch(VM *vm);
static void op_exit(VM *vm);

/* Initialize the VM */
void vm_init(VM *vm) {
    int i;

    vm->data_sp = 0;
    vm->return_sp = 0;
    vm->ip = NULL;
    vm->thread = NULL;
    vm->mode = VM_MODE_INTERPRET;
    vm->error = 0;
    vm->halted = 0;
    vm->dictionary_count = 0;
    vm->blocks = NULL;

    for (i = 0; i < OPCODE_TABLE_SIZE; i++) {
        vm->opcode_table[i] = NULL;
    }

    /* Register special VM opcodes */
    vm_register_opcode(vm, 0x00, op_lit);
    vm_register_opcode(vm, 0x01, op_branch);
    vm_register_opcode(vm, 0x02, op_0branch);
    vm_register_opcode(vm, 0x03, op_exit);

    io_init(vm);
    log_set_level(LOG_INFO);
    log_message(LOG_INFO, "VM initialized");
}

/* Stack operations */
void vm_push(VM *vm, cell_t value) {
    if (vm->data_sp >= STACK_SIZE) {
        log_message(LOG_ERROR, "Data stack overflow");
        vm->error = 1;
        return;
    }
    vm->data_stack[vm->data_sp++] = value;
    vm->error = 0;
    log_message(LOG_DEBUG, "Pushed %d onto data stack", value);
}

cell_t vm_pop(VM *vm) {
    if (vm->data_sp <= 0) {
        log_message(LOG_ERROR, "Data stack underflow");
        vm->error = 1;
        return 0;
    }
    vm->error = 0;
    vm->data_sp--;
    return vm->data_stack[vm->data_sp];
}

void vm_rpush(VM *vm, cell_t value) {
    if (vm->return_sp >= STACK_SIZE) {
        log_message(LOG_ERROR, "Return stack overflow");
        vm->error = 1;
        return;
    }
    vm->return_stack[vm->return_sp++] = value;
    vm->error = 0;
    log_message(LOG_DEBUG, "Pushed %d onto return stack", value);
}

cell_t vm_rpop(VM *vm) {
    if (vm->return_sp <= 0) {
        log_message(LOG_ERROR, "Return stack underflow");
        vm->error = 1;
        return 0;
    }
    vm->error = 0;
    vm->return_sp--;
    return vm->return_stack[vm->return_sp];
}

/* Register user runtime word */
void vm_register_word(VM *vm, const char *name, word_func_t func) {
    if (vm->dictionary_count >= DICTIONARY_SIZE) {
        log_message(LOG_ERROR, "Dictionary full, cannot register '%s'", name);
        return;
    }
    vm->dictionary[vm->dictionary_count].name = strdup(name);
    vm->dictionary[vm->dictionary_count].func = func;
    vm->dictionary_count++;
    log_message(LOG_DEBUG, "Registered word: %s", name);
}

/* Lookup user runtime word */
word_func_t vm_lookup_word(VM *vm, const char *name) {
    int i;
    for (i = 0; i < vm->dictionary_count; i++) {
        if (strcmp(vm->dictionary[i].name, name) == 0) {
            return vm->dictionary[i].func;
        }
    }
    return NULL;
}

/* Register special VM opcode */
void vm_register_opcode(VM *vm, uint8_t opcode, opcode_func_t func) {
    vm->opcode_table[opcode] = func;
}

/* Lookup special VM opcode */
opcode_func_t vm_lookup_opcode(VM *vm, uint8_t opcode) {
    return vm->opcode_table[opcode];
}

/* VM interpreter loop */
void vm_execute(VM *vm, cell_t *thread) {
    cell_t instr;
    uint8_t opcode;
    opcode_func_t op_func;
    word_func_t word;

    if (vm->halted) {
        log_message(LOG_WARN, "VM is halted; cannot execute");
        return;
    }

    vm->ip = thread;

    while (1) {
        if (vm->error || vm->halted) {
            log_message(LOG_INFO, "Stopping execution due to error or halt");
            break;
        }

        instr = *(vm->ip)++;
        opcode = (uint8_t)instr;

        op_func = vm_lookup_opcode(vm, opcode);
        if (op_func) {
            op_func(vm);
        } else {
            word = (word_func_t)(uintptr_t)instr;
            if (word) {
                word(vm);
            } else {
                log_message(LOG_ERROR, "Unknown instruction or word at %p", (void *)(uintptr_t)instr);
                vm->error = 1;
                break;
            }
        }
    }
}

/* Special VM opcode handlers */

/* LIT - push literal value */
static void op_lit(VM *vm) {
    cell_t literal = *(vm->ip)++;
    vm_push(vm, literal);
}

/* BRANCH - unconditional jump */
static void op_branch(VM *vm) {
    cell_t offset = *(vm->ip)++;
    vm->ip += offset;
}

/* 0BRANCH - conditional jump if top of stack zero */
static void op_0branch(VM *vm) {
    cell_t offset = *(vm->ip)++;
    cell_t flag = vm_pop(vm);
    if (flag == 0) {
        vm->ip += offset;
    }
}

/* EXIT - return from current word/thread */
static void op_exit(VM *vm) {
    if (vm->return_sp <= 0) {
        log_message(LOG_WARN, "Return stack underflow on EXIT");
        vm->error = 1;
        return;
    }
    vm->ip = (cell_t *)(uintptr_t)vm_rpop(vm);
}

/* REPL loop */
void vm_repl(VM *vm) {
    char line[256];
    char *token;

    log_message(LOG_INFO, "Starting REPL");

    while (1) {
        printf("ok> ");
        if (!fgets(line, sizeof(line), stdin)) {
            log_message(LOG_INFO, "EOF received, exiting REPL");
            printf("\nExiting REPL.\n");
            break;
        }

        line[strcspn(line, "\r\n")] = 0; /* strip newline */

        if (strcmp(line, "exit") == 0)
            break;

        token = strtok(line, " ");
        while (token != NULL) {
            vm->error = 0;

            /* Check if token is a number */
            {
                char *endptr = NULL;
                long val = strtol(token, &endptr, 10);
                if (endptr != token && *endptr == '\0') {
                    vm_push(vm, (cell_t)val);
                    token = strtok(NULL, " ");
                    continue;
                }
            }

            /* Lookup user word */
            {
                word_func_t word = vm_lookup_word(vm, token);
                if (word) {
                    word(vm);
                    if (vm->error) {
                        log_message(LOG_ERROR, "Error executing word: %s", token);
                        break;
                    }
                } else {
                    log_message(LOG_ERROR, "Unknown word: %s", token);
                    break;
                }
            }

            token = strtok(NULL, " ");
        }
    }
}
