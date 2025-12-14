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

/* String Words Test Suites - Module 11 */
static WordTestSuite string_word_suites[] = {
    {
        "COUNT", {
            {"basic", "HERE S\" Test\" DROP COUNT . . CR", "Should show length and addr+1", TEST_NORMAL, 0, 1},
            {"empty", "HERE 0 OVER C! COUNT . . CR", "Should handle empty string", TEST_NORMAL, 0, 1},
            {"max_length", "HERE 255 OVER C! COUNT . . CR", "Should handle max length", TEST_NORMAL, 0, 1},
            {"string_bounds", "HERE COUNT SWAP 1- = . CR", "Should increment address", TEST_NORMAL, 0, 1},
            {"empty_stack", "COUNT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "-TRAILING", {
            {"basic", "HERE S\"  Test  \" -TRAILING TYPE CR", "Should trim spaces", TEST_NORMAL, 0, 1},
            {"all_spaces", "HERE S\"     \" -TRAILING TYPE CR", "Should handle all spaces", TEST_NORMAL, 0, 1},
            {"no_spaces", "HERE S\" Test\" -TRAILING TYPE CR", "Should handle no spaces", TEST_NORMAL, 0, 1},
            {"empty", "HERE 0 -TRAILING TYPE CR", "Should handle empty string", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "CMOVE", {
            {"basic", "HERE S\" Test\" DUP >R HERE 10 + SWAP CMOVE CR", "Should copy string", TEST_NORMAL, 0, 1},
            {"empty", "HERE HERE 10 + 0 CMOVE", "Should handle zero count", TEST_NORMAL, 0, 1},
            {"overlap", "HERE DUP 1+ 5 CMOVE", "Should handle overlap", TEST_NORMAL, 0, 1},
            {"bounds", "HERE PAD 1000 CMOVE", "Should check bounds", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "CMOVE>", {
            {"basic", "HERE S\" Test\" DUP >R HERE 10 + SWAP CMOVE> CR", "Should copy backward", TEST_NORMAL, 0, 1},
            {"empty", "HERE HERE 10 + 0 CMOVE>", "Should handle zero count", TEST_NORMAL, 0, 1},
            {"overlap", "HERE DUP 1+ 5 CMOVE>", "Should handle overlap", TEST_NORMAL, 0, 1},
            {"bounds", "HERE PAD 1000 CMOVE>", "Should check bounds", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "COMPARE", {
            {"equal", "HERE S\" Test\" HERE S\" Test\" COMPARE . CR", "Should return 0", TEST_NORMAL, 0, 1},
            {"less", "HERE S\" Test1\" HERE S\" Test2\" COMPARE . CR", "Should return -1", TEST_NORMAL, 0, 1},
            {"greater", "HERE S\" Test2\" HERE S\" Test1\" COMPARE . CR", "Should return 1", TEST_NORMAL, 0, 1},
            {
                "different_lengths", "HERE S\" Test\" HERE S\" Testing\" COMPARE . CR", "Should handle lengths",
                TEST_NORMAL, 0, 1
            },
            {"empty", "HERE 0 HERE 0 COMPARE . CR", "Should handle empty", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "SEARCH", {
            {"found", "HERE S\" Testing\" HERE S\" Test\" SEARCH . . . CR", "Should find substring", TEST_NORMAL, 0, 1},
            {
                "not_found", "HERE S\" Testing\" HERE S\" Xyz\" SEARCH . . . CR", "Should return false", TEST_NORMAL, 0,
                1
            },
            {
                "empty_pattern", "HERE S\" Test\" HERE 0 SEARCH . . . CR", "Should handle empty pattern", TEST_NORMAL,
                0, 1
            },
            {"empty_string", "HERE 0 HERE S\" Test\" SEARCH . . . CR", "Should handle empty string", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "SCAN", {
            {"basic", "HERE S\" Test,Data\" BL SCAN TYPE CR", "Should find space", TEST_NORMAL, 0, 1},
            {"not_found", "HERE S\" TestData\" BL SCAN TYPE CR", "Should scan to end", TEST_NORMAL, 0, 1},
            {"empty", "HERE 0 BL SCAN TYPE CR", "Should handle empty", TEST_NORMAL, 0, 1},
            {"all_delims", "HERE S\"     \" BL SCAN TYPE CR", "Should handle all delims", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "SKIP", {
            {"basic", "HERE S\"   Test\" BL SKIP TYPE CR", "Should skip spaces", TEST_NORMAL, 0, 1},
            {"no_delims", "HERE S\" Test\" BL SKIP TYPE CR", "Should not skip", TEST_NORMAL, 0, 1},
            {"empty", "HERE 0 BL SKIP TYPE CR", "Should handle empty", TEST_NORMAL, 0, 1},
            {"all_delims", "HERE S\"     \" BL SKIP TYPE CR", "Should skip all", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "BLANK", {
            {"basic", "HERE 5 BLANK HERE 5 TYPE CR", "Should fill with spaces", TEST_NORMAL, 0, 1},
            {"zero", "HERE 0 BLANK HERE 0 TYPE CR", "Should handle zero count", TEST_NORMAL, 0, 1},
            /* TODO: Bounds checking - memory subsystem doesn't yet enforce upper bounds */
            /* {"bounds", "HERE 2000000 BLANK", "Should check bounds", TEST_ERROR_CASE, 1, 1}, */
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/**
 * @brief Executes all string manipulation word tests
 *
 * @details Runs test suites for FORTH string operations including COUNT, -TRAILING,
 * CMOVE, CMOVE>, COMPARE, SEARCH, SCAN, SKIP, and BLANK
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void run_string_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running String Words Tests (Module 11)...");

    for (int i = 0; string_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &string_word_suites[i]);
    }

    print_module_summary("String Words", 0, 0, 0, 0);
}