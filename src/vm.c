/*

                                 ***   StarForth   ***
  vm.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/12/25, 6:11 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/**
 * @file vm.c
 * @brief Implementation of the StarForth virtual machine
 *
 * This file contains the implementation of the core virtual machine functionality
 * for the StarForth Forth interpreter, including memory management, word compilation,
 * and execution of Forth words.
 */

#include "../include/vm.h"
#include "../include/vm_debug.h"
#include "../include/log.h"
#include "../include/io.h"
#include "../include/word_registry.h"
#include <string.h>
#include <stdlib.h>

/* forward decls for bootstraps defined later */
static void execute_colon_word(VM *vm);
static void vm_bootstrap_scr(VM *vm);

unsigned vm_get_base(const VM *vm) {
    if (!vm) return 10u;
    size_t idx = (size_t)vm->base_addr;
    if (idx < (size_t)VM_MEMORY_SIZE) {
        cell_t v = vm->memory[idx];
        if (v >= 2 && v <= 36) return (unsigned)v;
    }
    if (vm->base >= 2 && vm->base <= 36) return (unsigned)vm->base;
    return 10u;
}

void vm_set_base(VM *vm, unsigned b) {
    if (!vm) return;
    if (b < 2 || b > 36) b = 10;
    size_t idx = (size_t)vm->base_addr;
    if (idx < (size_t)VM_MEMORY_SIZE) {
        vm->memory[idx] = (cell_t)b;   /* VM-visible variable */
    }
    vm->base = (cell_t)b;              /* host mirror for legacy paths */
}

/* Ensure BASE variable exists in VM memory and is DECIMAL.
   Call this once during VM init/bootstrap (after memory is allocated). */
void vm_bootstrap_base(VM *vm) {
    if (!vm) return;

    /* If base_addr is invalid, allocate one VM cell for BASE at HERE. */
    size_t idx = (size_t)vm->base_addr;
    if (!(idx < (size_t)VM_MEMORY_SIZE)) {
        /* Assume HERE is in cell units. If you have an allocator, use it. */
        idx = (size_t)vm->here;
        if (idx >= (size_t)VM_MEMORY_SIZE) {
            /* Out of memory; pick a safe fallback and mark error if you want */
            idx = 0;
        } else {
            vm->here++;
        }
        vm->base_addr = (vaddr_t)idx;
    }

    /* Initialize both memory cell and mirror to DECIMAL. */
    vm_set_base(vm, 10u);
}

/**
 * @brief Initialize a new Forth virtual machine instance
 *
 * Allocates memory for the VM, initializes stacks, dictionary space and registers
 * the standard Forth-79 word set.
 *
 * @param vm Pointer to VM structure to initialize
 */
void vm_init(VM *vm) {
    /* Clear all fields */
    memset(vm, 0, sizeof(VM));

    /* Allocate unified VM memory buffer */
    vm->memory = malloc(VM_MEMORY_SIZE);
    if (vm->memory == NULL) {
        log_message(LOG_ERROR, "Failed to allocate VM memory");
        vm->error = 1;
        return;
    }

    /* Stacks empty */
    vm->dsp = -1;
    vm->rsp = -1;

    /* Dictionary starts at offset 0 */
    vm->here = 0;
    vm_align(vm);

    /* -------- VM variables (backed by VM memory) -------- */

    /* SCR (screen/block number) */
    {
        void *p = vm_allot(vm, sizeof(cell_t));
        if (!p) { log_message(LOG_ERROR, "vm_init: failed to allot SCR cell"); vm->error = 1; return; }
        vm->scr_addr = (vaddr_t)((uint8_t*)p - vm->memory);
        *(cell_t*)(&vm->memory[vm->scr_addr]) = 0;
    }

    /* STATE (0=interpret, -1=compile) */
    {
        void *p = vm_allot(vm, sizeof(cell_t));
        if (!p) { log_message(LOG_ERROR, "vm_init: failed to allot STATE cell"); vm->error = 1; return; }
        vm->state_addr = (vaddr_t)((uint8_t*)p - vm->memory);
        *(cell_t*)(&vm->memory[vm->state_addr]) = 0;
    }

    /* BASE (numeric radix, default 10) */
    {
        void *p = vm_allot(vm, sizeof(cell_t));
        if (!p) { log_message(LOG_ERROR, "vm_init: failed to allot BASE cell"); vm->error = 1; return; }
        vm->base_addr = (vaddr_t)((uint8_t*)p - vm->memory);
        vm_store_cell(vm, vm->base_addr, (cell_t)10);
    }

    vm_bootstrap_scr(vm);
    /* ---------------------------------------------------- */

    /* Core VM state (host bookkeeping) */
    vm->latest         = NULL;
    vm->mode           = MODE_INTERPRET;
    vm->compiling_word = NULL;

    vm->state_var = 0;   /* legacy host field; not exposed */
    vm->error     = 0;
    vm->halted    = 0;
    vm->base      = 10;  /* legacy host field; parser will use VM cell */

    /* Input */
    vm->input_length = 0;
    vm->input_pos    = 0;

    vm->current_executing_entry = NULL;

    vm_debug_set_current_vm(vm);
    vm_debug_install_signal_handlers();

    log_message(LOG_DEBUG, "VM initialized - memory=%p, here=%zu (dict_blocks=%d)",
                (void*)vm->memory, vm->here, DICTIONARY_BLOCKS);

    register_forth79_words(vm);

    vm->dict_fence_latest = vm->latest;
    vm->dict_fence_here   = vm->here;
}

