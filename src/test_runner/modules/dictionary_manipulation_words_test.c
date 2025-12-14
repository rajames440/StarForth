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

/* Dictionary Manipulation Words Test Suites - Module 14 */
static WordTestSuite dict_manip_word_suites[] = {
    {
        "CREATE", {
            {"basic", "CREATE test1 42 , test1 @ . CR", "Should create and store", TEST_NORMAL, 0, 1},
            {"empty_name", "CREATE", "Should handle empty name", TEST_ERROR_CASE, 1, 1},
            {
                "redefine_shadows", "CREATE T 1 , CREATE T 2 , T @ . CR", "Newest definition should win (prints 2)",
                TEST_NORMAL, 0, 1
            },
            {"duplicate", "CREATE test2 CREATE test2", "Should allow redefinition (latest shadows)", TEST_NORMAL, 0, 1},
            {"long_name", "CREATE abcdefghijklmnopqrstuvwxyz", "Should handle long name", TEST_EDGE_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "FORGET", {
            {"basic", "CREATE temp1 FORGET temp1", "Should forget word", TEST_NORMAL, 0, 1},
            {"nonexistent", "FORGET nonexistent", "Should handle missing word", TEST_ERROR_CASE, 1, 1},
            {"protected", "FORGET FORGET", "Should protect system words", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "IMMEDIATE", {
            {"basic", ": test3 42 ; IMMEDIATE test3 . CR", "Should execute immediately", TEST_NORMAL, 0, 1},
            {
                "already_immediate", ": test4 43 ; IMMEDIATE IMMEDIATE", "Should handle double immediate", TEST_NORMAL,
                0, 1
            },
            /* TODO: IMMEDIATE outside definition - requires compile-state tracking in IMMEDIATE */
            /* {"outside_def", "IMMEDIATE", "Should error outside definition", TEST_ERROR_CASE, 1, 1}, */
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "FIND", {
            {"existing", "FIND DUP . CR", "Should find system word", TEST_NORMAL, 0, 1},
            {"user_word", ": test5 44 ; FIND test5 . CR", "Should find user word", TEST_NORMAL, 0, 1},
            {"nonexistent", "FIND nonexistent . CR", "Should return 0", TEST_NORMAL, 0, 1},
            {"empty", "FIND", "Should error (no token)", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "DEFINITIONS", {
            {"basic", "FORTH DEFINITIONS", "Should set current to context", TEST_NORMAL, 0, 1},
            {"multiple", "FORTH DEFINITIONS DEFINITIONS", "Should be idempotent", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "SMUDGE", {
            {"during_def", ": test6 SMUDGE 45 ;", "Should hide incomplete def", TEST_NORMAL, 0, 1},
            {"outside_def", "SMUDGE", "Should error outside def", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "LATEST", {
            {"basic", "LATEST @ . CR", "Should show latest word", TEST_NORMAL, 0, 1},
            {"after_def", ": test7 46 ; LATEST @ . CR", "Should update after def", TEST_NORMAL, 0, 1},
            {
                "after_forget", "CREATE test8 FORGET test8 LATEST @ . CR", "Should update after forget", TEST_NORMAL, 0,
                1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        ">BODY", {
            {"basic", "CREATE test9 ' test9 >BODY . CR", "Should get param field", TEST_NORMAL, 0, 1},
            {"variable", "VARIABLE var1 ' var1 >BODY . CR", "Should get var storage", TEST_NORMAL, 0, 1},
            {"nonexistent", "' NONEXISTENT >BODY", "Should handle missing word", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "HIDDEN", {
            {"basic", ": test10 HIDDEN 47 ;", "Should hide word", TEST_NORMAL, 0, 1},
            {"outside_def", "HIDDEN", "Should error outside def", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/**
 * @brief Executes all dictionary manipulation words test suites
 * @param vm Pointer to the Forth virtual machine instance
 * @details Runs through all test cases for CREATE, FORGET, IMMEDIATE, FIND,
 *          DEFINITIONS, SMUDGE, LATEST, >BODY, and HIDDEN words
 */
void run_dictionary_manipulation_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Dictionary Manipulation Words Tests (Module 14)...");

    for (int i = 0; dict_manip_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &dict_manip_word_suites[i]);
    }

    print_module_summary("Dictionary Manipulation Words", 0, 0, 0, 0);
}