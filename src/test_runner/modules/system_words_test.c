/*

                                 ***   StarForth   ***
  system_words_test.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/13/25, 9:10 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/* System Words Test Suites - Module 18 */
static WordTestSuite system_word_suites[] = {
    {
        "QUIT", {
            {"basic", "QUIT", "Should reset stacks and state", TEST_NORMAL, 0, 0}, // Interactive
            {"in_definition", ": BAD-WORD QUIT ;", "Should prevent compilation", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "ABORT", {
            {"basic", "ABORT", "Should clear stacks and return to QUIT", TEST_NORMAL, 0, 1},
            {"with_data", "1 2 3 ABORT DEPTH . CR", "Should clear stack", TEST_NORMAL, 0, 1},
            {
                "in_definition_runtime", ": BAD-WORD ABORT ;  123 BAD-WORD  DEPTH . CR",
                "ABORT may appear in a definition; when executed it clears both stacks", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "ABORT\"", {
            {
                "condition_true", "-1 ABORT\" Error\"", "Should abort with message (no error; stacks cleared to QUIT)",
                TEST_NORMAL, 0, 1
            },
            {"condition_false", "0 ABORT\" Error\"", "Should not abort", TEST_NORMAL, 0, 1},
            {"empty_stack", "ABORT\"", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "BYE", {
            {"basic", "BYE", "Should exit cleanly", TEST_NORMAL, 0, 0}, // Can't really test
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        1
    },

    {
        "COLD", {
            {"basic", "COLD", "Should reset system", TEST_NORMAL, 0, 0}, // Dangerous to test
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        1
    },

    {
        "WARM", {
            {"basic", "WARM", "Should soft reset", TEST_NORMAL, 0, 0}, // Dangerous to test
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        1
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_system_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running System Words Tests (Module 18)...");
    log_message(LOG_WARN, "Some system tests marked as unimplemented due to system-level effects");

    for (int i = 0; system_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &system_word_suites[i]);
    }

    print_module_summary("System Words", 0, 0, 0, 0);
}