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

/**
 * @brief Read the current number base from the VM.
 *
 * Prefers the cell stored at @c vm->base_addr (the canonical FORTH
 * @c BASE variable) if it is aligned and in the valid range [2, 36].
 * Falls back to the host-mirror @c vm->base if the cell is out of
 * range, and finally returns 10 if both are invalid or @p vm is NULL.
 *
 * The double-read (cell + mirror) keeps the VM consistent when C code
 * modifies @c vm->base directly without going through @c vm_set_base().
 *
 * @param vm  VM instance to query (may be NULL; returns 10 in that case).
 * @return    Current number base in range [2, 36].
 */
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

/**
 * @brief Set the VM number base and update both the cell and the host mirror.
 *
 * Clamps @p b to [2, 36] (replacing out-of-range values with 10), then
 * writes to the FORTH @c BASE variable cell at @c vm->base_addr and to
 * the host-mirror field @c vm->base so both sources agree.
 *
 * @param vm  VM instance to update.
 * @param b   Desired number base; clamped to [2, 36].
 */
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
 * @brief Parse the next whitespace-delimited token from the input buffer.
 *
 * Skips leading whitespace (space, tab, CR, LF), then extracts characters
 * until the next whitespace or end of buffer, storing up to @p max_len − 1
 * characters in @p word followed by a NUL terminator. Advances
 * @c vm->input_pos past the consumed token.
 *
 * @param vm       VM instance; @c vm->input_buffer / @c input_pos / @c input_length
 *                 must be populated (typically by @c vm_interpret()).
 * @param word     Output buffer for the parsed token.
 * @param max_len  Size of @p word in bytes (including NUL terminator).
 * @return         Number of characters written (excluding NUL), or 0 if
 *                 the buffer is exhausted or @p vm / @p word is NULL.
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
 * @brief Attempt to parse a string as an integer in the current number base.
 *
 * Reads the current base from @c vm_get_base(), then converts each
 * character of @p s (case-insensitive, digits 0–9 and letters A–Z/a–z)
 * to a digit value. Returns 0 immediately on any character that is not
 * a valid digit in the current base, or on an empty string after the
 * optional sign.
 *
 * Handles an optional leading @c '+' or @c '-' sign. The result is a
 * full-width @c cell_t (sign extended for negative values).
 *
 * @param vm  VM instance (used for base only; may be NULL, in which case
 *            base defaults to 10).
 * @param s   NUL-terminated string to parse.
 * @param out Pointer to receive the parsed value on success.
 * @return    1 on successful parse, 0 on any parse failure.
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

/**
 * @brief Begin a colon definition and switch the VM to compile mode.
 *
 * Creates a new dictionary entry for @p name via @c vm_create_word(),
 * marks it @c WORD_SMUDGED so it is invisible to lookups during
 * compilation, sets the VM mode to @c MODE_COMPILE, and writes the
 * current @c HERE address into the entry's data field as the start of
 * the threaded body.
 *
 * The SMUDGE flag is cleared by @c vm_exit_compile_mode() when the
 * semicolon is encountered.
 *
 * @param vm   VM instance.
 * @param name Word name string (does not need to be NUL-terminated).
 * @param len  Length of @p name; clamped to @c WORD_NAME_MAX.
 */
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

/**
 * @brief Append a word reference (DictEntry*) to the current threaded body.
 *
 * Aligns @c HERE, then allots one @c cell_t slot and stores the
 * @c DictEntry* as a cell. This is the fundamental compilation step used
 * by the inner interpreter: each slot in the threaded body is a pointer
 * to the @c DictEntry of the word to call.
 *
 * Sets @c vm->error if the VM is not in compile mode, @p entry is NULL,
 * or allotment fails.
 *
 * @param vm     VM instance in compile mode.
 * @param entry  Dictionary entry of the word to compile; must not be NULL.
 */
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

/**
 * @brief Compile a literal value into the current threaded body (or push it).
 *
 * In compile mode, appends the @c LIT word reference followed by the
 * literal value so the inner interpreter will push @p value at runtime.
 * In interpret mode, pushes @p value directly onto the data stack.
 *
 * Sets @c vm->error if @c LIT cannot be found in the dictionary.
 *
 * @param vm     VM instance.
 * @param value  Cell value to compile or push.
 */
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

