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
            /* Basic functionality */
            {"basic", "5 7 DROP . CR", "Should print: 5", TEST_NORMAL, 0, 1},
            {"zero", "42 0 DROP . CR", "Should drop zero, print 42", TEST_NORMAL, 0, 1},
            {"negative", "10 -5 DROP . CR", "Should drop negative, print 10", TEST_NORMAL, 0, 1},

            /* Multiple drops */
            {"double_drop", "1 2 3 DROP DROP . CR", "Should print: 1", TEST_NORMAL, 0, 1},
            {"triple_drop", "1 2 3 4 DROP DROP DROP . CR", "Should print: 1", TEST_NORMAL, 0, 1},
            {"sequential", "5 DROP 6 DROP 7 . CR", "Should print: 7", TEST_NORMAL, 0, 1},

            /* Stack depth effects */
            {"single_item", "99 DROP DEPTH . CR", "Should empty stack, depth=0", TEST_NORMAL, 0, 1},
            {"depth_change", "DEPTH 1 2 DROP DEPTH SWAP - . CR", "Should change depth by 1", TEST_NORMAL, 0, 1},
            {"preserve_depth", "DEPTH 5 6 DROP DROP DEPTH = . CR", "Should preserve net depth", TEST_NORMAL, 0, 1},

            /* Edge cases */
            {"max_int", "2147483647 DROP DEPTH . CR", "Should drop max int", TEST_NORMAL, 0, 1},
            {"min_int", "-2147483648 DROP DEPTH . CR", "Should drop min int", TEST_NORMAL, 0, 1},

            /* Chaining with other operations */
            {"with_dup", "5 DUP DROP . CR", "Should duplicate then drop", TEST_NORMAL, 0, 1},
            {"with_swap", "1 2 SWAP DROP . CR", "Should swap then drop", TEST_NORMAL, 0, 1},
            {"complex", "1 2 3 ROT DROP SWAP DROP . CR", "Should handle complex sequence", TEST_NORMAL, 0, 1},

            /* Error case */
            {"empty_stack", "DROP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},

            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        15
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
 *          and reports results_run_01_2025_12_08 through the logging system
 */
void run_stack_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Stack Words Tests (Module 1: Foundation)...");

    for (int i = 0; stack_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &stack_word_suites[i]);
    }

    print_module_summary("Stack Words", 0, 0, 0, 0); /* Will be calculated by test_suite */
}