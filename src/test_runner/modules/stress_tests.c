/*
 * stress_tests.c - Stress testing for StarForth
 * Tests extreme scenarios: deep nesting, large definitions, stack exhaustion
 */

#include "test_common.h"
#include "test_runner.h"

/* Stress test suites */
static WordTestSuite stress_suites[] = {
    {
        "STACK_DEPTH", {
            {"deep_push", ": PUSH-100 0 100 0 DO I LOOP ; PUSH-100 DEPTH . CR", "Push 100 items", TEST_NORMAL, 0, 1},
            {"deep_math", ": MATH-STRESS 1 100 0 DO 1+ LOOP . CR ; MATH-STRESS", "100 additions", TEST_NORMAL, 0, 1},
            {
                "dup_stress", ": DUP-STRESS 42 50 0 DO DUP LOOP DEPTH . CR 51 0 DO DROP LOOP ; DUP-STRESS", "50 DUPs",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },
    {
        "CONTROL_NESTING", {
            {
                "nested_if_3",
                ": NEST3 DUP 0> IF DUP 10 > IF DUP 20 > IF 3 ELSE 2 THEN ELSE 1 THEN ELSE 0 THEN . CR ; 25 NEST3",
                "3-level IF nesting", TEST_NORMAL, 0, 1
            },
            {
                "nested_loops", ": NEST-LOOP 0 3 0 DO 3 0 DO 1+ LOOP LOOP . CR ; NEST-LOOP", "Nested DO loops",
                TEST_NORMAL, 0, 1
            },
            {
                "deep_begin", ": COUNT-DOWN 10 BEGIN DUP . 1- DUP 0= UNTIL DROP CR ; COUNT-DOWN",
                "BEGIN UNTIL countdown", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },
    {
        "LARGE_DEFINITIONS", {
            {
                "long_sequence", ": LONG 1 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ 1+ . CR ; LONG",
                "20+ operations", TEST_NORMAL, 0, 1
            },
            {
                "many_words", ": W1 1 ; : W2 W1 1+ ; : W3 W2 1+ ; : W4 W3 1+ ; : W5 W4 1+ ; W5 . CR", "5 word chain",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },
    {
        "RECURSION", {
            {
                "factorial", ": FACT DUP 2 < IF DROP 1 ELSE DUP 1- FACT * THEN ; 5 FACT . CR", "Factorial(5)=120",
                TEST_NORMAL, 0, 1
            },
            {
                "fibonacci", ": FIB DUP 2 < IF ELSE DUP 1- FIB SWAP 2 - FIB + THEN ; 7 FIB . CR", "Fib(7)=13",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },
    {
        "MEMORY_STRESS", {
            {
                "many_variables",
                ": VARS VARIABLE V1 VARIABLE V2 VARIABLE V3 42 V1 ! 43 V2 ! 44 V3 ! V1 @ V2 @ + V3 @ + . CR ; VARS",
                "Multiple variables", TEST_NORMAL, 0, 1
            },
            {
                "allot_stress", ": ALLOT-TEST HERE 100 ALLOT HERE SWAP - . CR ; ALLOT-TEST", "ALLOT 100 bytes",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },
};

void run_stress_tests(VM *vm) {
    if (!vm) return;

    log_message(LOG_INFO, "==============================================");
    log_message(LOG_INFO, "   StarForth Stress Test Suite");
    log_message(LOG_INFO, "==============================================\n");

    for (size_t i = 0; i < sizeof(stress_suites) / sizeof(WordTestSuite); i++) {
        run_test_suite(vm, &stress_suites[i]);
    }

    log_message(LOG_INFO, "\n==============================================");
    log_message(LOG_INFO, "   Stress Tests Complete");
    log_message(LOG_INFO, "==============================================\n");
}