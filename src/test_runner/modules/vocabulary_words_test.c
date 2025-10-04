/*

                                 ***   StarForth   ***
  vocabulary_words_test.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/12/25, 8:28 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/** @brief Test suites for vocabulary-related FORTH words (Module 15)
 *  @details Contains test cases for each vocabulary word, including normal operations
 *           and error cases. Each suite tests specific functionality of the vocabulary system.
 */
static WordTestSuite vocabulary_word_suites[] = {
    {
        "VOCABULARY", {
            /* Basic functionality */
            {"basic", "VOCABULARY TEST-VOC1 TEST-VOC1 DEFINITIONS", "Should create vocabulary", TEST_NORMAL, 0, 1},
            {
                "create_and_switch", "VOCABULARY MYVOC MYVOC DEFINITIONS CONTEXT @ . CR", "Should create and switch",
                TEST_NORMAL, 0, 1
            },

            /* Word isolation */
            {
                "word_isolation", "VOCABULARY ISOLATED ISOLATED DEFINITIONS : ISOWORD 99 ; FORTH DEFINITIONS",
                "Should create word in isolated vocab", TEST_NORMAL, 0, 1
            },
            {
                "cross_vocab_access", "VOCABULARY V1 V1 DEFINITIONS : V1WORD 11 ; FORTH V1WORD . CR",
                "Should access word across vocabs", TEST_NORMAL, 0, 1
            },

            /* Multiple vocabularies */
            {
                "multiple_vocabs", "VOCABULARY VA VOCABULARY VB VOCABULARY VC VA DEFINITIONS",
                "Should create multiple vocabs", TEST_NORMAL, 0, 1
            },

            /* Error cases */
            {
                "duplicate", "VOCABULARY TESTVOC VOCABULARY TESTVOC", "Should handle duplicate", TEST_ERROR_CASE, 1,
                1
            },
            {"empty_name", "VOCABULARY", "Should handle empty name", TEST_ERROR_CASE, 1, 1},

            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        "FORTH", {
            {"basic", "FORTH DEFINITIONS", "Should select FORTH vocab", TEST_NORMAL, 0, 1},
            {"persistence", "FORTH : TEST1 42 ; TEST1 . CR", "Should find word in FORTH", TEST_NORMAL, 0, 1},
            {"from_other", "VOCABULARY OTHER-VOC OTHER-VOC FORTH", "Should return to FORTH", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "DEFINITIONS", {
            {"basic", "FORTH DEFINITIONS", "Should set current vocabulary", TEST_NORMAL, 0, 1},
            {"new_vocab", "VOCABULARY TEST-VOC6 TEST-VOC6 DEFINITIONS", "Should set new current", TEST_NORMAL, 0, 1},
            {
                "word_creation", "VOCABULARY TEST-VOC7 TEST-VOC7 DEFINITIONS : TEST2 43 ;",
                "Should create in correct vocab", TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "CONTEXT", {
            {"basic", "CONTEXT @ . CR", "Should show current context", TEST_NORMAL, 0, 1},
            {"modify", "CONTEXT @ 1+ CONTEXT !", "Should allow modification (impl-defined effect)", TEST_NORMAL, 0, 1},
            {"initial", "FORTH CONTEXT @ . CR", "Should show FORTH vocab", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "CURRENT", {
            {"basic", "CURRENT @ . CR", "Should show current vocab", TEST_NORMAL, 0, 1},
            {
                "after_def", "VOCABULARY TEST-VOC8 TEST-VOC8 DEFINITIONS CURRENT @ . CR", "Should show new vocab",
                TEST_NORMAL, 0, 1
            },
            {"protect", "CURRENT @ 1+ CURRENT !", "Should allow modification (impl-defined effect)", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "ORDER", {
            {"basic", "ORDER", "Should list search order", TEST_NORMAL, 0, 1},
            {
                "multiple", "VOCABULARY TEST-VOC9 TEST-VOC9 DEFINITIONS ORDER", "Should show all vocabs", TEST_NORMAL,
                0, 1
            },
            {"after_forth", "TEST-VOC9 FORTH ORDER", "Should show FORTH only", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },
    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/** @brief Executes all vocabulary word test suites
 *  @param vm Pointer to the Forth virtual machine instance
 *  @details Runs through all vocabulary-related test suites, executing each test case
 *           and logging the results. Tests vocabulary creation, selection, and manipulation.
 */
void run_vocabulary_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Vocabulary Words Tests (Module 15)...");

    for (int i = 0; vocabulary_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &vocabulary_word_suites[i]);
    }

    print_module_summary("Vocabulary Words", 0, 0, 0, 0);
}