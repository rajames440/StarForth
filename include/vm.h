#ifndef VM_H
#define VM_H

#include <stdint.h>
#include "log.h"   /* for logging */

#define STACK_SIZE 2048

typedef enum {
    VM_MODE_INTERPRET = 0,
    VM_MODE_COMPILE
} vm_mode_t;

typedef uint32_t cell_t;

struct VM;

typedef void (*word_func_t)(struct VM *vm);

typedef word_func_t XT;

typedef struct VM {
    /* Data stack */
    cell_t data_stack[STACK_SIZE];
    int data_sp;

    /* Return stack */
    cell_t return_stack[STACK_SIZE];
    int return_sp;

    /* Thread execution */
    XT *ip;
    XT *thread;

    vm_mode_t mode;

    int error;
    int halted;
} VM;

/* VM core functions */
void vm_init(VM *vm);
void vm_push(VM *vm, cell_t value);
cell_t vm_pop(VM *vm);
void vm_rpush(VM *vm, cell_t value);
cell_t vm_rpop(VM *vm);

/* Stack helpers */
int vm_peek(VM *vm, int depth, cell_t *out);
int vm_poke(VM *vm, int depth, cell_t value);
int vm_drop_n(VM *vm, int n);

/* Threaded interpreter */
void vm_execute(VM *vm, XT *thread);
void vm_repl(VM *vm);

/* Debugging snapshot */
void vm_snapshot(VM *vm);

#endif /* VM_H */
