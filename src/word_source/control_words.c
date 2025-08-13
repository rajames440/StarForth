/*

                                 ***   StarForth   ***
  control_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/13/25, 12:09 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/*

                                 ***   StarForth   ***
  control_words.c - FORTH-79 Standard and ANSI C99 ONLY
  Last modified - 8/10/25, 4:50 PM
  Public Domain / CC0

*/

#include "include/control_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <stddef.h>
#include <stdint.h>

#ifndef RETURN_STACK_SIZE
#define RETURN_STACK_SIZE STACK_SIZE
#endif

/* --- SAFETY WRAPPERS: return stack (runtime use only) --- */
static inline int rs_push(VM *vm, cell_t v) {
    if (vm->rsp + 1 >= RETURN_STACK_SIZE) {
        vm->error = 1;
        log_message(LOG_ERROR, "RSP overflow (rsp=%d)", vm->rsp);
        return 0;
    }
    vm->return_stack[++vm->rsp] = v;
    return 1;
}
static inline int rs_pop(VM *vm, cell_t *out) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "RSP underflow (rsp=%d)", vm->rsp);
        return 0;
    }
    *out = vm->return_stack[vm->rsp--];
    return 1;
}

/* ---------- Compile-time control-flow stack (CF stack) ---------- */
/* We never use the runtime return stack during compile. */
#define CF_STACK_MAX 64
static size_t cf_stack[CF_STACK_MAX];  /* BYTE offsets into vm->memory */
static int cf_sp = -1;

static inline int cf_push(size_t v) {
    if (cf_sp + 1 >= CF_STACK_MAX) {
        log_message(LOG_ERROR, "CF: overflow");
        return 0;
    }
    cf_stack[++cf_sp] = v;
    return 1;
}
static inline int cf_pop(size_t *out) {
    if (cf_sp < 0) {
        log_message(LOG_ERROR, "CF: underflow");
        return 0;
    }
    *out = cf_stack[cf_sp--];
    return 1;
}

/* Forward declarations for runtime functions */
static void control_forth_runtime_do(VM *vm);
static void control_forth_runtime_loop(VM *vm);
static void control_forth_runtime_plus_loop(VM *vm);

/* Loop parameter stack for DO/LOOP constructs (runtime) */
#define LOOP_STACK_SIZE 16
typedef struct {
    cell_t limit;
    cell_t index;
    void *loop_start_ip;  /* address where loop body starts (C pointer into code stream) */
} loop_params_t;
static loop_params_t loop_stack[LOOP_STACK_SIZE];
static int loop_sp = -1;

static void loop_push(cell_t limit, cell_t index, void *ip) {
    if (loop_sp >= LOOP_STACK_SIZE - 1) {
        log_message(LOG_ERROR, "Loop stack overflow");
        return;
    }
    loop_sp++;
    loop_stack[loop_sp].limit = limit;
    loop_stack[loop_sp].index = index;
    loop_stack[loop_sp].loop_start_ip = ip;
}
static void loop_pop(void) {
    if (loop_sp < 0) {
        log_message(LOG_ERROR, "Loop stack underflow");
        return;
    }
    loop_sp--;
}

/* ===================== Runtime primitives ===================== */
/* Runtime expects BRANCH/0BRANCH offsets in BYTES:
   ip = (cell_t*)((char*)ip + offset); */

/* BRANCH ( -- ) */
static void control_forth_branch(VM *vm) {
    if (vm->rsp < 0) { vm->error = 1; log_message(LOG_ERROR, "BRANCH: RSP underflow"); return; }
    cell_t *ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];
    cell_t offset = *ip; /* BYTES */
    ip = (cell_t*)((char*)ip + offset);
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
    log_message(LOG_DEBUG, "BRANCH: +%ld bytes", (long)offset);
}

/* 0BRANCH ( flag -- ) */
static void control_forth_0branch(VM *vm) {
    if (vm->rsp < 0) { vm->error = 1; log_message(LOG_ERROR, "0BRANCH: RSP underflow"); return; }
    cell_t *ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    if (vm->dsp < 0) { vm->error = 1; log_message(LOG_ERROR, "0BRANCH: DSP underflow"); return; }
    cell_t flag = vm_pop(vm);

    if (flag == 0) {
        cell_t offset = *ip; /* BYTES */
        ip = (cell_t*)((char*)ip + offset);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "0BRANCH: taken +%ld bytes", (long)offset);
    } else {
        ip++; /* skip literal */
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "0BRANCH: not taken");
    }
}

