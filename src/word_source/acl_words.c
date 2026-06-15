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

 */

/*
 * acl_words.c — C primitive words for the word-level ACL system
 *
 * All policy logic lives in capsules/ACL.4th. These primitives expose
 * DictEntry ACL fields to FORTH: ACL-MODE@, ACL-MODE!, ACL-PINNED?,
 * ACL-PIN, ACL-TTL@, ACL-TTL!, ACL-ALLOW@, ACL-ALLOW!, ACL-HEAT@,
 * ACL-INHERIT, ACL-INIT-PRIMITIVES, ACL-WORD-ID.
 *
 * All words that take an XT expect a DictEntry* cast to cell_t on the
 * stack — exactly what ' (tick) produces in StarForth.
 */

#include "include/acl_words.h"
#include "../../include/vm.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* ------------------------------------------------------------------
 * Helper: pop XT from stack and return DictEntry*.
 * Returns NULL and sets vm->error on underflow or null pointer.
 * ------------------------------------------------------------------ */
static DictEntry *pop_xt(VM *vm)
{
    if (!vm || vm->dsp < 0) {
        log_message(LOG_ERROR, "ACL word: stack underflow");
        if (vm) vm->error = 1;
        return NULL;
    }
    cell_t xt = vm_pop(vm);
    DictEntry *entry = (DictEntry *)(uintptr_t)xt;
    if (!entry) {
        log_message(LOG_ERROR, "ACL word: NULL XT");
        vm->error = 1;
        return NULL;
    }
    return entry;
}

/* ------------------------------------------------------------------
 * ACL-MODE@ ( xt -- mode )
 * Push the enforcement mode: 0=STRICT, 1=TTL.
 * ------------------------------------------------------------------ */
static void forth_acl_mode_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->acl_mode);
}

/* ------------------------------------------------------------------
 * ACL-MODE! ( mode xt -- )
 * Set enforcement mode. No-op if word is pinned.
 * ------------------------------------------------------------------ */
static void forth_acl_mode_store(VM *vm)
{
    if (!vm || vm->dsp < 1) {
        log_message(LOG_ERROR, "ACL-MODE!: stack underflow");
        if (vm) vm->error = 1;
        return;
    }
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    cell_t mode = vm_pop(vm);
    if (e->acl_pinned) return;          /* one-way ratchet: pinned is immutable */
    e->acl_mode = (uint8_t)(mode & 0xFF);
}

/* ------------------------------------------------------------------
 * ACL-PINNED? ( xt -- flag )
 * Push 1 if pinned, 0 otherwise.
 * ------------------------------------------------------------------ */
static void forth_acl_pinned_query(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, e->acl_pinned ? -1 : 0);
}

/* ------------------------------------------------------------------
 * ACL-PIN ( xt -- )
 * Set acl_pinned=1. One-way ratchet: never clears.
 * ------------------------------------------------------------------ */
static void forth_acl_pin(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    e->acl_pinned = 1;
}

/* ------------------------------------------------------------------
 * ACL-TTL@ ( xt -- n )
 * Push the current TTL countdown value.
 * ------------------------------------------------------------------ */
static void forth_acl_ttl_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->acl_ttl);
}

/* ------------------------------------------------------------------
 * ACL-TTL! ( n xt -- )
 * Set the TTL counter. No-op if pinned.
 * ------------------------------------------------------------------ */
static void forth_acl_ttl_store(VM *vm)
{
    if (!vm || vm->dsp < 1) {
        log_message(LOG_ERROR, "ACL-TTL!: stack underflow");
        if (vm) vm->error = 1;
        return;
    }
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    cell_t n = vm_pop(vm);
    if (e->acl_pinned) return;
    e->acl_ttl = (uint32_t)(n < 0 ? 0 : (uint64_t)n > UINT32_MAX ? UINT32_MAX : (uint32_t)n);
}

/* ------------------------------------------------------------------
 * ACL-ALLOW@ ( xt -- flag )
 * Push the cached allow/deny decision: -1=allow, 0=deny.
 * ------------------------------------------------------------------ */
static void forth_acl_allow_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, e->acl_allow ? -1 : 0);
}

/* ------------------------------------------------------------------
 * ACL-ALLOW! ( flag xt -- )
 * Set the cached allow/deny decision. No-op if pinned.
 * ------------------------------------------------------------------ */
