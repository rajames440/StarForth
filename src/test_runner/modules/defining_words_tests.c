/*
 * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
 * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/* Defining Words Test Suites - Module 13 */
static WordTestSuite defining_word_suites[] = {
    {
        ":", {
            {"basic", ": TEST1 42 ; TEST1 . CR", "Should define and execute", TEST_NORMAL, 0, 1},
            {"recursive", ": TEST2 DUP 0= IF DROP EXIT THEN DUP . 1- RECURSE ; 3 TEST2 CR", "Should handle recursion", TEST_NORMAL, 0, 1},
            {"empty_name", ":", "Should handle empty definition", TEST_ERROR_CASE, 1, 1},
            {"nested", ": TEST3 : ;", "Should prevent nested definition", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 4
    },

    {
        ";", {
            {"alone", ";", "Should error outside definition", TEST_ERROR_CASE, 1, 1},
            {"immediate", ": TEST4 42 ; IMMEDIATE TEST4 . CR", "Should handle immediate", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    {
        "CONSTANT", {
            {"basic", "42 CONSTANT MEANING MEANING . CR", "Should create constant", TEST_NORMAL, 0, 1},
            {"zero", "0 CONSTANT ZERO ZERO . CR", "Should handle zero", TEST_NORMAL, 0, 1},
            {"negative", "-1 CONSTANT MINUS MINUS . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"empty_stack", "CONSTANT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 4
    },

    {
        "VARIABLE", {
            {"basic", "VARIABLE VAR1 42 VAR1 ! VAR1 @ . CR", "Should create variable", TEST_NORMAL, 0, 1},
            {"multiple", "VARIABLE VAR2 VARIABLE VAR3", "Should create multiple", TEST_NORMAL, 0, 1},
            {"store_fetch", "VARIABLE VAR4 -99 VAR4 ! VAR4 @ . CR", "Should store/fetch", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "CREATE", {
            {"basic", "CREATE OBJ1", "Should create word", TEST_NORMAL, 0, 1},
            {"with_data", "CREATE OBJ2 42 , OBJ2 @ . CR", "Should allow data", TEST_NORMAL, 0, 1},
            {"empty_name", "CREATE", "Should handle empty name", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "DOES>", {
            {"basic", ": CONST CREATE , DOES> @ ; 42 CONST MEANING MEANING . CR", "Should define behavior", TEST_NORMAL, 0, 1},
            {"multiple", ": ARRAY CREATE DOES> SWAP CELLS + ; CREATE ARR 10 CELLS ALLOT", "Should work with arrays", TEST_NORMAL, 0, 1},
            {"outside", "DOES>", "Should error outside definition", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "<BUILDS", {
            {"basic", ": CONST <BUILDS , DOES> @ ; 42 CONST MEANING2 MEANING2 . CR", "Should create word", TEST_NORMAL, 0, 1},
            {"outside", "<BUILDS", "Should error outside definition", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    {
        "[", {
            {"basic", ": TEST5 [ 42 ] LITERAL ; TEST5 . CR", "Should enter interpret", TEST_NORMAL, 0, 1},
            {"outside", "[", "Should error outside definition", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    {
        "]", {
            {"basic", "[ 42 ] . CR", "Should resume compile", TEST_NORMAL, 0, 1},
            {"nested", ": TEST6 [ ] 42 ; TEST6 . CR", "Should handle nesting", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_defining_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Defining Words Tests (Module 13)...");
    
    for (int i = 0; defining_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &defining_word_suites[i]);
    }
    
    print_module_summary("Defining Words", 0, 0, 0, 0);
}
