/* vm.c */

#include "vm.h"
#include "io.h"
#include "log.h"
#include "word_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* VM Initialization */
void vm_init(VM *vm) {
    vm->data_sp = 0;
    vm->return_sp = 0;
    vm->ip = NULL;
    vm->thread = NULL;
    vm->mode = VM_MODE_INTERPRET;
    vm->error = 0;
    vm->halted = 0;
    vm->dictionary_count = 0;
    vm->blocks = NULL;
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
    log_message(LOG_DEBUG, "Pushed %d onto data stack", (int)value);
}

cell_t vm_pop(VM *vm) {
    if (vm->data_sp <= 0) {
        log_message(LOG_ERROR, "Data stack underflow");
        vm->error = 1;
        return 0;
    }
    vm->error = 0;
    cell_t val = vm->data_stack[--vm->data_sp];
    log_message(LOG_DEBUG, "Popped %d from data stack", (int)val);
    return val;
}

void vm_rpush(VM *vm, cell_t value) {
    if (vm->return_sp >= STACK_SIZE) {
        log_message(LOG_ERROR, "Return stack overflow");
        vm->error = 1;
        return;
    }
    vm->return_stack[vm->return_sp++] = value;
    vm->error = 0;
    log_message(LOG_DEBUG, "Pushed %d onto return stack", (int)value);
}

cell_t vm_rpop(VM *vm) {
    if (vm->return_sp <= 0) {
        log_message(LOG_ERROR, "Return stack underflow");
        vm->error = 1;
        return 0;
    }
    vm->error = 0;
    cell_t val = vm->return_stack[--vm->return_sp];
    log_message(LOG_DEBUG, "Popped %d from return stack", (int)val);
    return val;
}

/* Word registration */
void vm_register_word(VM *vm, const char *name, word_func_t func, const void *code) {
    if (vm->dictionary_count >= 65535) {
        log_message(LOG_ERROR, "Dictionary full, cannot register word: %s", name);
        return;
    }
    vm->dictionary[vm->dictionary_count].name = name;
    vm->dictionary[vm->dictionary_count].func = func;
    vm->dictionary[vm->dictionary_count].code = code;
    vm->dictionary_count++;
}

/* Threaded interpreter loop */
void vm_execute(VM *vm, const void *thread) {
    const word_func_t *ip = (const word_func_t *)thread;
    vm->ip = thread;
    vm->thread = thread;
    vm->halted = 0;

    while (!vm->halted) {
        word_func_t instr = *ip++;
        if (instr == NULL) {
            log_message(LOG_DEBUG, "End of thread reached");
            break;
        }
        log_message(LOG_DEBUG, "Executing word at %p", (void *)instr);
        instr(vm);
        if (vm->error) {
            log_message(LOG_ERROR, "Execution aborted due to previous error");
            break;
        }
    }
}

/* Helper: Check if string is decimal number */
static int is_number(const char *str) {
    if (*str == '\0') return 0;
    if (*str == '-' || *str == '+') str++;
    if (*str == '\0') return 0;
    while (*str) {
        if (!isdigit((unsigned char)*str))
            return 0;
        str++;
    }
    return 1;
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

        /* Strip newline */
        line[strcspn(line, "\r\n")] = 0;

        if (strcmp(line, "exit") == 0)
            break;

        /* Check for comment - if line contains \, ignore everything after \ */
        char *comment_pos = strchr(line, '\\');
        if (comment_pos != NULL) {
            *comment_pos = '\0';  /* Truncate at comment */
        }

        /* Skip empty lines or lines that were only comments */
        size_t line_len = strlen(line);
        if (line_len == 0 || strspn(line, " \t") == line_len) {
            continue;
        }

        token = strtok(line, " \t");
        while (token != NULL) {
            vm->error = 0;

            if (is_number(token)) {
                vm_push(vm, (cell_t)strtol(token, NULL, 10));
                if (vm->error) {
                    log_message(LOG_ERROR, "Stack overflow pushing number %s", token);
                    break;
                }
            } else {
                word_func_t word = lookup_word(vm, token);
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
            token = strtok(NULL, " \t");
        }
    }
}

/* Snapshot for debugging */
void vm_snapshot(VM *vm) {
    log_message(LOG_INFO, "VM Snapshot:");
    log_message(LOG_INFO, "  Data stack depth: %d", vm->data_sp);
    log_message(LOG_INFO, "  Return stack depth: %d", vm->return_sp);
    log_message(LOG_INFO, "  Mode: %s", vm->mode == VM_MODE_INTERPRET ? "Interpret" : "Compile");
    log_message(LOG_INFO, "  Error flag: %d", vm->error);
    log_message(LOG_INFO, "  Halted flag: %d", vm->halted);
}