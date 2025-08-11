/*

                                 ***   StarForth   ***
  defining_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/11/25, 5:15 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/*

                                 ***   StarForth   ***
  defining_words.c - FORTH-79 Standard and ANSI C99 ONLY
  Last modified - 8/11/25
  (c) 2025 Robert A. James — CC0 1.0

*/

#include "include/defining_words.h"
#include "../../include/vm.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

#include <string.h>
#include <stdlib.h>   /* malloc/free */

/* ───── Forward declarations so registration sees these symbols ───── */
static void defining_runtime_create(VM *vm);
static void defining_word_colon(VM *vm);
static void defining_word_semicolon(VM *vm);

/* ───────────────────────────── Runtimes ───────────────────────────── */

/* CONSTANT runtime: ( -- n )  Push stored value from header DF cell */
static void defining_runtime_constant(VM *vm) {
    if (!vm) return;

    DictEntry *entry = vm->current_executing_entry;
    if (!entry) {
        log_message(LOG_ERROR, "CONSTANT runtime: no current entry");
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        log_message(LOG_ERROR, "CONSTANT runtime: no data field");
        vm->error = 1;
        return;
    }

    vm_push(vm, *df);
    log_message(LOG_DEBUG, "CONSTANT runtime: pushed %ld for %.*s",
                (long)*df, entry->name_len, entry->name);
}

/* VARIABLE runtime: ( -- addr )  Push VM offset stored in header DF cell */
static void defining_runtime_variable(VM *vm) {
    if (!vm) return;

    DictEntry *entry = vm->current_executing_entry;
    if (!entry) {
        log_message(LOG_ERROR, "VARIABLE runtime: no current entry");
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        log_message(LOG_ERROR, "VARIABLE runtime: no data field");
        vm->error = 1;
        return;
    }

    vm_push(vm, *df);
    log_message(LOG_DEBUG, "VARIABLE runtime: pushed VM addr %ld for %.*s",
                (long)*df, entry->name_len, entry->name);
}

/* CREATE runtime: ( -- addr )  Push DFA (VM byte offset) captured at CREATE */
static void defining_runtime_create(VM *vm) {
    if (!vm) return;

    DictEntry *entry = vm->current_executing_entry;
    if (!entry) {
        log_message(LOG_ERROR, "CREATE runtime: no current entry");
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        log_message(LOG_ERROR, "CREATE runtime: no data field");
        vm->error = 1;
        return;
    }

    vaddr_t dfa = (vaddr_t)(uint64_t)(*df);
    if (!vm_addr_ok(vm, dfa, sizeof(cell_t))) {
        log_message(LOG_ERROR, "CREATE runtime: bad DFA=%ld for %.*s",
                    (long)dfa, entry->name_len, entry->name);
        vm->error = 1;
        return;
    }

    vm_push(vm, CELL(dfa));
    log_message(LOG_DEBUG, "CREATE runtime: pushed DFA %ld for %.*s",
                (long)dfa, entry->name_len, entry->name);
}

/* ───────────────────────────── Defining words ─────────────────────── */

/* CREATE ( "name" -- )  Forth-79: does NOT allocate data. Captures cell-aligned HERE as DFA. */
void defining_word_create(VM *vm) {
    if (!vm) return;

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        log_message(LOG_ERROR, "CREATE: missing name");
        vm->error = 1;
        return;
    }

    DictEntry *entry = vm_create_word(vm, namebuf, (size_t)nlen, defining_runtime_create);
    if (!entry) {
        log_message(LOG_ERROR, "CREATE: vm_create_word failed");
        vm->error = 1;
        return;
    }

    /* Align HERE for cell data, then record DFA */
    vm_align(vm);
    vaddr_t dfa = (vaddr_t)vm->here;

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        log_message(LOG_ERROR, "CREATE: no data field in header");
        vm->error = 1;
        return;
    }
    *df = (cell_t)(int64_t)dfa;

    log_message(LOG_DEBUG, "CREATE: Created '%.*s' with DFA=%ld (HERE=%ld)",
                nlen, namebuf, (long)dfa, (long)vm->here);
}

/* :  ( "name" -- )  begin colon definition */
static void defining_word_colon(VM *vm) {
    if (!vm) return;

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        log_message(LOG_ERROR, ": missing name");
        vm->error = 1;
        return;
    }

    vm_enter_compile_mode(vm, namebuf, (size_t)nlen);
    log_message(LOG_DEBUG, ": Started definition of '%.*s'", nlen, namebuf);
}

/* ;  ( -- )  end colon definition — IMMEDIATE */
static void defining_word_semicolon(VM *vm) {
    if (!vm) return;
    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "; used outside compile mode");
        vm->error = 1;
        return;
    }
    /* vm_exit_compile_mode compiles EXIT in this codebase */
    vm_exit_compile_mode(vm);
    log_message(LOG_DEBUG, "; Completed definition");
}

