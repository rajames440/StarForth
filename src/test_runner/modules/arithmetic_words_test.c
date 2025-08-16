/*

                                 ***   StarForth   ***
  arithmetic_words_test.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


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
            {"basic", "17 5 /MOD . . CR", "Should print: 3 2", TEST_NORMAL, 0, 0}, /* Stub */
            {"exact", "15 3 /MOD . . CR", "Should print: 5 0", TEST_NORMAL, 0, 0},
            {"by_zero", "42 0 /MOD", "Should cause division by zero", TEST_ERROR_CASE, 1, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "ABS", {
            {"positive", "42 ABS . CR", "Should print: 42", TEST_NORMAL, 0, 0}, /* Stub */
            {"negative", "-42 ABS . CR", "Should print: 42", TEST_NORMAL, 0, 0},
            {"zero", "0 ABS . CR", "Should print: 0", TEST_NORMAL, 0, 0},
            {"min_int", "-2147483648 ABS . CR", "Should handle min int", TEST_EDGE_CASE, 0, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "NEGATE", {
            {"positive", "42 NEGATE . CR", "Should print: -42", TEST_NORMAL, 0, 0}, /* Stub */
            {"negative", "-42 NEGATE . CR", "Should print: 42", TEST_NORMAL, 0, 0},
            {"zero", "0 NEGATE . CR", "Should print: 0", TEST_NORMAL, 0, 0},
            {"min_int", "-2147483648 NEGATE . CR", "Should handle min int", TEST_EDGE_CASE, 0, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "MIN", {
            {"basic", "5 3 MIN . CR", "Should print: 3", TEST_NORMAL, 0, 0}, /* Stub */
            {"equal", "42 42 MIN . CR", "Should print: 42", TEST_NORMAL, 0, 0},
            {"negative", "-5 -3 MIN . CR", "Should print: -5", TEST_NORMAL, 0, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "MAX", {
            {"basic", "5 3 MAX . CR", "Should print: 5", TEST_NORMAL, 0, 0}, /* Stub */
            {"equal", "42 42 MAX . CR", "Should print: 42", TEST_NORMAL, 0, 0},
            {"negative", "-5 -3 MAX . CR", "Should print: -3", TEST_NORMAL, 0, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_arithmetic_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Arithmetic Words Tests (Module 5)...");

    for (int i = 0; arithmetic_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &arithmetic_word_suites[i]);
    }

    print_module_summary("Arithmetic Words", 0, 0, 0, 0);
}