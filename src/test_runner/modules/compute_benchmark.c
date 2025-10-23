/*
                                  ***   StarForth   ***

  compute_benchmark.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.529-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/test_runner/modules/compute_benchmark.c
 */

/**
 * @file compute_benchmark.c
 * @brief Compute-intensive benchmarks for StarForth
 *
 * Tests CPU-bound operations to measure optimization effectiveness:
 * - Arithmetic in tight loops
 * - Stack manipulation
 * - Memory access patterns
 * - Control flow (loops, conditionals)
 */

#include "test_common.h"
#include "test_runner.h"
#include <time.h>

/** 
 * @brief Benchmark test suites for compute-intensive operations
 *
 * Array of test suites that measure performance of various CPU-bound operations:
 * - Arithmetic operations in tight loops
 * - Stack manipulation operations
 * - Memory access patterns
 * - Control flow constructs
 */
static WordTestSuite compute_suites[] = {
    {
        "ARITHMETIC-BENCH", {
            {
                "tight_loop", ": BENCH-ARITH 0 1000000 0 DO I + LOOP DROP ; BENCH-ARITH", "Sum 1M integers",
                TEST_NORMAL, 0, 1
            },
            {
                "multiply", ": BENCH-MUL 1 1000000 0 DO I * LOOP DROP ; BENCH-MUL", "Multiply 1M integers", TEST_NORMAL,
                0, 1
            },
            {
                "mixed_ops", ": BENCH-MIXED 0 100000 0 DO I 2 * 3 + 5 / LOOP DROP ; BENCH-MIXED", "Mixed arithmetic",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },
    {
        "STACK-BENCH", {
            {
                "dup_drop", ": BENCH-STACK 42 1000000 0 DO DUP DROP LOOP DROP ; BENCH-STACK", "DUP/DROP 1M times",
                TEST_NORMAL, 0, 1
            },
            {
                "swap_rot", ": BENCH-SWAP 1 2 3 100000 0 DO SWAP ROT LOOP DROP DROP DROP ; BENCH-SWAP",
                "SWAP/ROT 100K times", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },
    {
        "MEMORY-BENCH", {
            {
                "store_fetch", "VARIABLE BM-VAR : BENCH-MEM 1000000 0 DO I BM-VAR ! BM-VAR @ DROP LOOP ; BENCH-MEM",
                "Store/fetch 1M times", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        1
    },
    {
        "CONTROL-BENCH", {
            {
                "nested_loops", ": BENCH-NEST 1000 0 DO 1000 0 DO J I + DROP LOOP LOOP ; BENCH-NEST",
                "Nested loops 1M iterations", TEST_NORMAL, 0, 1
            },
            {
                "conditionals", ": BENCH-IF 1000000 0 DO I 2 MOD 0= IF 1 ELSE 2 THEN DROP LOOP ; BENCH-IF",
                "IF/ELSE 1M times", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    }
};

/**
 * @brief Runs all compute-intensive benchmark suites
 *
 * @param vm Pointer to the Forth virtual machine instance
 *
 * Executes all benchmark test suites measuring CPU-bound operations
 * and reports execution time for each suite and total time.
 */
void run_compute_benchmarks(VM *vm) {
    clock_t start = clock();

    log_message(LOG_INFO, "Testing CPU-bound operations...\n");

    for (size_t i = 0; i < sizeof(compute_suites) / sizeof(WordTestSuite); i++) {
        clock_t suite_start = clock();
        run_test_suite(vm, &compute_suites[i]);
        clock_t suite_end = clock();

        double elapsed = (double) (suite_end - suite_start) / CLOCKS_PER_SEC;
        log_message(LOG_INFO, "  %s completed in %.3f seconds\n",
                    compute_suites[i].word_name, elapsed);
    }

    clock_t end = clock();
    double total = (double) (end - start) / CLOCKS_PER_SEC;

    log_message(LOG_INFO, "=== COMPUTE BENCHMARK COMPLETE ===");
    log_message(LOG_INFO, "Total compute time: %.3f seconds\n", total);
}