/* CONSTANT ( n "name" -- ) */
void defining_word_constant(VM *vm) {
    if (!vm) return;

    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "CONSTANT: stack underflow");
        vm->error = 1;
        return;
    }
    cell_t value = vm_pop(vm);

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        log_message(LOG_ERROR, "CONSTANT: missing name");
        vm->error = 1;
        return;
    }

    DictEntry *entry = vm_create_word(vm, namebuf, (size_t)nlen, defining_runtime_constant);
    if (!entry) { vm->error = 1; return; }

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) { vm->error = 1; return; }

    *df = value;
    log_message(LOG_DEBUG, "CONSTANT: '%.*s' = %ld", nlen, namebuf, (long)value);
}

/* VARIABLE ( "name" -- )  allocate one cell in VM and store its addr in header DF */
void defining_word_variable(VM *vm) {
    if (!vm) return;

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        log_message(LOG_ERROR, "VARIABLE: missing name");
        vm->error = 1;
        return;
    }

    /* Allocate one VM cell for the variable and init to 0 */
    vaddr_t addr = (vaddr_t)vm->here;
    void *p = vm_allot(vm, sizeof(cell_t));
    if (!p) { vm->error = 1; return; }
    *(cell_t*)p = 0;

    DictEntry *entry = vm_create_word(vm, namebuf, (size_t)nlen, defining_runtime_variable);
    if (!entry) { vm->error = 1; return; }

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) { vm->error = 1; return; }

    *df = (cell_t)(int64_t)addr;
    log_message(LOG_DEBUG, "VARIABLE: '%.*s' at VM addr %ld", nlen, namebuf, (long)addr);
}

/* [  ( -- )  enter interpret state — IMMEDIATE */
void defining_word_left_bracket(VM *vm) {
    if (!vm) return;
    vm_store_cell(vm, vm->state_addr, 0);
    vm->mode = MODE_INTERPRET;
    log_message(LOG_DEBUG, "[: interpret mode");
}

/* ]  ( -- )  enter compile state — IMMEDIATE */
void defining_word_right_bracket(VM *vm) {
    if (!vm) return;
    vm_store_cell(vm, vm->state_addr, (cell_t)-1);
    vm->mode = MODE_COMPILE;
    log_message(LOG_DEBUG, "]: compile mode");
}

/* STATE ( -- addr )  push VM addr of STATE cell */
void defining_word_state(VM *vm) {
    if (!vm) return;
    vm_push(vm, (cell_t)vm->state_addr);
}

/* COMPILE ( "word" -- ) — IMMEDIATE (obsolete, but present) */
void defining_word_compile(VM *vm) {
    if (!vm) return;

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        log_message(LOG_ERROR, "COMPILE: missing word");
        vm->error = 1;
        return;
    }

    DictEntry *entry = vm_find_word(vm, namebuf, (size_t)nlen);
    if (!entry) {
        log_message(LOG_ERROR, "COMPILE: '%.*s' not found", nlen, namebuf);
        vm->error = 1;
        return;
    }

    vm_compile_word(vm, entry);
    log_message(LOG_DEBUG, "COMPILE: compiled '%.*s'", nlen, namebuf);
}

/* [COMPILE] ( "word" -- ) — IMMEDIATE: compile even if the word is IMMEDIATE */
void defining_word_bracket_compile(VM *vm) {
    /* For now, same as COMPILE per your codebase; refine later if needed */
    defining_word_compile(vm);
}

/* LITERAL ( n -- ) — IMMEDIATE: compile a literal */
void defining_word_literal(VM *vm) {
    if (!vm) return;
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "LITERAL: stack underflow");
        vm->error = 1;
        return;
    }
    cell_t v = vm_pop(vm);
    vm_compile_literal(vm, v);
    log_message(LOG_DEBUG, "LITERAL: compiled %ld", (long)v);
}

