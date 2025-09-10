/*
                                 ***   StarForth   ***
  control_words.c - FORTH-79 Standard and ANSI C99 ONLY

  This module implements the FORTH-79 control-flow words using ONLY the
  data stack and the return stack. Loop control parameters (index, limit)
  are stored on the RETURN STACK *below* the per-step IP cell your VM
  keeps on RS top. No third runtime stack is used.

  Byte-offset branches: all compiled branch offsets are in BYTES and
  are relative to the literal cell read by the runtime primitive, matching
  your debug traces and existing codegen.

  Runtime convention (inside a colon):
      RS top (vm->return_stack[vm->rsp]) holds the current IP (cell_t* as cell).
      Under that, this module inserts loop control as:
          [ ... ,  index (rsp-1) , limit (rsp-2) ,  <-- deeper cells ... ]
      So:
          I pushes RS[rsp-1]
          J pushes RS[rsp-3]   (index of next-outer DO)

  Compile-time:
      We use a small control-flow stack for forward/backpatching and a simple
      LEAVE-site list (compile-time only). These are not runtime stacks; they
      only exist during compilation to hold addresses to patch.

  Public domain / CC0 — No warranty.
*/

#include "include/control_words.h"
#include "../../include/vm.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

#include <stdint.h>
#include <stddef.h>

/* ================================================================
 * Helpers for RS layout used by this module
 * ----------------------------------------------------------------
 * RS TOS: vm->return_stack[vm->rsp] == current IP (cell_t-cast of pointer)
 * Immediately below (toward lower indices) we keep loop params:
 *   index at (rsp-1), limit at (rsp-2), then deeper frames/returns...
 * ================================================================ */

static inline int rs_has(VM *vm, int n) {
    /* need at least n cells on RS */
    return vm->rsp >= (n - 1);
}

static inline int rs_room(VM *vm, int n) {
    /* need room for n more pushes */
    return (vm->rsp + n) < STACK_SIZE;
}

/* Insert (limit,index) directly UNDER the IP cell at RS top.
   After insertion:
      RS[rsp]   = IP (restored)
      RS[rsp-1] = index
      RS[rsp-2] = limit
*/
static inline int rs_insert_loop_under_ip(VM *vm, cell_t limit, cell_t index) {
    if (!rs_has(vm, 1) || !rs_room(vm, 2)) return 0;
    cell_t ipcell = vm->return_stack[vm->rsp];
    vm->return_stack[vm->rsp] = limit; /* overwrite IP slot with limit */
    vm->rsp++;
    vm->return_stack[vm->rsp] = index; /* push index */
    vm->rsp++;
    vm->return_stack[vm->rsp] = ipcell; /* restore IP to top */
    return 1;
}

/* Drop the current loop frame (two cells) that sits under IP.
   After drop:
      RS[rsp] stays IP; (index,limit) removed.
*/
static inline int rs_drop_loop_under_ip(VM *vm) {
    if (!rs_has(vm, 3)) return 0; /* need IP + index + limit */
    /* Move IP down over the two cells, then shrink RSP by 2 */
    vm->return_stack[vm->rsp - 2] = vm->return_stack[vm->rsp];
    vm->rsp -= 2;
    return 1;
}

/* Accessors for loop params under IP */
static inline cell_t rs_peek_index(VM *vm) { return vm->return_stack[vm->rsp - 1]; }
static inline cell_t rs_peek_limit(VM *vm) { return vm->return_stack[vm->rsp - 2]; }
static inline void rs_store_index(VM *vm, cell_t v) { vm->return_stack[vm->rsp - 1] = v; }

/* ================================================================
 * Low-level code emission
 * ================================================================ */

static inline size_t emit_cell(VM *vm, cell_t v) {
    if (vm->mode != MODE_COMPILE) {
        VM_PUSH(vm, v);
        return (size_t) -1;
    }
    vm_align(vm);
    cell_t *p = (cell_t *) vm_allot(vm, sizeof(cell_t));
    if (!p) {
        vm->error = 1;
        log_message(LOG_ERROR, "emit_cell: out of space");
        return (size_t) -1;
    }
    *p = v;
    return (size_t) ((uint8_t *) p - vm->memory);
}

/* ================================================================
 * Compile-time control-flow stack (forward/backpatching)
 * ================================================================ */

#define CF_STACK_MAX 64

