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

/* Logical Words Test Suites - Module 8 */
static WordTestSuite logical_word_suites[] = {
    {
        "AND", {
            {"both_true", "-1 -1 AND . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"first_false", "0 -1 AND . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"second_false", "-1 0 AND . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"both_false", "0 0 AND . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"bitwise", "85 51 AND . CR", "Should print: 17", TEST_NORMAL, 0, 1},
            {"empty_stack", "AND", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 AND", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        "OR", {
            {"both_true", "-1 -1 OR . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"first_false", "0 -1 OR . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"second_false", "-1 0 OR . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"both_false", "0 0 OR . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"bitwise", "85 51 OR . CR", "Should print: 119", TEST_NORMAL, 0, 1},
            {"empty_stack", "OR", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 OR", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        "XOR", {
            {"both_true", "-1 -1 XOR . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"first_false", "0 -1 XOR . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"second_false", "-1 0 XOR . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"both_false", "0 0 XOR . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"bitwise", "85 51 XOR . CR", "Should print: 102", TEST_NORMAL, 0, 1},
            {"empty_stack", "XOR", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 XOR", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        "NOT", {
            {"true", "-1 NOT . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"false", "0 NOT . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"positive", "42 NOT . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "-42 NOT . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"empty_stack", "NOT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "=", {
            {"equal", "42 42 = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"not_equal", "42 43 = . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"zero_equal", "0 0 = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"negative_equal", "-42 -42 = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"negative_positive", "-42 42 = . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"empty_stack", "=", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 =", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        "<>", {
            {"not_equal", "42 43 <> . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"equal", "42 42 <> . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"zero_equal", "0 0 <> . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "-42 42 <> . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"empty_stack", "<>", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 <>", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "<", {
            {"less_than", "5 7 < . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"greater_than", "7 5 < . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"equal", "5 5 < . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative_positive", "-5 5 < . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"negative_negative", "-7 -5 < . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"empty_stack", "<", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 <", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        ">", {
            {"greater_than", "7 5 > . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"less_than", "5 7 > . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"equal", "5 5 > . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"positive_negative", "5 -5 > . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"negative_negative", "-5 -7 > . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"empty_stack", ">", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 >", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        "0=", {
            {"zero", "0 0= . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"positive", "42 0= . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "-42 0= . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"empty_stack", "0=", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "0<", {
            {"negative", "-42 0< . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"zero", "0 0< . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"positive", "42 0< . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"empty_stack", "0<", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "0>", {
            {"positive", "42 0> . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"zero", "0 0> . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "-42 0> . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"empty_stack", "0>", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/**
 * @brief Executes all logical words test suites
 *
 * This function runs comprehensive tests for all FORTH logical and comparison operators,
 * including stack underflow checks and edge cases.
 *
 * @param vm Pointer to the FORTH virtual machine instance
 */
void run_logical_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Logical Words Tests (Module 8)...");

    for (int i = 0; logical_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &logical_word_suites[i]);
    }

    print_module_summary("Logical Words", 0, 0, 0, 0);
}