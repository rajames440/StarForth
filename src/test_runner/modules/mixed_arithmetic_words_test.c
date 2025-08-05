/*
 * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
 *
 * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 * To the extent possible under law, the author(s) have dedicated all copyright and related
 * and neighboring rights to this software to the public domain worldwide.
 * This software is distributed without any warranty.
 *
 * See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/* Mixed Arithmetic Words Test Suites - Module 7 */
static WordTestSuite mixed_arithmetic_word_suites[] = {
    {
        "*/", {
            {"basic", "6 7 4 */ . CR", "Should print: 10 (6*7/4)", TEST_NORMAL, 0, 1},
            {"exact_division", "12 5 3 */ . CR", "Should print: 20 (12*5/3)", TEST_NORMAL, 0, 1},
            {"zero_multiply", "0 999 123 */ . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 5 1 */ . CR", "Should print: 210 (42*5/1)", TEST_NORMAL, 0, 1},
            {"truncation", "7 3 2 */ . CR", "Should print: 10 (21/2 truncated)", TEST_NORMAL, 0, 1},
            {"negative_multiply", "-6 7 4 */ . CR", "Should print: -10", TEST_NORMAL, 0, 1},
            {"negative_divisor", "6 7 -4 */ . CR", "Should print: -10", TEST_NORMAL, 0, 1},
            {"div_by_zero", "6 7 0 */", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "*/", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"two_items", "1 2 */", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 10
    },
    
    {
        "*/MOD", {
            {"basic", "17 3 5 */MOD . . CR", "Should print: 10 1 (51/5 = 10 rem 1)", TEST_NORMAL, 0, 1},
            {"exact_division", "15 4 3 */MOD . . CR", "Should print: 20 0 (60/3)", TEST_NORMAL, 0, 1},
            {"zero_multiply", "0 999 123 */MOD . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 5 1 */MOD . . CR", "Should print: 210 0", TEST_NORMAL, 0, 1},
            {"remainder", "7 8 5 */MOD . . CR", "Should print: 11 1 (56/5)", TEST_NORMAL, 0, 1},
            {"negative", "-7 3 4 */MOD . . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"div_by_zero", "6 7 0 */MOD", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "*/MOD", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 8
    },
    
    {
        "M+", {
            {"basic", "100 200 M+ . . CR", "Should print: 300 0 (single->double)", TEST_NORMAL, 0, 1},
            {"overflow", "2147483647 1 M+ . . CR", "Should handle overflow", TEST_EDGE_CASE, 0, 1},
            {"negative", "-100 50 M+ . . CR", "Should print: -50 -1", TEST_NORMAL, 0, 1},
            {"zero_add", "42 0 M+ . . CR", "Should print: 42 0", TEST_NORMAL, 0, 1},
            {"both_negative", "-100 -200 M+ . . CR", "Should print: -300 -1", TEST_NORMAL, 0, 1},
            {"empty_stack", "M+", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 M+", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 7
    },
    
    {
        "M*", {
            {"basic", "123 456 M* . . CR", "Should print: 56088 0 (as double)", TEST_NORMAL, 0, 1},
            {"by_zero", "999 0 M* . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 1 M* . . CR", "Should print: 42 0", TEST_NORMAL, 0, 1},
            {"negative", "-123 456 M* . . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"large_multiply", "65536 65536 M* . . CR", "Should handle large result", TEST_EDGE_CASE, 0, 1},
            {"both_negative", "-100 -200 M* . . CR", "Should print: 20000 0", TEST_NORMAL, 0, 1},
            {"empty_stack", "M*", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 M*", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 8
    },
    
    {
        "M/MOD", {
            {"basic", "1000 0 100 M/MOD . . . CR", "Should print: 10 0 0 (1000/100)", TEST_NORMAL, 0, 1},
            {"with_remainder", "1234 0 100 M/MOD . . . CR", "Should handle remainder", TEST_NORMAL, 0, 1},
            {"double_dividend", "500 1 10 M/MOD . . . CR", "Should handle double dividend", TEST_NORMAL, 0, 1},
            {"by_one", "42 0 1 M/MOD . . . CR", "Should print: 42 0 0", TEST_NORMAL, 0, 1},
            {"negative_dividend", "1000 -1 100 M/MOD . . . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"div_by_zero", "100 0 0 M/MOD", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "M/MOD", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 7
    },
    
    {
        "SM/REM", {
            {"basic", "1000 0 100 SM/REM . . . CR", "Should print: 10 0 0", TEST_NORMAL, 0, 1},
            {"symmetric", "100 0 7 SM/REM . . . CR", "Should use symmetric division", TEST_NORMAL, 0, 1},
            {"negative_dividend", "100 -1 7 SM/REM . . . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"negative_divisor", "100 0 -7 SM/REM . . . CR", "Should handle negative divisor", TEST_NORMAL, 0, 1},
            {"div_by_zero", "100 0 0 SM/REM", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 5
    },
    
    {
        "FM/MOD", {
            {"basic", "1000 0 100 FM/MOD . . . CR", "Should print: 10 0 0", TEST_NORMAL, 0, 1},
            {"floored", "100 0 7 FM/MOD . . . CR", "Should use floored division", TEST_NORMAL, 0, 1},
            {"negative_dividend", "100 -1 7 FM/MOD . . . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"negative_divisor", "100 0 -7 FM/MOD . . . CR", "Should handle negative divisor", TEST_NORMAL, 0, 1},
            {"div_by_zero", "100 0 0 FM/MOD", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 5
    },
    
    {
        "UM*", {
            {"basic", "1000 2000 UM* . . CR", "Should print: 2000000 0 (unsigned)", TEST_NORMAL, 0, 1},
            {"by_zero", "999 0 UM* . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 1 UM* . . CR", "Should print: 42 0", TEST_NORMAL, 0, 1},
            {"large_multiply", "65535 65535 UM* . . CR", "Should handle large unsigned", TEST_EDGE_CASE, 0, 1},
            {"max_values", "4294967295 2 UM* . . CR", "Should handle max unsigned", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "UM*", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "UM/MOD", {
            {"basic", "2000000 0 1000 UM/MOD . . . CR", "Should print: 2000 0 0", TEST_NORMAL, 0, 1},
            {"with_remainder", "2000001 0 1000 UM/MOD . . . CR", "Should print: 2000 1 0", TEST_NORMAL, 0, 1},
            {"double_dividend", "1000 1 100 UM/MOD . . . CR", "Should handle double dividend", TEST_NORMAL, 0, 1},
            {"by_one", "42 0 1 UM/MOD . . . CR", "Should print: 42 0 0", TEST_NORMAL, 0, 1},
            {"large_values", "4294967295 0 65536 UM/MOD . . . CR", "Should handle large unsigned", TEST_EDGE_CASE, 0, 1},
            {"div_by_zero", "100 0 0 UM/MOD", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_mixed_arithmetic_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Mixed Arithmetic Words Tests (Module 7)...");
    
    for (int i = 0; mixed_arithmetic_word_suites[i].word_name != NULL; i++) {
        run_test_suite(vm, &mixed_arithmetic_word_suites[i]);
    }
    
    print_module_summary("Mixed Arithmetic Words", 0, 0, 0, 0);
}
