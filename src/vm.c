#include "vm.h"
#include "io.h"
#include "forth79_words.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* VM Initialization */
void vm_init(VM *vm) {
    vm->data_sp = 0;
    vm->return_sp = 0;
    vm->ip = NULL;
    vm->thread = NULL;
    vm->mode = VM_MODE_INTERPRET;
    vm->error = 0;
    vm->halted = 0;
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
    log_message(LOG_DEBUG, "Pushed %u onto data stack", value);
}

cell_t vm_pop(VM *vm) {
    if (vm->data_sp <= 0) {
        log_message(LOG_ERROR, "Data stack underflow");
        vm->error = 1;
        return 0;
    }
    vm->error = 0;
    cell_t val = vm->data_stack[--vm->data_sp];
    log_message(LOG_DEBUG, "Popped %u from data stack", val);
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
    log_message(LOG_DEBUG, "Pushed %u onto return stack", value);
}

cell_t vm_rpop(VM *vm) {
    if (vm->return_sp <= 0) {
        log_message(LOG_ERROR, "Return stack underflow");
        vm->error = 1;
        return 0;
    }
    vm->error = 0;
    cell_t val = vm->return_stack[--vm->return_sp];
    log_message(LOG_DEBUG, "Popped %u from return stack", val);
    return val;
}

/* Stack helpers */
int vm_peek(VM *vm, int depth, cell_t *out) {
    if (depth < 0 || depth >= vm->data_sp) {
        log_message(LOG_WARN, "vm_peek: invalid depth %d", depth);
        return -1;
    }
    *out = vm->data_stack[vm->data_sp - 1 - depth];
    return 0;
}

int vm_poke(VM *vm, int depth, cell_t value) {
    if (depth < 0 || depth >= vm->data_sp) {
        log_message(LOG_WARN, "vm_poke: invalid depth %d", depth);
        return -1;
    }
    vm->data_stack[vm->data_sp - 1 - depth] = value;
    return 0;
}

int vm_drop_n(VM *vm, int n) {
    if (n < 0 || n > vm->data_sp) {
        log_message(LOG_WARN, "vm_drop_n: invalid count %d", n);
        vm->error = 1;
        return -1;
    }
    vm->data_sp -= n;
    return 0;
}

/* Threaded interpreter loop */
void vm_execute(VM *vm, XT *thread) {
    if (vm->halted) {
        log_message(LOG_WARN, "VM is halted; cannot execute");
        return;
    }
    vm->ip = thread;

    while (1) {
        XT instr = *(vm->ip)++;
        if (instr == NULL) {
            log_message(LOG_DEBUG, "End of thread reached");
            break;
        }
        log_message(LOG_DEBUG, "Executing word at %p", (void*)instr);
        instr(vm);
        if (vm->error) {
            log_message(LOG_ERROR, "Execution aborted due to previous error");
            break;
        }
        if (vm->halted) {
            log_message(LOG_INFO, "VM halted during execution");
            break;
        }
    }
}

/* Helper to check if string is number (decimal only) */
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

/* Debug snapshot */
void vm_snapshot(VM *vm) {
    log_message(LOG_INFO, "VM Snapshot:");
    log_message(LOG_INFO, "  Data stack depth: %d", vm->data_sp);
    log_message(LOG_INFO, "  Return stack depth: %d", vm->return_sp);
    log_message(LOG_INFO, "  Mode: %s", vm->mode == VM_MODE_INTERPRET ? "Interpret" : "Compile");
    log_message(LOG_INFO, "  Error flag: %d", vm->error);
    log_message(LOG_INFO, "  Halted flag: %d", vm->halted);
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

        line[strcspn(line, "\r\n")] = 0;  /* strip newline */

        if (strcmp(line, "exit") == 0)
            break;

        token = strtok(line, " ");
        while (token != NULL) {
            vm->error = 0;  /* Clear error before word */

            if (is_number(token)) {
                vm_push(vm, (cell_t)strtol(token, NULL, 10));
                if (vm->error) {
                    log_message(LOG_ERROR, "Stack overflow pushing number %s", token);
                    break;
                }
            } else {
                word_func_t word = lookup_word(token);
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
