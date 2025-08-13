/*

                                 ***   StarForth   ***
  editor_words_test.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/13/25, 9:18 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/* Editor Words Test Suites - Module 17 */
static WordTestSuite editor_word_suites[] = {
    {
        "L", {
            {"basic", "100 L", "Should list current line", TEST_NORMAL, 0, 0},
            {"out_of_range", "1025 L", "Should handle invalid line", TEST_ERROR_CASE, 1, 0},
            {"empty_stack", "L", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "I", {
            {"bounds", "0 I", "Should handle line 0", TEST_ERROR_CASE, 1, 0},
            {"empty_stack", "I", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    {
        "R", {
            {"bounds", "0 R", "Should handle line 0", TEST_ERROR_CASE, 1, 0},
            {"empty_stack", "R", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    {
        "E", {
            {"bounds", "0 E", "Should handle line 0", TEST_ERROR_CASE, 1, 0},
            {"empty_stack", "E", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_editor_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Editor Words Tests (Module 17)...");
    log_message(LOG_WARN, "Most editor tests marked as unimplemented due to interactive nature");
    
    for (int i = 0; editor_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &editor_word_suites[i]);
    }
    
    print_module_summary("Editor Words", 0, 0, 0, 0);
}
