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

#include "include/defining_words.h"
#include "../../include/vm.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/profiler.h"
#include "../../include/physics_metadata.h"
#include "../../include/platform_time.h"
#include "../../include/physics_pipelining_metrics.h"

#include <string.h>
#include <stdlib.h>   /* malloc/free */

/* ───── Forward declarations so registration sees these symbols ───── */
static void defining_runtime_create(VM * vm);
static void defining_runtime_constant(VM * vm);
static void defining_runtime_variable(VM * vm);
static void defining_runtime_lit(VM * vm);

static void defining_runtime_dodoes(VM * vm);
static void defining_runtime_does_rt(VM * vm);

static void defining_word_colon(VM * vm);
static void defining_word_semicolon(VM * vm);

static void defining_word_create(VM * vm);
static void defining_word_constant(VM * vm);
static void defining_word_variable(VM * vm);
static void defining_word_left_bracket(VM * vm);
static void defining_word_right_bracket(VM * vm);
static void defining_word_state(VM * vm);
static void defining_word_compile(VM * vm);
static void defining_word_bracket_compile(VM * vm);
static void defining_word_literal(VM * vm);
static void defining_word_does(VM * vm);
static void defining_word_immediate(VM * vm);
static void dictionary_word_forget(VM * vm);

/* ───────────────────────────── Runtimes ───────────────────────────── */

/* CONSTANT runtime: ( -- n )  Push stored value from header DF cell */
/**
 * @brief Runtime behavior for CONSTANT words
 *
 * Pushes the constant value stored in the word's data field onto the stack.
 * Stack effect: ( -- n )
 *
 * @param vm Pointer to the VM context
 */
static void defining_runtime_constant(VM *vm) {
    if (!vm) return;

    DictEntry *entry = vm->current_executing_entry;
    if (!entry) {
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        vm->error = 1;
        return;
    }

    vm_push(vm, *df);
    log_message(LOG_DEBUG, "CONSTANT runtime: pushed %ld for %.*s",
                (long) *df, entry->name_len, entry->name);
}

/* VARIABLE runtime: ( -- addr )  Push VM offset stored in header DF cell */
/**
 * @brief Runtime behavior for VARIABLE words
 *
 * Pushes the variable's address (VM offset) stored in the word's data field.
 * Stack effect: ( -- addr )
 *
 * @param vm Pointer to the VM context
 */
static void defining_runtime_variable(VM *vm) {
    if (!vm) return;

    DictEntry *entry = vm->current_executing_entry;
    if (!entry) {
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        vm->error = 1;
        return;
    }

    vm_push(vm, *df);
    log_message(LOG_DEBUG, "VARIABLE runtime: pushed VM addr %ld for %.*s",
                (long) *df, entry->name_len, entry->name);
}

/* CREATE runtime: ( -- addr )  Push DFA (VM byte offset) captured at CREATE */
/**
 * @brief Runtime behavior for words defined by CREATE
 *
 * Pushes the word's data field address (DFA) captured at CREATE time.
 * Stack effect: ( -- addr )
 *
 * @param vm Pointer to the VM context
 */
static void defining_runtime_create(VM *vm) {
    if (!vm) return;

    DictEntry *entry = vm->current_executing_entry;
    if (!entry) {
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        vm->error = 1;
        return;
    }

    vaddr_t dfa = (vaddr_t)(uint64_t)(*df);
    if (!vm_addr_ok(vm, dfa, sizeof(cell_t))) {
        vm->error = 1;
        return;
    }

    vm_push(vm, CELL(dfa));
    log_message(LOG_DEBUG, "CREATE runtime: pushed DFA %ld for %.*s",
                (long) dfa, entry->name_len, entry->name);
}

/* LIT runtime: ( -- n ) — fetch next cell from threaded code via R-stack IP */
/**
 * @brief Runtime behavior for literal values in threaded code
 *
 * Fetches the next cell from threaded code via IP and pushes it.
 * Stack effect: ( -- n )
 *
 * @param vm Pointer to the VM context
 */
static void defining_runtime_lit(VM *vm) {
    if (!vm) return;
    if (vm->rsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t *rip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp]; /* peek IP */
    if (!rip) {
        vm->error = 1;
        return;
    }

    cell_t value = *rip; /* read literal cell */
    vm_push(vm, value);

    rip++; /* advance IP to after literal */
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
    rip; /* write-back */
    log_message(LOG_DEBUG, "LIT: pushed %ld", (long) value);
}