typedef enum {
    CF_BEGIN, /* mark: byte addr of BEGIN target */
    CF_IF, /* mark: byte addr of IF’s 0BRANCH literal */
    CF_ELSE, /* mark: byte addr of ELSE’s BRANCH literal */
    CF_WHILE, /* mark: byte addr of WHILE’s 0BRANCH literal (paired with BEGIN) */
    CF_DO /* mark: byte addr of loop-body start (back target for LOOP/+LOOP) */
} cf_tag_t;

typedef struct {
    size_t addr; /* byte address in vm->memory to patch/back-link to */
    cf_tag_t tag;
} cf_item_t;

static cf_item_t cf_stack[CF_STACK_MAX];
static int cf_sp = -1;

static inline int cf_push(cf_tag_t tag, size_t addr) {
    if (cf_sp + 1 >= CF_STACK_MAX) {
        log_message(LOG_ERROR, "CF: overflow");
        return 0;
    }
    cf_stack[++cf_sp].tag = tag;
    cf_stack[cf_sp].addr = addr;
    return 1;
}

static inline int cf_pop(cf_item_t *out) {
    if (cf_sp < 0) {
        log_message(LOG_ERROR, "CF: underflow");
        return 0;
    }
    if (out) *out = cf_stack[cf_sp];
    --cf_sp;
    return 1;
}

static inline int cf_peek(cf_item_t *out) {
    if (cf_sp < 0) return 0;
    if (out) *out = cf_stack[cf_sp];
    return 1;
}

/* Reset CF stack across mode transitions to avoid leakage between definitions */
static int cf_last_mode = -999;

static inline void cf_epoch_sync(VM *vm) {
    if (cf_last_mode == -999) {
        cf_last_mode = vm->mode;
        return;
    }
    if (vm->mode != cf_last_mode) {
        cf_sp = -1;
        cf_last_mode = vm->mode;
        log_message(LOG_DEBUG, "CF: reset due to mode transition");
    }
}

/* ================================================================
 * Compile-time LEAVE site tracking (compile-time only)
 * ================================================================ */

static size_t leave_sites[CF_STACK_MAX]; /* BRANCH literal addrs needing patch */
static int leave_sp = -1;
static int leave_mark[CF_STACK_MAX]; /* mark per DO nesting for scoping LEAVEs */
static int leave_mark_sp = -1;

/* ================================================================
 * Runtime primitives (inner interpreter helpers)
 * ================================================================ */

/* (BRANCH) — ip := ip + rel (rel in BYTES, relative to this literal cell) */
static void rt_BRANCH(VM *vm) {
    if (!rs_has(vm, 1)) {
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

/* (0BRANCH) — ( f -- ) if f==0 then ip := ip + rel else ip := ip + 1 */
static void rt_0BRANCH(VM *vm) {
    if (!rs_has(vm, 1) || vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "0BRANCH: underflow");
        return;
    }
    cell_t flag = VM_POP(vm);
    cell_t *ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp];
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

/* (?DO) — ( limit start -- )  empty? skip body; non-empty: insert loop frame under IP */
static void rt_QDO(VM *vm) {
    if (!rs_has(vm, 1) || vm->dsp < 1) {
        vm->error = 1;
        log_message(LOG_ERROR, "?DO: underflow");
        return;
    }

    cell_t start = VM_POP(vm);
    cell_t limit = VM_POP(vm);

    cell_t *ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp];
    cell_t rel = *ip; /* forward offset if empty */

    if (start == limit) {
        ip = (cell_t *) ((uint8_t *) ip + (intptr_t) rel);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
        ip;
        log_message(LOG_DEBUG, "?DO: empty, jump +%ld", (long) rel);
        return;
    }

    /* enter: skip rel cell, insert loop frame under IP */
    ip = ip + 1;
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
    ip;

    if (!rs_insert_loop_under_ip(vm, limit, start)) {
        vm->error = 1;
        log_message(LOG_ERROR, "?DO: RS overflow");
        return;
    }
    log_message(LOG_DEBUG, "?DO: enter (index=%ld limit=%ld)", (long) start, (long) limit);
}

