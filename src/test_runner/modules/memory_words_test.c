/*
                                  ***   StarForth   ***

  memory_words_test.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.549-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/test_runner/modules/memory_words_test.c
 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/* Memory Words Test Suites - Module 3 */
static WordTestSuite memory_word_suites[] = {
    {
        "!", {
            {"basic", "42 HERE ! HERE @ . CR", "Should Should be misaligned", TEST_ERROR_CASE, 1, 0},
            {"zero", "0 HERE ! HERE @ . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "-999 HERE ! HERE @ . CR", "Should print: -999", TEST_NORMAL, 0, 1},
            {"overwrite", "111 HERE ! 222 HERE ! HERE @ . CR", "Should print: 222", TEST_NORMAL, 0, 1},
            {"max_int", "2147483647 HERE ! HERE @ . CR", "Should store max int", TEST_EDGE_CASE, 0, 1},
            {"min_int", "-2147483648 HERE ! HERE @ . CR", "Should store min int", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "!", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 !", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        8
    },

    {
        "@", {
            {"after_store", "123 HERE ! HERE @ . CR", "Should print: 123", TEST_NORMAL, 0, 1},
            {"multiple_reads", "456 HERE ! HERE @ HERE @ = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"empty_stack", "@", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "C!", {
            {"basic", "65 HERE C! HERE C@ . CR", "Should print: 65", TEST_NORMAL, 0, 1},
            {"zero", "0 HERE C! HERE C@ . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"high_byte", "255 HERE C! HERE C@ . CR", "Should print: 255", TEST_NORMAL, 0, 1},
            {"truncation", "256 HERE C! HERE C@ . CR", "Should print: 0 (truncated)", TEST_EDGE_CASE, 0, 0}, /* Stub */
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "C@", {
            {"after_cstore", "97 HERE C! HERE C@ . CR", "Should print: 97", TEST_NORMAL, 0, 1},
            {"zero_byte", "0 HERE C! HERE C@ . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        ",", {
            {"basic", "42 , HERE 8 - @ . CR", "Should compile 42", TEST_NORMAL, 0, 1},
            {"negative", "-999 , HERE 8 - @ . CR", "Should compile -999", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "C,", {
            {"basic", "65 C, HERE 1 - C@ . CR", "Should compile byte 65", TEST_NORMAL, 0, 1},
            {"zero", "0 C, HERE 1 - C@ . CR", "Should compile byte 0", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "2,", {
            {
                "roundtrip", "12345 67890 2, HERE 16 - 2@ . . CR", "Should store and retrieve 2-cell double",
                TEST_NORMAL, 0, 0
            },
            {
                "save-and-verify", "12345 67890 2, HERE 16 - dup 2@ swap 67890 = swap 12345 = and . CR",
                "Should verify values via comparison", TEST_NORMAL, 0, 0
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "HERE", {
            {"basic", "HERE HERE = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"after_comma", "HERE 42 , HERE SWAP - . CR", "Should print: 4", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "ALLOT", {
            {"basic", "HERE 10 ALLOT HERE SWAP - . CR", "Should print: 10", TEST_NORMAL, 0, 1},
            {"zero", "HERE 0 ALLOT HERE SWAP - . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "HERE -4 ALLOT HERE SWAP - . CR", "Should print: -4", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "PAD", {
            {"basic", "PAD PAD = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"different_from_here", "PAD HERE = . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/**
 * @brief Executes the test suite for memory manipulation words
 * @details Runs through a series of test cases for Forth memory operations including
 *          store, fetch, allot, and other memory-related words
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void run_memory_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Memory Words Tests (Module 3)...");

    for (int i = 0; memory_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &memory_word_suites[i]);
    }

    print_module_summary("Memory Words", 0, 0, 0, 0);
}