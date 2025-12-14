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

/** @brief Test suites for Forth defining words (Module 13)
 *  @details Contains test cases for core defining words including:
 *           :, ;, CONSTANT, VARIABLE, CREATE, DOES>, [, and ]
 */
static WordTestSuite defining_word_suites[] = {
    {
        ":", {
            {"basic", ": TEST1 42 ; TEST1 . CR", "Should define and execute", TEST_NORMAL, 0, 1},
            {"empty_name", ":", "Should handle empty definition", TEST_ERROR_CASE, 1, 1},
            {"nested", ": TEST3 : ;", "Should prevent nested definition", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        ";", {
            {"alone", ";", "Should error outside definition", TEST_ERROR_CASE, 1, 1},
            {"immediate", ": TEST4 42 ; IMMEDIATE TEST4 . CR", "Should handle immediate", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "CONSTANT", {
            {"basic", "42 CONSTANT MEANING MEANING . CR", "Should create constant", TEST_NORMAL, 0, 1},
            {"zero", "0 CONSTANT ZERO ZERO . CR", "Should handle zero", TEST_NORMAL, 0, 1},
            {"negative", "-1 CONSTANT MINUS MINUS . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"empty_stack", "CONSTANT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "VARIABLE", {
            {"basic", "VARIABLE VAR1 42 VAR1 ! VAR1 @ . CR", "Should create variable", TEST_NORMAL, 0, 1},
            {"multiple", "VARIABLE VAR2 VARIABLE VAR3", "Should create multiple", TEST_NORMAL, 0, 1},
            {"store_fetch", "VARIABLE VAR4 -99 VAR4 ! VAR4 @ . CR", "Should store/fetch", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "CREATE", {
            {"basic", "CREATE OBJ1", "Should create word", TEST_NORMAL, 0, 1},
            {"with_data", "CREATE OBJ2 42 , OBJ2 @ . CR", "Should allow data", TEST_NORMAL, 0, 1},
            {"empty_name", "CREATE", "Should handle empty name", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "DOES>", {
            {
                "basic", ": CONST CREATE , DOES> @ ; 42 CONST MEANING MEANING . CR", "Should define behavior",
                TEST_NORMAL, 0, 1
            },
            {
                "multiple", ": ARRAY CREATE DOES> SWAP CELLS + ; CREATE ARR 10 CELLS ALLOT", "Should work with arrays",
                TEST_NORMAL, 0, 1
            },
            {"outside", "DOES>", "Should error outside definition", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "[", {
            {"basic", ": TEST5 [ 42 ] LITERAL ; TEST5 . CR", "Should enter interpret", TEST_NORMAL, 0, 1},
            {"outside", "[", "Should be allowed outside definition (no error)", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "]", {
            {"basic", "[ 42 ] . CR", "Should resume compile", TEST_NORMAL, 0, 1},
            {"nested", ": TEST6 [ ] 42 ; TEST6 . CR", "Should handle nesting", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/** @brief Executes all test suites for Forth defining words
 *  @param vm Pointer to the Forth virtual machine instance
 *  @details Runs through each test suite in defining_word_suites array,
 *           executing individual test cases and logging results_run_01_2025_12_08
 */
void run_defining_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Defining Words Tests (Module 13)...");

    for (int i = 0; defining_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &defining_word_suites[i]);
    }

    print_module_summary("Defining Words", 0, 0, 0, 0);
}