/**
 * @brief Clean up VM resources
 *
 * Frees all allocated memory associated with the VM.
 *
 * @param vm Pointer to VM structure to clean up
 */
void vm_cleanup(VM *vm) {
    if (vm->memory != NULL) {
        free(vm->memory);
        vm->memory = NULL;
    }
    vm->here = 0;
}

/**
 * @brief Parse next word from input buffer
 *
 * Extracts the next space-delimited word from the VM's input buffer.
 *
 * @param vm Pointer to VM structure
 * @param word Buffer to store parsed word
 * @param max_len Maximum length of word buffer
 * @return Length of parsed word, or 0 if no word found
 */
int vm_parse_word(VM *vm, char *word, size_t max_len) {
    size_t len = 0;
    char ch;

    if (word == NULL || max_len == 0) {
        return 0;
    }

    /* Skip leading whitespace */
    while (vm->input_pos < vm->input_length) {
        ch = vm->input_buffer[vm->input_pos];
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            break;
        }
        vm->input_pos++;
    }

    /* Check if we've reached end of input */
    if (vm->input_pos >= vm->input_length) {
        return 0;
    }

    /* Read word characters until whitespace or end of input */
    while (vm->input_pos < vm->input_length && len < max_len - 1) {
        ch = vm->input_buffer[vm->input_pos];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            break;
        }
        word[len++] = ch;
        vm->input_pos++;
    }

    /* Null terminate */
    word[len] = '\0';

    return (int)len;
}

/**
 * @brief Parse string as number
 *
 * Attempts to parse a string as a decimal number.
 *
 * @param str String to parse
 * @param value Pointer to store parsed value
 * @return 1 if successful, 0 if parsing failed
 */
int vm_parse_number(VM *vm, const char *s, cell_t *out) {
    if (!s || !*s || !out) return 0;

    unsigned base = vm_get_base(vm);

    int neg = 0;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; if (!*s) return 0; }

    unsigned long long acc = 0;
    int any = 0;

    for (const char *p = s; *p; ++p) {
        unsigned d;
        unsigned char c = (unsigned char)*p;

        if (c >= '0' && c <= '9')       d = (unsigned)(c - '0');
        else if (c >= 'A' && c <= 'Z')  d = 10u + (unsigned)(c - 'A');
        else if (c >= 'a' && c <= 'z')  d = 10u + (unsigned)(c - 'a');
        else                            return 0;        /* not a pure number */

        if (d >= base) return 0;                         /* digit outside radix */

        acc = acc * base + d;
        any = 1;
    }

    if (!any) return 0;

    cell_t v = (cell_t)acc;                              /* truncation by design */
    if (neg) v = (cell_t)(-v);

    *out = v;
    return 1;
}

/**
 * @brief Enter compilation mode for a new word
 *
 * Sets up the VM for compiling a new colon definition.
 *
 * @param vm Pointer to VM structure
 * @param name Name of word being defined
 * @param len Length of word name
 */
