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

#include "include/test.h"
#include "../../include/log.h"
#include "../../include/word_registry.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Comprehensive test suites for each word */
static WordTestSuite test_suites[] = {
    /* ===== STACK MANIPULATION WORDS ===== */
    {
        "DUP", {
            {"basic", "5 DUP . . CR", "Should print: 5 5", TEST_NORMAL, 0, 1},
            {"zero", "0 DUP . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"negative", "-42 DUP . . CR", "Should print: -42 -42", TEST_NORMAL, 0, 1},
            {"max_int", "2147483647 DUP . . CR", "Should duplicate max int", TEST_EDGE_CASE, 0, 1},
            {"min_int", "-2147483648 DUP . . CR", "Should duplicate min int", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "DUP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "DROP", {
            {"basic", "5 7 DROP . CR", "Should print: 5", TEST_NORMAL, 0, 1},
            {"zero", "42 0 DROP . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"single_item", "99 DROP DEPTH . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"empty_stack", "DROP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 4
    },
    
    {
        "SWAP", {
            {"basic", "5 7 SWAP . . CR", "Should print: 7 5", TEST_NORMAL, 0, 1},
            {"same_values", "42 42 SWAP . . CR", "Should print: 42 42", TEST_NORMAL, 0, 1},
            {"zero_nonzero", "0 99 SWAP . . CR", "Should print: 99 0", TEST_NORMAL, 0, 1},
            {"negative", "-5 10 SWAP . . CR", "Should print: 10 -5", TEST_NORMAL, 0, 1},
            {"one_item", "42 SWAP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "SWAP", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "OVER", {
            {"basic", "5 7 OVER . . . CR", "Should print: 5 7 5", TEST_NORMAL, 0, 1},
            {"zeros", "0 0 OVER . . . CR", "Should print: 0 0 0", TEST_NORMAL, 0, 1},
            {"mixed", "-1 42 OVER . . . CR", "Should print: -1 42 -1", TEST_NORMAL, 0, 1},
            {"one_item", "42 OVER", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "OVER", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 5
    },
    
    {
        "ROT", {
            {"basic", "1 2 3 ROT . . . CR", "Should print: 2 3 1", TEST_NORMAL, 0, 1},
            {"zeros", "0 0 0 ROT . . . CR", "Should print: 0 0 0", TEST_NORMAL, 0, 1},
            {"mixed", "-1 0 1 ROT . . . CR", "Should print: 0 1 -1", TEST_NORMAL, 0, 1},
            {"two_items", "1 2 ROT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 ROT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "ROT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    /* ===== ARITHMETIC WORDS ===== */
    {
        "+", {
            {"basic", "5 7 + . CR", "Should print: 12", TEST_NORMAL, 0, 1},
            {"zero_add", "42 0 + . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"negative", "-5 3 + . CR", "Should print: -2", TEST_NORMAL, 0, 1},
            {"both_negative", "-5 -3 + . CR", "Should print: -8", TEST_NORMAL, 0, 1},
            {"overflow", "2147483647 1 + . CR", "Should overflow", TEST_EDGE_CASE, 0, 1},
            {"underflow", "-2147483648 -1 + . CR", "Should underflow", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "+", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 +", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 8
    },
    
    {
        "-", {
            {"basic", "10 3 - . CR", "Should print: 7", TEST_NORMAL, 0, 1},
            {"zero_sub", "42 0 - . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"from_zero", "0 5 - . CR", "Should print: -5", TEST_NORMAL, 0, 1},
            {"negative", "5 -3 - . CR", "Should print: 8", TEST_NORMAL, 0, 1},
            {"same_values", "42 42 - . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"overflow", "-2147483648 1 - . CR", "Should underflow", TEST_EDGE_CASE, 0, 1},
            {"underflow", "2147483647 -1 - . CR", "Should overflow", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "-", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 -", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 9
    },
    
    {
        "*", {
            {"basic", "6 7 * . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"by_zero", "42 0 * . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 1 * . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"by_negative", "6 -7 * . CR", "Should print: -42", TEST_NORMAL, 0, 1},
            {"negative_negative", "-6 -7 * . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"large_numbers", "32767 2 * . CR", "Should print: 65534", TEST_EDGE_CASE, 0, 1},
            {"overflow", "65536 65536 * . CR", "May overflow", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "*", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 *", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 9
    },
    
    {
        "/", {
            {"basic", "15 3 / . CR", "Should print: 5", TEST_NORMAL, 0, 1},
            {"by_one", "42 1 / . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"negative_dividend", "-15 3 / . CR", "Should print: -5", TEST_NORMAL, 0, 1},
            {"negative_divisor", "15 -3 / . CR", "Should print: -5", TEST_NORMAL, 0, 1},
            {"both_negative", "-15 -3 / . CR", "Should print: 5", TEST_NORMAL, 0, 1},
            {"truncation", "7 3 / . CR", "Should print: 2 (truncated)", TEST_NORMAL, 0, 1},
            {"by_zero", "42 0 /", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "/", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 /", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 9
    },
    
    {
        "MOD", {
            {"basic", "17 5 MOD . CR", "Should print: 2", TEST_NORMAL, 0, 1},
            {"exact_division", "15 3 MOD . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 1 MOD . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative_dividend", "-17 5 MOD . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"negative_divisor", "17 -5 MOD . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"small_by_large", "3 7 MOD . CR", "Should print: 3", TEST_NORMAL, 0, 1},
            {"by_zero", "42 0 MOD", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "MOD", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 MOD", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 9
    },
    
    /* ===== LOGICAL & COMPARISON WORDS ===== */
    {
        "=", {
            {"equal_positive", "5 5 = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"equal_zero", "0 0 = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"equal_negative", "-42 -42 = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"not_equal", "5 3 = . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"zero_nonzero", "0 1 = . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"positive_negative", "5 -5 = . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"empty_stack", "=", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 =", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 8
    },
    
    {
        "<", {
            {"basic_true", "3 5 < . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"basic_false", "5 3 < . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"equal", "5 5 < . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"zero_positive", "0 1 < . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"negative_zero", "-1 0 < . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"negative_positive", "-5 3 < . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"both_negative", "-10 -5 < . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"large_numbers", "2147483646 2147483647 < . CR", "Should print: -1", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "<", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 <", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 10
    },
    
    /* ===== I/O WORDS ===== */
    {
        "EMIT", {
            {"printable_char", "65 EMIT CR", "Should print: A", TEST_NORMAL, 0, 1},
            {"digit", "48 EMIT CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"space", "32 EMIT CR", "Should print space", TEST_NORMAL, 0, 1},
            {"newline", "10 EMIT", "Should print newline", TEST_NORMAL, 0, 1},
            {"high_ascii", "126 EMIT CR", "Should print: ~", TEST_NORMAL, 0, 1},
            {"zero", "0 EMIT CR", "Should print null char", TEST_EDGE_CASE, 0, 1},
            {"high_value", "255 EMIT CR", "Should print high ASCII", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "EMIT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 8
    },
    
    {
        "CR", {
            {"basic", "CR", "Should print newline", TEST_NORMAL, 0, 1},
            {"after_text", "42 . CR", "Should print 42 then newline", TEST_NORMAL, 0, 1},
            {"multiple", "CR CR CR", "Should print 3 newlines", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },
    
    /* ===== MEMORY WORDS ===== */
    {
        "!", {
            {"basic", "42 HERE ! HERE @ . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"zero", "0 HERE ! HERE @ . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "-999 HERE ! HERE @ . CR", "Should print: -999", TEST_NORMAL, 0, 1},
            {"overwrite", "111 HERE ! 222 HERE ! HERE @ . CR", "Should print: 222", TEST_NORMAL, 0, 1},
            {"max_int", "2147483647 HERE ! HERE @ . CR", "Should store max int", TEST_EDGE_CASE, 0, 1},
            {"min_int", "-2147483648 HERE ! HERE @ . CR", "Should store min int", TEST_EDGE_CASE, 0, 1},
            {"empty_stack", "!", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 !", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 8
    },
    
    {
        "@", {
            {"after_store", "123 HERE ! HERE @ . CR", "Should print: 123", TEST_NORMAL, 0, 1},
            {"multiple_reads", "456 HERE ! HERE @ HERE @ = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"empty_stack", "@", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },
    
    /* ===== RETURN STACK WORDS ===== */
    {
        ">R", {
            {"basic", "42 >R R@ . R> . CR", "Should print: 42 42", TEST_NORMAL, 0, 1},
            {"zero", "0 >R R@ . R> . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"negative", "-123 >R R@ . R> . CR", "Should print: -123 -123", TEST_NORMAL, 0, 1},
            {"multiple", "1 2 >R >R R@ . R> . R@ . R> . CR", "Should print: 2 2 1 1", TEST_NORMAL, 0, 1},
            {"empty_stack", ">R", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 5
    },
    
    {
        "R>", {
            {"basic", "42 >R R> . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"lifo_order", "1 2 >R >R R> . R> . CR", "Should print: 2 1", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },
    
    {
        "R@", {
            {"basic", "42 >R R@ . R> DROP CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"non_destructive", "99 >R R@ R@ = . R> DROP CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 2
    },
    
    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/* Run all test suites */
void run_all_tests(VM *vm) {
    log_message(LOG_INFO, "Starting comprehensive FORTH-79 test suite...");
    log_message(LOG_INFO, "==============================================");
    
    int total_tests = 0;
    int total_pass = 0;
    int total_fail = 0;
    int total_skip = 0;
    int total_error = 0;
    
    for (int i = 0; test_suites[i].word_name != NULL; i++) {
        WordTestSuite *suite = &test_suites[i];
        
        log_message(LOG_INFO, "\nTesting word: %s", suite->word_name);
        log_message(LOG_INFO, "------------------------");
        
        int suite_pass = 0;
        int suite_fail = 0;
        int suite_skip = 0;
        int suite_error = 0;
        
        for (int j = 0; j < suite->test_count && suite->tests[j].name != NULL; j++) {
            TestCase *test = &suite->tests[j];
            
            if (!test->implemented) {
                log_test_result(suite->word_name, test->name, TEST_SKIP);
                suite_skip++;
                continue;
            }
            
            test_result_t result = run_single_test(vm, suite->word_name, test);
            
            switch (result) {
                case TEST_PASS: suite_pass++; break;
                case TEST_FAIL: suite_fail++; break;
                case TEST_SKIP: suite_skip++; break;
                case TEST_ERROR: suite_error++; break;
            }
        }
        
        total_tests += suite->test_count;
        total_pass += suite_pass;
        total_fail += suite_fail;
        total_skip += suite_skip;
        total_error += suite_error;
        
        log_message(LOG_INFO, "  %s: %d passed, %d failed, %d skipped, %d errors",
                   suite->word_name, suite_pass, suite_fail, suite_skip, suite_error);
    }
    
    /* Print final summary */
    log_message(LOG_INFO, "\n==============================================");
    log_message(LOG_INFO, "FINAL TEST SUMMARY:");
    log_message(LOG_INFO, "  Total tests: %d", total_tests);
    log_message(LOG_INFO, "  Passed: %d", total_pass);
    log_message(LOG_INFO, "  Failed: %d", total_fail);
    log_message(LOG_INFO, "  Skipped: %d", total_skip);
    log_message(LOG_INFO, "  Errors: %d", total_error);
    
    if (total_fail == 0 && total_error == 0) {
        log_message(LOG_INFO, "ALL IMPLEMENTED TESTS PASSED!");
    } else {
        log_message(LOG_ERROR, "%d tests FAILED or had ERRORS!", total_fail + total_error);
    }
    log_message(LOG_INFO, "==============================================");
}

/* Run a single test case */
test_result_t run_single_test(VM *vm, const char *word_name, const TestCase *test) {
    if (!test->implemented) {
        log_test_result(word_name, test->name, TEST_SKIP);
        return TEST_SKIP;
    }
    
    /* Save VM state */
    int saved_dsp = vm->dsp;
    int saved_rsp = vm->rsp;
    int saved_error = vm->error;
    vm_mode_t saved_mode = vm->mode;
    
    /* Clear error state */
    vm->error = 0;
    
    log_message(LOG_DEBUG, "  Running %s.%s: %s", word_name, test->name, test->input);
    
    /* Execute the test */
    vm_interpret(vm, test->input);
    
    /* Check results */
    test_result_t result = TEST_FAIL;
    
    if (test->should_error) {
        /* This test should have caused an error */
        if (vm->error != 0) {
            result = TEST_PASS;
            log_message(LOG_DEBUG, "    Expected error occurred");
        } else {
            result = TEST_FAIL;
            log_message(LOG_ERROR, "    Expected error but none occurred");
        }
    } else {
        /* This test should not have caused an error */
        if (vm->error != 0) {
            result = TEST_FAIL;
            log_message(LOG_ERROR, "    Unexpected VM error: %d", vm->error);
        } else {
            result = TEST_PASS;
            log_message(LOG_DEBUG, "    Test passed");
        }
    }
    
    /* Log the result */
    log_test_result(word_name, test->name, result);
    if (result == TEST_PASS) {
        log_message(LOG_DEBUG, "    Expected: %s", test->expected);
    }
    
    /* Restore VM state */
    vm->dsp = saved_dsp;
    vm->rsp = saved_rsp;
    vm->error = saved_error;
    vm->mode = saved_mode;
    
    return result;
}

/* Test specific word by name */
void run_word_tests(VM *vm, const char *word_name) {
    for (int i = 0; test_suites[i].word_name != NULL; i++) {
        if (strcmp(test_suites[i].word_name, word_name) == 0) {
            run_test_suite(vm, &test_suites[i]);
            return;
        }
    }
    log_message(LOG_WARN, "No test suite found for word: %s", word_name);
}

/* Run a specific test suite */
void run_test_suite(VM *vm, const WordTestSuite *suite) {
    log_message(LOG_INFO, "Running test suite for: %s", suite->word_name);
    
    for (int i = 0; i < suite->test_count && suite->tests[i].name != NULL; i++) {
        run_single_test(vm, suite->word_name, &suite->tests[i]);
    }
}

/* Log test result */
void log_test_result(const char *word_name, const char *test_name, test_result_t result) {
    const char *result_str;
    log_level_t level;
    
    switch (result) {
        case TEST_PASS:
            result_str = "PASS";
            level = LOG_INFO;
            break;
        case TEST_FAIL:
            result_str = "FAIL";
            level = LOG_ERROR;
            break;
        case TEST_SKIP:
            result_str = "SKIP";
            level = LOG_WARN;
            break;
        case TEST_ERROR:
            result_str = "ERROR";
            level = LOG_ERROR;
            break;
        default:
            result_str = "UNKNOWN";
            level = LOG_ERROR;
            break;
    }
    
    log_message(level, "  [%s] %s.%s", result_str, word_name, test_name);
}

/* Assert functions for more detailed testing */
int assert_stack_depth(VM *vm, int expected_depth) {
    if (vm->dsp == expected_depth) {
        return 1;
    }
    log_message(LOG_ERROR, "Stack depth mismatch: expected %d, got %d", expected_depth, vm->dsp);
    return 0;
}

int assert_stack_top(VM *vm, int expected_value) {
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "Stack underflow when checking top value");
        return 0;
    }
    if (vm->dstack[vm->dsp - 1] == expected_value) {
        return 1;
    }
    log_message(LOG_ERROR, "Stack top mismatch: expected %d, got %d", 
               expected_value, vm->dstack[vm->dsp - 1]);
    return 0;
}

int assert_vm_error(VM *vm, int should_have_error) {
    int has_error = (vm->error != 0);
    if (has_error == should_have_error) {
        return 1;
    }
    if (should_have_error) {
        log_message(LOG_ERROR, "Expected VM error but none occurred");
    } else {
        log_message(LOG_ERROR, "Unexpected VM error: %d", vm->error);
    }
    return 0;
}