/* DODOES runtime: when a created word runs, push PFA+8 (user data addr) then execute DOES>-body
   PFA layout: [body_vaddr][user_data...]
   We push the address of user_data, then execute body_vaddr code. */
/**
 * @brief Runtime behavior for words defined with DOES>
 *
 * Pushes the address of user data area then executes the DOES> body code.
 * PFA layout: [body_vaddr][user_data...]
 *
 * @param vm Pointer to the VM context
 */
static void defining_runtime_dodoes(VM *vm) {
    if (!vm) return;

    DictEntry *self = vm->current_executing_entry;
    if (!self) {
        vm->error = 1;
        log_message(LOG_ERROR, "DODOES: no current entry");
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(self);
    if (!df) {
        vm->error = 1;
        return;
    }
    vaddr_t pfa = (vaddr_t)(uint64_t)(*df);

    /* PFA[0] holds vaddr of DOES>-body */
    if (!vm_addr_ok(vm, pfa, sizeof(cell_t))) {
        vm->error = 1;
        log_message(LOG_ERROR, "DODOES: invalid PFA address %zu", (size_t) pfa);
        return;
    }
    vaddr_t body_vaddr = (vaddr_t)(uint64_t)(*(cell_t *) vm_ptr(vm, pfa));
    log_message(LOG_DEBUG, "DODOES: body_vaddr=%zu (loaded from PFA[0] at %zu)", (size_t) body_vaddr, (size_t) pfa);
    if (!vm_addr_ok(vm, body_vaddr, sizeof(cell_t))) {
        vm->error = 1;
        log_message(LOG_ERROR, "DODOES: invalid body_vaddr %zu", (size_t) body_vaddr);
        return;
    }

    /* Push address of user data (PFA + sizeof(cell_t)) per DOES> semantics */
    vaddr_t user_data_addr = pfa + sizeof(cell_t);
    vm_push(vm, (cell_t) user_data_addr);

    /* Execute like a colon word until EXIT pops the frame */
    int base_rsp = vm->rsp;
    cell_t *ip = (cell_t *) vm_ptr(vm, body_vaddr);

    vm->rsp++;
    if (vm->rsp >= STACK_SIZE) {
        vm->error = 1;
        vm->rsp = base_rsp;
        return;
    }
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
    ip;

    while (!vm->error && !vm->exit_colon && vm->rsp >= base_rsp + 1) {
        cell_t *cur_ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp];
        DictEntry *entry = (DictEntry *) (uintptr_t)(*cur_ip++);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
        cur_ip;

        if (!entry || !entry->func) {
            vm->error = 1;
            break;
        }
        vm->current_executing_entry = entry;
        physics_execution_heat_increment(entry);
        profiler_word_count(entry);
        profiler_word_enter(entry);
        entry->func(vm);
        physics_metadata_touch(entry, entry->execution_heat, sf_monotonic_ns());
        profiler_word_exit(entry);
    }

    /* Handle EXIT from the DOES> body */
    if (vm->exit_colon) {
        vm->rsp = base_rsp;
        vm->exit_colon = 0;
    }

    if (vm->rsp < base_rsp) vm->rsp = base_rsp;
}

/* does_rt: runs inside the defining word at DOES> time.
   Converts the just-created child so its runtime is DODOES and
   records the DOES>-body start address at the child's PFA[0].

   NOTE: PFA already contains user data from , or other compile-time words.
   We need to build a new PFA structure: [body_vaddr][old_user_data...]
   and update the child's DF to point to this new PFA. */
/**
 * @brief Helper routine executed at DOES> time
 *
 * Converts the just-created child word to use DODOES runtime behavior
 * and records the DOES> body address in the child's PFA.
 *
 * @param vm Pointer to the VM context
 */
