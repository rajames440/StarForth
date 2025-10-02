/*

                                 ***   StarForth   ***
  test_runner.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include "../../../include/vm.h"
#include <stdint.h>

/**
 * @brief Structure holding test execution statistics
 */
typedef struct {
    int total_tests; /**< Total number of tests executed */
    int total_pass; /**< Number of passed tests */
    int total_fail; /**< Number of failed tests */
    int total_skip; /**< Number of skipped tests */
    int total_error; /**< Number of tests that resulted in errors */
} TestStats;

/**
 * @brief Structure representing a test module
 */
typedef struct {
    const char *module_name; /**< Name of the test module */
    void *reserved; /**< Reserved for future use */
    int reserved_int; /**< Reserved integer value */

    void (*run_module_tests)(VM *vm); /**< Function pointer to run module tests */
} TestModule;

/* Global test statistics - extern declaration */
extern TestStats global_test_stats;

/**
 * @brief Run tests for stack manipulation words
 * @param vm Pointer to the VM instance
 */
void run_stack_words_tests(VM * vm);

void run_return_stack_words_tests(VM * vm);
void run_memory_words_tests(VM * vm);
void run_arithmetic_words_tests(VM * vm);
void run_logical_words_tests(VM * vm);
void run_mixed_arithmetic_words_tests(VM * vm);
void run_double_words_tests(VM * vm);
void run_dictionary_words_tests(VM * vm);
void run_io_words_tests(VM * vm);
void run_format_words_tests(VM * vm);
void run_string_words_tests(VM * vm);
void run_dictionary_manipulation_words_tests(VM * vm);
void run_vocabulary_words_tests(VM * vm);
void run_block_words_tests(VM * vm);
void run_editor_words_tests(VM * vm);
void run_system_words_tests(VM * vm);
void run_defining_words_tests(VM * vm);
void run_control_words_tests(VM * vm);

/**
 * @brief Enable benchmark mode for performance testing
 * @param iterations Number of iterations for benchmark tests
 */
void enable_benchmark_mode(int iterations);

/**
 * @brief Run compute-intensive benchmark tests
 * @param vm Pointer to the VM instance
 */
void run_compute_benchmarks(VM * vm);

/**
 * @brief Execute stress tests
 * @param vm Pointer to the VM instance
 */
void run_stress_tests(VM * vm);

/**
 * @brief Execute integration tests
 * @param vm Pointer to the VM instance
 */
void run_integration_tests(VM * vm);

/**
 * @brief Run all available tests
 * @param vm Pointer to the VM instance
 */
void run_all_tests(VM * vm);

/**
 * @brief Run tests for a specific module
 * @param vm Pointer to the VM instance
 * @param module_name Name of the module to test
 */
void run_module_tests(VM *vm, const char *module_name);

/**
 * @brief Run tests for a specific word
 * @param vm Pointer to the VM instance
 * @param word_name Name of the word to test
 */
void run_word_tests(VM *vm, const char *word_name);

#endif /* TEST_RUNNER_H */