/* (DO) — ( limit start -- ) insert loop frame under IP */
static void rt_DO(VM *vm) {
    if (!rs_has(vm, 1) || vm->dsp < 1) {
        vm->error = 1;
        log_message(LOG_ERROR, "DO: underflow");
        return;
    }

    cell_t start = VM_POP(vm);
    cell_t limit = VM_POP(vm);

    if (!rs_insert_loop_under_ip(vm, limit, start)) {
        vm->error = 1;
        log_message(LOG_ERROR, "DO: RS overflow");
        return;
    }
    log_message(LOG_DEBUG, "DO: enter (index=%ld limit=%ld)", (long) start, (long) limit);
}

/* (LOOP) — index++ ; if index == limit then exit (drop frame and skip literal)
              else branch back by byte offset literal at IP */
static void rt_LOOP(VM *vm) {
    if (!rs_has(vm, 3)) {
        vm->error = 1;
        log_message(LOG_ERROR, "LOOP: RS underflow");
        return;
    }

    cell_t idx = rs_peek_index(vm) + 1;
    cell_t lim = rs_peek_limit(vm);

    cell_t *ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp];
    cell_t back = *ip; /* BYTES back to DO-body start */

    if (idx == lim) {
        /* exit: drop loop frame, skip back-literal */
        if (!rs_drop_loop_under_ip(vm)) {
            vm->error = 1;
            log_message(LOG_ERROR, "LOOP: frame drop failed");
            return;
        }
        ip = ip + 1;
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
        ip;
        log_message(LOG_DEBUG, "LOOP: exit");
    } else {
        /* continue */
        rs_store_index(vm, idx);
        ip = (cell_t *) ((uint8_t *) ip + (intptr_t) back);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
        ip;
        log_message(LOG_DEBUG, "LOOP: continue (index=%ld)", (long) idx);
    }
}

/* (+LOOP) — ( n -- )
   Continue unless index crosses the limit in the direction of n.
   If n==0: continue (degenerate but defined). */
static void rt_PLOOP(VM *vm) {
    if (!rs_has(vm, 3) || vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "+LOOP: underflow");
        return;
    }

    cell_t n = VM_POP(vm);
    cell_t idx = rs_peek_index(vm);
    cell_t lim = rs_peek_limit(vm);

    cell_t *ip = (cell_t *) (uintptr_t) vm->return_stack[vm->rsp];
    cell_t back = *ip; /* BYTES */

    if (n > 0) {
        cell_t newv = idx + n;
        if (newv >= lim) {
            if (!rs_drop_loop_under_ip(vm)) {
                vm->error = 1;
                log_message(LOG_ERROR, "+LOOP: frame drop failed");
                return;
            }
            ip = ip + 1;
            vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
            ip;
            log_message(LOG_DEBUG, "+LOOP: exit (index=%ld -> %ld >= %ld)", (long) idx, (long) newv, (long) lim);
        } else {
            rs_store_index(vm, newv);
            ip = (cell_t *) ((uint8_t *) ip + (intptr_t) back);
            vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
            ip;
            log_message(LOG_DEBUG, "+LOOP: cont (index=%ld)", (long) newv);
        }
    } else if (n < 0) {
        cell_t newv = idx + n;
        if (newv < lim) {
            if (!rs_drop_loop_under_ip(vm)) {
                vm->error = 1;
                log_message(LOG_ERROR, "+LOOP: frame drop failed");
                return;
            }
            ip = ip + 1;
            vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
            ip;
            log_message(LOG_DEBUG, "+LOOP: exit (index=%ld -> %ld < %ld)", (long) idx, (long) newv, (long) lim);
        } else {
            rs_store_index(vm, newv);
            ip = (cell_t *) ((uint8_t *) ip + (intptr_t) back);
            vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
            ip;
            log_message(LOG_DEBUG, "+LOOP: cont (index=%ld)", (long) newv);
        }
    } else {
        /* n == 0: always continue (no progress, user’s problem) */
        ip = (cell_t *) ((uint8_t *) ip + (intptr_t) back);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)
        ip;
        log_message(LOG_DEBUG, "+LOOP: zero increment, continue");
    }
}

/* (LEAVE) — drop the current loop frame immediately (so the subsequent BRANCH
   skips cleanly past the loop end). */
static void rt_LEAVE(VM *vm) {
    if (!rs_drop_loop_under_ip(vm)) {
        vm->error = 1;
        log_message(LOG_ERROR, "LEAVE: outside DO loop");
        return;
    }
    log_message(LOG_DEBUG, "LEAVE: frame dropped");
}

