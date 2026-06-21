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

/**
 * @brief Pop an execution token (XT) from the data stack and return the
 *        corresponding DictEntry pointer.
 *
 * Sets @c vm->error and returns NULL on stack underflow or if the XT is zero.
 * All ACL word implementations call this before accessing DictEntry fields.
 *
 * @param vm Pointer to the VM structure
 * @return Pointer to the DictEntry, or NULL on error
 */
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

/**
 * @brief Implements FORTH word @c ACL-MODE@ — fetch ACL enforcement mode
 *
 * Stack effect: ( xt -- mode )
 * Pushes the enforcement mode of the word identified by @c xt:
 * 0 = STRICT (allow/deny is permanent), 1 = TTL (expires after countdown).
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_mode_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->acl_mode);
}

/**
 * @brief Implements FORTH word @c ACL-MODE! — store ACL enforcement mode
 *
 * Stack effect: ( mode xt -- )
 * Sets the enforcement mode of the word identified by @c xt.
 * No-op if the word is pinned — pinned entries are immutable.
 *
 * @param vm Pointer to the VM structure
 */
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

/**
 * @brief Implements FORTH word @c ACL-PINNED? — query pin status
 *
 * Stack effect: ( xt -- flag )
 * Pushes -1 (true) if the word is pinned, 0 (false) otherwise.
 * A pinned word's ACL fields cannot be modified by any FORTH operation.
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_pinned_query(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, e->acl_pinned ? -1 : 0);
}

/**
 * @brief Implements FORTH word @c ACL-PIN — pin a word's ACL entry
 *
 * Stack effect: ( xt -- )
 * Sets @c acl_pinned = 1 on the word identified by @c xt.
 * This is a one-way ratchet: once set it can never be cleared, even by
 * ACL-INHERIT. Use with @c ' WORD ACL-PIN in FORTH, never from C policy code.
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_pin(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    e->acl_pinned = 1;
}

/**
 * @brief Implements FORTH word @c ACL-TTL@ — fetch TTL countdown value
 *
 * Stack effect: ( xt -- n )
 * Pushes the current TTL (time-to-live) countdown of the word's ACL entry.
 * When TTL reaches zero in TTL mode the interpreter calls @c acl_recheck().
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_ttl_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->acl_ttl);
}

/**
 * @brief Implements FORTH word @c ACL-TTL! — store TTL countdown value
 *
 * Stack effect: ( n xt -- )
 * Sets the TTL countdown on the word's ACL entry. Negative values are
 * clamped to 0; values above UINT32_MAX are clamped to UINT32_MAX.
 * No-op if the word is pinned.
 *
 * @param vm Pointer to the VM structure
 */
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

/**
 * @brief Implements FORTH word @c ACL-ALLOW@ — fetch cached allow/deny flag
 *
 * Stack effect: ( xt -- flag )
 * Pushes the cached ACL decision for the word: -1 = allowed, 0 = denied.
 * This cached value is checked on every execution in TTL mode; the cache is
 * refreshed by @c acl_recheck() when TTL expires.
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_allow_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, e->acl_allow ? -1 : 0);
}

/**
 * @brief Implements FORTH word @c ACL-ALLOW! — store cached allow/deny flag
 *
 * Stack effect: ( flag xt -- )
 * Sets the cached ACL decision: any non-zero @c flag = allowed, 0 = denied.
 * No-op if the word is pinned.
 *
 * @param vm Pointer to the VM structure
 */
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

/**
 * @brief Implements FORTH word @c ACL-HEAT@ — fetch execution heat
 *
 * Stack effect: ( xt -- heat )
 * Pushes the @c execution_heat counter for the word identified by @c xt.
 * Used by @c ACL-TTL-COMPUTE in @c ACL.4th to calibrate the TTL value based
 * on how frequently the word is executed.
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_heat_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->execution_heat);
}

/**
 * @brief Implements FORTH word @c ACL-WORD-ID — fetch stable word identifier
 *
 * Stack effect: ( xt -- n )
 * Pushes the stable @c word_id for the entry identified by @c xt. Unlike
 * dictionary positions, @c word_id is assigned at registration and never
 * changes, making it safe to use as a persistent table index in @c ACL.4th.
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_word_id(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->word_id);
}

/**
 * @brief Implements FORTH word @c ACL-INHERIT — inherit ACL policy from parent
 *
 * Stack effect: ( src dst -- )
 * Copies @c acl_mode from @c src to @c dst, then clears @c dst->acl_pinned,
 * resets @c acl_ttl to 0, and sets @c acl_allow to 1 (optimistic default).
 * This is a C primitive because FORTH code cannot clear @c acl_pinned — pin
 * is a one-way ratchet that only C can reset during inheritance.
 *
 * @param vm Pointer to the VM structure
 */
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