static void forth_acl_allow_store(VM *vm)
{
    if (!vm || vm->dsp < 1) {
        log_message(LOG_ERROR, "ACL-ALLOW!: stack underflow");
        if (vm) vm->error = 1;
        return;
    }
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    cell_t flag = vm_pop(vm);
    if (e->acl_pinned) return;
    e->acl_allow = (flag != 0) ? 1 : 0;
}

/* ------------------------------------------------------------------
 * ACL-HEAT@ ( xt -- heat )
 * Push execution_heat for this word (used by ACL-TTL-COMPUTE in FORTH).
 * ------------------------------------------------------------------ */
static void forth_acl_heat_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->execution_heat);
}

/* ------------------------------------------------------------------
 * ACL-WORD-ID ( xt -- n )
 * Push the stable word_id (used by ACL-ENTRY for table indexing).
 * ------------------------------------------------------------------ */
static void forth_acl_word_id(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->word_id);
}

/* ------------------------------------------------------------------
 * ACL-INHERIT ( src dst -- )
 * Copy acl_mode from src to dst; clear pin; reset ttl and allow.
 * This is a C primitive because FORTH cannot clear acl_pinned.
 * ------------------------------------------------------------------ */
static void forth_acl_inherit(VM *vm)
{
    if (!vm || vm->dsp < 1) {
        log_message(LOG_ERROR, "ACL-INHERIT: stack underflow");
        if (vm) vm->error = 1;
        return;
    }
    DictEntry *dst = pop_xt(vm);
    if (!dst) return;
    DictEntry *src = pop_xt(vm);
    if (!src) return;
    acl_inherit_entry(src, dst);
}

/* ------------------------------------------------------------------
 * ACL-INIT-PRIMITIVES ( -- )
 * Walk the dictionary and reset ACL fields on every unpinned entry.
 * Called from ACL-BOOT after ACL.4th loads.
 * ------------------------------------------------------------------ */
static void forth_acl_init_primitives(VM *vm)
{
    if (!vm) return;
    for (DictEntry *e = vm->latest; e; e = e->link) {
        if (!e->acl_pinned) {
            /* Re-assert permissive defaults; ACL-RECHECK will calibrate on first exec */
            e->acl_ttl   = 0;     /* force recheck on first execution */
            e->acl_allow = 1;
            e->acl_mode  = ACL_MODE_TTL;
        }
    }
}

/* ------------------------------------------------------------------
 * acl_inherit_entry — direct C entry point for birth path.
 * No stack manipulation; called directly from capsule birth code.
 * ------------------------------------------------------------------ */
void acl_inherit_entry(DictEntry *src, DictEntry *dst)
{
    if (!src || !dst) return;
    dst->acl_mode   = src->acl_mode;
    dst->acl_pinned = 0;        /* pin is contextual, not viral */
    dst->acl_ttl    = 0;        /* child has no history; force recheck */
    dst->acl_allow  = 1;        /* optimistic default */
}

/* ==================================================================
 * ACL Rolling Window of Truth primitives
 *
 * Mirrors the Loop #2+#3+#6 topology applied to the per-word ACL TTL
 * signal.  Ring buffer (depth 8) stores heartbeat tick stamps at each
 * ACL-RECHECK.  Slope is inferred from the interval sequence and stored
 * per-word so FORTH can apply it in ACL-TTL-COMPUTE-RW.
 *
 * Naming: ACL-RWT-* to keep namespace distinct from main RWT.
 * Policy logic (TTL formula, slope application) lives in ACL.4th.
 * ================================================================== */

/* ACL-RWT-TICK@ ( -- tick )
 * Push the current heartbeat tick count (low 32 bits).            */
static void forth_acl_rwt_tick_fetch(VM *vm)
{
    if (!vm) return;
    vm_push(vm, (cell_t)(uint32_t)(vm->heartbeat.tick_count & 0xFFFFFFFFu));
}

/* ACL-RWT-PUSH ( tick xt -- )
 * Push tick stamp into xt's ring buffer; advance head; cap count.  */