// /* (LIT) ( -- n ) */
// static void control_forth_lit(VM *vm) {
//     if (vm->rsp < 0) { vm->error = 1; log_message(LOG_ERROR, "(LIT): RSP underflow"); return; }
//     cell_t *ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];
//     cell_t v = *ip++;
//     vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
//     vm_push(vm, v);
//     log_message(LOG_DEBUG, "(LIT): %ld", (long)v);
// }

/* ===================== Compile-time words (CF stack) ===================== */

/* emit a raw cell into the code stream (FORTH-79 equivalent of ",") */
static size_t emit_cell(VM *vm, cell_t value) {
    if (vm->mode != MODE_COMPILE) {
        vm_push(vm, value);        /* interpret-mode: behave like pushing a literal */
        return (size_t)-1;
    }
    vm_align(vm);
    cell_t *addr = (cell_t*)vm_allot(vm, sizeof(cell_t));
    if (!addr) {
        vm->error = 1;
        log_message(LOG_ERROR, "emit_cell: out of space");
        return (size_t)-1;
    }
    *addr = value;                 /* raw cell (no LIT) */
    return (size_t)((uint8_t*)addr - vm->memory); /* byte address in code space */
}

/* IF ( flag -- ) compiles: 0BRANCH <placeholder> */
static void control_forth_if(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "IF: compile-only"); return; }

    vm_compile_call(vm, control_forth_0branch);
    /* record the actual cell location AFTER alignment */
    size_t lit = emit_cell(vm, 0);

    if (!cf_push(lit)) { vm->error = 1; log_message(LOG_ERROR, "IF: CF overflow"); return; }
    log_message(LOG_DEBUG, "IF: placeholder @ %zu", lit);
}

/* ELSE compiles: BRANCH <placeholder>; patches prior 0BRANCH */
static void control_forth_else(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "ELSE: compile-only"); return; }

    vm_compile_call(vm, control_forth_branch);
    /* create new placeholder and remember its exact byte address */
    size_t new_lit = emit_cell(vm, 0);

    size_t old_lit;
    if (!cf_pop(&old_lit)) { vm->error = 1; log_message(LOG_ERROR, "ELSE: CF underflow"); return; }

    /* patch previous 0BRANCH with distance to HERE (in BYTES) */
    cell_t off_bytes = (cell_t)((size_t)vm->here - old_lit);
    *(cell_t *)(vm->memory + old_lit) = off_bytes;

    if (!cf_push(new_lit)) { vm->error = 1; log_message(LOG_ERROR, "ELSE: CF overflow (new)"); return; }

    log_message(LOG_DEBUG, "ELSE: patched 0BRANCH @ %zu -> +%ld bytes; new lit @ %zu",
                old_lit, (long)off_bytes, new_lit);
}

/* THEN patches the outstanding forward jump */
static void control_forth_then(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "THEN: compile-only"); return; }

    size_t lit;
    if (!cf_pop(&lit)) { vm->error = 1; log_message(LOG_ERROR, "THEN: CF underflow"); return; }

    cell_t off_bytes = (cell_t)((size_t)vm->here - lit); /* target - literal */
    *(cell_t *)(vm->memory + lit) = off_bytes;

    log_message(LOG_DEBUG, "THEN: patched literal @ %zu -> +%ld bytes", lit, (long)off_bytes);
}

/* BEGIN marks a back-branch target */
static void control_forth_begin(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "BEGIN: compile-only"); return; }
    if (!cf_push(vm->here)) { vm->error = 1; log_message(LOG_ERROR, "BEGIN: CF overflow"); return; }
    log_message(LOG_DEBUG, "BEGIN: mark @ %zu", vm->here);
}

/* UNTIL ( flag -- ) compiles: 0BRANCH <back-offset> */
static void control_forth_until(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "UNTIL: compile-only"); return; }

    size_t begin_addr;
    if (!cf_pop(&begin_addr)) { vm->error = 1; log_message(LOG_ERROR, "UNTIL: CF underflow"); return; }

    vm_compile_call(vm, control_forth_0branch);
    cell_t off_bytes = (cell_t)((cell_t)begin_addr - (cell_t)vm->here);
    (void)emit_cell(vm, off_bytes); /* raw back-offset */

    log_message(LOG_DEBUG, "UNTIL: back -> %zu (%ld bytes)", begin_addr, (long)off_bytes);
}

