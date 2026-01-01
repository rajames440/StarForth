/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

/*
 * vm.c (vm_core) - The VM is: state, invariants, execution spine
 *
 * Contains:
 *   - instruction dispatch loop
 *   - stack ops
 *   - dictionary lookup
 *   - word execution
 *   - VM flags / modes
 *   - anything that mutates VM state directly
 *
 * Rule: If it touches the stacks, it lives here.
 */

#include "vm_internal.h"
#include "../include/log.h"
#include "../include/profiler.h"
#include "../include/physics_metadata.h"
#include "../include/physics_execution_hooks.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ====================== Base helpers ======================= */

unsigned vm_get_base(const VM* vm)
{
    if (!vm) return 10u;
    /* Prefer VM cell if valid */
    vaddr_t a = vm->base_addr;
    if ((a % sizeof(cell_t)) == 0 && (size_t)a + sizeof(cell_t) <= VM_MEMORY_SIZE)
    {
        cell_t v = vm_load_cell((VM*)vm, a); /* cast-away const for accessor */
        if (v >= 2 && v <= 36) return (unsigned)v;
    }
    /* Fallback to host mirror */
    if (vm->base >= 2 && vm->base <= 36) return (unsigned)vm->base;
    return 10u;
}

void vm_set_base(VM* vm, unsigned b)
{
    if (!vm) return;
    if (b < 2 || b > 36) b = 10;
    vm_store_cell(vm, vm->base_addr, (cell_t)b);
    vm->base = (cell_t)b; /* host mirror */
}

/* vm_init and vm_cleanup moved to vm_bootstrap.c */
/* vm_tick* and heartbeat functions moved to vm_time.c */

/* ====================== Parser / number ======================= */

/**
 * @brief Parse next word from input buffer
 *
 * Skips leading whitespace and extracts next word delimited by whitespace.
 *
 * @param vm Pointer to VM instance
 * @param word Buffer to store parsed word
 * @param max_len Maximum length of word buffer
 * @return Length of parsed word or 0 if no word found
 */
int vm_parse_word(VM* vm, char* word, size_t max_len)
{
    if (!vm || !word || max_len == 0) return 0;

    /* Skip whitespace */
    while (vm->input_pos < vm->input_length)
    {
        char c = vm->input_buffer[vm->input_pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
        vm->input_pos++;
    }
    if (vm->input_pos >= vm->input_length) return 0;

    size_t len = 0;
    while (vm->input_pos < vm->input_length && len < max_len - 1)
    {
        char c = vm->input_buffer[vm->input_pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        word[len++] = c;
        vm->input_pos++;
    }
    word[len] = '\0';
    return (int)len;
}

/**
 * @brief Parse string as number in current base
 *
 * Attempts to parse string as number using VM's current number base.
 * Handles optional sign prefix.
 *
 * @param vm Pointer to VM instance 
 * @param s String to parse
 * @param out Pointer to store parsed value
 * @return 1 on success, 0 on parse failure
 */
int vm_parse_number(VM* vm, const char* s, cell_t* out)
{
    if (!s || !*s || !out) return 0;

    unsigned base = vm_get_base(vm);
    int neg = 0;
    if (*s == '+' || *s == '-')
    {
        neg = (*s == '-');
        s++;
        if (!*s) return 0;
    }

    unsigned long long acc = 0;
    int any = 0;
    for (const char* p = s; *p; ++p)
    {
        unsigned d;
        unsigned char c = (unsigned char)*p;
        if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
        else if (c >= 'A' && c <= 'Z') d = 10u + (unsigned)(c - 'A');
        else if (c >= 'a' && c <= 'z') d = 10u + (unsigned)(c - 'a');
        else return 0;
        if (d >= base) return 0;
        acc = acc * base + d;
        any = 1;
    }
    if (!any) return 0;
    cell_t v = (cell_t)acc;
    if (neg) v = (cell_t)(-v);
    *out = v;
    return 1;
}

/* ====================== Compile state ======================= */

void vm_enter_compile_mode(VM* vm, const char* name, size_t len)
{
    if (!vm) return;
    vm->mode = MODE_COMPILE;
    vm->state_var = -1;
    vm_store_cell(vm, vm->state_addr, vm->state_var);

    if (len > WORD_NAME_MAX) len = WORD_NAME_MAX;
    memcpy(vm->current_word_name, name, len);
    vm->current_word_name[len] = '\0';

    /* Create colon word header with code pointer = execute_colon_word */
    DictEntry* de = vm_create_word(vm, name, len, execute_colon_word);
    vm->compiling_word = de;
    if (!de)
    {
        vm->error = 1;
        return;
    }
    de->flags |= WORD_SMUDGED;

    /* DF (first data cell) will hold the VM-relative address of threaded body */
    vm_align(vm);
    cell_t* df = vm_dictionary_get_data_field(de);
    if (!df)
    {
        vm->error = 1;
        return;
    }
    *df = (cell_t)(int64_t)((vaddr_t)vm->here);

    log_message(LOG_DEBUG, ": started '%s' at HERE=%zu", vm->current_word_name, vm->here);
}

void vm_compile_word(VM* vm, DictEntry* entry)
{
    if (!vm || vm->mode != MODE_COMPILE) return;
    if (!entry)
    {
        vm->error = 1;
        return;
    }
    vm_align(vm);
    cell_t* slot = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (!slot)
    {
        vm->error = 1;
        return;
    }
    *slot = (cell_t)(uintptr_t)
    entry; /* threaded code stores DictEntry* as cell */
}

void vm_compile_literal(VM* vm, cell_t value)
{
    if (!vm) return;
    if (vm->mode != MODE_COMPILE)
    {
        vm_push(vm, value);
        return;
    }

    DictEntry* LIT = vm_find_word(vm, "LIT", 3);
    if (!LIT)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "LIT not found");
        return;
    }
    vm_compile_word(vm, LIT);

    vm_align(vm);
    cell_t* val = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (!val)
    {
        vm->error = 1;
        return;
    }
    *val = value;
}