static void defining_runtime_does_rt(VM *vm) {
    if (!vm) return;

    DictEntry *child = vm->latest;
    if (!child) {
        vm->error = 1;
        log_message(LOG_ERROR, "DOES>: no child to patch");
        return;
    }

    /* IP (top R-stack) points at the next entry (EXIT we compile below); body starts after that */
    if (vm->rsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t *ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp]; /* IP is a real pointer, not vaddr */
    cell_t *body_ip = ip + 1; /* first entry AFTER EXIT */

    vaddr_t body_vaddr = (vaddr_t)((uint8_t *) body_ip - vm->memory);

    cell_t *df = vm_dictionary_get_data_field(child);
    if (!df) {
        vm->error = 1;
        return;
    }
    vaddr_t old_pfa = (vaddr_t)(uint64_t)(*df);

    /* Calculate how much user data was allocated between old PFA and current HERE */
    vaddr_t current_here = (vaddr_t) vm->here;
    size_t user_data_size = (old_pfa < current_here) ? (current_here - old_pfa) : 0;

    /* Allocate new PFA: [body_vaddr][user_data...] */
    vm_align(vm);
    vaddr_t new_pfa = (vaddr_t) vm->here;

    /* Allocate space for body_vaddr pointer */
    cell_t *body_ptr = (cell_t *) vm_allot(vm, sizeof(cell_t));
    if (!body_ptr) {
        vm->error = 1;
        return;
    }
    *body_ptr = (cell_t) body_vaddr;

    /* Copy user data if any was allocated */
    if (user_data_size > 0) {
        void *user_data_dest = vm_allot(vm, user_data_size);
        if (!user_data_dest) {
            vm->error = 1;
            return;
        }
        if (vm_addr_ok(vm, old_pfa, user_data_size)) {
            memcpy(user_data_dest, vm_ptr(vm, old_pfa), user_data_size);
        }
    }

    /* Update child's DF to point to new PFA */
    *df = (cell_t) new_pfa;
    child->func = defining_runtime_dodoes;
}

/* ───────────────────────────── Defining words ─────────────────────── */

/* CREATE ( "name" -- ) — 79: does NOT allocate data; captures cell-aligned HERE as DFA */
static void defining_word_create(VM *vm) {
    if (!vm) return;

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        vm->error = 1;
        return;
    }

    DictEntry *entry = vm_create_word(vm, namebuf, (size_t) nlen, defining_runtime_create);
    if (!entry) {
        vm->error = 1;
        return;
    }

    vm_align(vm); /* align HERE for cell data */
    vaddr_t dfa = (vaddr_t) vm->here;

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        vm->error = 1;
        return;
    }
    *df = (cell_t)(int64_t)
    dfa;

    log_message(LOG_DEBUG, "CREATE: '%.*s' DFA=%ld (HERE=%ld)",
                nlen, namebuf, (long) dfa, (long) vm->here);
}

/* :  ( "name" -- )  begin colon definition — IMMEDIATE */
static void defining_word_colon(VM *vm) {
    if (!vm) return;

    /* 79: nested colon definitions are illegal */
    if (vm->mode == MODE_COMPILE) {
        log_message(LOG_ERROR, "Nested ':' inside a definition is not allowed");
        vm->error = 1;
        return;
    }

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        vm->error = 1;
        return;
    }

    vm_enter_compile_mode(vm, namebuf, (size_t) nlen);
    log_message(LOG_DEBUG, ": Started definition of '%.*s'", nlen, namebuf);
}

/* ;  ( -- )  end colon definition — IMMEDIATE (compiles EXIT in vm_exit_compile_mode) */
static void defining_word_semicolon(VM *vm) {
    if (!vm) return;
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        return;
    }
    vm_exit_compile_mode(vm);
}

/* CONSTANT ( n "name" -- ) */
static void defining_word_constant(VM *vm) {
    if (!vm) return;
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t value = vm_pop(vm);

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        vm->error = 1;
        return;
    }

    DictEntry *entry = vm_create_word(vm, namebuf, (size_t) nlen, defining_runtime_constant);
    if (!entry) {
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        vm->error = 1;
        return;
    }

    *df = value;
    log_message(LOG_DEBUG, "CONSTANT: '%.*s' = %ld", nlen, namebuf, (long) value);
}

/* VARIABLE ( "name" -- )  allocate one cell in VM and store its addr in header DF */
static void defining_word_variable(VM *vm) {
    if (!vm) return;

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        vm->error = 1;
        return;
    }

    vaddr_t addr = (vaddr_t) vm->here;
    void *p = vm_allot(vm, sizeof(cell_t));
    if (!p) {
        vm->error = 1;
        return;
    }
    *(cell_t *) p = 0;

    DictEntry *entry = vm_create_word(vm, namebuf, (size_t) nlen, defining_runtime_variable);
    if (!entry) {
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        vm->error = 1;
        return;
    }

    *df = (cell_t)(int64_t)
    addr;
    log_message(LOG_DEBUG, "VARIABLE: '%.*s' at VM addr %ld", nlen, namebuf, (long) addr);
}