/* AGAIN compiles: BRANCH <back-offset> */
static void control_forth_again(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "AGAIN: compile-only"); return; }

    size_t begin_addr;
    if (!cf_pop(&begin_addr)) { vm->error = 1; log_message(LOG_ERROR, "AGAIN: CF underflow"); return; }

    vm_compile_call(vm, control_forth_branch);
    cell_t off_bytes = (cell_t)((cell_t)begin_addr - (cell_t)vm->here);
    (void)emit_cell(vm, off_bytes); /* raw back-offset */

    log_message(LOG_DEBUG, "AGAIN: back -> %zu (%ld bytes)", begin_addr, (long)off_bytes);
}

/* DO ( limit index -- ) compiles: runtime DO and marks back target for LOOP/+LOOP */
static void control_forth_do(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "DO: compile-only"); return; }

    vm_compile_call(vm, control_forth_runtime_do);
    if (!cf_push(vm->here)) { vm->error = 1; log_message(LOG_ERROR, "DO: CF overflow"); return; }

    log_message(LOG_DEBUG, "DO: mark @ %zu", vm->here);
}

/* LOOP compiles: runtime LOOP <back-offset> */
static void control_forth_loop(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "LOOP: compile-only"); return; }

    size_t do_addr;
    if (!cf_pop(&do_addr)) { vm->error = 1; log_message(LOG_ERROR, "LOOP: CF underflow"); return; }

    vm_compile_call(vm, control_forth_runtime_loop);
    cell_t off_bytes = (cell_t)((cell_t)do_addr - (cell_t)vm->here);
    (void)emit_cell(vm, off_bytes); /* raw back-offset */

    log_message(LOG_DEBUG, "LOOP: back -> %zu (%ld bytes)", do_addr, (long)off_bytes);
}

/* +LOOP compiles: runtime +LOOP <back-offset> */
static void control_forth_plus_loop(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "+LOOP: compile-only"); return; }

    size_t do_addr;
    if (!cf_pop(&do_addr)) { vm->error = 1; log_message(LOG_ERROR, "+LOOP: CF underflow"); return; }

    vm_compile_call(vm, control_forth_runtime_plus_loop);
    cell_t off_bytes = (cell_t)((cell_t)do_addr - (cell_t)vm->here);
    (void)emit_cell(vm, off_bytes); /* raw back-offset */

    log_message(LOG_DEBUG, "+LOOP: back -> %zu (%ld bytes)", do_addr, (long)off_bytes);
}

/* ===================== Runtime DO/LOOP/+LOOP ===================== */

static void control_forth_runtime_do(VM *vm) {
    if (vm->rsp < 0) { vm->error = 1; log_message(LOG_ERROR, "Runtime DO: RSP underflow"); return; }

    cell_t *ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    if (vm->dsp < 1) { vm->error = 1; log_message(LOG_ERROR, "Runtime DO: DSP underflow"); return; }

    cell_t index = vm_pop(vm);
    cell_t limit = vm_pop(vm);

    loop_push(limit, index, ip);
    log_message(LOG_DEBUG, "Runtime DO: index=%ld limit=%ld", (long)index, (long)limit);
}

static void control_forth_runtime_loop(VM *vm) {
    if (vm->rsp < 0) { vm->error = 1; log_message(LOG_ERROR, "Runtime LOOP: RSP underflow"); return; }

    cell_t *ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    if (loop_sp < 0) { vm->error = 1; log_message(LOG_ERROR, "Runtime LOOP: loop stack underflow"); return; }

    loop_stack[loop_sp].index++;

    if (loop_stack[loop_sp].index < loop_stack[loop_sp].limit) {
        cell_t off = *ip; /* BYTES */
        ip = (cell_t*)((char*)ip + off);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "Runtime LOOP: continue (index=%ld)", (long)loop_stack[loop_sp].index);
    } else {
        loop_pop();
        ip++; /* skip the back-offset literal */
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "Runtime LOOP: exit");
    }
}

static void control_forth_runtime_plus_loop(VM *vm) {
    if (vm->rsp < 0) { vm->error = 1; log_message(LOG_ERROR, "Runtime +LOOP: RSP underflow"); return; }

    cell_t *ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    if (vm->dsp < 0) { vm->error = 1; log_message(LOG_ERROR, "Runtime +LOOP: DSP underflow"); return; }
    cell_t n = vm_pop(vm);

    if (loop_sp < 0) { vm->error = 1; log_message(LOG_ERROR, "Runtime +LOOP: loop stack underflow"); return; }

    cell_t old = loop_stack[loop_sp].index;
    cell_t lim = loop_stack[loop_sp].limit;
    cell_t newv = old + n;
    loop_stack[loop_sp].index = newv;

    int cont = (n >= 0) ? (newv < lim) : (newv >= lim);

    if (cont) {
        cell_t off = *ip; /* BYTES */
        ip = (cell_t*)((char*)ip + off);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "Runtime +LOOP: continue (index=%ld)", (long)newv);
    } else {
        loop_pop();
        ip++; /* skip literal */
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "Runtime +LOOP: exit");
    }
}