void vm_compile_call(VM* vm, word_func_t func)
{
    if (!vm || vm->mode != MODE_COMPILE)
    {
        vm->error = 1;
        return;
    }
    DictEntry* entry = vm_dictionary_find_by_func(vm, func);
    if (!entry)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "vm_compile_call: entry not found");
        return;
    }
    vm_compile_word(vm, entry);
}

void vm_compile_exit(VM* vm)
{
    if (!vm || vm->mode != MODE_COMPILE) return;
    DictEntry* EXIT = vm_find_word(vm, "EXIT", 4);
    if (!EXIT)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "EXIT not found");
        return;
    }
    vm_compile_word(vm, EXIT);
}

void vm_exit_compile_mode(VM* vm)
{
    if (!vm || !vm->compiling_word)
    {
        vm->error = 1;
        return;
    }

    DictEntry* EXIT = vm_find_word(vm, "EXIT", 4);
    if (!EXIT)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "EXIT not found");
        return;
    }
    vm_compile_word(vm, EXIT);

    vm->compiling_word->flags &= ~WORD_SMUDGED;
    vm->compiling_word->flags |= WORD_COMPILED;

    cell_t* df = vm_dictionary_get_data_field(vm->compiling_word);
    if (df)
    {
        uint64_t header_bytes = (uint64_t)(((uint8_t*)df + sizeof(cell_t)) - (uint8_t*)vm->compiling_word);
        uint64_t body_start = (uint64_t)(vaddr_t)(uint64_t)(*df);
        uint64_t here_bytes = (uint64_t)vm->here;
        uint64_t body_bytes = (here_bytes >= body_start) ? (here_bytes - body_start) : 0;
        uint64_t total = header_bytes + body_bytes;
        uint32_t mass = (total > UINT32_MAX) ? UINT32_MAX : (uint32_t)total;
        physics_metadata_set_mass(vm->compiling_word, mass);
    }
    physics_metadata_refresh_state(vm->compiling_word);

    vm->mode = MODE_INTERPRET;
    vm->state_var = 0;
    vm_store_cell(vm, vm->state_addr, vm->state_var);
    vm->compiling_word = NULL;

    log_message(LOG_DEBUG, "; end definition");
}

