
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

/* Double Number Words Test Suites - Module 6 */
static WordTestSuite double_word_suites[] = {
    {
        "2!", {
            {"basic", "12345 67890 HERE 2! HERE 2@ . . CR", "Should store double number", TEST_NORMAL, 0, 1},
            {"zero", "0 0 HERE 2! HERE 2@ . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"negative", "-1000 -2000 HERE 2! HERE 2@ . . CR", "Should store negative double", TEST_NORMAL, 0, 1},
            {"overwrite", "111 222 HERE 2! 333 444 HERE 2! HERE 2@ . . CR", "Should overwrite", TEST_NORMAL, 0, 1},
            {"empty_stack", "2!", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"three_items", "1 2 3 2!", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "2@", {
            {"after_store", "12345 67890 HERE 2! HERE 2@ . . CR", "Should retrieve double", TEST_NORMAL, 0, 1},
            {"zero", "0 0 HERE 2! HERE 2@ . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"negative", "-1000 -2000 HERE 2! HERE 2@ . . CR", "Should retrieve negative", TEST_NORMAL, 0, 1},
            {"multiple_reads", "100 200 HERE 2! HERE 2@ HERE 2@ D= . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"empty_stack", "2@", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 5
    },
    
    {
        "2DROP", {
            {"basic", "1 2 3 4 2DROP . . CR", "Should print: 1 2", TEST_NORMAL, 0, 1},
            {"zeros", "0 0 42 99 2DROP . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"negative", "-1 -2 100 200 2DROP . . CR", "Should print: -1 -2", TEST_NORMAL, 0, 1},
            {"exact_four", "10 20 30 40 2DROP DEPTH . CR", "Should print: 2", TEST_NORMAL, 0, 1},
            {"empty_stack", "2DROP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 2DROP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "2DUP", {
            {"basic", "100 200 2DUP . . . . CR", "Should print: 100 200 100 200", TEST_NORMAL, 0, 1},
            {"zeros", "0 0 2DUP . . . . CR", "Should print: 0 0 0 0", TEST_NORMAL, 0, 1},
            {"negative", "-100 -200 2DUP . . . . CR", "Should duplicate negatives", TEST_NORMAL, 0, 1},
            {"depth_check", "10 20 2DUP DEPTH . CR", "Should print: 4", TEST_NORMAL, 0, 1},
            {"empty_stack", "2DUP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 2DUP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "2OVER", {
            {"basic", "10 20 30 40 2OVER . . . . . . CR", "Should print: 10 20 30 40 10 20", TEST_NORMAL, 0, 1},
            {"zeros", "0 0 99 88 2OVER . . . . . . CR", "Should print: 0 0 99 88 0 0", TEST_NORMAL, 0, 1},
            {"mixed", "-1 2 -3 4 2OVER . . . . . . CR", "Should handle mixed signs", TEST_NORMAL, 0, 1},
            {"depth_check", "1 2 3 4 2OVER DEPTH . CR", "Should print: 6", TEST_NORMAL, 0, 1},
            {"three_items", "1 2 3 2OVER", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "2OVER", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "2SWAP", {
            {"basic", "10 20 30 40 2SWAP . . . . CR", "Should print: 30 40 10 20", TEST_NORMAL, 0, 1},
            {"zeros", "0 0 99 88 2SWAP . . . . CR", "Should print: 99 88 0 0", TEST_NORMAL, 0, 1},
            {"same_pairs", "42 42 42 42 2SWAP . . . . CR", "Should print: 42 42 42 42", TEST_NORMAL, 0, 1},
            {"negative", "-10 -20 30 40 2SWAP . . . . CR", "Should print: 30 40 -10 -20", TEST_NORMAL, 0, 1},
            {"depth_check", "1 2 3 4 2SWAP DEPTH . CR", "Should print: 4", TEST_NORMAL, 0, 1},
            {"three_items", "1 2 3 2SWAP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "2SWAP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 7
    },
    
    {
        "2ROT", {
            {"basic", "10 20 30 40 50 60 2ROT . . . . . . CR", "Should print: 30 40 50 60 10 20", TEST_NORMAL, 0, 1},
            {"zeros", "0 0 1 1 2 2 2ROT . . . . . . CR", "Should rotate with zeros", TEST_NORMAL, 0, 1},
            {"negative", "-1 -2 3 4 -5 -6 2ROT . . . . . . CR", "Should handle negatives", TEST_NORMAL, 0, 1},
            {"depth_check", "1 2 3 4 5 6 2ROT DEPTH . CR", "Should print: 6", TEST_NORMAL, 0, 1},
            {"five_items", "1 2 3 4 5 2ROT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "2ROT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "D+", {
            {"basic", "100 200 300 400 D+ . . CR", "Should print: 400 600", TEST_NORMAL, 0, 1},
            {"zero_add", "42 99 0 0 D+ . . CR", "Should print: 42 99", TEST_NORMAL, 0, 1},
            {"negative", "-100 -200 50 75 D+ . . CR", "Should handle negatives", TEST_NORMAL, 0, 1},
            {"carry", "2147483647 0 1 0 D+ . . CR", "Should handle carry", TEST_EDGE_CASE, 0, 1},
            {"both_negative", "-100 -200 -300 -400 D+ . . CR", "Should add negatives", TEST_NORMAL, 0, 1},
            {"three_items", "1 2 3 D+", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "D+", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 7
    },
    
    {
        "D-", {
            {"basic", "500 600 100 200 D- . . CR", "Should print: 400 400", TEST_NORMAL, 0, 1},
            {"zero_sub", "42 99 0 0 D- . . CR", "Should print: 42 99", TEST_NORMAL, 0, 1},
            {"from_zero", "0 0 100 200 D- . . CR", "Should print: -100 -200", TEST_NORMAL, 0, 1},
            {"negative", "100 200 -50 -75 D- . . CR", "Should handle negatives", TEST_NORMAL, 0, 1},
            {"borrow", "0 0 1 0 D- . . CR", "Should handle borrow", TEST_EDGE_CASE, 0, 1},
            {"same_values", "100 200 100 200 D- . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"three_items", "1 2 3 D-", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "D-", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 8
    },
    
    {
        "DNEGATE", {
            {"positive", "100 200 DNEGATE . . CR", "Should negate positive double", TEST_NORMAL, 0, 1},
            {"negative", "-100 -200 DNEGATE . . CR", "Should negate negative double", TEST_NORMAL, 0, 1},
            {"zero", "0 0 DNEGATE . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"max_positive", "2147483647 0 DNEGATE . . CR", "Should negate max positive", TEST_EDGE_CASE, 0, 1},
            {"one_item", "42 DNEGATE", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "DNEGATE", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "DABS", {
            {"positive", "100 200 DABS . . CR", "Should print: 100 200", TEST_NORMAL, 0, 1},
            {"negative", "-100 -200 DABS . . CR", "Should print: 100 200", TEST_NORMAL, 0, 1},
            {"zero", "0 0 DABS . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"mixed_sign", "100 -1 DABS . . CR", "Should handle mixed sign", TEST_NORMAL, 0, 1},
            {"min_double", "-2147483648 -1 DABS . . CR", "Should handle min double", TEST_EDGE_CASE, 0, 1},
            {"one_item", "42 DABS", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "DABS", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 7
    },
    
    {
        "DMIN", {
            {"basic", "100 200 300 400 DMIN . . CR", "Should print: 100 200", TEST_NORMAL, 0, 1},
            {"equal", "42 99 42 99 DMIN . . CR", "Should print: 42 99", TEST_NORMAL, 0, 1},
            {"negative", "-100 -200 -50 -75 DMIN . . CR", "Should find min negative", TEST_NORMAL, 0, 1},
            {"mixed", "-100 -1 100 0 DMIN . . CR", "Should handle mixed signs", TEST_NORMAL, 0, 1},
            {"three_items", "1 2 3 DMIN", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "DMIN", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "DMAX", {
            {"basic", "100 200 300 400 DMAX . . CR", "Should print: 300 400", TEST_NORMAL, 0, 1},
            {"equal", "42 99 42 99 DMAX . . CR", "Should print: 42 99", TEST_NORMAL, 0, 1},
            {"negative", "-300 -400 -100 -200 DMAX . . CR", "Should find max negative", TEST_NORMAL, 0, 1},
            {"mixed", "-100 -1 100 0 DMAX . . CR", "Should handle mixed signs", TEST_NORMAL, 0, 1},
            {"three_items", "1 2 3 DMAX", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "DMAX", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "D=", {
            {"equal", "100 200 100 200 D= . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"not_equal_low", "100 200 101 200 D= . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"not_equal_high", "100 200 100 201 D= . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"zero_equal", "0 0 0 0 D= . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"negative_equal", "-100 -200 -100 -200 D= . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"three_items", "1 2 3 D=", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "D=", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 7
    },
    
    {
        "D<", {
            {"less_than", "100 200 300 400 D< . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"greater_than", "300 400 100 200 D< . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"equal", "100 200 100 200 D< . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative_positive", "-100 -1 100 0 D< . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"negative_negative", "-300 -1 -100 -1 D< . CR", "Should compare negatives", TEST_NORMAL, 0, 1},
            {"high_word_diff", "100 200 100 300 D< . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"three_items", "1 2 3 D<", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "D<", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 8
    },
    
    {
        "D0=", {
            {"zero", "0 0 D0= . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"positive", "100 200 D0= . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "-100 -200 D0= . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"zero_high", "0 100 D0= . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"zero_low", "100 0 D0= . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"one_item", "42 D0=", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "D0=", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 7
    },
    
    {
        "D0<", {
            {"negative", "-100 -1 D0< . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"zero", "0 0 D0< . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"positive", "100 0 D0< . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative_high_only", "100 -1 D0< . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"large_negative", "-1 -1 D0< . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"one_item", "42 D0<", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "D0<", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 7
    },
    
    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_double_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Double Number Words Tests (Module 6)...");
    
    for (int i = 0; double_word_suites[i].word_name != NULL; i++) {
        run_test_suite(vm, &double_word_suites[i]);
    }
    
    print_module_summary("Double Number Words", 0, 0, 0, 0);
}
