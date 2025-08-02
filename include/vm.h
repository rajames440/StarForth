#ifndef VM_H
#define VM_H

#include <stddef.h>  /* For NULL */

/* Forward declaration */
struct VM;

typedef int cell_t;

/* Function pointer type for words */
typedef void (*word_func_t)(struct VM *vm);

#define STACK_SIZE 1024

typedef struct VM {
    /* Data stack */
    cell_t data_stack[STACK_SIZE];
    int data_sp;

    /* Return stack */
    cell_t return_stack[STACK_SIZE];
    int return_sp;

    /* Instruction pointer for threaded code */
    const void *ip;
    const void *thread;

    /* VM mode: 0=interpret, 1=compile */
    int mode;

    int error;
    int halted;

    /* Optional block memory pointer */
    unsigned char *blocks;

    /* Dictionary for registered words */
    struct {
        const char *name;
        word_func_t func;
        const void *code;
    } dictionary[65535];
    unsigned int dictionary_count;
} VM;

#define VM_MODE_INTERPRET 0
#define VM_MODE_COMPILE 1

/* Declare vm_init and vm_repl here so compiler knows about them */
void vm_init(VM *vm);
void vm_repl(VM *vm);

#endif /* VM_H */