/**
 * @file control_words.c
 * @brief FORTH-79 Standard control flow implementation using ANSI C99
 * @details Provides control flow words and runtime support for the StarForth interpreter
 *
 * Design notes (FORTH-79 compliance):
 * - Exactly TWO stacks at runtime: parameter (data) and return.
 * - Loop parameters (index, limit) are stored on the RETURN STACK beneath IP.
 *   RS layout within a DO…LOOP (top on right):
 *     …, limit_outer, index_outer, ip_outer, limit_inner, index_inner, ip_inner
 *   I  -> RS[-1] (current index)
 *   J  -> RS[-3] (outer index)   <-- matches code below
 * - Compile-time: forward/backpatch via a small CF (control-flow) stack.
 * - Runtime branch words mutate the IP at top of RS.
 *
 * @copyright StarForth project contributors
 * @license CC0-1.0
 */

#include "include/control_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"

#include <stddef.h>
#include <stdint.h>

/** @name Internal Runtime Functions
 * Forward declarations for internal runtime helpers and control flow primitives
 * @{
 */

/** Execute unconditional branch */
static void control_forth_branch(VM * vm);

/** Execute conditional branch based on top of stack */
static void control_forth_0branch(VM * vm);

/** Runtime handler for ?DO loops */
static void control_forth_runtime_qdo(VM * vm);

/** Runtime handler for DO loops */
static void control_forth_runtime_do(VM * vm);

/** Runtime handler for LOOP */
static void control_forth_runtime_loop(VM * vm);

/** Runtime handler for +LOOP */
static void control_forth_runtime_plus_loop(VM * vm);

/** Runtime handler for LEAVE */
static void control_forth_runtime_leave(VM * vm);

/** Get current loop index */
static void control_forth_I(VM * vm);

/** Get outer loop index */
static void control_forth_J(VM * vm);

/** Exit from current definition */
static void control_forth_EXIT(VM * vm);

/** @} */

/**
 * @defgroup cf Control Flow Stack
 * Compile-time control flow stack for tracking branch targets and loop structures
 * @{
 */

/** Maximum depth of control flow stack */
#define CF_STACK_MAX 64

/** Control flow item types */
typedef enum {
    CF_BEGIN, /**< Address of BEGIN target */
    CF_IF, /**< Address of IF's 0BRANCH literal */
    CF_ELSE, /**< Address of ELSE's BRANCH literal */
    CF_WHILE, /**< Address of WHILE's 0BRANCH literal (paired with prior BEGIN) */
    CF_DO /**< Address of loop body start (back target for LOOP/+LOOP) */
} cf_tag_t;

/** @} */

typedef struct {
    size_t addr; /* byte offset in vm->memory used for patching/back edges */
    cf_tag_t tag;
} cf_item_t;

static cf_item_t cf_stack[CF_STACK_MAX];
static int cf_sp = -1;

/* Reset CF stack on mode transitions (between INTERPRET/COMPILE) */
static int cf_last_mode = -999;

static inline void cf_epoch_sync(VM *vm) {
    if (!vm) return;
    if (cf_last_mode == -999) {
        cf_last_mode = vm->mode;
        return;
    }
    if (vm->mode != cf_last_mode) {
        cf_sp = -1;
        cf_last_mode = vm->mode;
        log_message(LOG_DEBUG, "CF: reset (mode transition)");
    }
}

static inline int cf_push_item(cf_tag_t tag, size_t mark) {
    if (cf_sp + 1 >= CF_STACK_MAX) {
        log_message(LOG_ERROR, "CF: overflow");
        return 0;
    }
    ++cf_sp;
    cf_stack[cf_sp].tag = tag;
    cf_stack[cf_sp].addr = mark;
    return 1;
}

static inline int cf_pop_item(cf_item_t *out) {
    if (cf_sp < 0) {
        log_message(LOG_ERROR, "CF: underflow");
        return 0;
    }
    if (out) {
        *out = cf_stack[cf_sp];
    }
    --cf_sp;
    return 1;
}

static inline int cf_peek_item(cf_item_t *out) {
    if (cf_sp < 0) return 0;
    if (out) {
        *out = cf_stack[cf_sp];
    }
    return 1;
}

/* ===================== LEAVE patching (compile-time) ===================== */
/* We collect BRANCH literals for LEAVE sites and patch them at LOOP/+LOOP. */