/**
 * @brief Compile a call to a word identified by its C function pointer.
 *
 * Looks up the @c DictEntry whose @c func field matches @p func via
 * @c vm_dictionary_find_by_func(), then delegates to @c vm_compile_word().
 * Used by generating words (e.g., @c DOES>) that need to embed a specific
 * runtime routine into a compiled definition without knowing its name.
 *
 * Sets @c vm->error if not in compile mode or if no matching entry is found.
 *
 * @param vm    VM instance in compile mode.
 * @param func  C function pointer identifying the word to compile.
 */
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

/**
 * @brief Compile an @c EXIT reference into the current threaded body.
 *
 * Appends the @c EXIT word entry so that when the threaded body is
 * executed, @c execute_colon_word() sets @c vm->exit_colon and unwinds
 * the inner loop. Used by control-flow generating words (e.g., @c IF
 * optimisation paths) that need an explicit early-return.
 *
 * Sets @c vm->error if not in compile mode or if @c EXIT is not found.
 *
 * @param vm  VM instance in compile mode.
 */
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

/**
 * @brief Finalise a colon definition and return to interpret mode.
 *
 * Appends a final @c EXIT reference, clears the @c WORD_SMUDGED flag so
 * the new word becomes visible in the dictionary, and sets @c WORD_COMPILED.
 * Also computes the word's mass (header + body bytes) for the physics
 * engine and calls @c physics_metadata_refresh_state().
 *
 * Resets @c vm->mode to @c MODE_INTERPRET, clears @c vm->state_var,
 * and NULLs @c vm->compiling_word.
 *
 * Sets @c vm->error if @p vm has no active @c compiling_word or if
 * @c EXIT cannot be found.
 *
 * @param vm  VM instance currently in compile mode.
 */
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

    /* Inherit creator's ACL from session context:
     * zuse-compiled words bypass ACL-RECHECK on first execution (ACL_TTL_OPEN).
     * Non-zuse words keep ttl=0, triggering ACL-RECHECK on first execution. */
    if (vm->zuse_session) {
        vm->compiling_word->acl_ttl   = ACL_TTL_OPEN;
        vm->compiling_word->acl_allow = 1;
    }

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

/**
 * @brief Cold-path ACL enforcement called when a word's TTL counter hits zero.
 *
 * Looks up the FORTH word @c ACL-RECHECK in the dictionary. If found,
 * calls it with @c emergency_console temporarily set to 1 so the recheck
 * itself cannot be denied by the ACL system. The FORTH word is expected
 * to update @c entry->acl_allow and @c entry->acl_ttl before returning.
 *
 * If @c ACL-RECHECK is not yet loaded (early boot before @c ACL.4th runs),
 * the function **fails open**: sets @c acl_allow=1 and @c acl_ttl=ACL_TTL_OPEN
 * so execution continues unimpeded. The same fail-open behaviour applies
 * if @c ACL-RECHECK itself raises an error (error flag is cleared).
 *
 * This function is the cold path; the hot path in @c execute_colon_word()
 * and @c vm_interpret_word() simply decrements @c acl_ttl.
 *
 * @param vm     VM instance.
 * @param entry  Dictionary entry whose TTL has expired and needs rechecking.
 */
static void acl_recheck(VM *vm, DictEntry *entry)
{
    DictEntry *recheck = vm_find_word(vm, "ACL-RECHECK", 11);
    if (!recheck || !recheck->func) {
        /* ACL.4th not yet loaded — fail open */
        entry->acl_allow = 1;
        entry->acl_ttl   = ACL_TTL_OPEN;
        return;
    }

    /* Push XT, call ACL-RECHECK with emergency bypass so it cannot deny itself */
    uint8_t saved_ec = vm->emergency_console;
    vm->emergency_console = 1;
    vm_push(vm, (cell_t)(uintptr_t)entry);
    DictEntry *saved_ce = vm->current_executing_entry;
    vm->current_executing_entry = recheck;
    recheck->func(vm);
    vm->current_executing_entry = saved_ce;
    vm->emergency_console = saved_ec;

    /* If ACL-RECHECK itself errored, fail open and clear the error */
    if (vm->error) {
        vm->error = 0;
        entry->acl_allow = 1;
        entry->acl_ttl   = ACL_TTL_OPEN;
        log_message(LOG_WARN, "acl_recheck: ACL-RECHECK errored for '%.*s'; failing open",
                    (int)entry->name_len, entry->name);
    }
}

