/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

/**
 * @file test_common.h
 * @brief Common test utilities and structures for StarForth test runner
 * @details This file contains test configuration, structures, and utility functions
 * used throughout the StarForth test framework.
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "../include/vm.h"
#include "../include/log.h"

/** @name Test Configuration
 * @{
 */
/** Maximum length of test output buffer */
#define MAX_TEST_OUTPUT 1024
/** Maximum length of test input buffer */
#define MAX_TEST_INPUT 256
/** Maximum number of test cases per word */
#define MAX_TESTS_PER_WORD 20
/** @} */

/* fail fast */
extern int fail_fast;

/** @brief Types of test cases */
typedef enum {
    TEST_NORMAL = 0, /**< Standard test case */
    TEST_EDGE_CASE = 1, /**< Edge case test */
    TEST_ERROR_CASE = 2 /**< Error condition test */
} TestType;

/** @brief Structure representing a single test case */
typedef struct {
    const char *name; /**< Name of the test case */
    const char *input; /**< Input Forth code */
    const char *expected; /**< Expected output */
    TestType type; /**< Type of test case */
    int should_error; /**< Whether test should produce an error */
    int implemented; /**< Whether the functionality is implemented */
} TestCase;

/** @brief Collection of test cases for a single Forth word */
typedef struct {
    const char *word_name; /**< Name of the Forth word being tested */
    TestCase tests[MAX_TESTS_PER_WORD]; /**< Array of test cases */
    int test_count; /**< Number of tests in the suite */
} WordTestSuite;

/** @name Core Test Execution Functions
 * @{
 */
/** 
 * @brief Executes a single test case
 * @param vm Pointer to the VM instance
 * @param word_name Name of the word being tested
 * @param test Test case to execute
 * @return Test execution result
 */
TestResult run_single_test(VM *vm, const char *word_name, const TestCase *test);

/**
 * @brief Runs all tests in a test suite
 * @param vm Pointer to the VM instance
 * @param suite Test suite to execute
 */
void run_test_suite(VM *vm, const WordTestSuite *suite);

/**
 * @brief Prints test results_run_01_2025_12_08 summary for a module
 * @param module_name Name of the module
 * @param pass Number of passed tests
 * @param fail Number of failed tests
 * @param skip Number of skipped tests
 * @param error Number of errors
 */
void print_module_summary(const char *module_name, int pass, int fail, int skip, int error);

/** @} */

/** @name Test Assertion Functions
 * @{
 */
/**
 * @brief Verifies the VM stack depth
 * @param vm Pointer to the VM instance
 * @param expected_depth Expected stack depth
 * @return 1 if assertion passes, 0 otherwise
 */
int assert_stack_depth(VM *vm, int expected_depth);

/**
 * @brief Verifies the value at the top of the stack
 * @param vm Pointer to the VM instance
 * @param expected_value Expected value
 * @return 1 if assertion passes, 0 otherwise
 */
int assert_stack_top(VM *vm, int expected_value);

/**
 * @brief Verifies VM error state
 * @param vm Pointer to the VM instance
 * @param should_have_error Expected error state
 * @return 1 if assertion passes, 0 otherwise
 */
int assert_vm_error(VM *vm, int should_have_error);

/** @} */

/** @name VM State Management Functions
 * @{
 */
/**
 * @brief Saves current VM state
 * @param vm Pointer to the VM instance
 * @param dsp Pointer to store data stack pointer
 * @param rsp Pointer to store return stack pointer
 * @param error Pointer to store error state
 * @param mode Pointer to store VM mode
 */
void save_vm_state(VM *vm, int *dsp, int *rsp, int *error, vm_mode_t *mode);

/**
 * @brief Restores VM state from saved values
 * @param vm Pointer to the VM instance
 * @param dsp Data stack pointer to restore
 * @param rsp Return stack pointer to restore
 * @param error Error state to restore
 * @param mode VM mode to restore
 */
void restore_vm_state(VM *vm, int dsp, int rsp, int error, vm_mode_t mode);

/**
 * @brief Saves dictionary state before test execution
 * @param vm Pointer to VM instance
 * @param latest Pointer to store latest dictionary entry
 * @param here Pointer to store HERE pointer
 */
void save_dict_state(VM * vm, DictEntry * *latest, size_t * here);

/**
 * @brief Restores dictionary state after test execution
 * @param vm Pointer to VM instance
 * @param latest Latest dictionary entry to restore
 * @param here HERE pointer to restore
 * @details Removes all words added during test by restoring dictionary pointers
 */
void restore_dict_state(VM *vm, DictEntry *latest, size_t here);

/** @} */

#endif /* TEST_COMMON_H */