/* [  ( -- )  enter interpret state — IMMEDIATE */
static void defining_word_left_bracket(VM *vm) {
    if (!vm) return;
    /* Forth-79 requires STATE to be 0 for interpret and non-zero (typically -1) for compile */
    vm_store_cell(vm, vm->state_addr, 0);
    vm->mode = MODE_INTERPRET;
    log_message(LOG_DEBUG, "[: interpret mode");
}

/* ]  ( -- )  enter compile state — IMMEDIATE */
static void defining_word_right_bracket(VM *vm) {
    if (!vm) return;
    vm_store_cell(vm, vm->state_addr, (cell_t) - 1);
    vm->mode = MODE_COMPILE;
    log_message(LOG_DEBUG, "]: compile mode");
}

/* STATE ( -- addr )  push VM addr of STATE cell */
static void defining_word_state(VM *vm) {
    if (!vm) return;
    vm_push(vm, (cell_t) vm->state_addr);
}

/* COMPILE ( "word" -- ) — IMMEDIATE (legacy) */
static void defining_word_compile(VM *vm) {
    if (!vm) return;

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        vm->error = 1;
        return;
    }

    DictEntry *entry = vm_find_word(vm, namebuf, (size_t) nlen);
    if (!entry) {
        vm->error = 1;
        return;
    }

    vm_compile_word(vm, entry);
    log_message(LOG_DEBUG, "COMPILE: compiled '%.*s'", nlen, namebuf);
}

/* [COMPILE] ( "word" -- ) — IMMEDIATE: compile even if the word is IMMEDIATE */
static void defining_word_bracket_compile(VM *vm) {
    defining_word_compile(vm);
}

/* LITERAL ( n -- ) — IMMEDIATE: compile a literal */
static void defining_word_literal(VM *vm) {
    if (!vm) return;
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t v = vm_pop(vm);
    vm_compile_literal(vm, v);
    log_message(LOG_DEBUG, "LITERAL: compiled %ld", (long) v);
}

/* FORGET ( "name" -- )  remove target and newer words; rewind HERE to target DFA */
static void dictionary_word_forget(VM *vm) {
    if (!vm) return;

    char namebuf[WORD_NAME_MAX + 1];
    int nlen = vm_parse_word(vm, namebuf, sizeof(namebuf));
    if (nlen <= 0) {
        vm->error = 1;
        return;
    }

    int forget_failed = 0;
    sf_mutex_lock(&vm->dict_lock);

    /* Find target and its previous entry */
    DictEntry *prev = NULL, *e = vm->latest;
    DictEntry *target = NULL, *target_prev = NULL;
    while (e) {
        if (!(e->flags & WORD_HIDDEN) &&
            e->name_len == (uint8_t) nlen &&
            memcmp(e->name, namebuf, (size_t) nlen) == 0) {
            target = e;
            target_prev = prev;
            break;
        }
        prev = e;
        e = e->link;
    }
    if (!target) {
        vm->error = 1;
        forget_failed = 1;
        goto forget_exit;
    }

    /* Ensure target is NEWER than the fence */
    int allowed = 0;
    for (e = vm->latest; e; e = e->link) {
        if (e == target) {
            allowed = 1;
            break;
        }
        if (e == vm->dict_fence_latest) break;
    }
    if (!allowed) {
        vm->error = 1;
        forget_failed = 1;
        goto forget_exit;
    }

    /* Compute new HERE from target’s DFA (bounded by fence HERE) */
    vaddr_t new_here = (vaddr_t) vm->here;
    cell_t *df = vm_dictionary_get_data_field(target);
    if (df) {
        vaddr_t dfa = (vaddr_t)(uint64_t)(*df);
        if (vm_addr_ok(vm, dfa, 0)) {
            vaddr_t fence_here = (vaddr_t) vm->dict_fence_here;
            new_here = (dfa < fence_here) ? fence_here : dfa;
        }
    } else {
        new_here = (vaddr_t) vm->dict_fence_here;
    }

    /* Save target's link before freeing (needed for relinking) */
    DictEntry *target_next = target->link;

    /* Free headers from latest down to and including target, stopping before fence */
    e = vm->latest;
    while (e && e != vm->dict_fence_latest) {
        DictEntry *next = e->link;
        int is_target = (e == target);
        vm_dictionary_untrack_entry(vm, e);

        /* Free transition metrics before freeing entry */
        if (e->transition_metrics) {
            transition_metrics_cleanup(e->transition_metrics);
            free(e->transition_metrics);
            e->transition_metrics = NULL;
        }

        free(e);
        if (is_target) break;
        e = next;
    }

    /* Relink: if target_prev exists, point it past the freed chain */
    if (target_prev) {
        target_prev->link = target_next; /* Skip freed entries */
        vm->latest = target_prev; /* Latest is now the entry before target */
    } else {
        /* No prev means target was latest - reset to fence */
        vm->latest = vm->dict_fence_latest;
    }

    vm->here = (size_t) new_here;

    log_message(LOG_DEBUG, "FORGET: forgot '%.*s'; HERE=%ld (fence HERE=%ld)",
                nlen, namebuf, (long) vm->here, (long) vm->dict_fence_here);

forget_exit:
    sf_mutex_unlock(&vm->dict_lock);
    if (forget_failed)
        return;
}