/* ───────────── Add near other compile-time helpers ───────────── */

/* WHILE ( flag -- ) compiles: 0BRANCH <fwd-placeholder>
   Stack discipline on CF stack after BEGIN ... WHILE:
   [ ... begin_addr, while_lit_addr ] */
static void control_forth_while(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "WHILE: compile-only"); return; }

    /* Compile a forward 0BRANCH and remember where its literal lives for patching */
    vm_compile_call(vm, control_forth_0branch);
    size_t lit = emit_cell(vm, 0); /* placeholder to be patched at REPEAT */

    if (!cf_push(lit)) { vm->error = 1; log_message(LOG_ERROR, "WHILE: CF overflow"); return; }
    log_message(LOG_DEBUG, "WHILE: placeholder @ %zu", lit);
}

/* REPEAT ( -- ) compiles: BRANCH <back-to-BEGIN>, then patches WHILE forward jump */
static void control_forth_repeat(VM *vm) {
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "REPEAT: compile-only"); return; }

    size_t while_lit;
    if (!cf_pop(&while_lit)) { vm->error = 1; log_message(LOG_ERROR, "REPEAT: CF underflow (WHILE placeholder)"); return; }

    size_t begin_addr;
    if (!cf_pop(&begin_addr)) { vm->error = 1; log_message(LOG_ERROR, "REPEAT: CF underflow (BEGIN addr)"); return; }

    /* Back branch to BEGIN */
    vm_compile_call(vm, control_forth_branch);
    cell_t back = (cell_t)((cell_t)begin_addr - (cell_t)vm->here);
    (void)emit_cell(vm, back);

    /* Patch the WHILE’s 0BRANCH to land here (just after we emitted the back-branch literal) */
    cell_t fwd = (cell_t)((size_t)vm->here - while_lit);
    *(cell_t *)(vm->memory + while_lit) = fwd;

    log_message(LOG_DEBUG, "REPEAT: patched WHILE @ %zu -> +%ld; back to %zu (%ld)",
                while_lit, (long)fwd, begin_addr, (long)back);
}

/* I ( -- n )  current loop index */
static void control_forth_i(VM *vm) {
    if (loop_sp < 0) { vm->error = 1; log_message(LOG_ERROR, "I: outside DO loop"); return; }
    vm_push(vm, loop_stack[loop_sp].index);
}

/* J ( -- n )  next-outer loop index */
static void control_forth_j(VM *vm) {
    if (loop_sp < 1) { vm->error = 1; log_message(LOG_ERROR, "J: needs nested DO loops"); return; }
    vm_push(vm, loop_stack[loop_sp - 1].index);
}

/* Registration of control flow words */
void register_control_words(VM *vm) {
    if (!vm) return;

    /* Internal building blocks: must be registered so vm_compile_call() can find them.
       We give them paren-names so they aren't “public” words in practice. */
    register_word(vm, "(BRANCH)",  control_forth_branch);
    register_word(vm, "(0BRANCH)", control_forth_0branch);

    /* IF/ELSE/THEN */
    register_word(vm, "IF",      control_forth_if);          vm_make_immediate(vm);
    register_word(vm, "ELSE",    control_forth_else);        vm_make_immediate(vm);
    register_word(vm, "THEN",    control_forth_then);        vm_make_immediate(vm);

    /* BEGIN-family */
    register_word(vm, "BEGIN",   control_forth_begin);       vm_make_immediate(vm);
    register_word(vm, "UNTIL",   control_forth_until);       vm_make_immediate(vm);
    register_word(vm, "AGAIN",   control_forth_again);       vm_make_immediate(vm);
    register_word(vm, "WHILE",   control_forth_while);       vm_make_immediate(vm);
    register_word(vm, "REPEAT",  control_forth_repeat);      vm_make_immediate(vm);

    /* DO-family */
    register_word(vm, "DO",      control_forth_do);          vm_make_immediate(vm);
    register_word(vm, "LOOP",    control_forth_loop);        vm_make_immediate(vm);
    register_word(vm, "+LOOP",   control_forth_plus_loop);   vm_make_immediate(vm);

    /* Loop indices */
    register_word(vm, "I",       control_forth_i);
    register_word(vm, "J",       control_forth_j);

    log_message(LOG_INFO, "Registering FORTH-79 control flow words (+internal branches)");
}
