/*

                                 ***   StarForth   ***
  control_words.c - FORTH-79 Standard and ANSI C99 ONLY
  Last modified - 8/13/25, 9:42 PM
  (c) 2025 Robert A. James (rajames) - StarshipOS Forth Project. CC0-1.0 / No warranty.

*/

#include "include/control_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"
#include <stddef.h>
#include <stdint.h>

/* -------- Forward decls for runtime handlers & loop index words -------- */
static void control_forth_runtime_qdo(VM *vm);
static void control_forth_runtime_do(VM *vm);
static void control_forth_runtime_loop(VM *vm);
static void control_forth_runtime_plus_loop(VM *vm);
static void control_forth_runtime_leave(VM *vm);

static void control_forth_I(VM *v);
static void control_forth_J(VM *v);

/* We use the data stack and the VM’s return stack; sizes come from vm.h */
#ifndef RETURN_STACK_SIZE
#define RETURN_STACK_SIZE STACK_SIZE
#endif

/* ===================== Compile-time control-flow stack ===================== */
#define CF_STACK_MAX 64

typedef enum {
    CF_BEGIN,     /* byte address of BEGIN target */
    CF_IF,        /* byte address where IF’s 0BRANCH literal lives */
    CF_ELSE,      /* byte address where ELSE’s BRANCH literal lives */
    CF_DO,        /* byte address of DO back-target (start of loop body) */
    CF_WHILE      /* byte address where a forward 0BRANCH literal lives */
} cf_tag_t;

typedef struct {
    size_t  addr;   /* byte address in code space (vm->memory) */
    cf_tag_t tag;
} cf_item_t;

static cf_item_t cf_stack[CF_STACK_MAX];
static int cf_sp = -1;

/* Auto-reset CF stack when VM mode flips (no edits to ':' / ';' needed) */
static int cf_last_mode = -999;
static inline void cf_epoch_sync(VM *vm) {
    if (!vm) return;
    if (cf_last_mode == -999) { cf_last_mode = vm->mode; return; } /* init */
    if (vm->mode != cf_last_mode) {
        cf_sp = -1;                 /* new definition started or ended */
        cf_last_mode = vm->mode;
        log_message(LOG_DEBUG, "CF: reset (mode transition)");
    }
}

static inline int cf_push(size_t addr, cf_tag_t tag) {
    if (cf_sp + 1 >= CF_STACK_MAX) {
        log_message(LOG_ERROR, "CF: overflow");
        return 0;
    }
    cf_stack[++cf_sp].addr = addr;
    cf_stack[cf_sp].tag = tag;
    return 1;
}
static inline int cf_pop(cf_item_t *out) {
    if (cf_sp < 0) {
        log_message(LOG_ERROR, "CF: underflow");
        return 0;
    }
    *out = cf_stack[cf_sp--];
    return 1;
}
static inline int cf_peek(cf_item_t *out) {
    if (cf_sp < 0) return 0;
    *out = cf_stack[cf_sp];
    return 1;
}

/* ===================== Dedicated LEAVE site stacks (do not touch CF stack) ===================== */
static size_t leave_addrs[CF_STACK_MAX];  /* linear stack of BRANCH literal addrs */
static int leave_sp = -1;
static int leave_mark_stack[CF_STACK_MAX]; /* one mark per DO nesting level (index into leave_addrs) */
static int leave_mark_sp = -1;

/* ===================== Runtime loop parameter stack ===================== */
#define LOOP_STACK_SIZE 16
typedef struct {
    cell_t limit;
    cell_t index;
    void  *loop_start_ip;  /* C pointer into code stream (begin of loop body) */
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

/* ===================== Runtime primitives (executed by inner interp) ===================== */
/* Offsets are in BYTES:
   ip = (cell_t*)((uint8_t*)ip + offset); */

/* (BRANCH) ( -- )  — unconditional, offset is relative to the OFFSET CELL */
static void control_forth_branch(VM *vm) {
    if (vm->rsp < 0) { vm->error = 1; log_message(LOG_ERROR, "BRANCH: RSP underflow"); return; }
    cell_t *ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];
    cell_t offset = *ip; /* BYTES, relative to THIS cell */
    ip = (cell_t*)((uint8_t*)ip + offset);
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
    log_message(LOG_DEBUG, "BRANCH: +%ld bytes", (long)offset);
}

