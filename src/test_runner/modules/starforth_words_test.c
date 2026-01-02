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
 * @brief Test suites for StarForth-specific words
 *
 * Contains test cases for the following StarForth implementation words:
 * - ENTROPY@: Fetch word execution count
 * - ENTROPY!: Store word execution count
 * - WORD-ENTROPY: Get execution count for a word by name
 * - RESET-ENTROPY: Reset all word execution counts
 * - TOP-WORDS: Display most frequently executed words
 * - (-: Comment to end of line
 * - INIT: System initialization
 */
static WordTestSuite starforth_word_suites[] = {
    {
        "ENTROPY@", {
            /* Basic functionality - ' returns xt (DictEntry*), NOT body address */
            {
                "after_execution", ": TESTWORD 1 1 + DROP ; TESTWORD ' TESTWORD ENTROPY@ . CR",
                "Should show execution count > 0", TEST_NORMAL, 0, 1
            },
            {
                "zero_initial", ": NEWWORD ; ' NEWWORD ENTROPY@ . CR",
                "Should show 0 for unused word", TEST_NORMAL, 0, 1
            },

            /* Multiple executions */
            {
                "multiple_calls", ": ENTTEST 1 DROP ; ENTTEST ENTTEST ENTTEST ' ENTTEST ENTROPY@ . CR",
                "Should increment with each call", TEST_NORMAL, 0, 1
            },

            /* Error cases */
            {"null_addr", "0 ENTROPY@", "Should handle null address", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "ENTROPY@", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},

            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "ENTROPY!", {
            /* Basic functionality - ' returns xt (DictEntry*), NOT body address */
            {
                "set_entropy", ": ENTSET ; 42 ' ENTSET ENTROPY! ' ENTSET ENTROPY@ . CR",
                "Should set entropy to 42", TEST_NORMAL, 0, 1
            },
            {
                "overwrite",
                ": ENTOV ; 10 ' ENTOV ENTROPY! 20 ' ENTOV ENTROPY! ' ENTOV ENTROPY@ . CR",
                "Should overwrite previous value", TEST_NORMAL, 0, 1
            },
            {
                "zero_entropy", ": ENTZERO ; 0 ' ENTZERO ENTROPY! ' ENTZERO ENTROPY@ . CR",
                "Should set entropy to 0", TEST_NORMAL, 0, 1
            },

            /* Error cases */
            {"null_addr", "42 0 ENTROPY!", "Should handle null address", TEST_ERROR_CASE, 1, 1},
            {"one_arg", "42 ENTROPY!", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "ENTROPY!", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},

            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    {
        "WORD-ENTROPY", {
            /* Basic functionality - WORD-ENTROPY just prints stats, no stack interaction */
            {
                "display_all", "1 DUP DUP DUP DROP DROP DROP DROP WORD-ENTROPY",
                "Should display all word entropy stats", TEST_NORMAL, 0, 1
            },
            {
                "after_execution", ": WDENT 1 2 + DROP ; WDENT WDENT WORD-ENTROPY",
                "Should show defined word entropy", TEST_NORMAL, 0, 1
            },

            /* No stack effect */
            {
                "stack_preserved", "DEPTH WORD-ENTROPY DEPTH = . CR",
                "Should not affect stack", TEST_NORMAL, 0, 1
            },

            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "RESET-ENTROPY", {
            /* Basic functionality */
            {
                "reset_defined", ": RESETTEST 1 DROP ; RESETTEST RESETTEST RESETTEST RESET-ENTROPY",
                "Should reset all entropy counters", TEST_NORMAL, 0, 1
            },

            /* Idempotent */
            {"double_reset", "RESET-ENTROPY RESET-ENTROPY", "Should handle multiple resets", TEST_NORMAL, 0, 1},

            /* Verify it runs without error */
            {
                "after_usage", "1 DUP DUP DUP DROP DROP DROP DROP RESET-ENTROPY", "Should reset after word usage",
                TEST_NORMAL, 0, 1
            },

            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "TOP-WORDS", {
            /* Basic functionality - TOP-WORDS requires ( n -- ) stack parameter */
            {
                "display_top5", ": TOPTEST 1 DROP ; TOPTEST TOPTEST TOPTEST TOPTEST TOPTEST 5 TOP-WORDS",
                "Should display top 5 hot words", TEST_NORMAL, 0, 1
            },
            {"display_top10", "10 TOP-WORDS", "Should display top 10 words", TEST_NORMAL, 0, 1},
            {"after_reset", "RESET-ENTROPY 5 TOP-WORDS", "Should handle display after reset", TEST_NORMAL, 0, 1},

            /* Different counts */
            {"single_word", "1 TOP-WORDS", "Should display top 1 word", TEST_NORMAL, 0, 1},
            {"large_count", "100 TOP-WORDS", "Should handle large count", TEST_NORMAL, 0, 1},

            /* Error cases */
            {"zero_count", "0 TOP-WORDS", "Should handle zero count", TEST_NORMAL, 0, 1},
            {"negative_count", "-5 TOP-WORDS", "Should handle negative count", TEST_NORMAL, 0, 1},
            {"empty_stack", "TOP-WORDS", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},

            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        8
    },

    {
        "(-", {
            /* Basic functionality */
            {"comment_line", "(- This is a comment\n1 2 + . CR", "Should ignore rest of line", TEST_NORMAL, 0, 1},
            {"mid_line", "1 (- comment here\n2 + . CR", "Should work mid-expression", TEST_NORMAL, 0, 1},

            /* Stack preservation */
            {"no_stack_effect", "DEPTH (- comment\nDEPTH = . CR", "Should not affect stack", TEST_NORMAL, 0, 1},

            /* Multiple comments */
            {"multiple", "1 (- first\n2 (- second\n+ . CR", "Should handle multiple comments", TEST_NORMAL, 0, 1},

            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    /* INIT intentionally not tested here - it's a startup operation that loads
     * and executes init.4th. Running it during POST would cause double-initialization. */

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/**
 * @brief Executes all StarForth words test suites
 * @param vm Pointer to the FORTH virtual machine instance
 *
 * Runs through all test cases for StarForth-specific implementation words,
 * including entropy tracking, comments, and initialization.
 */
void run_starforth_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running StarForth Words Tests (Module 17: StarForth Extensions)...");

    for (int i = 0; starforth_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &starforth_word_suites[i]);
    }

    print_module_summary("StarForth Words", 0, 0, 0, 0);
}