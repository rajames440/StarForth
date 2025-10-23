/*
                                  ***   StarForth   ***

  vm.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:55:24.706-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/include/vm.h
 */

#ifndef VM_H
#define VM_H

/* Bare metal - no system headers */
#include <stdint.h>
#include <stddef.h>

/* Forward declaration */
struct VM;

/* Bare metal type definitions */
typedef signed long cell_t;


typedef void (*word_func_t)(struct VM *vm);

/* ===== VM address model (Phase 1 scaffolding) ============================
 * StarForth rule: addresses on the data stack are VM OFFSETS, not C pointers.
 * vaddr_t is a byte offset into the VM's virtual address space.
 * Implementations come in a later change; these are declarations only.
 */
typedef uint64_t vaddr_t;

/* Bounds check: nonzero if [addr, addr+len) is a valid VM range */
int vm_addr_ok(struct VM *vm, vaddr_t addr, size_t len);

/* Internal pointer materialization for subsystems (NOT for word sources) */
uint8_t *vm_ptr(struct VM *vm, vaddr_t addr);

/* Canonical memory accessors; all word code will funnel through these */
uint8_t vm_load_u8(struct VM *vm, vaddr_t addr);

void vm_store_u8(struct VM *vm, vaddr_t addr, uint8_t v);

cell_t vm_load_cell(struct VM *vm, vaddr_t addr); /* requires alignment */
void vm_store_cell(struct VM *vm, vaddr_t addr, cell_t v);

/* Explicit stack<->offset conversions (keep intent obvious) */
static inline vaddr_t VM_ADDR(cell_t c) { return (vaddr_t) (uint64_t) c; }
static inline cell_t CELL(vaddr_t a) { return (cell_t) (int64_t) a; }
/* ======================================================================== */

#define STACK_SIZE 1024
#define DICTIONARY_SIZE 1024
#define VM_MEMORY_SIZE (5 * 1024 * 1024)  /* 5 MB total VM memory */
#define INPUT_BUFFER_SIZE 256
#define WORD_NAME_MAX 31
#define COMPILE_BUFFER_SIZE 1024

/* Block system configuration */
#define BLOCK_SIZE 1024                                 /* 1KB per block */
#define MAX_BLOCKS (VM_MEMORY_SIZE / BLOCK_SIZE)        /* 5120 blocks from 5MB */

/* Memory layout constants */
#define DICTIONARY_BLOCKS 2048                          /* First 2048 blocks (2MB) for dictionary */
#define DICTIONARY_MEMORY_SIZE (DICTIONARY_BLOCKS * BLOCK_SIZE)
#define USER_BLOCKS_START DICTIONARY_BLOCKS             /* User blocks start at block 2048 */

/* Persistent log configuration */
#define LOG_LINE_MAX 64                                 /* Each log line is 64 bytes */
#define LOG_LINES_PER_BLOCK (BLOCK_SIZE / LOG_LINE_MAX) /* 16 lines per 1KB block */
#define LOG_BLOCKS_START 3072                           /* Log starts at block 3072 */
#define LOG_BLOCKS_END 5120                             /* Log ends at block 5120 (exclusive) */
#define LOG_BLOCKS (LOG_BLOCKS_END - LOG_BLOCKS_START)  /* 2048 blocks (2MB) for logs */
#define LOG_LAYER1_MAX_LINES (LOG_BLOCKS * LOG_LINES_PER_BLOCK) /* 32768 max lines */

/* Word flags */
#define WORD_IMMEDIATE  0x80    /* Word executes immediately even in compile mode */
#define WORD_HIDDEN     0x40    /* Word is hidden from dictionary searches */
#define WORD_SMUDGED    0x20    /* Word is smudged (being defined) - FORTH-79 */
#define WORD_COMPILED   0x10    /* Word is user-compiled (not built-in) */
#define WORD_PINNED     0x08    /* Word's entropy is pinned (cannot decay to zero) */

/* ACL (Access Control List) defaults - stub implementation */
#define ACL_USER_DEFAULT 0x01   /* Default access: users can execute and compile */

