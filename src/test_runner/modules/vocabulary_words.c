/*
 * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
 * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/* Vocabulary Words Test Suites - Module 15 */
static WordTestSuite vocabulary_word_suites[] = {
    {
        "VOCABULARY", {
            {"basic", "VOCABULARY TEST-VOC TEST-VOC DEFINITIONS", "Should create vocabulary", TEST_NORMAL, 0, 1},
            {"duplicate", "VOCABULARY TEST-VOC2 VOCABULARY TEST-VOC2", "Should handle duplicate", TEST_ERROR_CASE, 1, 1},
            {"empty_name", "VOCABULARY", "Should handle empty name", TEST_ERROR_CASE, 1, 1},
            {"memory_full", "VOCABULARY TEST-VOC3 VOCABULARY TEST-VOC4 VOCABULARY TEST-VOC5", "Should handle memory full", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 4
    },

    {
        "FORTH", {
            {"basic", "FORTH DEFINITIONS", "Should select FORTH vocab", TEST_NORMAL, 0, 1},
            {"persistence", "FORTH : TEST1 42 ; TEST1 . CR", "Should find word in FORTH", TEST_NORMAL, 0, 1},
            {"from_other", "VOCABULARY OTHER-VOC OTHER-VOC FORTH", "Should return to FORTH", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "DEFINITIONS", {
            {"basic", "FORTH DEFINITIONS", "Should set current vocabulary", TEST_NORMAL, 0, 1},
            {"new_vocab", "VOCABULARY TEST-VOC6 TEST-VOC6 DEFINITIONS", "Should set new current", TEST_NORMAL, 0, 1},
            {"word_creation", "VOCABULARY TEST-VOC7 TEST-VOC7 DEFINITIONS : TEST2 43 ;", "Should create in correct vocab", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "CONTEXT", {
            {"basic", "CONTEXT @ . CR", "Should show current context", TEST_NORMAL, 0, 1},
            {"modify", "CONTEXT @ 1+ CONTEXT !", "Should handle modification", TEST_ERROR_CASE, 1, 1},
            {"initial", "FORTH CONTEXT @ . CR", "Should show FORTH vocab", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "CURRENT", {
            {"basic", "CURRENT @ . CR", "Should show current vocab", TEST_NORMAL, 0, 1},
            {"after_def", "VOCABULARY TEST-VOC8 TEST-VOC8 DEFINITIONS CURRENT @ . CR", "Should show new vocab", TEST_NORMAL, 0, 1},
            {"protect", "CURRENT @ 1+ CURRENT !", "Should protect from modification", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "ORDER", {
            {"basic", "ORDER", "Should list search order", TEST_NORMAL, 0, 1},
            {"multiple", "VOCABULARY TEST-VOC9 TEST-VOC9 DEFINITIONS ORDER", "Should show all vocabs", TEST_NORMAL, 0, 1},
            {"after_forth", "TEST-VOC9 FORTH ORDER", "Should show FORTH only", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "ONLY", {
            {"basic", "ONLY", "Should reset search order", TEST_NORMAL, 0, 1},
            {"after_vocab", "VOCABULARY TEST-VOC10 TEST-VOC10 ONLY", "Should clear other vocabs", TEST_NORMAL, 0, 1},
            {"definitions", "ONLY DEFINITIONS", "Should affect current", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "ALSO", {
            {"basic", "VOCABULARY TEST-VOC11 ALSO TEST-VOC11", "Should add to search order", TEST_NORMAL, 0, 1},
            {"multiple", "ALSO FORTH ALSO TEST-VOC11", "Should stack vocabularies", TEST_NORMAL, 0, 1},
            {"overflow", "ALSO ALSO ALSO ALSO ALSO ALSO ALSO ALSO", "Should handle overflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "PREVIOUS", {
            {"basic", "ALSO FORTH PREVIOUS", "Should remove from search", TEST_NORMAL, 0, 1},
            {"empty", "ONLY PREVIOUS", "Should handle empty search order", TEST_ERROR_CASE, 1, 1},
            {"multiple", "ALSO FORTH ALSO TEST-VOC11 PREVIOUS PREVIOUS", "Should remove multiple", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_vocabulary_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Vocabulary Words Tests (Module 15)...");
    
    for (int i = 0; vocabulary_word_suites[i].word_name != NULL; i++) {
        run_test_suite(vm, &vocabulary_word_suites[i]);
    }
    
    print_module_summary("Vocabulary Words", 0, 0, 0, 0);
}