/* (EXIT) — runtime: request return from current colon definition. */
static void rt_EXIT(VM *vm) {
    vm->exit_colon = 1;
    log_message(LOG_DEBUG, "EXIT: return from colon");
}

/* ================================================================
 * Compile-time words
 * ================================================================ */

/* IF ( flag -- ) compiles: (0BRANCH) <fwd> */
static void w_IF(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "IF: compile-only");
        return;
    }
    vm_compile_call(vm, rt_0BRANCH);
    size_t lit = emit_cell(vm, 0);
    if (!cf_push(CF_IF, lit)) {
        vm->error = 1;
        return;
    }
}

/* ELSE compiles: (BRANCH) <fwd>; patches IF’s 0BRANCH */
static void w_ELSE(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "ELSE: compile-only");
        return;
    }
    cf_item_t t;
    if (!cf_peek(&t) || t.tag != CF_IF) {
        vm->error = 1;
        log_message(LOG_ERROR, "ELSE: needs IF");
        return;
    }
    (void) cf_pop(&t);

    vm_compile_call(vm, rt_BRANCH);
    size_t new_lit = emit_cell(vm, 0);
    *(cell_t *) (void *) (vm->memory + t.addr) = (cell_t)((size_t) vm->here - t.addr);

    if (!cf_push(CF_ELSE, new_lit)) {
        vm->error = 1;
        return;
    }
}

/* THEN — patch outstanding IF/ELSE forward */
static void w_THEN(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "THEN: compile-only");
        return;
    }
    cf_item_t t;
    if (!cf_pop(&t) || (t.tag != CF_IF && t.tag != CF_ELSE)) {
        vm->error = 1;
        log_message(LOG_ERROR, "THEN: unmatched");
        return;
    }
    *(cell_t *) (void *) (vm->memory + t.addr) = (cell_t)((size_t) vm->here - t.addr);
}

/* BEGIN — mark back target */
static void w_BEGIN(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "BEGIN: compile-only");
        return;
    }
    if (!cf_push(CF_BEGIN, vm->here)) { vm->error = 1; }
}

/* UNTIL ( flag -- ) — compile (0BRANCH) <back> */
static void w_UNTIL(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "UNTIL: compile-only");
        return;
    }
    cf_item_t b;
    if (!cf_pop(&b) || b.tag != CF_BEGIN) {
        vm->error = 1;
        log_message(LOG_ERROR, "UNTIL: needs BEGIN");
        return;
    }
    vm_compile_call(vm, rt_0BRANCH);
    (void) emit_cell(vm, (cell_t)((cell_t) b.addr - (cell_t) vm->here));
}

/* AGAIN — compile (BRANCH) <back> */
static void w_AGAIN(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "AGAIN: compile-only");
        return;
    }
    cf_item_t b;
    if (!cf_pop(&b) || b.tag != CF_BEGIN) {
        vm->error = 1;
        log_message(LOG_ERROR, "AGAIN: needs BEGIN");
        return;
    }
    vm_compile_call(vm, rt_BRANCH);
    (void) emit_cell(vm, (cell_t)((cell_t) b.addr - (cell_t) vm->here));
}

/* WHILE ( flag -- ) — compile (0BRANCH) <fwd>, paired with prior BEGIN */
static void w_WHILE(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "WHILE: compile-only");
        return;
    }
    cf_item_t b;
    if (!cf_peek(&b) || b.tag != CF_BEGIN) {
        vm->error = 1;
        log_message(LOG_ERROR, "WHILE: needs BEGIN");
        return;
    }
    vm_compile_call(vm, rt_0BRANCH);
    size_t lit = emit_cell(vm, 0);
    if (!cf_push(CF_WHILE, lit)) { vm->error = 1; }
}

/* REPEAT — close WHILE .. REPEAT back to BEGIN and patch WHILE forward */
static void w_REPEAT(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "REPEAT: compile-only");
        return;
    }
    cf_item_t w, b;
    if (!cf_pop(&w) || w.tag != CF_WHILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "REPEAT: needs WHILE");
        return;
    }
    if (!cf_pop(&b) || b.tag != CF_BEGIN) {
        vm->error = 1;
        log_message(LOG_ERROR, "REPEAT: needs BEGIN");
        return;
    }
    vm_compile_call(vm, rt_BRANCH);
    (void) emit_cell(vm, (cell_t)((cell_t) b.addr - (cell_t) vm->here));
    *(cell_t *) (void *) (vm->memory + w.addr) = (cell_t)((size_t) vm->here - w.addr);
}

