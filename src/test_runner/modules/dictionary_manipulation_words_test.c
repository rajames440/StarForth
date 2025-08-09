/*

                                 ***   StarForth   ***
  dictionary_manipulation_words_test.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/* Dictionary Manipulation Words Test Suites - Module 14 */
static WordTestSuite dict_manip_word_suites[] = {
    {
        "CREATE", {
            {"basic", "CREATE test1 42 , test1 @ . CR", "Should create and store", TEST_NORMAL, 0, 1},
            {"empty_name", "CREATE", "Should handle empty name", TEST_ERROR_CASE, 1, 1},
            {"duplicate", "CREATE test2 CREATE test2", "Should handle duplicate", TEST_ERROR_CASE, 1, 1},
            {"long_name", "CREATE abcdefghijklmnopqrstuvwxyz", "Should handle long name", TEST_EDGE_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 4
    },

    {
        "FORGET", {
            {"basic", "CREATE temp1 FORGET temp1", "Should forget word", TEST_NORMAL, 0, 1},
            {"nonexistent", "FORGET nonexistent", "Should handle missing word", TEST_ERROR_CASE, 1, 1},
            {"protected", "FORGET FORGET", "Should protect system words", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "IMMEDIATE", {
            {"basic", ": test3 42 ; IMMEDIATE test3 . CR", "Should execute immediately", TEST_NORMAL, 0, 1},
            {"already_immediate", ": test4 43 ; IMMEDIATE IMMEDIATE", "Should handle double immediate", TEST_NORMAL, 0, 1},
            {"outside_def", "IMMEDIATE", "Should error outside definition", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "FIND", {
            {"existing", "' DUP FIND . CR", "Should find system word", TEST_NORMAL, 0, 1},
            {"user_word", ": test5 44 ; ' test5 FIND . CR", "Should find user word", TEST_NORMAL, 0, 1},
            {"nonexistent", "' nonexistent FIND . CR", "Should return 0", TEST_NORMAL, 0, 1},
            {"empty", "0 FIND", "Should handle null string", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 4
    },

    {
        "DEFINITIONS", {
            {"basic", "FORTH DEFINITIONS", "Should set current to context", TEST_NORMAL, 0, 1},
            {"multiple", "FORTH DEFINITIONS DEFINITIONS", "Should be idempotent", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    {
        "SMUDGE", {
            {"during_def", ": test6 SMUDGE 45 ;", "Should hide incomplete def", TEST_NORMAL, 0, 1},
            {"outside_def", "SMUDGE", "Should error outside def", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    {
        "LATEST", {
            {"basic", "LATEST @ . CR", "Should show latest word", TEST_NORMAL, 0, 1},
            {"after_def", ": test7 46 ; LATEST @ . CR", "Should update after def", TEST_NORMAL, 0, 1},
            {"after_forget", "CREATE test8 FORGET test8 LATEST @ . CR", "Should update after forget", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        ">BODY", {
            {"basic", "CREATE test9 ' test9 >BODY . CR", "Should get param field", TEST_NORMAL, 0, 1},
            {"variable", "VARIABLE var1 ' var1 >BODY . CR", "Should get var storage", TEST_NORMAL, 0, 1},
            {"nonexistent", "' NONEXISTENT >BODY", "Should handle missing word", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "HIDDEN", {
            {"basic", ": test10 HIDDEN 47 ;", "Should hide word", TEST_NORMAL, 0, 1},
            {"outside_def", "HIDDEN", "Should error outside def", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_dictionary_manipulation_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Dictionary Manipulation Words Tests (Module 14)...");
    
    for (int i = 0; dict_manip_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &dict_manip_word_suites[i]);
    }
    
    print_module_summary("Dictionary Manipulation Words", 0, 0, 0, 0);
}
