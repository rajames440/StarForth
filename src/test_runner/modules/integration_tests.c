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
 * @file integration_tests.c
 * @brief Full program integration tests for StarForth
 *
 * This file contains integration tests that verify complete Forth programs
 * as regression tests. It includes test suites for complete programs,
 * data structures, and algorithms implementations.
 */

#include "test_common.h"
#include "test_runner.h"

/* Integration test suites - complete Forth programs */
static WordTestSuite integration_suites[] = {
    {
        "COMPLETE_PROGRAMS", {
            {
                "prime_check",
                ": PRIME? ( n -- flag ) "
                "  DUP 2 < IF DROP 0 EXIT THEN "
                "  DUP 2 = IF DROP -1 EXIT THEN "
                "  DUP 2 MOD 0= IF DROP 0 EXIT THEN "
                "  DUP 3 DO "
                "    DUP I MOD 0= IF DROP 0 EXIT THEN "
                "  2 +LOOP "
                "  DROP -1 "
                "; "
                "17 PRIME? . CR",
                "Prime number checker",
                TEST_NORMAL, 0, 1
            },
            {
                "array_sum",
                ": ARRAY 5 CELLS ALLOT ; "
                "ARRAY NUMS "
                ": STORE-NUMS "
                "  10 NUMS 0 CELLS + ! "
                "  20 NUMS 1 CELLS + ! "
                "  30 NUMS 2 CELLS + ! "
                "  40 NUMS 3 CELLS + ! "
                "  50 NUMS 4 CELLS + ! "
                "; "
                ": SUM-NUMS "
                "  0 5 0 DO "
                "    NUMS I CELLS + @ + "
                "  LOOP "
                "; "
                "STORE-NUMS SUM-NUMS . CR",
                "Array sum program",
                TEST_NORMAL, 0, 1
            },
            {
                "fizzbuzz_mini",
                ": FIZZ? 3 MOD 0= ; "
                ": BUZZ? 5 MOD 0= ; "
                ": FIZZBUZZ "
                "  DUP FIZZ? OVER BUZZ? AND IF DROP .\" FizzBuzz \" EXIT THEN "
                "  DUP FIZZ? IF DROP .\" Fizz \" EXIT THEN "
                "  DUP BUZZ? IF DROP .\" Buzz \" EXIT THEN "
                "  . "
                "; "
                "15 FIZZBUZZ CR",
                "FizzBuzz for 15",
                TEST_NORMAL, 0, 1
            },
            {
                "calculator",
                ": + DEPTH 2 < IF .\" Stack underflow\" CR EXIT THEN + ; "
                ": CALC "
                "  5 3 + "
                "  2 * "
                "  4 - "
                "  . CR "
                "; "
                "CALC",
                "Simple calculator",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },
    {
        "DATA_STRUCTURES", {
            {
                "linked_list",
                "VARIABLE HEAD "
                "0 HEAD ! "
                ": NODE ( value -- addr ) "
                "  HERE "
                "  , " /* value */
                "  HEAD @ , " /* next pointer */
                "  DUP HEAD ! "
                "; "
                ": PRINT-LIST ( -- ) "
                "  HEAD @ "
                "  BEGIN DUP WHILE "
                "    DUP @ . "
                "    CELL+ @ "
                "  REPEAT DROP CR "
                "; "
                "10 NODE DROP "
                "20 NODE DROP "
                "30 NODE DROP "
                "PRINT-LIST",
                "Simple linked list",
                TEST_NORMAL, 0, 1
            },
            {
                "stack_min_max",
                ": INIT-MINMAX "
                "  2147483647 VARIABLE MIN " /* Max int */
                "  -2147483648 VARIABLE MAX " /* Min int */
                "; "
                ": UPDATE-MINMAX ( n -- ) "
                "  DUP MIN @ < IF DUP MIN ! ELSE DROP THEN "
                "  DUP MAX @ > IF DUP MAX ! ELSE DROP THEN "
                "; "
                "INIT-MINMAX "
                "42 UPDATE-MINMAX "
                "17 UPDATE-MINMAX "
                "99 UPDATE-MINMAX "
                "MIN @ . MAX @ . CR",
                "Min/Max tracker",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },
    {
        "ALGORITHMS", {
            {
                "bubble_sort_concept",
                ": SWAP-IF-NEEDED ( addr1 addr2 -- ) "
                "  OVER @ OVER @ > IF "
                "    OVER @ OVER @ "
                "    ROT ! SWAP ! "
                "  ELSE 2DROP THEN "
                "; "
                "VARIABLE A "
                "VARIABLE B "
                "5 A ! 3 B ! "
                "A B SWAP-IF-NEEDED "
                "A @ . B @ . CR",
                "Swap if needed (sort helper)",
                TEST_NORMAL, 0, 1
            },
            {
                "gcd",
                ": GCD ( a b -- gcd ) "
                "  BEGIN DUP WHILE "
                "    TUCK MOD "
                "  REPEAT DROP "
                "; "
                "48 18 GCD . CR",
                "Greatest Common Divisor",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },
};

/**
 * @brief Executes all integration test suites for StarForth
 *
 * @param vm Pointer to the Forth virtual machine instance
 *
 * This function runs through all defined integration test suites, including:
 * - Complete Programs suite (prime checker, array operations, etc)
 * - Data Structures suite (linked list, min/max tracker)
 * - Algorithms suite (sorting, GCD)
 */
void run_integration_tests(VM *vm) {
    if (!vm) return;

    log_message(LOG_INFO, "==============================================");
    log_message(LOG_INFO, "   StarForth Integration Test Suite");
    log_message(LOG_INFO, "   Complete Forth Programs");
    log_message(LOG_INFO, "==============================================\n");

    for (size_t i = 0; i < sizeof(integration_suites) / sizeof(WordTestSuite); i++) {
        run_test_suite(vm, &integration_suites[i]);
    }

    log_message(LOG_INFO, "\n==============================================");
    log_message(LOG_INFO, "   Integration Tests Complete");
    log_message(LOG_INFO, "==============================================\n");
}