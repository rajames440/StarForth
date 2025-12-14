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

/**
 * @file stress_tests.c
 * @brief FORTH-79 Standard Comprehensive Stress Testing Module
 *
 * This module contains extensive stress tests that verify the interpreter's behavior
 * under extreme conditions while remaining within FORTH-79 standard compliance.
 *
 * Test categories (expanded):
 * - Deep nesting of control structures (up to 5 levels)
 * - Stack exhaustion and boundary tests (near-limit operations)
 * - Numeric boundary conditions (cell_t min/max, overflow)
 * - Dictionary pressure (many definitions, long names)
 * - Word execution chains (deep call stacks)
 * - Control flow edge cases (empty loops, large counts, EXIT)
 * - Memory allocation patterns (fragmentation, large blocks)
 * - Error recovery (ABORT, stack underflow recovery)
 * - Large definitions and word chains
 *
 * IMPORTANT: These tests only use FORTH-79 standard words. Features from
 * later standards (ANS Forth 1994, Forth 2012) are NOT tested here:
 * - RECURSE (ANS Forth) - not in FORTH-79
 * - Self-referential definitions - not supported in FORTH-79
 * - Extended VARIABLE usage - only standard semantics tested
 *
 * For testing extended/non-standard features, see integration_tests.c
 */

#include <stdio.h>
#include "test_common.h"
#include "test_runner.h"