/* (0BRANCH) ( f -- ) — conditional, same base as (BRANCH) */
static void control_forth_0branch(VM *vm) {
    if (!vm) return;

    if (vm->rsp < 0) { vm->error = 1; log_message(LOG_ERROR, "0BRANCH: RSP underflow"); return; }
    if (vm->dsp < 0) { vm->error = 1; log_message(LOG_ERROR, "0BRANCH: DSP underflow"); return; }

    cell_t *ip = (cell_t *)(uintptr_t)vm->return_stack[vm->rsp];
    cell_t flag = vm_pop(vm);

    cell_t rel = *ip;  /* RELATIVE TO THE OFFSET CELL */
    if (flag == 0) {
        ip = (cell_t *)((uint8_t*)ip + (intptr_t)rel);
        log_message(LOG_DEBUG, "0BRANCH: taken +%ld bytes", (long)rel);
    } else {
        ip = ip + 1;   /* skip the offset cell */
        log_message(LOG_DEBUG, "0BRANCH: not taken");
    }

    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
}

/* (?DO) runtime: conditionally enter loop */
/* (?DO) runtime: conditionally enter loop */
static void control_forth_runtime_qdo(VM *vm) {
    if (vm->rsp < 0) { vm->error = 1; log_message(LOG_ERROR, "?DO: RSP underflow"); return; }
    if (vm->dsp < 1) { vm->error = 1; log_message(LOG_ERROR, "?DO: DSP underflow"); return; }

    cell_t *ip = (cell_t *)(uintptr_t)vm->return_stack[vm->rsp];

    /* FIX: pop order must be (limit index --) → pop limit first, then index */
    cell_t limit = vm_pop(vm);
    cell_t index = vm_pop(vm);

    cell_t rel = *ip; /* forward BYTES, relative to THIS cell */

    if (index == limit) {
        /* skip whole loop body */
        ip = (cell_t *)((uint8_t*)ip + (intptr_t)rel);
        log_message(LOG_DEBUG, "?DO: skip loop (index==limit)");
    } else {
        /* enter loop: advance past the offset cell and push loop frame */
        ip = ip + 1;
        loop_push(limit, index, ip);
        log_message(LOG_DEBUG, "?DO: enter loop (index=%ld limit=%ld)", (long)index, (long)limit);
    }

    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
}


/* (LEAVE) runtime: pop one loop frame; branch compiled immediately after handles control flow */
static void control_forth_runtime_leave(VM *vm) {
    if (loop_sp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "LEAVE: outside DO loop");
        return;
    }
    loop_pop();
    log_message(LOG_DEBUG, "LEAVE: popped loop frame");
}

/* ===================== Compile-time helpers ===================== */

/* Emit a raw cell into the code stream (no LIT). Returns byte address in code space. */
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

/* ===================== Compile-time words ===================== */

/* IF ( flag -- ) compiles: 0BRANCH <placeholder> */
static void control_forth_if(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "IF: compile-only"); return; }

    vm_compile_call(vm, control_forth_0branch);
    size_t lit = emit_cell(vm, 0);           /* location of placeholder literal */
    if (!cf_push(lit, CF_IF)) { vm->error = 1; log_message(LOG_ERROR, "IF: CF overflow"); return; }
    log_message(LOG_DEBUG, "IF: placeholder @ %zu", lit);
}