/**
 * @brief Inner interpreter for threaded colon definitions.
 *
 * Runs the body of a colon-defined word whose @c DictEntry points to a
 * sequence of @c DictEntry* cells in VM memory (indirect threaded code).
 * The interpreter pointer (IP) is maintained on the return stack so that
 * runtime control-flow words (@c (BRANCH), @c (0BRANCH), @c (DO), etc.)
 * can redirect execution by modifying the top-of-return-stack IP.
 *
 * **Execution loop per iteration:**
 *  1. Read @c DictEntry* @c w from @c *ip.
 *  2. Run physics pre-execute hooks (heat, decay, pipelining).
 *  3. ACL hot-path check (decrement TTL; call @c acl_recheck() on zero).
 *  4. Push @c ip+1 onto the return stack as the resume point.
 *  5. Call @c w->func(vm).
 *  6. Run physics post-execute hooks.
 *  7. Check @c vm->exit_colon (set by @c EXIT): pop and discard the saved
 *     IP, then return to caller.
 *  8. Check @c vm->abort_requested: unwind and return immediately.
 *  9. Pop the (possibly modified) IP from the return stack and repeat.
 *
 * @c vm->ecw_nesting is incremented on entry and decremented on every
 * return path to enable recursive colon calls.
 *
 * @param vm  VM instance; @c vm->current_executing_entry must point to
 *            the @c DictEntry being executed.
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

    vm->ecw_nesting++;

    for (;;)
    {
        DictEntry* w = (DictEntry*)(uintptr_t)(*ip);

        /* Physics: pre-execution hooks (decay, heat, pipelining) */
        physics_pre_execute(vm, w, prev);
        profiler_word_count(w);

        /* ACL two-level check (hot path: decrement TTL; cold path: recheck) */
        if (w && !vm->emergency_console) {
            if (w->acl_ttl == 0)
                acl_recheck(vm, w);
            else
                w->acl_ttl--;
            if (!w->acl_allow) {
                log_message(LOG_WARN, "ACL: denied '%.*s'", (int)w->name_len, w->name);
                vm->dsp = -1;
                vm->rsp = -1;
                vm->error = 1;
                return;
            }
        }

        /* Advance IP and save resume point */
        ip++;
        vm_rpush(vm, (cell_t)(uintptr_t)ip);
        if (vm->error) { vm->ecw_nesting--; return; }

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
            const char *pname = (entry && entry->name_len > 0) ? entry->name : "?";
            if (!w)
                log_message(LOG_ERROR, "execute_colon_word: NULL DictEntry* in '%s' (ip-1=%p)",
                            pname, (void *)(ip - 1));
            else
                log_message(LOG_ERROR, "execute_colon_word: null func for '%s' in '%s' (ip-1=%p)",
                            w->name, pname, (void *)(ip - 1));
            vm->error = 1;
        }
        vm->current_executing_entry = entry;
        prev = w;

        if (vm->error) { vm->ecw_nesting--; return; }

        /* ABORT clears stacks and returns immediately */
        if (vm->abort_requested) { vm->abort_requested = 0; vm->ecw_nesting--; return; }

        /* EXIT discards saved IP and returns */
        if (vm->exit_colon)
        {
            vm->exit_colon = 0;
            (void)vm_rpop(vm);
            vm->ecw_nesting--;
            return;
        }

        /* Resume at (possibly modified) IP from return stack */
        ip = (cell_t*)(uintptr_t)vm_rpop(vm);
        if (vm->error) { vm->ecw_nesting--; return; }
    }
}


/* ====================== Outer interpreter ======================= */