static size_t leave_addrs[CF_STACK_MAX];
static int leave_sp = -1;

/* One mark per DO nesting: record leave_sp at DO/?DO; at LOOP/+LOOP patch and restore. */
static int leave_mark_stack[CF_STACK_MAX];
static int leave_mark_sp = -1;

/* ===================== Low-level compile helpers ===================== */

static inline size_t emit_cell(VM *vm, cell_t value) {
    if (vm->mode != MODE_COMPILE) {
        vm_push(vm, value);
        return (size_t) -1;
    }
    vm_align(vm);
    cell_t *addr = (cell_t *) vm_allot(vm, sizeof(cell_t));
    if (!addr) {
        vm->error = 1;
        log_message(LOG_ERROR, "emit_cell: out of space");
        return (size_t) -1;
    }
    *addr = value;
    return (size_t) ((uint8_t *) addr - vm->memory);
}

/**
 * @defgroup runtime Runtime Branch Helpers
 * Runtime support for control flow operations
 * @details All functions operate on the per-step return IP (top of RS).
 *          All offsets are in BYTES.
 * @{
 */

/** 
 * Runtime unconditional branch
 * @param vm Virtual machine state
 */
static void control_forth_branch(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "BRANCH: RSP underflow");
        return;
    }
    cell_t *ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp];
    cell_t rel = *ip;
    ip = (cell_t *) ((uint8_t *) ip + (intptr_t) rel);
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
    ip;
    log_message(LOG_DEBUG, "BRANCH: +%ld bytes", (long) rel);
}

/* (0BRANCH)  ( f -- ) */
static void control_forth_0branch(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "0BRANCH: RSP underflow");
        return;
    }
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "0BRANCH: DSP underflow");
        return;
    }
    cell_t *ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp];
    cell_t flag = vm_pop(vm);
    cell_t rel = *ip;
    if (flag == 0) {
        ip = (cell_t *) ((uint8_t *) ip + (intptr_t) rel);
        log_message(LOG_DEBUG, "0BRANCH: taken +%ld", (long) rel);
    } else {
        ip = ip + 1;
        log_message(LOG_DEBUG, "0BRANCH: not taken");
    }
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
    ip;
}

/* (?DO)  ( limit index -- ) */
static void control_forth_runtime_qdo(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "?DO: RSP underflow");
        return;
    }
    if (vm->dsp < 1) {
        vm->error = 1;
        log_message(LOG_ERROR, "?DO: DSP underflow");
        return;
    }

    cell_t index = vm_pop(vm);
    cell_t limit = vm_pop(vm);

    cell_t *ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp];
    cell_t rel = *ip;

    if (index == limit) {
        ip = (cell_t *) ((uint8_t *) ip + (intptr_t) rel);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
        ip;
        log_message(LOG_DEBUG, "?DO: empty -> +%ld", (long) rel);
        return;
    }

    /* enter loop at body (skip rel) and insert (limit,index) under IP */
    ip = ip + 1;
    if (vm->rsp + 2 >= STACK_SIZE) {
        vm->error = 1;
        log_message(LOG_ERROR, "?DO: RSTACK overflow");
        return;
    }
    vm->return_stack[vm->rsp + 2] = (cell_t)(uintptr_t)
    ip;
    vm->return_stack[vm->rsp + 1] = index;
    vm->return_stack[vm->rsp + 0] = limit;
    vm->rsp += 2;

    log_message(LOG_DEBUG, "?DO: enter (index=%ld limit=%ld)", (long) index, (long) limit);
}

/* (DO)  ( limit index -- ) */
static void control_forth_runtime_do(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "DO: RSP underflow");
        return;
    }
    if (vm->dsp < 1) {
        vm->error = 1;
        log_message(LOG_ERROR, "DO: DSP underflow");
        return;
    }

    cell_t index = vm_pop(vm);
    cell_t limit = vm_pop(vm);

    if (vm->rsp + 2 >= STACK_SIZE) {
        vm->error = 1;
        log_message(LOG_ERROR, "DO: RSTACK overflow");
        return;
    }
    cell_t ip_cell = vm->return_stack[vm->rsp];
    vm->return_stack[vm->rsp + 2] = ip_cell; /* move IP up */
    vm->return_stack[vm->rsp + 1] = index;
    vm->return_stack[vm->rsp + 0] = limit;
    vm->rsp += 2;

    log_message(LOG_DEBUG, "DO: enter (index=%ld limit=%ld)", (long) index, (long) limit);
}