void vm_enter_compile_mode(VM *vm, const char *name, size_t len) {
    /* Set compile mode */
    vm->mode = MODE_COMPILE;
    vm->state_var = -1;  /* FORTH-79 uses -1 for true/compile mode */

    if (len > WORD_NAME_MAX) len = WORD_NAME_MAX;
    memcpy(vm->current_word_name, name, len);
    vm->current_word_name[len] = '\0';

    /* Create dictionary entry (smudged) for colon word */
    vm->compiling_word = vm_create_word(vm, name, len, execute_colon_word);
    if (vm->compiling_word != NULL) {
        vm->compiling_word->flags |= WORD_SMUDGED;
    }

    /* Align and record DFA = HERE (VM byte offset) for the threaded body */
    vm_align(vm);
    cell_t *df = vm_dictionary_get_data_field(vm->compiling_word);
    if (!df) {
        log_message(LOG_ERROR, ": failed to get DF cell");
        vm->error = 1;
        return;
    }
    *df = (cell_t)(int64_t)((vaddr_t)vm->here);

    log_message(LOG_DEBUG, ": Started definition of '%s' (body at HERE=%zu)",
                vm->current_word_name, vm->here);
}


/**
 * @brief Compile a word reference
 *
 * Compiles a reference to an existing word into the current definition.
 *
 * @param vm Pointer to VM structure
 * @param entry Dictionary entry of word to compile
 */
void vm_compile_word(VM *vm, DictEntry *entry) {
    if (vm->mode != MODE_COMPILE) return;
    if (!entry) { vm->error = 1; return; }

    vm_align(vm);
    cell_t *slot = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (!slot) { vm->error = 1; return; }

    /* Store the dictionary entry pointer in threaded code */
    *slot = (cell_t)(uintptr_t)entry;

    log_message(LOG_DEBUG, "vm_compile_word: Compiled entry ptr for '%.*s'",
                (int)entry->name_len, entry->name);
}

/**
 * @brief Compile a literal value
 *
 * Compiles a literal number into the current definition.
 *
 * @param vm Pointer to VM structure
 * @param value Value to compile as literal
 */
void vm_compile_literal(VM *vm, cell_t value) {
    cell_t *addr;
    DictEntry *lit_entry;

    if (vm->mode != MODE_COMPILE) {
        vm_push(vm, value);
        return;
    }

    /* Find the LIT word */
    lit_entry = vm_find_word(vm, "LIT", 3);
    if (lit_entry == NULL) {
        log_message(LOG_ERROR, "LIT word not found");
        vm->error = 1;
        return;
    }

    /* Compile call to LIT */
    vm_compile_word(vm, lit_entry);

    /* Compile the literal value */
    vm_align(vm);
    addr = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (addr == NULL) {
        return;
    }
    *addr = value;

    log_message(LOG_DEBUG, "vm_compile_literal: Compiled literal %ld", (long)value);
}

/**
 * @brief Compile a function call
 *
 * Compiles a reference to a C function into the current definition.
 *
 * @param vm Pointer to VM structure
 * @param func Function pointer to compile
 */
void vm_compile_call(VM *vm, word_func_t func) {
    DictEntry *entry;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "vm_compile_call called outside compile mode");
        vm->error = 1;
        return;
    }

    /* Find the dictionary entry for this function */
    entry = vm_dictionary_find_by_func(vm, func);
    if (entry == NULL) {
        log_message(LOG_ERROR, "vm_compile_call: Could not find dictionary entry for function");
        vm->error = 1;
        return;
    }

    /* Compile reference to the word */
    vm_compile_word(vm, entry);
}

/**
 * @brief Compile EXIT word
 *
 * Compiles an EXIT instruction into the current definition.
 *
 * @param vm Pointer to VM structure
 */
void vm_compile_exit(VM *vm) {
    DictEntry *exit_entry;

    if (vm->mode != MODE_COMPILE) {
        return;
    }

    /* Find the EXIT word */
    exit_entry = vm_find_word(vm, "EXIT", 4);
    if (exit_entry == NULL) {
        log_message(LOG_ERROR, "EXIT word not found");
        vm->error = 1;
        return;
    }

    /* Compile call to EXIT */
    vm_compile_word(vm, exit_entry);
}

/**
 * @brief Exit compilation mode
 *
 * Finalizes the current word definition and returns to interpretation mode.
 *
 * @param vm Pointer to VM structure
 */
void vm_exit_compile_mode(VM *vm) {
    DictEntry *exit_entry;

    if (vm->compiling_word == NULL) {
        vm->error = 1;
        return;
    }

    /* Find the EXIT word */
    exit_entry = vm_find_word(vm, "EXIT", 4);
    if (exit_entry == NULL) {
        log_message(LOG_ERROR, "EXIT word not found");
        vm->error = 1;
        return;
    }

    /* Compile call to EXIT */
    vm_compile_word(vm, exit_entry);

    /* Clear smudge bit */
    vm->compiling_word->flags &= ~WORD_SMUDGED;

    log_message(LOG_DEBUG, "; Completed definition");

    /* Return to interpret mode */
    vm->mode = MODE_INTERPRET;
    vm->state_var = 0;  /* 0 = interpret mode */

    vm->compiling_word = NULL;
}