/**
 * @brief Interpret or compile a single word token.
 *
 * This is the outer interpreter's per-token dispatch:
 *
 *  1. **Dictionary lookup**: vocabulary-aware search, falling back to the
 *     canonical flat dictionary. Physics heat is recorded via
 *     @c physics_on_lookup().
 *  2. **Compile mode, non-immediate**: append the entry to the current
 *     threaded body via @c vm_compile_word().
 *  3. **Interpret mode, or immediate in compile mode**: ACL check (hot-path
 *     TTL decrement / cold-path @c acl_recheck()), then call @c entry->func().
 *  4. **Number literal**: if no dictionary match, attempt @c vm_parse_number().
 *     In compile mode, compile as a literal; in interpret mode, push directly.
 *  5. **Error**: unknown token — sets @c vm->error.
 *
 * @param vm        VM instance.
 * @param word_str  Token string (need not be NUL-terminated).
 * @param len       Length of @p word_str in bytes.
 */
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

        /* ACL check on outer-interpreter path */
        if (!vm->emergency_console) {
            if (entry->acl_ttl == 0)
                acl_recheck(vm, entry);
            else
                entry->acl_ttl--;
            if (!entry->acl_allow) {
                log_message(LOG_WARN, "ACL: denied '%.*s'", (int)len, word_str);
                vm->dsp = -1;
                vm->rsp = -1;
                vm->error = 1;
                vm->current_executing_entry = NULL;
                return;
            }
        }

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
 * @brief Interpret a NUL-terminated string of FORTH source code.
 *
 * Copies @p input into @c vm->input_buffer (capped at
 * @c INPUT_BUFFER_SIZE − 1 bytes, NUL-terminated), then drives the
 * word-parse / token-dispatch loop until the buffer is exhausted or
 * @c vm->error is set. Each token is dispatched to @c vm_interpret_word().
 *
 * This is the top-level entry point used by the REPL, @c INCLUDE, and
 * inline @c -c execution. It does not reset @c vm->error before running —
 * the caller is responsible for clearing error state if desired.
 *
 * @param vm     VM instance.
 * @param input  NUL-terminated FORTH source string to interpret.
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
 * @brief Validate that a VM address range lies within @c vm->memory.
 *
 * Checks that [addr, addr+len) is fully contained within the
 * @c VM_MEMORY_SIZE-byte flat address space. The check is done with
 * unsigned arithmetic to avoid UB on overflow: if @p len alone exceeds
 * @c VM_MEMORY_SIZE the range is immediately rejected.
 *
 * @param vm    VM instance; returns 0 if NULL or memory not allocated.
 * @param addr  Start of the range (inclusive).
 * @param len   Number of bytes in the range.
 * @return      1 if the full range [addr, addr+len) is in bounds; 0 otherwise.
 */
int vm_addr_ok(struct VM* vm, vaddr_t addr, size_t len)
{
    if (!vm || !vm->memory) return 0;
    if (len > VM_MEMORY_SIZE) return 0;
    return addr <= (vaddr_t)(VM_MEMORY_SIZE - len);
}

/**
 * @brief Return a host C pointer into VM memory at @p addr.
 *
 * Validates the address with @c vm_addr_ok() (minimum 1-byte range) and
 * returns @c NULL if the address is out of bounds or @p vm is uninitialised.
 * The returned pointer is valid for at least one byte; callers that need
 * multi-byte access should perform their own range check first.
 *
 * @param vm    VM instance.
 * @param addr  VM byte offset to dereference.
 * @return      Host pointer into @c vm->memory, or NULL on bounds failure.
 */
uint8_t* vm_ptr(struct VM* vm, vaddr_t addr)
{
    if (!vm || !vm->memory) return NULL;
    if (!vm_addr_ok(vm, addr, 1)) return NULL;
    return vm->memory + (size_t)addr;
}

/**
 * @brief Load an unsigned byte from VM memory.
 *
 * Resolves @p addr via @c vm_ptr(). Sets @c vm->error and returns 0 if
 * the address is out of bounds.
 *
 * @param vm    VM instance.
 * @param addr  VM byte offset to read.
 * @return      Byte value at @p addr, or 0 on error.
 */
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

/**
 * @brief Store an unsigned byte to VM memory.
 *
 * Resolves @p addr via @c vm_ptr(). Sets @c vm->error if the address
 * is out of bounds; otherwise writes @p v.
 *
 * @param vm    VM instance.
 * @param addr  VM byte offset to write.
 * @param v     Byte value to store.
 */
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

/**
 * @brief Load an aligned cell from VM memory.
 *
 * Validates that @p addr is within bounds for a full @c cell_t read and
 * that it is naturally aligned (addr % sizeof(cell_t) == 0). Uses
 * @c memcpy() rather than a direct pointer dereference to avoid strict
 * aliasing and alignment UB. Sets @c vm->error and returns 0 on failure.
 *
 * @param vm    VM instance.
 * @param addr  Aligned VM byte offset to read.
 * @return      Cell value at @p addr, or 0 on alignment or bounds error.
 */
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

/**
 * @brief Store an aligned cell to VM memory.
 *
 * Validates alignment and bounds, then copies @p v into VM memory via
 * @c memcpy(). Sets @c vm->error on alignment or bounds failure.
 *
 * @param vm    VM instance.
 * @param addr  Aligned VM byte offset to write.
 * @param v     Cell value to store.
 */
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

/**
 * @brief Mark the most recently created word as IMMEDIATE (FORTH-79).
 *
 * Sets @c WORD_IMMEDIATE on @c vm->latest so the word executes during
 * compilation rather than being compiled into the current definition.
 * Also calls @c physics_metadata_refresh_state() to update the physics
 * engine's record of the entry's flags.
 *
 * Sets @c vm->error if @p vm has no @c latest entry (i.e., the dictionary
 * is empty or no word has been created yet).
 *
 * @param vm  VM instance.
 */
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