/* (LOOP) */
static void control_forth_runtime_loop(VM *vm) {
    if (vm->rsp < 2) {
        vm->error = 1;
        log_message(LOG_ERROR, "LOOP: missing loop frame");
        return;
    }

    cell_t *ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp];
    cell_t *idxp = &vm->return_stack[vm->rsp - 1];
    cell_t *limp = &vm->return_stack[vm->rsp - 2];

    (*idxp) += 1;
    cell_t back = *ip; /* BYTES */

    if (*idxp < *limp) {
        ip = (cell_t *) ((uint8_t *) ip + (intptr_t) back);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
        ip;
        log_message(LOG_DEBUG, "LOOP: continue (index=%ld)", (long) *idxp);
    } else {
        vm->rsp -= 2; /* drop INDEX,LIMIT */
        ip = ip + 1; /* skip back-offset */
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
        ip;
        log_message(LOG_DEBUG, "LOOP: exit");
    }
}

/* (+LOOP)  ( n -- ) */
static void control_forth_runtime_plus_loop(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "+LOOP: DSP underflow");
        return;
    }
    if (vm->rsp < 2) {
        vm->error = 1;
        log_message(LOG_ERROR, "+LOOP: missing loop frame");
        return;
    }

    cell_t n = vm_pop(vm);
    cell_t *ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp];
    cell_t *idxp = &vm->return_stack[vm->rsp - 1];
    cell_t *limp = &vm->return_stack[vm->rsp - 2];

    cell_t newv = (*idxp) + n;
    *idxp = newv;

    cell_t back = *ip; /* BYTES */
    int cont = (n >= 0) ? (newv < *limp) : (newv >= *limp);

    if (cont) {
        ip = (cell_t *) ((uint8_t *) ip + (intptr_t) back);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
        ip;
        log_message(LOG_DEBUG, "+LOOP: continue (index=%ld)", (long) newv);
    } else {
        vm->rsp -= 2;
        ip = ip + 1;
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
        ip;
        log_message(LOG_DEBUG, "+LOOP: exit");
    }
}

/* (LEAVE)  ( -- ) — force loop to exit at next LOOP/+LOOP */
static void control_forth_runtime_leave(VM *vm) {
    if (vm->rsp < 2) {
        vm->error = 1;
        log_message(LOG_ERROR, "LEAVE: outside loop");
        return;
    }
    vm->return_stack[vm->rsp - 1] = vm->return_stack[vm->rsp - 2]; /* index = limit */
    log_message(LOG_DEBUG, "LEAVE: flagged exit");
}

/* UNLOOP ( -- ) — discard loop parameters from return stack */
static void control_forth_UNLOOP(VM *vm) {
    if (vm->rsp < 2) {
        vm->error = 1;
        log_message(LOG_ERROR, "UNLOOP: outside loop (return stack underflow)");
        return;
    }
    /* Return stack layout: ..., limit (rsp-2), index (rsp-1), ip (rsp) */
    /* Move IP from rsp down to rsp-2, then decrement rsp by 2 */
    vm->return_stack[vm->rsp - 2] = vm->return_stack[vm->rsp];
    vm->rsp -= 2;
    log_message(LOG_DEBUG, "UNLOOP: removed loop parameters (limit, index)");
}

/* I ( -- i ) */
static void control_forth_I(VM *vm) {
    if (vm->rsp < 2) {
        vm->error = 1;
        log_message(LOG_ERROR, "I: outside DO loop");
        return;
    }
    vm_push(vm, vm->return_stack[vm->rsp - 1]); /* INDEX */
}

/* J ( -- j ) — next-outer loop index */
static void control_forth_J(VM *vm) {
    if (vm->rsp < 4) {
        vm->error = 1;
        log_message(LOG_ERROR, "J: needs nested DO loops");
        return;
    }
    vm_push(vm, vm->return_stack[vm->rsp - 3]); /* outer INDEX */
}

/* EXIT — one-shot return from current colon definition (guarded) */
static void control_forth_EXIT(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "EXIT: interpret-time use is invalid");
        return;
    }
    vm->exit_colon = 1;
    log_message(LOG_DEBUG, "EXIT: return from colon");
}

/* ===================== Compile-time words ===================== */

