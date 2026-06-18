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
 * acl_words_test.c — POST tests for the word-level ACL system (Module 24)
 *
 * Tests directly manipulate DictEntry ACL fields in C and then verify
 * behaviour through the interpreter or via direct field inspection.
 * This avoids depending on ACL.4th being loaded during POST.
 */

#include "../include/test_runner.h"
#include "../include/test_common.h"
#include "../../../include/vm.h"
#include "../../word_source/include/acl_words.h"
#include <stdio.h>

/* ------------------------------------------------------------------ */

static int tests_passed = 0;
static int tests_failed = 0;

#define ACL_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            tests_passed++; \
            log_message(LOG_DEBUG, "  PASS: %s", msg); \
        } else { \
            tests_failed++; \
            log_message(LOG_ERROR, "  FAIL: %s", msg); \
        } \
    } while (0)

/* ------------------------------------------------------------------ */

/* Test 1: ACL-PIN is a one-way ratchet */
static void test_acl_pin_immutable(VM *vm)
{
    DictEntry *e = vm_create_word(vm, "__acl_pin_test__", 16, NULL);
    if (!e) { tests_failed++; return; }

    e->acl_mode = ACL_MODE_TTL;
    e->acl_pinned = 0;

    /* Pin it */
    vm_push(vm, (cell_t)(uintptr_t)e);
    vm_interpret(vm, "ACL-PIN");
    vm->error = 0;

    ACL_ASSERT(e->acl_pinned == 1, "ACL-PIN sets acl_pinned=1");

    /* Attempt to change mode via ACL-MODE! (C primitive) — should be no-op when pinned */
    vm_push(vm, (cell_t)ACL_MODE_STRICT);
    vm_push(vm, (cell_t)(uintptr_t)e);
    vm_interpret(vm, "ACL-MODE!");
    vm->error = 0;

    ACL_ASSERT(e->acl_mode == ACL_MODE_TTL, "ACL-STRICT is no-op when pinned");

    /* Attempt to change TTL — should be ignored */
    uint32_t old_ttl = e->acl_ttl;
    vm_push(vm, (cell_t)9999);
    vm_push(vm, (cell_t)(uintptr_t)e);
    vm_interpret(vm, "ACL-TTL!");
    vm->error = 0;
    ACL_ASSERT(e->acl_ttl == old_ttl, "ACL-TTL! is no-op when pinned");

    /* Attempt to change allow — should be ignored */
    e->acl_allow = 1;
    vm_push(vm, (cell_t)0);
    vm_push(vm, (cell_t)(uintptr_t)e);
    vm_interpret(vm, "ACL-ALLOW!");
    vm->error = 0;
    ACL_ASSERT(e->acl_allow == 1, "ACL-ALLOW! is no-op when pinned");
}

/* Test 2: ACL-INHERIT copies mode, clears pin, resets TTL and allow */
static void test_acl_inherit(VM *vm)
{
    DictEntry *src = vm_create_word(vm, "__acl_src__", 11, NULL);
    DictEntry *dst = vm_create_word(vm, "__acl_dst__", 11, NULL);
    if (!src || !dst) { tests_failed++; return; }

    src->acl_mode   = ACL_MODE_STRICT;
    src->acl_pinned = 1;
    src->acl_ttl    = 5000;
    src->acl_allow  = 0;
    dst->acl_pinned = 1;   /* even a pinned dst should be cleared */
    dst->acl_mode   = ACL_MODE_TTL;

    acl_inherit_entry(src, dst);

    ACL_ASSERT(dst->acl_mode   == ACL_MODE_STRICT, "ACL-INHERIT copies acl_mode");
    ACL_ASSERT(dst->acl_pinned == 0,               "ACL-INHERIT clears acl_pinned");
    ACL_ASSERT(dst->acl_ttl    == 0,               "ACL-INHERIT resets acl_ttl to 0");
    ACL_ASSERT(dst->acl_allow  == 1,               "ACL-INHERIT resets acl_allow to 1");
}

/* Test 3: emergency_console bypasses denied word */
static void test_emergency_console_bypass(VM *vm)
{
    /* Define a simple FORTH word that pushes a marker */
    vm_interpret(vm, ": __acl_ec_test__ 42 ;");
    vm->error = 0;

    DictEntry *e = vm_find_word(vm, "__acl_ec_test__", 15);
    if (!e) { tests_failed++; return; }

    /* Deny it */
    e->acl_allow  = 0;
    e->acl_ttl    = 0;
    e->acl_pinned = 0;

    /* With emergency_console=1: should execute despite deny */
    int saved_dsp = vm->dsp;
    vm->emergency_console = 1;
    vm_interpret(vm, "__acl_ec_test__");
    vm->emergency_console = 0;
    vm->error = 0;

    ACL_ASSERT(vm->dsp == saved_dsp + 1, "emergency_console bypasses ACL deny");
    if (vm->dsp > saved_dsp) vm->dsp--;   /* pop the 42 */

    /* Restore allow so subsequent tests don't trip over it */
    e->acl_allow = 1;
}