/* ELSE compiles: BRANCH <placeholder>; patches prior 0BRANCH */
static void control_forth_else(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "ELSE: compile-only"); return; }

    cf_item_t top;
    if (!cf_peek(&top)) { vm->error = 1; log_message(LOG_ERROR, "ELSE: CF underflow"); return; }
    if (top.tag != CF_IF) {
        vm->error = 1;
        log_message(LOG_ERROR, "ELSE: already seen ELSE or missing IF");
        return;
    }

    /* Pop IF’s placeholder and patch it to jump to HERE (start of ELSE-arm) */
    (void)cf_pop(&top);
    vm_compile_call(vm, control_forth_branch);
    size_t new_lit = emit_cell(vm, 0);       /* new forward placeholder for THEN */

    cell_t off_bytes = (cell_t)((size_t)vm->here - top.addr);
    *(cell_t *)(vm->memory + top.addr) = off_bytes;

    /* Push ELSE placeholder so THEN knows to patch this one */
    if (!cf_push(new_lit, CF_ELSE)) { vm->error = 1; log_message(LOG_ERROR, "ELSE: CF overflow (new)"); return; }

    log_message(LOG_DEBUG, "ELSE: patched 0BRANCH @ %zu -> +%ld bytes; new lit @ %zu",
                top.addr, (long)off_bytes, new_lit);
}

/* THEN patches the outstanding forward jump from IF or ELSE */
static void control_forth_then(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "THEN: compile-only"); return; }

    cf_item_t top;
    if (!cf_pop(&top)) { vm->error = 1; log_message(LOG_ERROR, "THEN: CF underflow"); return; }

    if (top.tag != CF_IF && top.tag != CF_ELSE) {
        vm->error = 1;
        log_message(LOG_ERROR, "THEN: unmatched THEN");
        return;
    }

    cell_t off_bytes = (cell_t)((size_t)vm->here - top.addr);
    *(cell_t *)(vm->memory + top.addr) = off_bytes;

    log_message(LOG_DEBUG, "THEN: patched literal @ %zu -> +%ld bytes", top.addr, (long)off_bytes);
}

/* BEGIN marks a back-branch target */
static void control_forth_begin(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "BEGIN: compile-only"); return; }
    if (!cf_push(vm->here, CF_BEGIN)) { vm->error = 1; log_message(LOG_ERROR, "BEGIN: CF overflow"); return; }
    log_message(LOG_DEBUG, "BEGIN: mark @ %zu", vm->here);
}

/* UNTIL ( flag -- ) compiles: 0BRANCH <back-offset> (to BEGIN) */
static void control_forth_until(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "UNTIL: compile-only"); return; }

    cf_item_t begin;
    if (!cf_pop(&begin) || begin.tag != CF_BEGIN) { vm->error = 1; log_message(LOG_ERROR, "UNTIL: missing BEGIN"); return; }

    vm_compile_call(vm, control_forth_0branch);
    cell_t off_bytes = (cell_t)((cell_t)begin.addr - (cell_t)vm->here);
    (void)emit_cell(vm, off_bytes); /* raw back-offset */

    log_message(LOG_DEBUG, "UNTIL: back -> %zu (%ld bytes)", begin.addr, (long)off_bytes);
}

/* AGAIN compiles: BRANCH <back-offset> (to BEGIN) */
static void control_forth_again(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "AGAIN: compile-only"); return; }

    cf_item_t begin;
    if (!cf_pop(&begin) || begin.tag != CF_BEGIN) { vm->error = 1; log_message(LOG_ERROR, "AGAIN: missing BEGIN"); return; }

    vm_compile_call(vm, control_forth_branch);
    cell_t off_bytes = (cell_t)((cell_t)begin.addr - (cell_t)vm->here);
    (void)emit_cell(vm, off_bytes); /* raw back-offset */

    log_message(LOG_DEBUG, "AGAIN: back -> %zu (%ld bytes)", begin.addr, (long)off_bytes);
}