/* Comprehensive stress test suites */
static WordTestSuite stress_suites[] = {
    /* ===== 1. STACK DEPTH TESTS ===== */
    {
        "STACK_DEPTH", {
            {"deep_push", ": PUSH-100 0 100 0 DO I LOOP ; PUSH-100 DEPTH . CR", "Push 100 items", TEST_NORMAL, 0, 1},
            {"deep_math", ": MATH-STRESS 1 100 0 DO 1+ LOOP . CR ; MATH-STRESS", "100 additions", TEST_NORMAL, 0, 1},
            {
                "dup_stress", ": DUP-STRESS 42 50 0 DO DUP LOOP DEPTH . CR 51 0 DO DROP LOOP ; DUP-STRESS", "50 DUPs",
                TEST_NORMAL, 0, 1
            },
            {
                "deep_push_200",
                ": PUSH-200 0 200 0 DO I LOOP ; : CLR-200 200 0 DO DROP LOOP ; PUSH-200 DEPTH . CR CLR-200",
                "Push 200 items", TEST_NORMAL, 0, 1
            },
            {
                "mixed_ops", ": MIXED 1 2 3 4 5 6 7 8 9 10 50 0 DO SWAP ROT LOOP 10 0 DO DROP LOOP ; MIXED",
                "Mixed stack ops", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    /* ===== 2. STACK BOUNDARY TESTS ===== */
    {
        "STACK_BOUNDARY", {
            {
                "near_limit_900",
                ": PUSH-900 0 900 0 DO I LOOP ; : CLR-900 900 0 DO DROP LOOP ; PUSH-900 DEPTH . CR CLR-900 FORGET CLR-900",
                "Push 900 items (near 1024 limit)", TEST_EDGE_CASE, 0, 1
            },
            {"underflow_detect", "DROP", "Stack underflow detection", TEST_ERROR_CASE, 1, 1},
            {"dup_underflow", "DUP", "DUP underflow detection", TEST_ERROR_CASE, 1, 1},
            {"swap_underflow", "1 SWAP", "SWAP underflow detection", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    /* ===== 3. RETURN STACK STRESS ===== */
    {
        "RETURN_STACK", {
            {"basic_to_r", "1 2 3 >R >R >R R> R> R> + + . CR", "Basic >R/R> test", TEST_NORMAL, 0, 1},
            {"r_fetch", "42 >R R@ . CR R> DROP", "R@ test", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* ===== 4. DEEP NESTING TESTS ===== */
    {
        "DEEP_NESTING", {
            {
                "nested_if_5",
                ": NEST5 DUP 0> IF DUP 10 > IF DUP 20 > IF DUP 30 > IF DUP 40 > IF 5 ELSE 4 THEN ELSE 3 THEN ELSE 2 THEN ELSE 1 THEN ELSE 0 THEN . CR ; 45 NEST5",
                "5-level IF nesting", TEST_NORMAL, 0, 1
            },
            {
                "nested_loops_4", ": NEST-4 0 2 0 DO 2 0 DO 2 0 DO 2 0 DO 1+ LOOP LOOP LOOP LOOP . CR ; NEST-4",
                "4-level DO loop nesting", TEST_NORMAL, 0, 1
            },
            {
                "mixed_if_do", ": MIX-NEST 0 10 0> IF 5 0 DO 1+ LOOP ELSE 3 0 DO 2 + LOOP THEN . CR ; MIX-NEST",
                "IF with DO inside", TEST_NORMAL, 0, 1
            },
            {
                "begin_simple", ": BEGIN-TEST 5 BEGIN 1- DUP 0= UNTIL DROP ; BEGIN-TEST",
                "Simple BEGIN/UNTIL loop", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    /* ===== 5. CONTROL FLOW EDGE CASES ===== */
    {
        "CONTROL_EDGE", {
            {"empty_loop", ": EMPTY-LOOP 0 0 DO LOOP ; EMPTY-LOOP", "Empty loop body", TEST_NORMAL, 0, 1},
            {"single_iter", ": SINGLE 1 0 DO 42 . LOOP CR ; SINGLE", "Single iteration", TEST_NORMAL, 0, 1},
            {"large_count", ": BIG-LOOP 0 10000 0 DO 1+ LOOP . CR ; BIG-LOOP", "10000 iterations", TEST_NORMAL, 0, 1},
            {
                "nested_exit", ": NEST-EXIT 10 0 DO I 5 = IF EXIT THEN I . LOOP CR ; NEST-EXIT", "EXIT in nested loop",
                TEST_NORMAL, 0, 1
            },
            {
                "begin_while", ": B-W-R 10 BEGIN DUP 0> WHILE DUP . 1- REPEAT DROP CR ; B-W-R", "BEGIN WHILE REPEAT",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    /* ===== 6. NUMERIC BOUNDARIES ===== */
    {
        "NUMERIC_BOUNDARY", {
            {"max_int", ": MAX-TEST 2147483647 DUP . 1+ . CR ; MAX-TEST", "Max int boundary", TEST_EDGE_CASE, 0, 1},
            {"min_int", ": MIN-TEST -2147483648 DUP . 1- . CR ; MIN-TEST", "Min int boundary", TEST_EDGE_CASE, 0, 1},
            {"large_mult", ": BIG-MULT 10000 10000 * . CR ; BIG-MULT", "Large multiplication", TEST_NORMAL, 0, 1},
            {
                "div_edge", ": DIV-TEST 100 1 / 100 2 / 100 3 / . . . CR ; DIV-TEST", "Division edge cases",
                TEST_NORMAL, 0, 1
            },
            {"mod_edge", ": MOD-TEST 100 7 MOD 100 3 MOD . . CR ; MOD-TEST", "Modulo operations", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    /* ===== 7. WORD EXECUTION CHAINS ===== */
    {
        "EXEC_CHAINS", {
            {
                "chain_10",
                ": C1 1 ; : C2 C1 1+ ; : C3 C2 1+ ; : C4 C3 1+ ; : C5 C4 1+ ; : C6 C5 1+ ; : C7 C6 1+ ; : C8 C7 1+ ; : C9 C8 1+ ; : C10 C9 1+ ; C10 . CR",
                "10-word call chain", TEST_NORMAL, 0, 1
            },
            {
                "chain_20",
                ": D1 1 ; : D2 D1 1+ ; : D3 D2 1+ ; : D4 D3 1+ ; : D5 D4 1+ ; : D6 D5 1+ ; : D7 D6 1+ ; : D8 D7 1+ ; : D9 D8 1+ ; : D10 D9 1+ ; : D11 D10 1+ ; : D12 D11 1+ ; : D13 D12 1+ ; : D14 D13 1+ ; : D15 D14 1+ ; : D16 D15 1+ ; : D17 D16 1+ ; : D18 D17 1+ ; : D19 D18 1+ ; : D20 D19 1+ ; D20 . CR",
                "20-word call chain", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* ===== 8. LARGE DEFINITIONS ===== */
    {
        "LARGE_DEF", {
            {
                "long_sequence_50",
                ": LONG50 1 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ . CR ; LONG50",
                "50+ operations", TEST_NORMAL, 0, 1
            },
            {
                "many_literals",
                ": LITS 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 + + + + + + + + + + + + + + + + + + + . CR ; LITS",
                "20 literals", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* ===== 9. DICTIONARY PRESSURE ===== */
    {
        "DICT_PRESSURE", {
            {
                "many_defs",
                ": T1 1 ; : T2 2 ; : T3 3 ; : T4 4 ; : T5 5 ; : T6 6 ; : T7 7 ; : T8 8 ; : T9 9 ; : T10 10 ; : T11 11 ; : T12 12 ; : T13 13 ; : T14 14 ; : T15 15 ; : T16 16 ; : T17 17 ; : T18 18 ; : T19 19 ; : T20 20 ; T20 . CR",
                "20 word definitions", TEST_NORMAL, 0, 1
            },
            {
                "long_name", ": VERY-LONG-WORD-NAME-TEST-MAX 42 . CR ; VERY-LONG-WORD-NAME-TEST-MAX",
                "Long word name (31 chars)", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* ===== 10. MEMORY ALLOCATION STRESS ===== */
    {
        "MEMORY_STRESS", {
            {
                "allot_100", ": ALLOT-100 HERE 100 ALLOT HERE SWAP - . CR ; ALLOT-100", "ALLOT 100 bytes", TEST_NORMAL,
                0, 1
            },
            {"allot_1000", ": ALLOT-1K HERE 1000 ALLOT HERE SWAP - . CR ; ALLOT-1K", "ALLOT 1KB", TEST_NORMAL, 0, 1},
            {"allot_8k", ": ALLOT-8K HERE 8192 ALLOT HERE SWAP - . CR ; ALLOT-8K", "ALLOT 8KB", TEST_NORMAL, 0, 1},
            {
                "multiple_allots", ": MULTI-ALLOT HERE 10 ALLOT 20 ALLOT 30 ALLOT HERE SWAP - . CR ; MULTI-ALLOT",
                "Multiple ALLOTs", TEST_NORMAL, 0, 1
            },
            {
                "allot_pattern", ": PATTERN 5 0 DO HERE 100 ALLOT DROP LOOP ; PATTERN", "Repeated allot pattern",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    /* ===== 11. ERROR RECOVERY ===== */
    {
        "ERROR_RECOVERY", {
            {"abort_recovery", ": AB-TEST 1 2 3 ABORT ; AB-TEST DEPTH . CR", "ABORT clears stacks", TEST_NORMAL, 0, 1},
            {"continue_after_abort", "ABORT 42 . CR", "Can continue after ABORT", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* ===== 12. DOUBLE NUMBER STRESS ===== */
    {
        "DOUBLE_STRESS", {
            {"large_double_add", "1000000 0 2000000 0 D+ . . CR", "Large double addition", TEST_NORMAL, 0, 1},
            {"double_chain", "100 200 2DUP D+ . . CR", "Double operation chain", TEST_NORMAL, 0, 1},
            {"double_negate", "100 200 DNEGATE DNEGATE . . CR", "Double negate twice", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    /* ===== 13. MIXED COMPLEXITY ===== */
    {
        "MIXED_COMPLEX", {
            {
                "nested_if_do",
                ": COMPLEX 10 0 DO I DUP 5 > IF 2 * ELSE 3 + THEN . LOOP CR ; COMPLEX",
                "Mixed IF/DO", TEST_NORMAL, 0, 1
            },
            {
                "nested_begin_do",
                ": NB-DO 5 BEGIN DUP 0> WHILE DUP 3 0 DO I . LOOP CR 1- REPEAT DROP ; NB-DO",
                "Nested BEGIN/DO", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* ===== 14. STRING/BUFFER EDGE CASES ===== */
    {
        "STRING_EDGE", {
            {"empty_def", ": EMPTY ; EMPTY", "Empty definition", TEST_NORMAL, 0, 1},
            {"single_word", ": SINGLE DUP ; 42 SINGLE . . CR", "Single word def", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },
};

/**
 * @brief Execute comprehensive stress test suites
 *
 * Runs an extensive set of stress tests to verify the VM's behavior
 * under extreme conditions. Tests include:
 * - Stack operations and boundaries
 * - Deep nesting and control flow
 * - Numeric boundaries and overflow
 * - Dictionary pressure
 * - Memory allocation patterns
 * - Error recovery
 * - Mixed complexity scenarios
 *
 * All tests are FORTH-79 compliant.
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void run_stress_tests(VM *vm) {
    if (!vm) return;

    // Reset global stats for stress tests
    extern TestStats global_test_stats;
    global_test_stats.total_tests = 0;
    global_test_stats.total_pass = 0;
    global_test_stats.total_fail = 0;
    global_test_stats.total_skip = 0;
    global_test_stats.total_error = 0;

    log_message(LOG_INFO, "==============================================");
    log_message(LOG_INFO, "   StarForth Comprehensive Stress Test Suite");
    log_message(LOG_INFO, "   FORTH-79 Standard Compliance");
    log_message(LOG_INFO, "==============================================\n");

    for (size_t i = 0; i < sizeof(stress_suites) / sizeof(WordTestSuite); i++) {
        log_message(LOG_TEST, "▶ Testing category: %s", stress_suites[i].word_name);
        run_test_suite(vm, &stress_suites[i]);
    }

    log_message(LOG_INFO, "==============================================");
    log_message(LOG_INFO, "   COMPREHENSIVE STRESS TEST RESULTS");
    log_message(LOG_INFO, "==============================================");
    log_message(LOG_INFO, "  Total tests: %d", global_test_stats.total_tests);
    log_message(LOG_INFO, "  Passed:      %d", global_test_stats.total_pass);
    log_message(LOG_INFO, "  Failed:      %d", global_test_stats.total_fail);
    log_message(LOG_INFO, "  Skipped:     %d", global_test_stats.total_skip);
    log_message(LOG_INFO, "  Errors:      %d", global_test_stats.total_error);

    if (global_test_stats.total_fail == 0 && global_test_stats.total_error == 0) {
        log_message(LOG_INFO, "\n  ✓ ALL STRESS TESTS PASSED!");
        log_message(LOG_INFO, "  StarForth demonstrates excellent stability");
        log_message(LOG_INFO, "  under extreme FORTH-79 workload conditions.");
    } else {
        log_message(LOG_INFO, "\n  ✗ SOME TESTS FAILED");
        log_message(LOG_INFO, "  Review output above for details");
    }
    log_message(LOG_INFO, "==============================================\n");
}