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
void run_system_words_tests(VM * vm);
void run_defining_words_tests(VM * vm);
void run_control_words_tests(VM * vm);
void run_starforth_words_tests(VM * vm);

/**
 * @brief Enable benchmark mode for performance testing
 * @param iterations Number of iterations for benchmark tests
 */
void enable_benchmark_mode(int iterations);

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

/**
 * @brief Run ultra-comprehensive break-me diagnostic suite
 * @param vm Pointer to the VM instance
 *
 * Executes the most exhaustive test battery ever designed for StarForth,
 * generates a detailed markdown report in docs/BREAK_ME_REPORT.md,
 * and includes a surprise easter egg at the end.
 */
void run_break_me_tests(VM * vm);

#endif /* TEST_RUNNER_H */