/* WHILE ( flag -- ) compiles: 0BRANCH <fwd-placeholder> */
static void control_forth_while(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "WHILE: compile-only"); return; }

    /* Must come after BEGIN; don’t pop BEGIN yet */
    cf_item_t top;
    if (!cf_peek(&top) || top.tag != CF_BEGIN) {
        vm->error = 1; log_message(LOG_ERROR, "WHILE: needs matching BEGIN"); return;
    }

    vm_compile_call(vm, control_forth_0branch);
    size_t lit = emit_cell(vm, 0); /* placeholder to be patched at REPEAT */

    if (!cf_push(lit, CF_WHILE)) { vm->error = 1; log_message(LOG_ERROR, "WHILE: CF overflow"); return; }
    log_message(LOG_DEBUG, "WHILE: placeholder @ %zu", lit);
}

/* REPEAT ( -- ) compiles: BRANCH <back-to-BEGIN>, then patches WHILE forward jump */
static void control_forth_repeat(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "REPEAT: compile-only"); return; }

    cf_item_t while_lit;
    if (!cf_pop(&while_lit) || while_lit.tag != CF_WHILE) {
        vm->error = 1; log_message(LOG_ERROR, "REPEAT: missing WHILE"); return;
    }

    cf_item_t begin;
    if (!cf_pop(&begin) || begin.tag != CF_BEGIN) {
        vm->error = 1; log_message(LOG_ERROR, "REPEAT: missing BEGIN"); return;
    }

    /* Back branch to BEGIN */
    vm_compile_call(vm, control_forth_branch);
    cell_t back = (cell_t)((cell_t)begin.addr - (cell_t)vm->here);
    (void)emit_cell(vm, back);

    /* Patch the WHILE’s 0BRANCH to land here */
    cell_t fwd = (cell_t)((size_t)vm->here - while_lit.addr);
    *(cell_t *)(vm->memory + while_lit.addr) = fwd;

    log_message(LOG_DEBUG, "REPEAT: patched WHILE @ %zu -> +%ld; back to %zu (%ld)",
                while_lit.addr, (long)fwd, begin.addr, (long)back);
}

/* ?DO ( limit index -- ) */
static void control_forth_qdo(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "?DO: compile-only"); return; }

    vm_compile_call(vm, control_forth_runtime_qdo);
    size_t fwd_lit = emit_cell(vm, 0);       /* forward placeholder to patch at LOOP/+LOOP */

    /* Back target is HERE (start of loop body) */
    if (!cf_push(vm->here, CF_DO))    { vm->error = 1; log_message(LOG_ERROR, "?DO: CF overflow (DO)"); return; }
    if (!cf_push(fwd_lit, CF_WHILE))  { vm->error = 1; log_message(LOG_ERROR, "?DO: CF overflow (fwd)"); return; }

    /* Mark LEAVE stack level for this DO */
    leave_mark_stack[++leave_mark_sp] = leave_sp;

    log_message(LOG_DEBUG, "?DO: fwd lit @ %zu, back mark @ %zu (leave_mark=%d)", fwd_lit, vm->here, leave_mark_stack[leave_mark_sp]);
}

/* DO ( limit index -- ) compiles: runtime DO and marks back target for LOOP/+LOOP */
static void control_forth_do(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "DO: compile-only"); return; }

    vm_compile_call(vm, control_forth_runtime_do);
    if (!cf_push(vm->here, CF_DO)) { vm->error = 1; log_message(LOG_ERROR, "DO: CF overflow"); return; }

    /* Mark LEAVE stack level for this DO */
    leave_mark_stack[++leave_mark_sp] = leave_sp;

    log_message(LOG_DEBUG, "DO: mark @ %zu (leave_mark=%d)", vm->here, leave_mark_stack[leave_mark_sp]);
}

/* LEAVE ( -- ) compile-only
   Emits: (LEAVE)  (BRANCH) <fwd-placeholder>
   At LOOP/+LOOP we patch all placeholders pushed since this DO's mark to HERE. */