/**
 * @brief Implements FORTH word @c ACL-INIT-PRIMITIVES — reset ACL fields on all
 *        unpinned dictionary entries
 *
 * Stack effect: ( -- )
 * Walks the entire dictionary and resets ACL fields on every entry that is not
 * pinned: sets @c acl_ttl = 0 (force recheck on first execution), @c acl_allow
 * = 1 (optimistic default), @c acl_mode = ACL_MODE_TTL. Called by @c ACL-BOOT
 * in @c ACL.4th immediately after the ACL capsule finishes loading.
 *
 * @param vm Pointer to the VM structure
 */
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

/**
 * @brief Direct C entry point for ACL policy inheritance on the capsule birth path.
 *
 * Copies @c acl_mode from @c src to @c dst, clears @c acl_pinned, resets
 * @c acl_ttl to 0, and sets @c acl_allow to 1. Called from capsule birth code
 * (not from FORTH) — no stack manipulation is performed.
 *
 * @param src Source DictEntry whose @c acl_mode is inherited
 * @param dst Destination DictEntry whose ACL fields are updated
 */
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

/**
 * @brief Implements FORTH word @c ACL-RWT-TICK@ — fetch current heartbeat tick
 *
 * Stack effect: ( -- tick )
 * Pushes the low 32 bits of @c vm->heartbeat.tick_count. Used by @c ACL.4th
 * as a timestamp when recording ACL recheck events into the per-word ring buffer.
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_rwt_tick_fetch(VM *vm)
{
    if (!vm) return;
    vm_push(vm, (cell_t)(uint32_t)(vm->heartbeat.tick_count & 0xFFFFFFFFu));
}

/**
 * @brief Implements FORTH word @c ACL-RWT-PUSH — push tick stamp into per-word ring
 *
 * Stack effect: ( tick xt -- )
 * Records @c tick in the 8-entry circular ring buffer of the word identified by
 * @c xt, advances the head pointer, and caps @c acl_rwt_count at 8. Older
 * entries are silently overwritten once the ring is full.
 *
 * @param vm Pointer to the VM structure
 */
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

/**
 * @brief Implements FORTH word @c ACL-RWT-COUNT@ — fetch ring buffer fill count
 *
 * Stack effect: ( xt -- n )
 * Pushes the number of valid tick stamps currently in the per-word ring buffer
 * (0 to 8). Used by @c ACL.4th to guard interval computations that require
 * at least 2 stamps.
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_rwt_count_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->acl_rwt_count);
}

/**
 * @brief Implements FORTH word @c ACL-RWT-INTERVAL@ — fetch inter-recheck interval
 *
 * Stack effect: ( xt n -- interval )
 * Returns the @c n th most-recent inter-recheck tick interval for the word
 * identified by @c xt (0 = newest pair). Computed as stamp[n] - stamp[n+1]
 * in reverse-chronological order from the ring buffer head. Pushes 0 if
 * @c n >= count-1 (insufficient history for the requested pair).
 *
 * @param vm Pointer to the VM structure
 */
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

/**
 * @brief Implements FORTH word @c ACL-RWT-SLOPE@ — fetch cached interval slope
 *
 * Stack effect: ( xt -- slope )
 * Pushes the cached Q8 slope value for the word's recheck interval series.
 * Slope is computed by @c ACL-TTL-COMPUTE-RW in @c ACL.4th using the ring
 * buffer history and stored here between rechecks.
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_rwt_slope_fetch(VM *vm)
{
    DictEntry *e = pop_xt(vm);
    if (!e) return;
    vm_push(vm, (cell_t)e->acl_rwt_slope);
}

/**
 * @brief Implements FORTH word @c ACL-RWT-SLOPE! — store cached interval slope
 *
 * Stack effect: ( slope xt -- )
 * Stores the Q8 slope into the word's @c acl_rwt_slope field. No-op if the
 * word is pinned. The pending slope stack value is consumed even on no-op.
 *
 * @param vm Pointer to the VM structure
 */
static void forth_acl_rwt_slope_store(VM *vm)
{
    if (!vm || vm->dsp < 1) { if (vm) vm->error = 1; return; }
    DictEntry *e = pop_xt(vm);
    if (!e || e->acl_pinned) { if (vm->dsp >= 0) vm_pop(vm); return; }
    cell_t slope = vm_pop(vm);
    e->acl_rwt_slope = (int32_t)slope;
}

/**
 * @brief Register all ACL C-primitive words with the VM dictionary.
 *
 * Registers: field accessors (@c ACL-MODE@ / @c ACL-MODE! / @c ACL-PINNED? /
 * @c ACL-TTL@ / @c ACL-TTL! / @c ACL-ALLOW@ / @c ACL-ALLOW! / @c ACL-HEAT@ /
 * @c ACL-WORD-ID), mutation words (@c ACL-PIN / @c ACL-INHERIT /
 * @c ACL-INIT-PRIMITIVES), and the ACL Rolling Window of Truth primitives
 * (@c ACL-RWT-TICK@ / @c ACL-RWT-PUSH / @c ACL-RWT-COUNT@ /
 * @c ACL-RWT-INTERVAL@ / @c ACL-RWT-SLOPE@ / @c ACL-RWT-SLOPE!).
 * Called from @c vm_bootstrap.c during VM initialisation.
 *
 * @param vm Pointer to the VM structure
 */
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