static void control_forth_if(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "IF: compile-only");
        return;
    }
    vm_compile_call(vm, control_forth_0branch);
    size_t lit = emit_cell(vm, 0);
    if (!cf_push_item(CF_IF, lit)) {
        vm->error = 1;
        return;
    }
    log_message(LOG_DEBUG, "IF: placeholder @ %zu", lit);
}

static void control_forth_else(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "ELSE: compile-only");
        return;
    }
    cf_item_t it;
    if (!cf_peek_item(&it) || it.tag != CF_IF) {
        vm->error = 1;
        log_message(LOG_ERROR, "ELSE: missing IF");
        return;
    }
    (void) cf_pop_item(&it);
    vm_compile_call(vm, control_forth_branch);
    size_t new_lit = emit_cell(vm, 0);
    cell_t off = (cell_t)((size_t) vm->here - it.addr);
    *(cell_t *) (vm->memory + it.addr) = off;
    if (!cf_push_item(CF_ELSE, new_lit)) {
        vm->error = 1;
        return;
    }
    log_message(LOG_DEBUG, "ELSE: patched IF @ %zu -> +%ld; new lit @ %zu", it.addr, (long) off, new_lit);
}

static void control_forth_then(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "THEN: compile-only");
        return;
    }
    cf_item_t it;
    if (!cf_pop_item(&it) || (it.tag != CF_IF && it.tag != CF_ELSE)) {
        vm->error = 1;
        log_message(LOG_ERROR, "THEN: unmatched");
        return;
    }
    cell_t off = (cell_t)((size_t) vm->here - it.addr);
    *(cell_t *) (vm->memory + it.addr) = off;
    log_message(LOG_DEBUG, "THEN: patched lit @ %zu -> +%ld", it.addr, (long) off);
}

static void control_forth_begin(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "BEGIN: compile-only");
        return;
    }
    if (!cf_push_item(CF_BEGIN, vm->here)) {
        vm->error = 1;
        return;
    }
    log_message(LOG_DEBUG, "BEGIN: mark @ %zu", vm->here);
}

static void control_forth_until(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "UNTIL: compile-only");
        return;
    }
    cf_item_t begin;
    if (!cf_pop_item(&begin) || begin.tag != CF_BEGIN) {
        vm->error = 1;
        log_message(LOG_ERROR, "UNTIL: missing BEGIN");
        return;
    }
    vm_compile_call(vm, control_forth_0branch);
    cell_t back = (cell_t)((cell_t) begin.addr - (cell_t) vm->here);
    (void) emit_cell(vm, back);
    log_message(LOG_DEBUG, "UNTIL: back -> %zu (%ld bytes)", begin.addr, (long) back);
}

static void control_forth_again(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "AGAIN: compile-only");
        return;
    }
    cf_item_t begin;
    if (!cf_pop_item(&begin) || begin.tag != CF_BEGIN) {
        vm->error = 1;
        log_message(LOG_ERROR, "AGAIN: missing BEGIN");
        return;
    }
    vm_compile_call(vm, control_forth_branch);
    cell_t back = (cell_t)((cell_t) begin.addr - (cell_t) vm->here);
    (void) emit_cell(vm, back);
    log_message(LOG_DEBUG, "AGAIN: back -> %zu (%ld bytes)", begin.addr, (long) back);
}

static void control_forth_while(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "WHILE: compile-only");
        return;
    }
    cf_item_t b;
    if (!cf_peek_item(&b) || b.tag != CF_BEGIN) {
        vm->error = 1;
        log_message(LOG_ERROR, "WHILE: needs BEGIN");
        return;
    }
    vm_compile_call(vm, control_forth_0branch);
    size_t lit = emit_cell(vm, 0);
    if (!cf_push_item(CF_WHILE, lit)) {
        vm->error = 1;
        return;
    }
    log_message(LOG_DEBUG, "WHILE: placeholder @ %zu", lit);
}

static void control_forth_repeat(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "REPEAT: compile-only");
        return;
    }
    cf_item_t w, b;
    if (!cf_pop_item(&w) || w.tag != CF_WHILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "REPEAT: missing WHILE");
        return;
    }
    if (!cf_pop_item(&b) || b.tag != CF_BEGIN) {
        vm->error = 1;
        log_message(LOG_ERROR, "REPEAT: missing BEGIN");
        return;
    }
    vm_compile_call(vm, control_forth_branch);
    cell_t back = (cell_t)((cell_t) b.addr - (cell_t) vm->here);
    (void) emit_cell(vm, back);
    cell_t fwd = (cell_t)((size_t) vm->here - w.addr);
    *(cell_t *) (vm->memory + w.addr) = fwd;
    log_message(LOG_DEBUG, "REPEAT: WHILE @ %zu -> +%ld; back=%ld to %zu", w.addr, (long) fwd, (long) back, b.addr);
}