/* FORGET ( "name" -- )  remove target and newer words; rewind HERE to target DFA */
void dictionary_word_forget(VM *vm) {
    if (!vm) return;

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        log_message(LOG_ERROR, "FORGET: missing name");
        vm->error = 1;
        return;
    }

    /* Find target and its previous entry */
    DictEntry *prev = NULL, *e = vm->latest;
    DictEntry *target = NULL, *target_prev = NULL;
    while (e) {
        if (!(e->flags & WORD_HIDDEN) &&
            e->name_len == (uint8_t)nlen &&
            memcmp(e->name, namebuf, (size_t)nlen) == 0) {
            target = e;
            target_prev = prev;
            break;
        }
        prev = e;
        e = e->link;
    }
    if (!target) {
        log_message(LOG_ERROR, "FORGET: Unknown word '%.*s'", nlen, namebuf);
        vm->error = 1;
        return;
    }

    /* Ensure target is NEWER than the fence (don’t allow nuking core words) */
    int allowed = 0;
    for (e = vm->latest; e; e = e->link) {
        if (e == target) { allowed = 1; break; }
        if (e == vm->dict_fence_latest) break; /* hit fence before target */
    }
    if (!allowed) {
        log_message(LOG_ERROR, "FORGET: '%.*s' is protected (at or below fence)", nlen, namebuf);
        vm->error = 1;
        return;
    }

    /* Compute new HERE from target’s DFA (bounded by fence HERE) */
    vaddr_t new_here = (vaddr_t)vm->here;
    cell_t *df = vm_dictionary_get_data_field(target);
    if (df) {
        vaddr_t dfa = (vaddr_t)(uint64_t)(*df);
        if (vm_addr_ok(vm, dfa, 0)) {
            /* Never move HERE below the fence */
            vaddr_t fence_here = (vaddr_t)vm->dict_fence_here;
            new_here = (dfa < fence_here) ? fence_here : dfa;
        }
    } else {
        /* No DF — at least don’t cross the fence */
        new_here = (vaddr_t)vm->dict_fence_here;
    }

    /* Free headers from latest down to and including target, stopping before fence */
    e = vm->latest;
    while (e) {
        DictEntry *next = e->link;
        free(e);
        if (e == target) break;
        if (e == vm->dict_fence_latest) break; /* paranoia */
        e = next;
    }

    /* Relink latest; never past fence */
    vm->latest = (target_prev ? target_prev : vm->dict_fence_latest);
    vm->here   = (size_t)new_here;

    log_message(LOG_DEBUG, "FORGET: forgot '%.*s'; HERE=%ld (fence HERE=%ld)",
                nlen, namebuf, (long)vm->here, (long)vm->dict_fence_here);
}


/* DOES> — IMMEDIATE (stub for now) */
void defining_word_does(VM *vm) {
    log_message(LOG_WARN, "DOES>: not implemented yet");
    vm->error = 1;
}

/* IMMEDIATE ( -- ) mark latest word immediate — IMMEDIATE itself */
void defining_word_immediate(VM *vm) {
    if (!vm) return;
    if (!vm->latest) {
        log_message(LOG_ERROR, "IMMEDIATE: no latest");
        vm->error = 1;
        return;
    }
    vm->latest->flags |= WORD_IMMEDIATE;
    log_message(LOG_DEBUG, "IMMEDIATE: marked '%.*s' immediate",
                (int)vm->latest->name_len, vm->latest->name);
}

/* LIT runtime: ( -- n )
   Fetch next cell from threaded code via the return IP on R-stack.
   Our execute loop does: ip++ ; R>push(ip) ; call word ; ip := R>pop()
   So inside LIT, we:
     - peek the top return address (current ip),
     - read *ip as the literal,
     - push it to data stack,
     - increment ip and write it back on the R-stack (so the caller resumes after the literal).
*/
static void defining_runtime_lit(VM *vm) {
    if (!vm) return;
    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "LIT: return stack underflow");
        vm->error = 1;
        return;
    }

    cell_t *rip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];  /* peek */
    if (!rip) {
        log_message(LOG_ERROR, "LIT: null IP");
        vm->error = 1;
        return;
    }

    cell_t value = *rip;   /* read literal cell */
    vm_push(vm, value);

    rip++;                 /* advance IP to after literal */
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)rip;  /* write-back */
    log_message(LOG_DEBUG, "LIT: pushed %ld", (long)value);
}


/* ───────────────────────────── Registration ───────────────────────── */

void register_defining_words(VM *vm) {
    log_message(LOG_INFO, "Registering defining words...");

    /* Core colon pair */
    register_word(vm, ":", defining_word_colon);
    register_word(vm, ";", defining_word_semicolon);
    vm_make_immediate(vm);                   /* ; is IMMEDIATE */

    /* CREATE / VARIABLE / CONSTANT */
    register_word(vm, "CREATE",   defining_word_create);
    register_word(vm, "VARIABLE", defining_word_variable);
    register_word(vm, "CONSTANT", defining_word_constant);

    /* IMMEDIATE (make IMMEDIATE itself immediate) */
    register_word(vm, "IMMEDIATE", defining_word_immediate);
    vm_make_immediate(vm);

    /* STATE and mode switchers */
    register_word(vm, "STATE", defining_word_state);
    register_word(vm, "[",     defining_word_left_bracket);
    vm_make_immediate(vm);
    register_word(vm, "]",     defining_word_right_bracket);
    vm_make_immediate(vm);

    /* Dictionary management */
    register_word(vm, "FORGET",  dictionary_word_forget);

    /* Compile helpers — immediate */
    register_word(vm, "COMPILE",    defining_word_compile);
    vm_make_immediate(vm);
    register_word(vm, "[COMPILE]",  defining_word_bracket_compile);
    vm_make_immediate(vm);
    /* LIT */
    register_word(vm, "LIT", defining_runtime_lit);

    register_word(vm, "LITERAL",    defining_word_literal);
    vm_make_immediate(vm);

    /* DOES> — immediate (stub) */
    register_word(vm, "DOES>", defining_word_does);
    vm_make_immediate(vm);


    log_message(LOG_INFO, "Defining words registered.");
}