static void forth_acl_rwt_push(VM *vm)
{
    if (!vm || vm->dsp < 1) { if (vm) vm->error = 1; return; }
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    cell_t tick = vm_pop(vm);
    e->acl_rwt[e->acl_rwt_head] = (uint32_t)tick;
    e->acl_rwt_head = (e->acl_rwt_head + 1) & 7;
    if (e->acl_rwt_count < 8) e->acl_rwt_count++;
}

/* ACL-RWT-COUNT@ ( xt -- n )
 * Push the number of valid tick stamps in the ring buffer (0..8).  */
static void forth_acl_rwt_count_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->acl_rwt_count);
}

/* ACL-RWT-INTERVAL@ ( xt n -- interval )
 * Return the nth most-recent inter-recheck interval (0=newest).
 * interval[n] = stamp[n] - stamp[n+1] where stamps are in reverse
 * chronological order from the ring buffer head.
 * Returns 0 if n >= count-1 (insufficient history).               */
static void forth_acl_rwt_interval_fetch(VM *vm)
{
    if (!vm || vm->dsp < 1) { if (vm) vm->error = 1; return; }
    cell_t n = vm_pop(vm);
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    if (n < 0 || (uint32_t)n >= (uint32_t)(e->acl_rwt_count - 1)) {
        vm_push(vm, 0);
        return;
    }
    /* Slots in reverse order from head: head-1, head-2, ... */
    int i_new = ((int)e->acl_rwt_head - 1 - (int)n + 16) & 7;
    int i_old = ((int)e->acl_rwt_head - 2 - (int)n + 16) & 7;
    uint32_t interval = e->acl_rwt[i_new] - e->acl_rwt[i_old];
    vm_push(vm, (cell_t)interval);
}

/* ACL-RWT-SLOPE@ ( xt -- slope )
 * Push the cached Q8 slope for this word's recheck interval series. */
static void forth_acl_rwt_slope_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->acl_rwt_slope);
}

/* ACL-RWT-SLOPE! ( slope xt -- )
 * Store slope. No-op if pinned.                                    */
static void forth_acl_rwt_slope_store(VM *vm)
{
    if (!vm || vm->dsp < 1) { if (vm) vm->error = 1; return; }
    DictEntry *e = pop_xt(vm);
    if (!e || e->acl_pinned) { if (vm->dsp >= 0) vm_pop(vm); return; }
    cell_t slope = vm_pop(vm);
    e->acl_rwt_slope = (int32_t)slope;
}

/* ------------------------------------------------------------------
 * register_acl_words
 * ------------------------------------------------------------------ */
void register_acl_words(VM *vm)
{
    /* Field accessors — read */
    register_word(vm, "ACL-MODE@",   forth_acl_mode_fetch);
    register_word(vm, "ACL-PINNED?", forth_acl_pinned_query);
    register_word(vm, "ACL-TTL@",    forth_acl_ttl_fetch);
    register_word(vm, "ACL-ALLOW@",  forth_acl_allow_fetch);
    register_word(vm, "ACL-HEAT@",   forth_acl_heat_fetch);
    register_word(vm, "ACL-WORD-ID", forth_acl_word_id);

    /* Field accessors — write (all no-op if pinned) */
    register_word(vm, "ACL-MODE!",   forth_acl_mode_store);
    register_word(vm, "ACL-TTL!",    forth_acl_ttl_store);
    register_word(vm, "ACL-ALLOW!",  forth_acl_allow_store);

    /* Pin ratchet — one-way set */
    register_word(vm, "ACL-PIN",     forth_acl_pin);

    /* Higher-level operations */
    register_word(vm, "ACL-INHERIT",         forth_acl_inherit);
    register_word(vm, "ACL-INIT-PRIMITIVES", forth_acl_init_primitives);

    /* ACL Rolling Window of Truth primitives */
    register_word(vm, "ACL-RWT-TICK@",     forth_acl_rwt_tick_fetch);
    register_word(vm, "ACL-RWT-PUSH",      forth_acl_rwt_push);
    register_word(vm, "ACL-RWT-COUNT@",    forth_acl_rwt_count_fetch);
    register_word(vm, "ACL-RWT-INTERVAL@", forth_acl_rwt_interval_fetch);
    register_word(vm, "ACL-RWT-SLOPE@",    forth_acl_rwt_slope_fetch);
    register_word(vm, "ACL-RWT-SLOPE!",    forth_acl_rwt_slope_store);
}