/* ?DO ( limit index -- ) compile */
static void control_forth_qdo(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "?DO: compile-only");
        return;
    }
    vm_compile_call(vm, control_forth_runtime_qdo);
    size_t fwd_lit = emit_cell(vm, 0);
    if (!cf_push_item(CF_DO, vm->here)) {
        vm->error = 1;
        return;
    } /* back target for LOOP */
    if (!cf_push_item(CF_WHILE, fwd_lit)) {
        vm->error = 1;
        return;
    } /* forward to loop-end */
    leave_mark_stack[++leave_mark_sp] = leave_sp;
    log_message(LOG_DEBUG, "?DO: fwd lit @ %zu; back mark=%d", fwd_lit, leave_mark_stack[leave_mark_sp]);
}

/* DO ( limit index -- ) compile */
static void control_forth_do(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "DO: compile-only");
        return;
    }
    vm_compile_call(vm, control_forth_runtime_do);
    if (!cf_push_item(CF_DO, vm->here)) {
        vm->error = 1;
        return;
    }
    leave_mark_stack[++leave_mark_sp] = leave_sp;
    log_message(LOG_DEBUG, "DO: mark @ %zu; leave_mark=%d", vm->here, leave_mark_stack[leave_mark_sp]);
}

/* LEAVE — compile runtime LEAVE plus BRANCH <placeholder>, collect patch site */
static void control_forth_leave(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "LEAVE: compile-only");
        return;
    }

    /* verify inside DO */
    int seen_do = 0;
    for (int i = cf_sp; i >= 0; --i) {
        if (cf_stack[i].tag == CF_DO) {
            seen_do = 1;
            break;
        }
    }
    if (!seen_do || leave_mark_sp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "LEAVE: needs DO");
        return;
    }

    vm_compile_call(vm, control_forth_runtime_leave);
    vm_compile_call(vm, control_forth_branch);
    size_t lit = emit_cell(vm, 0);
    if (leave_sp + 1 >= CF_STACK_MAX) {
        vm->error = 1;
        log_message(LOG_ERROR, "LEAVE: too many sites");
        return;
    }
    leave_addrs[++leave_sp] = lit;
    log_message(LOG_DEBUG, "LEAVE: site lit @ %zu (leave_sp=%d)", lit, leave_sp);
}

/* LOOP — compile runtime LOOP + backoffset; patch ?DO fwd and LEAVE sites */
static void control_forth_loop(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "LOOP: compile-only");
        return;
    }

    /* Optional ?DO forward */
    cf_item_t maybe_qdo;
    int have_qdo = 0;
    cf_item_t top;
    if (cf_peek_item(&top) && top.tag == CF_WHILE) {
        (void) cf_pop_item(&maybe_qdo);
        have_qdo = 1;
    }

    /* Required DO back mark */
    cf_item_t do_mark;
    if (!cf_pop_item(&do_mark) || do_mark.tag != CF_DO) {
        vm->error = 1;
        log_message(LOG_ERROR, "LOOP: missing DO");
        return;
    }

    vm_compile_call(vm, control_forth_runtime_loop);
    cell_t back = (cell_t)((cell_t) do_mark.addr - (cell_t) vm->here);
    (void) emit_cell(vm, back);

    if (have_qdo) {
        cell_t fwd = (cell_t)((size_t) vm->here - maybe_qdo.addr);
        *(cell_t *) (vm->memory + maybe_qdo.addr) = fwd;
        log_message(LOG_DEBUG, "LOOP: patched ?DO @ %zu -> +%ld", maybe_qdo.addr, (long) fwd);
    }

    if (leave_mark_sp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "LOOP: LEAVE mark underflow");
        return;
    }
    int mark = leave_mark_stack[leave_mark_sp--];
    for (int i = leave_sp; i > mark; --i) {
        size_t addr = leave_addrs[i];
        cell_t fwd = (cell_t)((size_t) vm->here - addr);
        *(cell_t *) (vm->memory + addr) = fwd;
        log_message(LOG_DEBUG, "LEAVE: patched @ %zu -> +%ld", addr, (long) fwd);
    }
    leave_sp = mark;

    log_message(LOG_DEBUG, "LOOP: back -> %zu (%ld bytes)", do_mark.addr, (long) back);
}

