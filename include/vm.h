/*

                                 ***   StarForth   ***
  vm.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef VM_H
#define VM_H

/* ANSI C99 only - minimal headers */
#include <stddef.h>
#include <stdint.h>

/* Forward declaration */
struct VM;

typedef intptr_t cell_t;
typedef void (*word_func_t)(struct VM *vm);

#define STACK_SIZE 1024
#define DICTIONARY_SIZE 1024
#define VM_MEMORY_SIZE (1024 * 1024)  /* 1 MB - UPDATE THIS TOO */
#define INPUT_BUFFER_SIZE 256
#define WORD_NAME_MAX 31
#define COMPILE_BUFFER_SIZE 1024

/* Block system configuration */
#define BLOCK_SIZE 1024                                 /* 1KB per block */
#define MAX_BLOCKS (VM_MEMORY_SIZE / BLOCK_SIZE)        /* 1024 blocks from 1MB */

/* Memory layout constants */
#define DICTIONARY_BLOCKS 64                            /* First 64 blocks (64KB) for dictionary */
#define DICTIONARY_MEMORY_SIZE (DICTIONARY_BLOCKS * BLOCK_SIZE)
#define USER_BLOCKS_START DICTIONARY_BLOCKS             /* User blocks start at block 64 */

/* Word flags */
#define WORD_IMMEDIATE  0x80    /* Word executes immediately even in compile mode */
#define WORD_HIDDEN     0x40    /* Word is hidden from dictionary searches */
#define WORD_SMUDGED    0x20    /* Word is smudged (being defined) - FORTH-79 */
#define WORD_COMPILED   0x10    /* Word is user-compiled (not built-in) */

/* Dictionary entry - enhanced for FORTH-79 compatibility */
typedef struct DictEntry {
    struct DictEntry *link;     /* Previous word (linked list) */
    word_func_t func;           /* Function pointer for execution */
    uint8_t flags;              /* Word flags */
    uint8_t name_len;           /* Length of name */
    char name[];                /* Variable-length name + optional code */
} DictEntry;

/* VM modes */
typedef enum {
    MODE_INTERPRET = 0,
    MODE_COMPILE = 1
} vm_mode_t;

typedef struct VM {
    /* Stacks */
    cell_t data_stack[STACK_SIZE];
    cell_t return_stack[STACK_SIZE];
    int dsp;  /* Data stack pointer */
    int rsp;  /* Return stack pointer */

    /* Dictionary */
    uint8_t *memory;            /* CHANGE: pointer instead of array */
    size_t here;                /* Next free memory location */
    DictEntry *latest;          /* Most recent word */

    /* Input system */
    char input_buffer[INPUT_BUFFER_SIZE];
    size_t input_length;
    size_t input_pos;

    /* Compiler state */
    vm_mode_t mode;
    DictEntry *compiling_word;  /* Word being compiled */

    /* Compilation system */
    char current_word_name[WORD_NAME_MAX + 1];  /* Name of word being compiled */
    cell_t *compile_buffer;                     /* Compilation buffer pointer */
    size_t compile_pos;                         /* Current position in compile buffer */
    size_t compile_size;                        /* Size of compile buffer */

    /* FORTH-79 Dictionary manipulation support */
    cell_t state_var;           /* STATE variable for compilation state */

    /* ADD THIS LINE: */
    DictEntry *current_executing_entry;         /* Current executing entry */

    /* VM state */
    int error;
    int halted;

    /* Block storage pointer (from io.c) */
    unsigned char *blocks;

} VM;

/* Core VM functions */
void vm_init(VM *vm);
void vm_interpret(VM *vm, const char *input);
void vm_repl(VM *vm);

/* Stack operations */
void vm_push(VM *vm, cell_t value);
cell_t vm_pop(VM *vm);
void vm_rpush(VM *vm, cell_t value);
cell_t vm_rpop(VM *vm);

/* Dictionary operations */
DictEntry* vm_find_word(VM *vm, const char *name, size_t len);
DictEntry* vm_create_word(VM *vm, const char *name, size_t len, word_func_t func);
void vm_make_immediate(VM *vm);
void vm_hide_word(VM *vm);
void vm_smudge_word(VM *vm);        /* Added for FORTH-79 SMUDGE */

/* Enhanced dictionary search functions */
DictEntry* vm_dictionary_find_by_func(VM *vm, word_func_t func);
DictEntry* vm_dictionary_find_latest_by_func(VM *vm, word_func_t func);
cell_t* vm_dictionary_get_data_field(DictEntry *entry);

/* Memory management */
void* vm_allot(VM *vm, size_t bytes);
void vm_align(VM *vm);

/* Input parsing */
int vm_parse_word(VM *vm, char *word, size_t max_len);
int vm_parse_number(const char *str, cell_t *value);

/* Compilation */
void vm_enter_compile_mode(VM *vm, const char *name, size_t len);
void vm_exit_compile_mode(VM *vm);
void vm_compile_call(VM *vm, word_func_t func);
void vm_compile_literal(VM *vm, cell_t value);
void vm_compile_exit(VM *vm);
void vm_interpret_word(VM *vm, const char *word_str, size_t len);

/* Block system integration */
void* vm_get_block_addr(VM *vm, int block_num);
int vm_addr_to_block(VM *vm, void *addr);

/* Testing functions */
void vm_run_smoke_tests(VM *vm);

/* Add cleanup function declaration */
void vm_cleanup(VM *vm);

#endif /* VM_H */