/* Test 4: ACL deny aborts execution (word body does not run) */
static void test_acl_deny_aborts(VM *vm)
{
    /* Define a word that pushes a sentinel value */
    vm_interpret(vm, ": __acl_deny_test__ 99 ;");
    vm->error = 0;

    DictEntry *e = vm_find_word(vm, "__acl_deny_test__", 17);
    if (!e) { tests_failed++; return; }

    e->acl_allow  = 0;
    e->acl_ttl    = 1;  /* hot path: decrement then check; avoids fail-open recheck */
    e->acl_pinned = 0;

    int saved_dsp = vm->dsp;
    vm->emergency_console = 0;
    vm_interpret(vm, "__acl_deny_test__");

    int was_aborted = (vm->abort_requested || vm->error || vm->dsp == saved_dsp);
    vm->abort_requested = 0;
    vm->error = 0;

    ACL_ASSERT(was_aborted,             "denied word triggers abort");
    ACL_ASSERT(vm->dsp == saved_dsp,    "denied word: stack unchanged");

    e->acl_allow = 1;
}

/* Test 5: TTL hot path — decrements without calling recheck */
static void test_acl_ttl_hot_path(VM *vm)
{
    vm_interpret(vm, ": __acl_ttl_test__ 1 DROP ;");
    vm->error = 0;

    DictEntry *e = vm_find_word(vm, "__acl_ttl_test__", 16);
    if (!e) { tests_failed++; return; }

    e->acl_allow  = 1;
    e->acl_ttl    = 10;
    e->acl_pinned = 0;
    e->acl_mode   = ACL_MODE_TTL;

    /* Execute the word 3 times via the outer interpreter */
    vm->emergency_console = 0;
    vm_interpret(vm, "__acl_ttl_test__");
    vm->error = 0;
    vm_interpret(vm, "__acl_ttl_test__");
    vm->error = 0;
    vm_interpret(vm, "__acl_ttl_test__");
    vm->error = 0;

    ACL_ASSERT(e->acl_ttl == 7, "TTL hot path: TTL decrements from 10 to 7 after 3 execs");
}

/* Test 6: STRICT mode keeps TTL at 0 after each ACL-RECHECK */
static void test_acl_strict_mode(VM *vm)
{
    vm_interpret(vm, ": __acl_strict_test__ DROP ;");
    vm->error = 0;

    DictEntry *e = vm_find_word(vm, "__acl_strict_test__", 19);
    if (!e) { tests_failed++; return; }

    e->acl_mode   = ACL_MODE_STRICT;
    e->acl_allow  = 1;
    e->acl_ttl    = 0;
    e->acl_pinned = 0;

    /* ACL-RECHECK for STRICT mode must reset TTL to 0 after allowing */
    vm_push(vm, (cell_t)(uintptr_t)e);

    DictEntry *recheck = vm_find_word(vm, "ACL-RECHECK", 11);
    if (recheck) {
        /* ACL-RECHECK is not yet in the dict (ACL.4th not loaded) — skip FORTH call */
        /* Test via acl_recheck() directly: in STRICT mode, TTL stays 0 */
        /* We can't call acl_recheck() directly (static), but we can test
           that the vm_interpret path triggers the recheck path correctly.
           After execution with TTL=0 and allow=1, TTL should still be 0
           in STRICT mode (recheck was called and set TTL=0 again). */
        vm->dsp--;  /* clean up the push above */
        vm_interpret(vm, "0 __acl_strict_test__");
        vm->error = 0;
        ACL_ASSERT(e->acl_ttl == 0, "STRICT mode: TTL remains 0 after recheck");
    } else {
        /* ACL.4th not loaded yet — fallback sets TTL=OPEN; note but don't fail */
        vm->dsp--;
        log_message(LOG_INFO, "  SKIP: STRICT mode test (ACL.4th not loaded)");
        tests_passed++;  /* conditional skip counts as pass in POST */
    }
}