/* ?DO ( limit start -- ) — compile (?DO) <fwd>; mark DO for LOOP/+LOOP back */
static void w_QDO(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "?DO: compile-only");
        return;
    }
    vm_compile_call(vm, rt_QDO);
    size_t fwd = emit_cell(vm, 0);
    if (!cf_push(CF_DO, vm->here)) {
        vm->error = 1;
        return;
    } /* back target */
    if (!cf_push(CF_WHILE, fwd)) {
        vm->error = 1;
        return;
    } /* reuse tag for 'pending fwd' */
    leave_mark[++leave_mark_sp] = leave_sp;
}

/* DO ( limit start -- ) — compile (DO); mark back target */
static void w_DO(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "DO: compile-only");
        return;
    }
    vm_compile_call(vm, rt_DO);
    if (!cf_push(CF_DO, vm->here)) {
        vm->error = 1;
        return;
    }
    leave_mark[++leave_mark_sp] = leave_sp;
}

/* LEAVE — compile (LEAVE) then (BRANCH) <fwd>; record for patching at LOOP */
static void w_LEAVE(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "LEAVE: compile-only");
        return;
    }
    /* Must be inside a DO; quick scan for a CF_DO */
    int ok = 0;
    for (int i = cf_sp; i >= 0; --i) if (cf_stack[i].tag == CF_DO) {
        ok = 1;
        break;
    }
    if (!ok || leave_mark_sp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "LEAVE: not in DO");
        return;
    }

    vm_compile_call(vm, rt_LEAVE);
    vm_compile_call(vm, rt_BRANCH);
    size_t lit = emit_cell(vm, 0);

    if (leave_sp + 1 >= CF_STACK_MAX) {
        vm->error = 1;
        log_message(LOG_ERROR, "LEAVE: too many");
        return;
    }
    leave_sites[++leave_sp] = lit;
}

/* LOOP — compile (LOOP) <back>; patch ?DO fwd (if any) and LEAVEs */
static void w_LOOP(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "LOOP: compile-only");
        return;
    }

    /* Optional ?DO forward */
    cf_item_t maybe_fwd;
    int have_fwd = 0;
    if (cf_peek(&maybe_fwd) && maybe_fwd.tag == CF_WHILE) {
        (void) cf_pop(&maybe_fwd);
        have_fwd = 1;
    }

    /* Required DO back mark */
    cf_item_t domark;
    if (!cf_pop(&domark) || domark.tag != CF_DO) {
        vm->error = 1;
        log_message(LOG_ERROR, "LOOP: needs DO");
        return;
    }

    vm_compile_call(vm, rt_LOOP);
    (void) emit_cell(vm, (cell_t)((cell_t) domark.addr - (cell_t) vm->here));

    if (have_fwd) {
        *(cell_t *) (void *) (vm->memory + maybe_fwd.addr) = (cell_t)((size_t) vm->here - maybe_fwd.addr);
    }

    if (leave_mark_sp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "LOOP: LEAVE mark underflow");
        return;
    }
    int mark = leave_mark[leave_mark_sp--];
    for (int i = leave_sp; i > mark; --i) {
        size_t a = leave_sites[i];
        *(cell_t *) (void *) (vm->memory + a) = (cell_t)((size_t) vm->here - a);
    }
    leave_sp = mark;
}

/* +LOOP — compile (+LOOP) <back>; same patching as LOOP */
static void w_PLOOP(VM *vm) {
    cf_epoch_sync(vm);
    if (vm->mode != MODE_COMPILE) {
        vm->error = 1;
        log_message(LOG_ERROR, "+LOOP: compile-only");
        return;
    }

    cf_item_t maybe_fwd;
    int have_fwd = 0;
    if (cf_peek(&maybe_fwd) && maybe_fwd.tag == CF_WHILE) {
        (void) cf_pop(&maybe_fwd);
        have_fwd = 1;
    }

    cf_item_t domark;
    if (!cf_pop(&domark) || domark.tag != CF_DO) {
        vm->error = 1;
        log_message(LOG_ERROR, "+LOOP: needs DO");
        return;
    }

    vm_compile_call(vm, rt_PLOOP);
    (void) emit_cell(vm, (cell_t)((cell_t) domark.addr - (cell_t) vm->here));

    if (have_fwd) {
        *(cell_t *) (void *) (vm->memory + maybe_fwd.addr) = (cell_t)((size_t) vm->here - maybe_fwd.addr);
    }

    if (leave_mark_sp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "+LOOP: LEAVE mark underflow");
        return;
    }
    int mark = leave_mark[leave_mark_sp--];
    for (int i = leave_sp; i > mark; --i) {
        size_t a = leave_sites[i];
        *(cell_t *) (void *) (vm->memory + a) = (cell_t)((size_t) vm->here - a);
    }
    leave_sp = mark;
}

