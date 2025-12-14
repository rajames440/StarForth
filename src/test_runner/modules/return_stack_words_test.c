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

#include "../include/test_runner.h"
#include "../include/test_common.h"

/** 
 * @brief Test suites for return stack manipulation words
 * @details Contains test cases for >R (push to return stack), 
 *          R> (pop from return stack), and R@ (copy top of return stack)
 */
static WordTestSuite return_stack_word_suites[] = {
    {
        ">R", {
            {"basic", "42 >R R@ . R> . CR", "Should print: 42 42", TEST_NORMAL, 0, 1},
            {"zero", "0 >R R@ . R> . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"negative", "-123 >R R@ . R> . CR", "Should print: -123 -123", TEST_NORMAL, 0, 1},
            {"multiple", "1 2 >R >R R@ . R> . R@ . R> . CR", "Should print: 2 2 1 1", TEST_NORMAL, 0, 1},
            {"empty_stack", ">R", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "R>", {
            {"basic", "42 >R R> . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"lifo_order", "1 2 >R >R R> . R> . CR", "Should print: 2 1", TEST_NORMAL, 0, 1},
            {"empty_rstack", "R>", "Should cause return stack underflow", TEST_ERROR_CASE, 1, 0}, /* Stub */
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "R@", {
            {"basic", "42 >R R@ . R> DROP CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"non_destructive", "99 >R R@ R@ = . R> DROP CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"empty_rstack", "R@", "Should cause return stack underflow", TEST_ERROR_CASE, 1, 0}, /* Stub */
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/**
 * @brief Executes all test suites for return stack manipulation words
 * @param vm Pointer to the Forth virtual machine instance
 * @details Runs through all defined test cases for >R, R>, and R@ words
 *          and reports results_run_01_2025_12_08 through the logging system
 */
void run_return_stack_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Return Stack Words Tests (Module 2)...");

    for (int i = 0; return_stack_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &return_stack_word_suites[i]);
    }

    print_module_summary("Return Stack Words", 0, 0, 0, 0);
}