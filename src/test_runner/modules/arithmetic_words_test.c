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

/* Arithmetic Words Test Suites - Module 5 */
static WordTestSuite arithmetic_word_suites[] = {
    {
        "+", {
            {"basic", "5 7 + . CR", "Should print: 12", TEST_NORMAL, 0, 1},
            {"zero_add", "42 0 + . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"negative", "-5 3 + . CR", "Should print: -2", TEST_NORMAL, 0, 1},
            {"both_negative", "-5 -3 + . CR", "Should print: -8", TEST_NORMAL, 0, 1},
            {"overflow", "2147483647 1 + . CR", "Should overflow", TEST_EDGE_CASE, 0, 1},
            {"underflow", "-2147483648 -1 + . CR", "Should underflow", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "+", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 +", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        8
    },

    {
        "-", {
            {"basic", "10 3 - . CR", "Should print: 7", TEST_NORMAL, 0, 1},
            {"zero_sub", "42 0 - . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"from_zero", "0 5 - . CR", "Should print: -5", TEST_NORMAL, 0, 1},
            {"negative", "5 -3 - . CR", "Should print: 8", TEST_NORMAL, 0, 1},
            {"same_values", "42 42 - . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"overflow", "-2147483648 1 - . CR", "Should underflow", TEST_EDGE_CASE, 0, 1},
            {"underflow", "2147483647 -1 - . CR", "Should overflow", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "-", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 -", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        9
    },

    {
        "*", {
            {"basic", "6 7 * . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"by_zero", "42 0 * . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 1 * . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"by_negative", "6 -7 * . CR", "Should print: -42", TEST_NORMAL, 0, 1},
            {"negative_negative", "-6 -7 * . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"large_numbers", "32767 2 * . CR", "Should print: 65534", TEST_EDGE_CASE, 0, 1},
            {"overflow", "65536 65536 * . CR", "May overflow", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "*", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 *", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        9
    },

    {
        "/", {
            {"basic", "15 3 / . CR", "Should print: 5", TEST_NORMAL, 0, 1},
            {"by_one", "42 1 / . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"negative_dividend", "-15 3 / . CR", "Should print: -5", TEST_NORMAL, 0, 1},
            {"negative_divisor", "15 -3 / . CR", "Should print: -5", TEST_NORMAL, 0, 1},
            {"both_negative", "-15 -3 / . CR", "Should print: 5", TEST_NORMAL, 0, 1},
            {"truncation", "7 3 / . CR", "Should print: 2 (truncated)", TEST_NORMAL, 0, 1},
            {"by_zero", "42 0 /", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "/", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 /", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        9
    },

    {
        "MOD", {
            {"basic", "17 5 MOD . CR", "Should print: 2", TEST_NORMAL, 0, 1},
            {"exact_division", "15 3 MOD . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 1 MOD . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative_dividend", "-17 5 MOD . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"negative_divisor", "17 -5 MOD . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"small_by_large", "3 7 MOD . CR", "Should print: 3", TEST_NORMAL, 0, 1},
            {"by_zero", "42 0 MOD", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "MOD", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 MOD", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        9
    },

    {
        "/MOD", {
            {"basic",       "17 5 /MOD . . CR", "Should print: 3 2",              TEST_NORMAL,     0, 1},
            {"exact",       "15 3 /MOD . . CR", "Should print: 5 0",              TEST_NORMAL,     0, 1},
            {"negative",    "-17 5 /MOD . . CR","Should handle negative dividend", TEST_NORMAL,     0, 1},
            {"by_zero",     "42 0 /MOD",        "Should cause division by zero",   TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "/MOD",             "Should cause stack underflow",    TEST_ERROR_CASE, 1, 1},
            {"one_item",    "42 /MOD",          "Should cause stack underflow",    TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "ABS", {
            {"positive",    "42 ABS . CR",          "Should print: 42",  TEST_NORMAL,     0, 1},
            {"negative",    "-42 ABS . CR",         "Should print: 42",  TEST_NORMAL,     0, 1},
            {"zero",        "0 ABS . CR",           "Should print: 0",   TEST_NORMAL,     0, 1},
            {"min_int",     "-2147483648 ABS . CR", "Should print: 2147483648", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "ABS",                  "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "NEGATE", {
            {"positive",    "42 NEGATE . CR",          "Should print: -42", TEST_NORMAL,     0, 1},
            {"negative",    "-42 NEGATE . CR",         "Should print: 42",  TEST_NORMAL,     0, 1},
            {"zero",        "0 NEGATE . CR",           "Should print: 0",   TEST_NORMAL,     0, 1},
            {"min_int",     "-2147483648 NEGATE . CR", "Should print: 2147483648", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "NEGATE",                  "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "MIN", {
            {"basic",       "5 3 MIN . CR",    "Should print: 3",   TEST_NORMAL,     0, 1},
            {"equal",       "42 42 MIN . CR",  "Should print: 42",  TEST_NORMAL,     0, 1},
            {"negative",    "-5 -3 MIN . CR",  "Should print: -5",  TEST_NORMAL,     0, 1},
            {"mixed",       "-1 1 MIN . CR",   "Should print: -1",  TEST_NORMAL,     0, 1},
            {"one_item",    "42 MIN",          "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "MIN",             "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "MAX", {
            {"basic",       "5 3 MAX . CR",    "Should print: 5",   TEST_NORMAL,     0, 1},
            {"equal",       "42 42 MAX . CR",  "Should print: 42",  TEST_NORMAL,     0, 1},
            {"negative",    "-5 -3 MAX . CR",  "Should print: -3",  TEST_NORMAL,     0, 1},
            {"mixed",       "-1 1 MAX . CR",   "Should print: 1",   TEST_NORMAL,     0, 1},
            {"one_item",    "42 MAX",          "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "MAX",             "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "1+", {
            {"basic",          "5 1+ . CR",          "Should print: 6",  TEST_NORMAL, 0, 1},
            {"zero",           "0 1+ . CR",           "Should print: 1",  TEST_NORMAL, 0, 1},
            {"negative",       "-1 1+ . CR",          "Should print: 0",  TEST_NORMAL, 0, 1},
            {"chain",          "3 1+ 1+ 1+ . CR",     "Should print: 6",  TEST_NORMAL, 0, 1},
            {"overflow",       "2147483647 1+ . CR",  "Should overflow",  TEST_EDGE_CASE, 0, 1},
            {"empty_stack",    "1+",                  "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "1-", {
            {"basic",          "5 1- . CR",           "Should print: 4",  TEST_NORMAL, 0, 1},
            {"to_zero",        "1 1- . CR",           "Should print: 0",  TEST_NORMAL, 0, 1},
            {"to_negative",    "0 1- . CR",           "Should print: -1", TEST_NORMAL, 0, 1},
            {"chain",          "6 1- 1- 1- . CR",     "Should print: 3",  TEST_NORMAL, 0, 1},
            {"underflow",      "-2147483648 1- . CR", "Should underflow", TEST_EDGE_CASE, 0, 1},
            {"empty_stack",    "1-",                  "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "2+", {
            {"basic",          "5 2+ . CR",           "Should print: 7",  TEST_NORMAL, 0, 1},
            {"zero",           "0 2+ . CR",           "Should print: 2",  TEST_NORMAL, 0, 1},
            {"negative",       "-3 2+ . CR",          "Should print: -1", TEST_NORMAL, 0, 1},
            {"empty_stack",    "2+",                  "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "2-", {
            {"basic",          "7 2- . CR",           "Should print: 5",  TEST_NORMAL, 0, 1},
            {"zero",           "2 2- . CR",           "Should print: 0",  TEST_NORMAL, 0, 1},
            {"negative",       "0 2- . CR",           "Should print: -2", TEST_NORMAL, 0, 1},
            {"empty_stack",    "2-",                  "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "2*", {
            {"basic",          "5 2* . CR",           "Should print: 10", TEST_NORMAL, 0, 1},
            {"zero",           "0 2* . CR",           "Should print: 0",  TEST_NORMAL, 0, 1},
            {"negative",       "-3 2* . CR",          "Should print: -6", TEST_NORMAL, 0, 1},
            {"one",            "1 2* . CR",           "Should print: 2",  TEST_NORMAL, 0, 1},
            {"large",          "1073741823 2* . CR",  "Should print: 2147483646", TEST_EDGE_CASE, 0, 1},
            {"empty_stack",    "2*",                  "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "2/", {
            {"basic",          "10 2/ . CR",          "Should print: 5",  TEST_NORMAL, 0, 1},
            {"odd",            "7 2/ . CR",           "Should print: 3",  TEST_NORMAL, 0, 1},
            {"zero",           "0 2/ . CR",           "Should print: 0",  TEST_NORMAL, 0, 1},
            {"negative_even",  "-10 2/ . CR",         "Should print: -5", TEST_NORMAL, 0, 1},
            {"one",            "1 2/ . CR",           "Should print: 0",  TEST_NORMAL, 0, 1},
            {"empty_stack",    "2/",                  "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/**
 * @brief Executes all arithmetic word test suites
 * @param vm Pointer to the Forth virtual machine instance
 * @details Runs through all test cases for arithmetic operations including
 *          basic operations (+, -, *, /), modulo operations (MOD, /MOD),
 *          and numeric functions (ABS, NEGATE, MIN, MAX)
 */
void run_arithmetic_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Arithmetic Words Tests (Module 5)...");

    WordContract mod = {CONTRACT_PHYSICS_TRANSPARENT, 0};
    for (int i = 0; arithmetic_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite_m(vm, &arithmetic_word_suites[i], mod);
    }

    print_module_summary("Arithmetic Words", 0, 0, 0, 0);
}