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
 * @brief Test suite definitions for system control words
 * @details Contains test cases for each system word including normal operations
 * and error conditions
 */
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

/**
 * @brief Executes all system word tests
 * @details Runs test suites for FORTH system control operations including QUIT,
 * ABORT, ABORT", BYE, COLD, and WARM. Some tests are marked as unimplemented
 * due to their system-level effects
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void run_system_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running System Words Tests (Module 18)...");
    log_message(LOG_WARN, "Some system tests marked as unimplemented due to system-level effects");

    for (int i = 0; system_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &system_word_suites[i]);
    }

    print_module_summary("System Words", 0, 0, 0, 0);
}