/* Physics model constants - stub implementation */
#define SPIN_IDLE        0      /* Particle spin state: idle */
#define CHARGE_NEUTRAL   0      /* Particle charge state: neutral */

/* Physics properties structure - stub implementation for elementary particle model */
typedef struct {
    uint32_t observations;          /* Number of times word has been observed/executed */
    uint32_t entropy_reciprocal;    /* Reciprocal of entropy (higher = colder/less used) */
    uint64_t last_observed_ns;      /* Timestamp of last observation in nanoseconds */
    uint32_t decay_rate;            /* Rate at which word decays when unused */
    uint32_t mass;                  /* Memory footprint of the word */
    uint32_t momentum;              /* Execution momentum */
    uint8_t spin;                   /* Particle spin state */
    uint8_t charge;                 /* Particle charge state */
    uint8_t superposition;          /* Superposition state */
    uint8_t heat_index;             /* Heat index (usage intensity) */
    void *topics;                   /* Topic subscriptions (NULL for now) */
    uint32_t topic_count;           /* Number of topic subscriptions */
} DictPhysics;

/* Dictionary entry - enhanced for FORTH-79 compatibility */
typedef struct DictEntry {
    struct DictEntry *link; /* Previous word (linked list) */
    word_func_t func; /* Function pointer for execution */
    uint8_t flags; /* Word flags */
    uint8_t name_len; /* Length of name */
    cell_t entropy; /* Usage counter - incremented on each execution */
    uint8_t acl_default; /* Access control list default permissions - stub */
    DictPhysics physics; /* Physics properties for elementary particle model */
    char name[]; /* Variable-length name + optional code */
} DictEntry;

/* VM modes */
typedef enum {
    MODE_INTERPRET = 0,
    MODE_COMPILE = 1
} vm_mode_t;

/**
 * @struct VM
 * @brief Main virtual machine state container
 *
 * Contains all state information for a StarForth VM instance including stacks,
 * dictionary, memory, and execution state.
 */
typedef struct VM {
    /** @name Stack Management
     * @{
     */
    cell_t data_stack[STACK_SIZE]; /**< Parameter stack storage */
    cell_t return_stack[STACK_SIZE]; /**< Return stack storage */
    int dsp; /**< Data stack pointer */
    int rsp; /**< Return stack pointer */
    int exit_colon; /**< Exit flag for colon definitions */
    int abort_requested; /**< ABORT flag for immediate termination */
    /** @} */

    /** @name Dictionary Management 
     * @{
     */
    uint8_t *memory; /**< Unified VM memory buffer */
    size_t here; /**< Next free memory location (byte offset) */
    DictEntry *latest; /**< Most recent word */

    /* Dictionary protection fence: words at/older than this are protected from FORGET */
    DictEntry *dict_fence_latest;
    size_t dict_fence_here;

    /* Input system */
    char input_buffer[INPUT_BUFFER_SIZE];
    size_t input_length;
    size_t input_pos;

    /* Compiler state */
    vm_mode_t mode;
    DictEntry *compiling_word; /* Word being compiled */

    /* Compilation support */
    char current_word_name[WORD_NAME_MAX + 1];
    cell_t *compile_buffer;
    size_t compile_pos;
    size_t compile_size;

    /* FORTH-79 Dictionary manipulation support */
    cell_t state_var; /* STATE variable (0 interpret, -1 compile) */

    /* Execution bookkeeping */
    DictEntry *current_executing_entry;
    cell_t *ip; /* direct-threaded instruction pointer (points into memory in CELLS) */

    /* VM state */
    int error;
    int halted;

    /* Numeric base (Forth BASE). Default 10. */
    cell_t base;

    /* TIB stuff */
    unsigned char *tib_buf; /* pointer to TIB buffer (host alloc for now) */
    size_t tib_cap; /* capacity in bytes */
    cell_t *in_var; /* >IN (legacy; will migrate to VM addr) */
    cell_t *span_var; /* SPAN (legacy; will migrate to VM addr) */

    /* Block system (VM-backed variables / addresses) */
    vaddr_t scr_addr;
    vaddr_t state_addr; /* VM cell holding STATE (0=interp, -1=compile) */
    vaddr_t base_addr; /* VM cell: numeric base (2..36), default 10 */
} VM;

