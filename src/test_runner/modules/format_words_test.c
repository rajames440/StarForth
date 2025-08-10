/*

                                 ***   StarForth   ***
  format_words_test.c - FORTH-79 Standard and ANSI C99 ONLY
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

/* Format Words Test Suites - Module 10 */
static WordTestSuite format_word_suites[] = {
    {
        "BASE", {
            {"decimal", "DECIMAL 42 . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"hex", "HEX FF . CR", "Should print: FF", TEST_NORMAL, 0, 1},
            {"octal", "OCTAL 52 . CR", "Should print: 52", TEST_NORMAL, 0, 1},
            {"base_store", "16 BASE ! 255 . CR", "Should print: FF", TEST_NORMAL, 0, 1},
            {"base_fetch", "BASE @ . CR", "Should print current base", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 5
    },

    {
        "DECIMAL", {
            {"from_hex", "HEX FF DECIMAL . CR", "Should print: 255", TEST_NORMAL, 0, 1},
            {"from_octal", "OCTAL 52 DECIMAL . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"state_persist", "DECIMAL 42 . CR 42 . CR HEX 42 . CR", "Should maintain base", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "HEX", {
            {"from_decimal", "DECIMAL 255 HEX . CR", "Should print: FF", TEST_NORMAL, 0, 1},
            {"from_octal", "OCTAL 52 HEX . CR", "Should print: 2A", TEST_NORMAL, 0, 1},
            {"state_persist", "HEX FF . CR DECIMAL 255 . CR", "Should maintain base", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "OCTAL", {
            {"from_decimal", "DECIMAL 42 OCTAL . CR", "Should print: 52", TEST_NORMAL, 0, 1},
            {"from_hex", "HEX 2A OCTAL . CR", "Should print: 52", TEST_NORMAL, 0, 1},
            {"state_persist", "DECIMAL 42 . CR OCTAL 52 . CR", "Should maintain base", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "<#", {
            {"basic", "DECIMAL 42 S>D <# #S #> TYPE CR", "Should format number", TEST_NORMAL, 0, 1},
            {"empty", "0 <# #> TYPE CR", "Should handle zero", TEST_NORMAL, 0, 1},
            {"negative", "-42 <# #S #> TYPE CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "#", {
            {"single_digit", "15 <# # #> TYPE CR", "Should print rightmost digit", TEST_NORMAL, 0, 1},
            {"multiple", "15 <# # # #> TYPE CR", "Should print digits right to left", TEST_NORMAL, 0, 1},
            {"zero_pad", "5 <# # 0 # #> TYPE CR", "Should handle zero padding", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "#S", {
            {"basic", "42 <# #S #> TYPE CR", "Should convert all digits", TEST_NORMAL, 0, 1},
            {"zero", "0 <# #S #> TYPE CR", "Should handle zero", TEST_NORMAL, 0, 1},
            {"large", "1234567890 <# #S #> TYPE CR", "Should handle large numbers", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "SIGN", {
            {"negative", "-42 ABS <# #S SIGN #> TYPE CR", "Should add minus", TEST_NORMAL, 0, 1},
            {"positive", "42 <# #S SIGN #> TYPE CR", "Should not add sign", TEST_NORMAL, 0, 1},
            {"zero", "0 <# #S SIGN #> TYPE CR", "Should handle zero", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "#>", {
            {"normal", "42 <# #S #> TYPE CR", "Should terminate formatting", TEST_NORMAL, 0, 1},
            {"empty", "0 <# #> TYPE CR", "Should handle empty format", TEST_NORMAL, 0, 1},
            {"stack_effect", "42 <# #S #> SWAP . . CR", "Should leave addr u", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },

    {
        "HOLD", {
            {"basic", "42 <# 46 HOLD #S #> TYPE CR", "Should insert dot", TEST_NORMAL, 0, 1},
            {"overflow", "<# 257 HOLD #>", "Should handle overflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_format_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Format Words Tests (Module 10)...");
    
    for (int i = 0; format_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &format_word_suites[i]);
    }
    
    print_module_summary("Format Words", 0, 0, 0, 0);
}