static void control_forth_leave(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "LEAVE: compile-only"); return; }

    /* Must be inside a DO…LOOP frame */
    int seen_do = 0;
    for (int i = cf_sp; i >= 0; --i) { if (cf_stack[i].tag == CF_DO) { seen_do = 1; break; } }
    if (!seen_do || leave_mark_sp < 0) { vm->error = 1; log_message(LOG_ERROR, "LEAVE: needs DO"); return; }

    vm_compile_call(vm, control_forth_runtime_leave);
    vm_compile_call(vm, control_forth_branch);
    size_t lit = emit_cell(vm, 0);

    if (leave_sp + 1 >= CF_STACK_MAX) { vm->error = 1; log_message(LOG_ERROR, "LEAVE: too many leaves"); return; }
    leave_addrs[++leave_sp] = lit;

    log_message(LOG_DEBUG, "LEAVE: placeholder @ %zu (leave_sp=%d)", lit, leave_sp);
}

/* LOOP compiles: runtime LOOP <back-offset> + patches (?DO) forward and LEAVEs */
static void control_forth_loop(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "LOOP: compile-only"); return; }

    /* Optional forward placeholder from ?DO */
    cf_item_t qdo_fwd; int have_qdo = 0;
    cf_item_t top;
    if (cf_peek(&top) && top.tag == CF_WHILE) {
        (void)cf_pop(&qdo_fwd);
        have_qdo = 1;
    }

    /* Required DO back mark */
    cf_item_t do_mark;
    if (!cf_pop(&do_mark) || do_mark.tag != CF_DO) { vm->error = 1; log_message(LOG_ERROR, "LOOP: missing DO"); return; }

    vm_compile_call(vm, control_forth_runtime_loop);
    cell_t off_bytes = (cell_t)((cell_t)do_mark.addr - (cell_t)vm->here);
    (void)emit_cell(vm, off_bytes); /* raw back-offset */

    /* Patch ?DO forward placeholder */
    if (have_qdo) {
        cell_t fwd = (cell_t)((size_t)vm->here - qdo_fwd.addr);
        *(cell_t *)(vm->memory + qdo_fwd.addr) = fwd;
        log_message(LOG_DEBUG, "LOOP: patched ?DO fwd @ %zu -> +%ld", qdo_fwd.addr, (long)fwd);
    }

    /* Patch all LEAVEs created since this DO */
    if (leave_mark_sp < 0) { vm->error = 1; log_message(LOG_ERROR, "LOOP: LEAVE mark stack underflow"); return; }
    int mark = leave_mark_stack[leave_mark_sp--];
    for (int i = leave_sp; i > mark; --i) {
        size_t addr = leave_addrs[i];
        cell_t fwd = (cell_t)((size_t)vm->here - addr);
        *(cell_t *)(vm->memory + addr) = fwd;
        log_message(LOG_DEBUG, "LEAVE: patched @ %zu -> +%ld", addr, (long)fwd);
    }
    leave_sp = mark;

    log_message(LOG_DEBUG, "LOOP: back -> %zu (%ld bytes)", do_mark.addr, (long)off_bytes);
}

