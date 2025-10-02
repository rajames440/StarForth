/*

                                 ***   StarForth   ***
  stack_words_test.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */
/**
 * @file stack_words_test.c
 * @brief Test module for Forth stack manipulation words
 * @details Contains test suites for basic stack operations including
 *          DUP, DROP, SWAP, OVER, ROT, DEPTH, PICK, and ROLL
 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/** 
 * @brief Test suites for stack manipulation words
 * @details Contains comprehensive test cases for all standard stack operations,
 *          including normal cases, edge cases, and error conditions
 */
static WordTestSuite stack_word_suites[] = {
    {
        "DUP", {
            {"basic", "5 DUP . . CR", "Should print: 5 5", TEST_NORMAL, 0, 1},
            {"zero", "0 DUP . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"negative", "-42 DUP . . CR", "Should print: -42 -42", TEST_NORMAL, 0, 1},
            {"max_int", "2147483647 DUP . . CR", "Should duplicate max int", TEST_EDGE_CASE, 0, 1},
            {"min_int", "-2147483648 DUP . . CR", "Should duplicate min int", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "DUP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "DROP", {
            {"basic", "5 7 DROP . CR", "Should print: 5", TEST_NORMAL, 0, 1},
            {"zero", "42 0 DROP . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"single_item", "99 DROP DEPTH . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"empty_stack", "DROP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "SWAP", {
            {"basic", "5 7 SWAP . . CR", "Should print: 7 5", TEST_NORMAL, 0, 1},
            {"same_values", "42 42 SWAP . . CR", "Should print: 42 42", TEST_NORMAL, 0, 1},
            {"zero_nonzero", "0 99 SWAP . . CR", "Should print: 99 0", TEST_NORMAL, 0, 1},
            {"negative", "-5 10 SWAP . . CR", "Should print: 10 -5", TEST_NORMAL, 0, 1},
            {"one_item", "42 SWAP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "SWAP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "OVER", {
            {"basic", "5 7 OVER . . . CR", "Should print: 5 7 5", TEST_NORMAL, 0, 1},
            {"zeros", "0 0 OVER . . . CR", "Should print: 0 0 0", TEST_NORMAL, 0, 1},
            {"mixed", "-1 42 OVER . . . CR", "Should print: -1 42 -1", TEST_NORMAL, 0, 1},
            {"one_item", "42 OVER", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "OVER", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "ROT", {
            {"basic", "1 2 3 ROT . . . CR", "Should print: 2 3 1", TEST_NORMAL, 0, 1},
            {"zeros", "0 0 0 ROT . . . CR", "Should print: 0 0 0", TEST_NORMAL, 0, 1},
            {"mixed", "-1 0 1 ROT . . . CR", "Should print: 0 1 -1", TEST_NORMAL, 0, 1},
            {"two_items", "1 2 ROT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 ROT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "ROT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "DEPTH", {
            {"empty", "DEPTH . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"one_item", "42 DEPTH . CR", "Should print: 1", TEST_NORMAL, 0, 1},
            {"multiple", "1 2 3 DEPTH . CR", "Should print: 3", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "PICK", {
            {"pick_0", "1 2 3 0 PICK . CR", "Should print: 3", TEST_NORMAL, 0, 0}, /* Not implemented yet */
            {"pick_1", "1 2 3 1 PICK . CR", "Should print: 2", TEST_NORMAL, 0, 0},
            {"pick_2", "1 2 3 2 PICK . CR", "Should print: 1", TEST_NORMAL, 0, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "ROLL", {
            {"roll_1", "1 2 3 1 ROLL . . . CR", "Should print: 1 3 2", TEST_NORMAL, 0, 0}, /* Stub */
            {"roll_2", "1 2 3 2 ROLL . . . CR", "Should print: 2 3 1", TEST_NORMAL, 0, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/**
 * @brief Executes all test suites for stack manipulation words
 * @param vm Pointer to the Forth virtual machine instance
 * @details Runs through all defined test cases for stack manipulation words
 *          and reports results through the logging system
 */
void run_stack_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Stack Words Tests (Module 1: Foundation)...");

    for (int i = 0; stack_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &stack_word_suites[i]);
    }

    print_module_summary("Stack Words", 0, 0, 0, 0); /* Will be calculated by test_suite */
}