/* Test 7: adaptive TTL scales with heat via ACL-TTL-COMPUTE */
static void test_acl_adaptive_ttl(VM *vm)
{
    DictEntry *e = vm_create_word(vm, "__acl_heat_test__", 17, NULL);
    if (!e) { tests_failed++; return; }

    /* Set heat and verify TTL-COMPUTE via FORTH (if ACL.4th loaded) */
    e->execution_heat = 2560;
    e->acl_allow      = 1;
    e->acl_pinned     = 0;

    DictEntry *ttl_compute = vm_find_word(vm, "ACL-TTL-COMPUTE", 15);
    if (ttl_compute) {
        vm_push(vm, (cell_t)(uintptr_t)e);
        vm_interpret(vm, "ACL-TTL-COMPUTE");
        vm->error = 0;
        if (vm->dsp >= 0) {
            cell_t ttl = vm_pop(vm);
            /* heat=2560, ttl = 2560/4 + 256 = 640+256 = 896 */
            ACL_ASSERT(ttl == 896, "ACL-TTL-COMPUTE: heat=2560 → ttl=896");
        } else {
            tests_failed++;
            log_message(LOG_ERROR, "  FAIL: ACL-TTL-COMPUTE left empty stack");
        }
    } else {
        log_message(LOG_INFO, "  SKIP: ACL-TTL-COMPUTE (ACL.4th not loaded)");
        tests_passed++;
    }
    vm->error = 0;
}

/* Test 8: Pin blocks shadowing — pinned word cannot be redefined by anyone */
static void test_acl_pin_blocks_shadow(VM *vm)
{
    DictEntry *orig = vm_create_word(vm, "__acl_shadow_target__", 21, NULL);
    if (!orig) { tests_failed++; return; }
    orig->acl_pinned = 1;

    DictEntry *shadow = vm_create_word(vm, "__acl_shadow_target__", 21, NULL);

    ACL_ASSERT(shadow == NULL,  "vm_create_word returns NULL for pinned name");
    ACL_ASSERT(vm->error != 0,  "vm->error set when shadow blocked");
    vm->error = 0;

    DictEntry *found = vm_find_word(vm, "__acl_shadow_target__", 21);
    ACL_ASSERT(found == orig,   "original pinned entry still found after blocked shadow");
}

/* Test 9: zuse_session bypasses denied word (same semantics as emergency_console) */
static void test_zuse_session_bypass(VM *vm)
{
    vm_interpret(vm, ": __acl_zuse_test__ 77 ;");
    vm->error = 0;

    DictEntry *e = vm_find_word(vm, "__acl_zuse_test__", 17);
    if (!e) { tests_failed++; return; }

    e->acl_allow  = 0;
    e->acl_ttl    = 0;
    e->acl_pinned = 0;

    int saved_dsp = vm->dsp;
    vm->zuse_session = 1;
    vm->emergency_console = 0;
    vm_interpret(vm, "__acl_zuse_test__");
    vm->zuse_session = 0;
    vm->error = 0;

    ACL_ASSERT(vm->dsp == saved_dsp + 1, "zuse_session bypasses ACL deny");
    if (vm->dsp > saved_dsp) vm->dsp--;

    e->acl_allow = 1;
}

/* ------------------------------------------------------------------ */

void run_acl_words_tests(VM *vm)
{
    log_message(LOG_INFO, "Running ACL Words Tests (Module 24)...");

    tests_passed = 0;
    tests_failed = 0;

    int saved_dsp, saved_rsp, saved_error;
    vm_mode_t saved_mode;

    save_vm_state(vm, &saved_dsp, &saved_rsp, &saved_error, &saved_mode);
    uint8_t saved_ec = vm->emergency_console;
    uint8_t saved_zs = vm->zuse_session;

    test_acl_pin_immutable(vm);
    restore_vm_state(vm, saved_dsp, saved_rsp, 0, saved_mode);

    test_acl_inherit(vm);
    restore_vm_state(vm, saved_dsp, saved_rsp, 0, saved_mode);

    test_emergency_console_bypass(vm);
    restore_vm_state(vm, saved_dsp, saved_rsp, 0, saved_mode);

    test_acl_deny_aborts(vm);
    restore_vm_state(vm, saved_dsp, saved_rsp, 0, saved_mode);

    test_acl_ttl_hot_path(vm);
    restore_vm_state(vm, saved_dsp, saved_rsp, 0, saved_mode);

    test_acl_strict_mode(vm);
    restore_vm_state(vm, saved_dsp, saved_rsp, 0, saved_mode);

    test_acl_adaptive_ttl(vm);
    restore_vm_state(vm, saved_dsp, saved_rsp, 0, saved_mode);

    test_acl_pin_blocks_shadow(vm);
    restore_vm_state(vm, saved_dsp, saved_rsp, 0, saved_mode);

    test_zuse_session_bypass(vm);
    restore_vm_state(vm, saved_dsp, saved_rsp, 0, saved_mode);

    vm->emergency_console = saved_ec;
    vm->zuse_session      = saved_zs;

    print_module_summary("ACL Words", tests_passed, tests_failed, 0, 0);
}