/* +LOOP compiles: runtime +LOOP <back-offset> + same patching */
static void control_forth_plus_loop(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) { vm->error = 1; log_message(LOG_ERROR, "+LOOP: compile-only"); return; }

    /* Optional forward placeholder from ?DO */
    cf_item_t qdo_fwd; int have_qdo = 0;
    cf_item_t top;
    if (cf_peek(&top) && top.tag == CF_WHILE) {
        (void)cf_pop(&qdo_fwd);
        have_qdo = 1;
    }

    /* Required DO back mark */
    cf_item_t do_mark;
    if (!cf_pop(&do_mark) || do_mark.tag != CF_DO) { vm->error = 1; log_message(LOG_ERROR, "+LOOP: missing DO"); return; }

    vm_compile_call(vm, control_forth_runtime_plus_loop);
    cell_t off_bytes = (cell_t)((cell_t)do_mark.addr - (cell_t)vm->here);
    (void)emit_cell(vm, off_bytes); /* raw back-offset */

    if (have_qdo) {
        cell_t fwd = (cell_t)((size_t)vm->here - qdo_fwd.addr);
        *(cell_t *)(vm->memory + qdo_fwd.addr) = fwd;
        log_message(LOG_DEBUG, "+LOOP: patched ?DO fwd @ %zu -> +%ld", qdo_fwd.addr, (long)fwd);
    }

    if (leave_mark_sp < 0) { vm->error = 1; log_message(LOG_ERROR, "+LOOP: LEAVE mark stack underflow"); return; }
    int mark = leave_mark_stack[leave_mark_sp--];
    for (int i = leave_sp; i > mark; --i) {
        size_t addr = leave_addrs[i];
        cell_t fwd = (cell_t)((size_t)vm->here - addr);
        *(cell_t *)(vm->memory + addr) = fwd;
        log_message(LOG_DEBUG, "LEAVE: patched @ %zu -> +%ld", addr, (long)fwd);
    }
    leave_sp = mark;

    log_message(LOG_DEBUG, "+LOOP: back -> %zu (%ld bytes)", do_mark.addr, (long)off_bytes);
}

/* ===================== Runtime DO/LOOP/+LOOP bodies ===================== */

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
        ip = (cell_t*)((uint8_t*)ip + off);
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
        ip = (cell_t*)((uint8_t*)ip + off);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "Runtime +LOOP: continue (index=%ld)", (long)newv);
    } else {
        loop_pop();
        ip++; /* skip literal */
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "Runtime +LOOP: exit");
    }
}

/* ===================== Loop index words (I, J) ===================== */

static void control_forth_I(VM *v) {
    if (loop_sp < 0) {
        v->error = 1;
        log_message(LOG_ERROR, "I: outside DO loop");
        return;
    }
    vm_push(v, loop_stack[loop_sp].index);
}

static void control_forth_J(VM *v) {
    if (loop_sp < 1) {
        v->error = 1;
        log_message(LOG_ERROR, "J: needs nested DO loops");
        return;
    }
    vm_push(v, loop_stack[loop_sp - 1].index);
}

/* ===================== Registration ===================== */
void register_control_words(VM *vm) {
    if (!vm) return;

    /* Internal branches & runtimes (for vm_compile_call lookups) */
    register_word(vm, "(BRANCH)",   control_forth_branch);
    register_word(vm, "(0BRANCH)",  control_forth_0branch);
    register_word(vm, "(?DO)",      control_forth_runtime_qdo);
    register_word(vm, "(DO)",       control_forth_runtime_do);
    register_word(vm, "(LOOP)",     control_forth_runtime_loop);
    register_word(vm, "(+LOOP)",    control_forth_runtime_plus_loop);
    register_word(vm, "(LEAVE)",    control_forth_runtime_leave);

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

    /* DO-family + ?DO + LEAVE */
    register_word(vm, "?DO",     control_forth_qdo);         vm_make_immediate(vm);
    register_word(vm, "DO",      control_forth_do);          vm_make_immediate(vm);
    register_word(vm, "LOOP",    control_forth_loop);        vm_make_immediate(vm);
    register_word(vm, "+LOOP",   control_forth_plus_loop);   vm_make_immediate(vm);
    register_word(vm, "LEAVE",   control_forth_leave);       vm_make_immediate(vm);

    /* Loop indices */
    register_word(vm, "I",       control_forth_I);
    register_word(vm, "J",       control_forth_J);

    log_message(LOG_INFO, "Registering FORTH-79 control flow words (+?DO, LEAVE w/ side-stack, internal runtimes)");
}