/**
 * @brief Execute a colon definition
 *
 * Executes a compiled Forth word (colon definition).
 *
 * @param vm Pointer to VM structure
 */
static void execute_colon_word(VM *vm) {
    if (!vm) { return; }

    DictEntry *entry = vm->current_executing_entry;
    if (!entry) {
        log_message(LOG_ERROR, "execute_colon_word: no current entry");
        vm->error = 1;
        return;
    }

    /* DF cell holds the VM offset of the threaded body start */
    cell_t *df_cell = vm_dictionary_get_data_field(entry);
    if (!df_cell) {
        log_message(LOG_ERROR, "execute_colon_word: no data field");
        vm->error = 1;
        return;
    }
    vaddr_t body_va = (vaddr_t)(uint64_t)(*df_cell);
    cell_t *ip = (cell_t*)vm_ptr(vm, body_va);
    if (!ip) {
        log_message(LOG_ERROR, "execute_colon_word: bad body VA=%ld", (long)body_va);
        vm->error = 1;
        return;
    }

    log_message(LOG_DEBUG, "execute_colon_word: '%.*s' body @ VA=%ld host=%p",
                (int)entry->name_len, entry->name, (long)body_va, (void*)ip);

    /* Inner interpreter: each cell is a DictEntry* compiled by vm_compile_word */
    while (!vm->error) {
        DictEntry *w = (DictEntry*)(uintptr_t)(*ip);
        if (!w) break;                 /* optional 0 terminator support */

        /* Advance IP and save on return stack so words like LIT can peek/adjust */
        ip++;
        vm_rpush(vm, (cell_t)(uintptr_t)ip);

        vm->current_executing_entry = w;
        if (w->func) {
            log_message(LOG_DEBUG, "execute_colon_word: exec '%.*s'",
                        (int)w->name_len, w->name);
            w->func(vm);
        } else {
            log_message(LOG_ERROR, "execute_colon_word: NULL func");
            vm->error = 1;
        }

        /* Resume at return IP */
        ip = (cell_t*)(uintptr_t)vm_rpop(vm);
    }

    /* Done */
}

/**
 * @brief Interpret or compile a word
 *
 * Processes a single word either by executing it immediately or compiling it
 * into the current definition.
 *
 * @param vm Pointer to VM structure
 * @param word_str Word to interpret
 * @param len Length of word string
 */
void vm_interpret_word(VM *vm, const char *word_str, size_t len) {
    /* Use the vocabulary-aware finder (prototype here to avoid header churn) */
    extern DictEntry* vm_vocabulary_find_word(VM *vm, const char *name, size_t nlen);

    log_message(LOG_DEBUG, "INTERPRET: '%.*s' (mode=%s)", (int)len, word_str,
                vm->mode == MODE_COMPILE ? "COMPILE" : "INTERPRET");

    DictEntry *entry = vm_vocabulary_find_word(vm, word_str, len);
    if (entry) {
        if (vm->mode == MODE_COMPILE && !(entry->flags & WORD_IMMEDIATE)) {
            log_message(LOG_DEBUG, "COMPILE: '%.*s'", (int)len, word_str);
            vm_compile_word(vm, entry);
        } else {
            log_message(LOG_DEBUG, "EXECUTE: '%.*s'", (int)len, word_str);
            vm->current_executing_entry = entry;
            if (entry->func) {
                entry->func(vm);
            } else {
                /* Colon word / code field execution */
                execute_colon_word(vm);
            }
        }
        return;
    }

    /* Not found — try as number */
    cell_t value;
    log_message(LOG_DEBUG, "NOT FOUND: '%.*s' — try number", (int)len, word_str);
    if (vm_parse_number(vm, word_str, &value)) {
        log_message(LOG_DEBUG, "NUMBER: '%.*s' = %ld", (int)len, word_str, (long)value);
        if (vm->mode == MODE_COMPILE) {
            vm_compile_literal(vm, value);
        } else {
            vm_push(vm, value);
        }
    } else {
        log_message(LOG_ERROR, "UNKNOWN WORD: '%.*s'", (int)len, word_str);
        vm->error = 1;
    }
}


/**
 * @brief Interpret a string of Forth code
 *
 * Processes a string containing Forth words, executing or compiling them
 * as appropriate.
 *
 * @param vm Pointer to VM structure
 * @param input String containing Forth code to interpret
 */