/* +LOOP — same as LOOP but with runtime +LOOP */
static void control_forth_plus_loop(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "+LOOP: compile-only");
        return;
    }

    cf_item_t maybe_qdo;
    int have_qdo = 0;
    cf_item_t top;
    if (cf_peek_item(&top) && top.tag == CF_WHILE) {
        (void) cf_pop_item(&maybe_qdo);
        have_qdo = 1;
    }

    cf_item_t do_mark;
    if (!cf_pop_item(&do_mark) || do_mark.tag != CF_DO) {
        vm->error = 1;
        log_message(LOG_ERROR, "+LOOP: missing DO");
        return;
    }

    vm_compile_call(vm, control_forth_runtime_plus_loop);
    cell_t back = (cell_t)((cell_t) do_mark.addr - (cell_t) vm->here);
    (void) emit_cell(vm, back);

    if (have_qdo) {
        cell_t fwd = (cell_t)((size_t) vm->here - maybe_qdo.addr);
        *(cell_t *) (vm->memory + maybe_qdo.addr) = fwd;
        log_message(LOG_DEBUG, "+LOOP: patched ?DO @ %zu -> +%ld", maybe_qdo.addr, (long) fwd);
    }

    if (leave_mark_sp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "+LOOP: LEAVE mark underflow");
        return;
    }
    int mark = leave_mark_stack[leave_mark_sp--];
    for (int i = leave_sp; i > mark; --i) {
        size_t addr = leave_addrs[i];
        cell_t fwd = (cell_t)((size_t) vm->here - addr);
        *(cell_t *) (vm->memory + addr) = fwd;
        log_message(LOG_DEBUG, "LEAVE: patched @ %zu -> +%ld", addr, (long) fwd);
    }
    leave_sp = mark;

    log_message(LOG_DEBUG, "+LOOP: back -> %zu (%ld bytes)", do_mark.addr, (long) back);
}

/* ===================== Registration ===================== */

void register_control_words(VM *vm) {
    if (!vm) return;

    /* Internal branches & runtimes */
    register_word(vm, "(BRANCH)", control_forth_branch);
    register_word(vm, "(0BRANCH)", control_forth_0branch);
    register_word(vm, "(?DO)", control_forth_runtime_qdo);
    register_word(vm, "(DO)", control_forth_runtime_do);
    register_word(vm, "(LOOP)", control_forth_runtime_loop);
    register_word(vm, "(+LOOP)", control_forth_runtime_plus_loop);
    register_word(vm, "(LEAVE)", control_forth_runtime_leave);

    /* IF/ELSE/THEN */
    register_word(vm, "IF", control_forth_if);
    vm_make_immediate(vm);
    register_word(vm, "ELSE", control_forth_else);
    vm_make_immediate(vm);
    register_word(vm, "THEN", control_forth_then);
    vm_make_immediate(vm);

    /* BEGIN/WHILE/REPEAT/AGAIN/UNTIL */
    register_word(vm, "BEGIN", control_forth_begin);
    vm_make_immediate(vm);
    register_word(vm, "WHILE", control_forth_while);
    vm_make_immediate(vm);
    register_word(vm, "REPEAT", control_forth_repeat);
    vm_make_immediate(vm);
    register_word(vm, "AGAIN", control_forth_again);
    vm_make_immediate(vm);
    register_word(vm, "UNTIL", control_forth_until);
    vm_make_immediate(vm);

    /* DO/?DO/LOOP/+LOOP/LEAVE */
    register_word(vm, "?DO", control_forth_qdo);
    vm_make_immediate(vm);
    register_word(vm, "DO", control_forth_do);
    vm_make_immediate(vm);
    register_word(vm, "LOOP", control_forth_loop);
    vm_make_immediate(vm);
    register_word(vm, "+LOOP", control_forth_plus_loop);
    vm_make_immediate(vm);
    register_word(vm, "LEAVE", control_forth_leave);
    vm_make_immediate(vm);

    /* Loop indices, UNLOOP & EXIT */
    register_word(vm, "I", control_forth_I);
    register_word(vm, "J", control_forth_J);
    register_word(vm, "UNLOOP", control_forth_UNLOOP);
    register_word(vm, "EXIT", control_forth_EXIT);
}