/*

                                 ***   StarForth   ***
  test_common.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "../../include/vm.h"
#include "../../include/log.h"

/* Test configuration */
#define MAX_TEST_OUTPUT 1024
#define MAX_TEST_INPUT 256
#define MAX_TESTS_PER_WORD 20

/* fail fast */
extern int fail_fast;

/* Test types */
typedef enum {
    TEST_NORMAL = 0,
    TEST_EDGE_CASE = 1,
    TEST_ERROR_CASE = 2
} TestType;

/* Test case structure */
typedef struct {
    const char *name;
    const char *input;
    const char *expected;
    TestType type;
    int should_error;
    int implemented;
} TestCase;

/* Test suite structure for a single word */
typedef struct {
    const char *word_name;
    TestCase tests[MAX_TESTS_PER_WORD];
    int test_count;
} WordTestSuite;

/* Core test execution functions */
TestResult run_single_test(VM *vm, const char *word_name, const TestCase *test);

void run_test_suite(VM *vm, const WordTestSuite *suite);

void print_module_summary(const char *module_name, int pass, int fail, int skip, int error);

/* Test assertion functions */
int assert_stack_depth(VM *vm, int expected_depth);

int assert_stack_top(VM *vm, int expected_value);

int assert_vm_error(VM *vm, int should_have_error);

/* VM state management for tests */
void save_vm_state(VM *vm, int *dsp, int *rsp, int *error, vm_mode_t *mode);

void restore_vm_state(VM *vm, int dsp, int rsp, int error, vm_mode_t mode);

#endif /* TEST_COMMON_H */