/* DOES> — IMMEDIATE: finalize the defining word’s create-part and compile DOES>-body */
static void defining_word_does(VM *vm) {
    if (!vm) return;
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "DOES>: compile-only");
        return;
    }

    /* Compile helper (void fn; check vm->error after) */
    vm_compile_call(vm, defining_runtime_does_rt);
    if (vm->error) {
        log_message(LOG_ERROR, "DOES>: could not compile does_rt");
        return;
    }

    /* Compile EXIT so the defining word returns immediately after patching */
    DictEntry *exitw = vm_find_word(vm, "EXIT", 4);
    if (!exitw) {
        vm->error = 1;
        log_message(LOG_ERROR, "DOES>: EXIT not found");
        return;
    }
    vm_compile_word(vm, exitw);
    if (vm->error) return;

    /* Continue compiling: the following words form the DOES>-body executed by DODOES later. */
}

/* IMMEDIATE ( -- ) mark latest word immediate — IMMEDIATE itself */
static void defining_word_immediate(VM *vm) {
    if (!vm) return;
    if (!vm->latest) {
        vm->error = 1;
        return;
    }
    vm->latest->flags |= WORD_IMMEDIATE;
    log_message(LOG_DEBUG, "IMMEDIATE: marked '%.*s' immediate",
                (int) vm->latest->name_len, vm->latest->name);
}

/* ───────────────────────────── Registration ───────────────────────── */

/**
 * @brief Register all defining words in the dictionary
 *
 * Registers core defining words like :, ;, CREATE, CONSTANT, VARIABLE,
 * and their support words like IMMEDIATE, STATE, etc.
 *
 * @param vm Pointer to the VM context
 */
void register_defining_words(VM *vm) {
    /* Core colon pair — both IMMEDIATE */
    register_word(vm, ":", defining_word_colon);
    vm_make_immediate(vm);
    register_word(vm, ";", defining_word_semicolon);
    vm_make_immediate(vm);

    /* CREATE / VARIABLE / CONSTANT */
    register_word(vm, "CREATE", defining_word_create);
    register_word(vm, "VARIABLE", defining_word_variable);
    register_word(vm, "CONSTANT", defining_word_constant);

    /* IMMEDIATE (make IMMEDIATE itself immediate) */
    register_word(vm, "IMMEDIATE", defining_word_immediate);
    vm_make_immediate(vm);

    /* STATE and mode switchers */
    register_word(vm, "STATE", defining_word_state);
    register_word(vm, "[", defining_word_left_bracket);
    vm_make_immediate(vm);
    register_word(vm, "]", defining_word_right_bracket);
    vm_make_immediate(vm);

    /* Dictionary management */
    register_word(vm, "FORGET", dictionary_word_forget);

    /* Compile helpers — immediate */
    register_word(vm, "COMPILE", defining_word_compile);
    vm_make_immediate(vm);
    register_word(vm, "[COMPILE]", defining_word_bracket_compile);
    vm_make_immediate(vm);

    /* LIT runtime + LITERAL (immediate) */
    register_word(vm, "LIT", defining_runtime_lit);
    register_word(vm, "LITERAL", defining_word_literal);
    vm_make_immediate(vm);

    /* DOES> plumbing (lowercase internal helper visible for tests/trace) */
    register_word(vm, "does_rt", defining_runtime_does_rt); /* internal helper */
    register_word(vm, "DOES>", defining_word_does);
    vm_make_immediate(vm);
}