/* ====================== Inner interpreter ======================= */
/*
   Threaded code layout (compiled by vm_compile_word / vm_compile_literal):

   DF cell (in DictEntry) holds a VM address (vaddr_t) of the first code cell.
   Each code cell is a cell_t that encodes a DictEntry* (for a word to call),
   or is a literal payload following a compiled LIT word.

   Control-flow runtime words (e.g., (BRANCH), (0BRANCH), (DO), loops) are
   responsible for *modifying the IP stored at the top of the return stack*.
   The inner interpreter saves the "next ip" on the return stack before
   calling the word; after the word returns, we pop the possibly-modified IP
   and continue. This matches your runtime branch helpers' contract.

   IMPORTANT: EXIT behavior —
     Words implement EXIT by setting vm->exit_colon = 1 (one-shot).
     We honor that flag here to unwind the *current* colon only,
     without disturbing the caller’s R-stack frame.
*/

/*
 * execute_colon_word - Inner interpreter for threaded code
 *
 * Clean execution loop. Physics is abstracted behind hooks.
 */
void execute_colon_word(VM* vm)
{
    if (!vm || !vm->current_executing_entry) return;

    DictEntry* entry = vm->current_executing_entry;
    cell_t* df = vm_dictionary_get_data_field(entry);
    if (!df) { vm->error = 1; return; }

    vaddr_t body_addr = (vaddr_t)(uint64_t)(*df);
    cell_t* ip = (cell_t*)vm_ptr(vm, body_addr);
    if (!ip) { vm->error = 1; return; }

    DictEntry* prev = NULL;

    for (;;)
    {
        DictEntry* w = (DictEntry*)(uintptr_t)(*ip);

        /* Physics: pre-execution hooks (decay, heat, pipelining) */
        physics_pre_execute(vm, w, prev);
        profiler_word_count(w);

        /* Advance IP and save resume point */
        ip++;
        vm_rpush(vm, (cell_t)(uintptr_t)ip);
        if (vm->error) return;

        /* Execute the word */
        vm->current_executing_entry = w;
        if (w && w->func)
        {
            profiler_word_enter(w);
            w->func(vm);
            physics_post_execute(vm, w);
        }
        else
        {
            log_message(LOG_ERROR, "execute_colon_word: null word func");
            vm->error = 1;
        }
        vm->current_executing_entry = entry;
        prev = w;

        if (vm->error) return;

        /* ABORT clears stacks and returns immediately */
        if (vm->abort_requested) { vm->abort_requested = 0; return; }

        /* EXIT discards saved IP and returns */
        if (vm->exit_colon)
        {
            vm->exit_colon = 0;
            (void)vm_rpop(vm);
            return;
        }

        /* Resume at (possibly modified) IP from return stack */
        ip = (cell_t*)(uintptr_t)vm_rpop(vm);
        if (vm->error) return;
    }
}


/* ====================== Outer interpreter ======================= */

void vm_interpret_word(VM* vm, const char* word_str, size_t len)
{
    if (!vm || !word_str) return;

    log_message(LOG_DEBUG, "INTERPRET: '%.*s' (mode=%s)",
                (int)len, word_str,
                vm->mode == MODE_COMPILE ? "COMPILE" : "INTERPRET");

    /* Vocabulary-aware lookup with fallback to canonical dictionary */
    extern DictEntry* vm_vocabulary_find_word(VM* vm, const char* name, size_t nlen);
    DictEntry* entry = vm_vocabulary_find_word(vm, word_str, len);
    DictEntry* canon = vm_find_word(vm, word_str, len);
    if (!entry) entry = canon;

    if (entry)
    {
        /* Physics: track lookup (heat, decay) */
        physics_on_lookup(vm, entry, canon);

        int is_immediate = ((entry->flags & WORD_IMMEDIATE) ||
                           (canon && (canon->flags & WORD_IMMEDIATE)));

        if (vm->mode == MODE_COMPILE && !is_immediate)
        {
            log_message(LOG_DEBUG, "COMPILE: '%.*s'", (int)len, word_str);
            vm_compile_word(vm, entry);
            return;
        }

        log_message(LOG_DEBUG, "EXECUTE: '%.*s'", (int)len, word_str);
        vm->current_executing_entry = entry;
        profiler_word_count(entry);

        if (entry->func)
        {
            profiler_word_enter(entry);
            entry->func(vm);
            physics_post_execute(vm, entry);
        }
        else
        {
            log_message(LOG_ERROR, "NULL func for '%.*s'", (int)len, word_str);
            vm->error = 1;
        }
        vm->current_executing_entry = NULL;
        return;
    }

    /* Try parsing as number */
    cell_t value;
    if (vm_parse_number(vm, word_str, &value))
    {
        log_message(LOG_DEBUG, "NUMBER: '%.*s' = %ld", (int)len, word_str, (long)value);
        if (vm->mode == MODE_COMPILE)
            vm_compile_literal(vm, value);
        else
            vm_push(vm, value);
        return;
    }

    log_message(LOG_ERROR, "UNKNOWN WORD: '%.*s'", (int)len, word_str);
    vm->error = 1;
}

