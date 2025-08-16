/*

                                 ***   StarForth   ***
  return_stack_words_test.c - FORTH-79 Standard and ANSI C99 ONLY
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

/* Return Stack Words Test Suites - Module 2 */
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

void run_return_stack_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Return Stack Words Tests (Module 2)...");

    for (int i = 0; return_stack_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &return_stack_word_suites[i]);
    }

    print_module_summary("Return Stack Words", 0, 0, 0, 0);
}