/** @name Core VM Functions
 * @{
 */
/**
 * @brief Initialize a new VM instance
 * @param vm Pointer to VM structure to initialize
 */
void vm_init(VM *vm);

/**
 * @brief Interpret a string of Forth code
 * @param vm VM instance to use
 * @param input String containing Forth code to interpret
 */
void vm_interpret(VM *vm, const char *input);

/**
 * @brief Start the VM's read-eval-print loop
 * @param vm VM instance to run
 */
void vm_repl(VM *vm);

/** @} */

/* Stack operations */
void vm_push(VM *vm, cell_t value);

cell_t vm_pop(VM *vm);

void vm_rpush(VM *vm, cell_t value);

cell_t vm_rpop(VM *vm);

/* Dictionary operations */
DictEntry *vm_find_word(VM *vm, const char *name, size_t len);

DictEntry *vm_create_word(VM *vm, const char *name, size_t len, word_func_t func);

void vm_make_immediate(VM *vm);

void vm_hide_word(VM *vm);

void vm_smudge_word(VM *vm); /* Added for FORTH-79 SMUDGE */
void vm_pin_entropy(VM *vm); /* Pin entropy (prevent decay) */
void vm_unpin_entropy(VM *vm); /* Unpin entropy (allow decay) */

/* Enhanced dictionary search functions */
DictEntry *vm_dictionary_find_by_func(VM *vm, word_func_t func);

DictEntry *vm_dictionary_find_latest_by_func(VM *vm, word_func_t func);

cell_t *vm_dictionary_get_data_field(DictEntry *entry);

void vm_compile_word(VM *vm, DictEntry *entry);

/* Memory management */
void *vm_allot(VM *vm, size_t bytes);

void vm_align(VM *vm);

/* Input parsing */
int vm_parse_word(VM *vm, char *word, size_t max_len);

int vm_parse_number(VM *vm, const char *str, cell_t *value);

/* Compilation */
void vm_enter_compile_mode(VM *vm, const char *name, size_t len);

void vm_exit_compile_mode(VM *vm);

/* Colon word execution (exposed for SEE decompiler) */
void execute_colon_word(VM *vm);

void vm_compile_call(VM *vm, word_func_t func);

void vm_compile_literal(VM *vm, cell_t value);

void vm_compile_exit(VM *vm);

void vm_interpret_word(VM *vm, const char *word_str, size_t len);

/* Block system integration */
void *vm_get_block_addr(VM *vm, int block_num);

int vm_addr_to_block(VM *vm, void *addr);

/* Testing functions */
void vm_run_smoke_tests(VM *vm);

/* Add cleanup function declaration */
void vm_cleanup(VM *vm);

/* ===== Performance optimizations for release builds ==================== */
#ifdef STARFORTH_PERFORMANCE

/* Fast inline stack operations - skip bounds checking in performance builds */
static inline void vm_push_fast(VM *vm, cell_t value) {
    vm->data_stack[++vm->dsp] = value;
}

static inline cell_t vm_pop_fast(VM *vm) {
    return vm->data_stack[vm->dsp--];
}

static inline void vm_rpush_fast(VM *vm, cell_t value) {
    vm->return_stack[++vm->rsp] = value;
}

static inline cell_t vm_rpop_fast(VM *vm) {
    return vm->return_stack[vm->rsp--];
}

/* Performance macros - use fast paths in hot code */
#define VM_PUSH(vm, val) vm_push_fast(vm, val)
#define VM_POP(vm) vm_pop_fast(vm)
#define VM_RPUSH(vm, val) vm_rpush_fast(vm, val)
#define VM_RPOP(vm) vm_rprop_fast(vm)

/* Likely/unlikely branch prediction hints */
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#else

/* Safe versions with bounds checking for debug builds */
#define VM_PUSH(vm, val) vm_push(vm, val)
#define VM_POP(vm) vm_pop(vm)
#define VM_RPUSH(vm, val) vm_rpush(vm, val)
#define VM_RPOP(vm) vm_rpop(vm)
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)

#endif

#endif /* VM_H */