/**
 * @brief Interpret a string of Forth code
 *
 * Main interpretation loop that:
 * - Loads input into VM buffer
 * - Parses words
 * - Executes or compiles each word
 * - Handles numbers
 *
 * @param vm Pointer to VM instance
 * @param input String containing Forth code to interpret
 */
void vm_interpret(VM* vm, const char* input)
{
    if (!vm || !input) return;

    /* Load into input buffer (cap + NUL) */
    size_t n = 0, cap = INPUT_BUFFER_SIZE ? INPUT_BUFFER_SIZE - 1 : 0;
    while (n < cap)
    {
        char c = input[n];
        vm->input_buffer[n] = c;
        if (c == '\0') break;
        ++n;
    }
    if (n == cap) vm->input_buffer[n] = '\0';

    vm->input_length = n;
    vm->input_pos = 0;

    char word[64];
    size_t wlen;
    while (!vm->error && (wlen = (size_t)vm_parse_word(vm, word, sizeof(word))) > 0)
    {
        vm_interpret_word(vm, word, wlen);
    }
}

/* ====================== VM memory helpers ======================= */

/**
 * @brief Check if memory address range is valid
 *
 * Validates that an address range falls within VM memory bounds.
 *
 * @param vm Pointer to VM instance
 * @param addr Virtual address to check
 * @param len Length of memory range
 * @return 1 if range is valid, 0 if invalid
 */
int vm_addr_ok(struct VM* vm, vaddr_t addr, size_t len)
{
    if (!vm || !vm->memory) return 0;
    if (len > VM_MEMORY_SIZE) return 0;
    return addr <= (vaddr_t)(VM_MEMORY_SIZE - len);
}

uint8_t* vm_ptr(struct VM* vm, vaddr_t addr)
{
    if (!vm || !vm->memory) return NULL;
    if (!vm_addr_ok(vm, addr, 1)) return NULL;
    return vm->memory + (size_t)addr;
}

uint8_t vm_load_u8(struct VM* vm, vaddr_t addr)
{
    uint8_t* p = vm_ptr(vm, addr);
    if (!p)
    {
        vm->error = 1;
        return 0;
    }
    return *p;
}

void vm_store_u8(struct VM* vm, vaddr_t addr, uint8_t v)
{
    uint8_t* p = vm_ptr(vm, addr);
    if (!p)
    {
        vm->error = 1;
        return;
    }
    *p = v;
}

cell_t vm_load_cell(struct VM* vm, vaddr_t addr)
{
    if (!vm_addr_ok(vm, addr, sizeof(cell_t)) || (addr % sizeof(cell_t)) != 0)
    {
        vm->error = 1;
        return 0;
    }
    cell_t out = 0;
    memcpy(&out, vm->memory + (size_t)addr, sizeof(cell_t));
    return out;
}

void vm_store_cell(struct VM* vm, vaddr_t addr, cell_t v)
{
    if (!vm_addr_ok(vm, addr, sizeof(cell_t)) || (addr % sizeof(cell_t)) != 0)
    {
        vm->error = 1;
        return;
    }
    memcpy(vm->memory + (size_t)addr, &v, sizeof(cell_t));
}

/* vm_bootstrap_scr moved to vm_bootstrap.c */

/* Make the most recently created word immediate (FORTH-79) */
void vm_make_immediate(VM* vm)
{
    if (!vm) return;
    if (!vm->latest)
    {
        log_message(LOG_ERROR, "vm_make_immediate: no latest word to mark IMMEDIATE");
        vm->error = 1;
        return;
    }
    vm->latest->flags |= WORD_IMMEDIATE;
    physics_metadata_refresh_state(vm->latest);
    log_message(LOG_DEBUG, "IMMEDIATE: '%.*s'",
                (int)vm->latest->name_len, vm->latest->name);
}