void vm_interpret(VM *vm, const char *input) {
    size_t len = strlen(input);
    if (len >= INPUT_BUFFER_SIZE) len = INPUT_BUFFER_SIZE - 1;

    memcpy(vm->input_buffer, input, len);
    vm->input_buffer[len] = '\0';
    vm->input_length = len;
    vm->input_pos = 0;

    char word[64];
    size_t word_len;

    while ((word_len = vm_parse_word(vm, word, sizeof(word))) > 0 && !vm->error) {
        vm_interpret_word(vm, word, word_len);
    }
}

/* ===== VM address helpers ================================================= */

int vm_addr_ok(struct VM *vm, vaddr_t addr, size_t len) {
    (void)vm;
    /* Valid if range [addr, addr+len) fits inside VM memory */
    if (len > (size_t)VM_MEMORY_SIZE) return 0;
    return addr <= (vaddr_t)((vaddr_t)VM_MEMORY_SIZE - (vaddr_t)len);
}

uint8_t *vm_ptr(struct VM *vm, vaddr_t addr) {
    /* Caller should vm_addr_ok() first; we still guard for safety */
    if (!vm || !vm->memory) return NULL;
    if (!vm_addr_ok(vm, addr, 1)) return NULL;
    return vm->memory + (size_t)addr;
}

/* ===== Canonical byte/cell accessors ===================================== */
/**
 * @brief Load an 8-bit unsigned value from VM memory
 *
 * @param vm Pointer to VM structure
 * @param addr Virtual address to load from
 * @return Loaded byte value, or 0 if error
 */
uint8_t vm_load_u8(struct VM *vm, vaddr_t addr) {
    uint8_t *p = vm_ptr(vm, addr);
    if (!p) { vm->error = 1; return 0; }
    return *p;
}

/**
 * @brief Store an 8-bit unsigned value to VM memory
 *
 * @param vm Pointer to VM structure
 * @param addr Virtual address to store to
 * @param v Value to store
 */
void vm_store_u8(struct VM *vm, vaddr_t addr, uint8_t v) {
    uint8_t *p = vm_ptr(vm, addr);
    if (!p) { vm->error = 1; return; }
    *p = v;
}

/**
 * @brief Load a cell value from VM memory
 *
 * Loads a naturally-aligned cell from VM memory using memcpy to avoid
 * undefined behavior. Sets error flag if address is invalid or misaligned.
 *
 * @param vm Pointer to VM structure
 * @param addr Virtual address to load from (must be cell-aligned)
 * @return Loaded cell value, or 0 if error
 */
cell_t vm_load_cell(struct VM *vm, vaddr_t addr) {
    /* Cells must be naturally aligned; use memcpy to avoid UB */
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) { vm->error = 1; return 0; }
    if ((addr % (vaddr_t)sizeof(cell_t)) != 0) { vm->error = 1; return 0; }
    cell_t out = 0;
    memcpy(&out, vm->memory + (size_t)addr, sizeof(cell_t));
    return out;
}

/**
 * @brief Store a cell value to VM memory
 *
 * Stores a cell to naturally-aligned VM memory using memcpy to avoid
 * undefined behavior. Sets error flag if address is invalid or misaligned.
 *
 * @param vm Pointer to VM structure
 * @param addr Virtual address to store to (must be cell-aligned)
 * @param v Value to store
 */
void vm_store_cell(struct VM *vm, vaddr_t addr, cell_t v) {
    if (!vm_addr_ok(vm, addr, sizeof(cell_t))) { vm->error = 1; return; }
    if ((addr % (vaddr_t)sizeof(cell_t)) != 0) { vm->error = 1; return; }
    memcpy(vm->memory + (size_t)addr, &v, sizeof(cell_t));
}

void vm_bootstrap_scr(VM *vm) {
    if (!vm) return;

    /* If scr_addr already looks valid, keep it. */
    size_t idx = (size_t)vm->scr_addr;
    if (idx >= (size_t)VM_MEMORY_SIZE) {
        /* Allocate 1 cell at HERE for SCR */
        idx = (size_t)vm->here;
        if (idx >= (size_t)VM_MEMORY_SIZE) {
            vm->error = 1; /* out of memory during bootstrap */
            return;
        }
        vm->here++;
        vm->scr_addr = (vaddr_t)idx;
    }

    /* Initialize SCR cell to 0 directly (no error side-effects). */
    vm->memory[idx] = (cell_t)0;
}