/* EXIT — compile-time only: compiles (EXIT). Interpret-time: error. */
static void w_EXIT(VM *vm) {
    /* If there is no active return stack frame, we're not in a colon definition. */
    if (vm->rsp < 0) {
        /* Mark the VM as errored and report that EXIT was misused at the interpreter prompt. */
        vm->error = 1;
        log_message(LOG_ERROR, "EXIT: interpret-time use is invalid");
        return;
    }
    /* Otherwise, set a flag telling the VM to break out of the current colon definition. */
    vm->exit_colon = 1;
    log_message(LOG_DEBUG, "EXIT: return from colon");
}

/* ================================================================
 * Loop index words (I, J)
 * ================================================================ */

static void w_I(VM *vm) {
    if (!rs_has(vm, 2)) {
        vm->error = 1;
        log_message(LOG_ERROR, "I: outside DO loop");
        return;
    }
    VM_PUSH(vm, rs_peek_index(vm));
}

static void w_J(VM *vm) {
    /* Need at least IP + (inner idx,lim) + (outer idx) = 1 + 2 + 1 = 4 cells */
    if (!rs_has(vm, 4)) {
        vm->error = 1;
        log_message(LOG_ERROR, "J: needs nested DO loops");
        return;
    }
    VM_PUSH(vm, vm->return_stack[vm->rsp - 3]); /* outer index */
}

/* ================================================================
 * Registration
 * ================================================================ */

void register_control_words(VM *vm) {
    if (!vm) return;

    /* Internal runtime helpers */
    register_word(vm, "(BRANCH)", rt_BRANCH);
    register_word(vm, "(0BRANCH)", rt_0BRANCH);
    register_word(vm, "(?DO)", rt_QDO);
    register_word(vm, "(DO)", rt_DO);
    register_word(vm, "(LOOP)", rt_LOOP);
    register_word(vm, "(+LOOP)", rt_PLOOP);
    register_word(vm, "(LEAVE)", rt_LEAVE);
    register_word(vm, "(EXIT)", rt_EXIT);

    /* IF/ELSE/THEN */
    register_word(vm, "IF", w_IF);
    vm_make_immediate(vm);
    register_word(vm, "ELSE", w_ELSE);
    vm_make_immediate(vm);
    register_word(vm, "THEN", w_THEN);
    vm_make_immediate(vm);

    /* BEGIN-family */
    register_word(vm, "BEGIN", w_BEGIN);
    vm_make_immediate(vm);
    register_word(vm, "UNTIL", w_UNTIL);
    vm_make_immediate(vm);
    register_word(vm, "AGAIN", w_AGAIN);
    vm_make_immediate(vm);
    register_word(vm, "WHILE", w_WHILE);
    vm_make_immediate(vm);
    register_word(vm, "REPEAT", w_REPEAT);
    vm_make_immediate(vm);

    /* DO-family */
    register_word(vm, "?DO", w_QDO);
    vm_make_immediate(vm);
    register_word(vm, "DO", w_DO);
    vm_make_immediate(vm);
    register_word(vm, "LOOP", w_LOOP);
    vm_make_immediate(vm);
    register_word(vm, "+LOOP", w_PLOOP);
    vm_make_immediate(vm);
    register_word(vm, "LEAVE", w_LEAVE);
    vm_make_immediate(vm);

    /* EXIT (compile-only) and loop indices */
    register_word(vm, "EXIT", w_EXIT);
    vm_make_immediate(vm);
    register_word(vm, "I", w_I);
    register_word(vm, "J", w_J);

    log_message(LOG_INFO, "Control words registered (FORTH-79 style: loop control on RS, byte offsets).");
}