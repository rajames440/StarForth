/* vm.h */
#ifndef VM_H
#define VM_H

#include <stdint.h>

typedef int cell_t;

/* Forward declaration */
typedef struct VM VM;

/* Function pointer type for runtime words */
typedef void (*word_func_t)(VM *vm);

/* Function pointer type for VM opcodes */
typedef void (*opcode_func_t)(VM *vm);

/* VM structure */
struct VM {
    /* Data stack */
    cell_t data_stack[1024];
    int data_sp;

    /* Return stack */
    cell_t return_stack[1024];
    int return_sp;

    /* Instruction pointer */
    cell_t *ip;       /* Points into instruction stream */
    cell_t *thread;   /* Current running thread/instruction array */

    int mode;         /* Interpret or compile mode */
    int error;        /* Error flag */
    int halted;       /* Halt flag */

    /* Dictionary of user/runtime words */
    struct {
        char *name;
        word_func_t func;
    } dictionary[512];
    int dictionary_count;

    /* Opcode dispatch table for special VM opcodes */
    opcode_func_t opcode_table[256]; /* 256 possible opcodes */

    /* For io.c compatibility, add blocks here */
    unsigned char *blocks; /* Pointer to memory blocks */
};

/* VM modes */
#define VM_MODE_INTERPRET 0
#define VM_MODE_COMPILE   1

/* VM API */
void vm_init(VM *vm);
void vm_push(VM *vm, cell_t value);
cell_t vm_pop(VM *vm);
void vm_rpush(VM *vm, cell_t value);
cell_t vm_rpop(VM *vm);

void vm_execute(VM *vm, cell_t *thread);

/* Word registration */
void vm_register_word(VM *vm, const char *name, word_func_t func);
word_func_t vm_lookup_word(VM *vm, const char *name);

/* Opcode registration */
void vm_register_opcode(VM *vm, uint8_t opcode, opcode_func_t func);

/* Helper to lookup opcode function */
opcode_func_t vm_lookup_opcode(VM *vm, uint8_t opcode);

/* REPL */
void vm_repl(VM *vm);

#endif /* VM_H */
