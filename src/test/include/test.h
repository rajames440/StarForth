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

#ifndef TEST_H
#define TEST_H

#include "../../include/vm.h"
#include "../../include/log.h"  /* This already defines TestResult */

/* Test configuration */
#define MAX_TEST_OUTPUT 1024
#define MAX_TEST_INPUT 256

/* Test types - only define TestType, not TestResult */
typedef enum {
    TEST_NORMAL = 0,
    TEST_EDGE_CASE = 1,
    TEST_ERROR_CASE = 2
} TestType;

/* Note: TestResult is already defined in log.h as:
 * typedef enum {
 *     TEST_PASS = 0,
 *     TEST_FAIL,
 *     TEST_SKIP
 * } TestResult;
 */

/* Test case structure - matches your test.c structure */
typedef struct {
    const char *name;
    const char *input;
    const char *expected;
    TestType type;
    int should_error;
    int implemented;
} TestCase;

/* Test suite structure - matches your test.c array structure */
typedef struct {
    const char *word_name;
    TestCase tests[20];  /* Max tests per word */
    int test_count;
} WordTestSuite;

/* Test execution functions */
void run_word_tests(VM *vm);
void run_all_tests(VM *vm);
void run_test_suite(VM *vm, const WordTestSuite *suite);
TestResult run_single_test(VM *vm, const char *word_name, const TestCase *test);

/* Individual word test functions */
void test_word(VM *vm, const char *word_name);

/* Test utility functions */
/* log_test_result is declared in log.h */
int assert_stack_depth(VM *vm, int expected_depth);
int assert_stack_top(VM *vm, int expected_value);
int assert_vm_error(VM *vm, int should_have_error);

#endif /* TEST_H */