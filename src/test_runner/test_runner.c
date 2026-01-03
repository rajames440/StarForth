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

#include "include/test_runner.h"
#include "../../include/log.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** @brief Global statistics for test execution */
TestStats global_test_stats = {0, 0, 0, 0, 0};

/** @brief Flag indicating if benchmark mode is enabled */
static int benchmark_mode = 0;
/** @brief Number of iterations for benchmark tests */
static int benchmark_iterations = 1000;


/**
 * @brief Get current time in nanoseconds
 *
 * Uses platform's clock() function to measure time with nanosecond precision.
 *
 * @return Current time in nanoseconds, or 0 on error
 */
static uint64_t get_time_ns(void) {
    clock_t t = clock();
    if (t == (clock_t)(-1)) {
        return 0; /* Error case */
    }
    /* Convert to nanoseconds: (clock_ticks / CLOCKS_PER_SEC) * 1,000,000,000 */
    return (uint64_t)((double) t / CLOCKS_PER_SEC * 1000000000.0);
}

/* Test modules in POST (Power-On Self Test) order:
 * 1. Unit tests (foundational math/algorithms) run FIRST
 * 2. Dictionary tests (FORTH word validation) run after POST passes
 */
static TestModule test_modules[] = {
    /* === Dictionary Tests: FORTH-79 Word Validation === */
    {"Stack Words", NULL, 0, run_stack_words_tests}, /* Module 5 */
    {"Return Stack Words", NULL, 0, run_return_stack_words_tests}, /* Module 6 */
    {"Memory Words", NULL, 0, run_memory_words_tests}, /* Module 7 */
    {"Arithmetic Words", NULL, 0, run_arithmetic_words_tests}, /* Module 8 */
    {"Logical Words", NULL, 0, run_logical_words_tests}, /* Module 9 */
    {"Mixed Arithmetic Words", NULL, 0, run_mixed_arithmetic_words_tests}, /* Module 10 */
    {"Double Words", NULL, 0, run_double_words_tests}, /* Module 11 */
    {"Format Words", NULL, 0, run_format_words_tests}, /* Module 12 */
    {"String Words", NULL, 0, run_string_words_tests}, /* Module 13 */
    {"I/O Words", NULL, 0, run_io_words_tests}, /* Module 14 */
    {"Block Words", NULL, 0, run_block_words_tests}, /* Module 15 */
    {"Dictionary Words", NULL, 0, run_dictionary_words_tests}, /* Module 16 */
    {"Dictionary Manipulation Words", NULL, 0, run_dictionary_manipulation_words_tests}, /* Module 17 */
    {"Vocabulary Words", NULL, 0, run_vocabulary_words_tests}, /* Module 18 */
    {"System Words", NULL, 0, run_system_words_tests}, /* Module 19 */
    {"Defining Words", NULL, 0, run_defining_words_tests}, /* Module 20 */
    {"Control Words", NULL, 0, run_control_words_tests}, /* Module 21 */
    {"StarForth Words", NULL, 0, run_starforth_words_tests}, /* Module 22 */
    {"Mama FORTH Words", NULL, 0, run_mama_forth_words_tests}, /* Module 23: Capsule System M7.1 */
    {NULL, NULL, 0, NULL} /* End marker */
};

/**
 * @brief Enable benchmark mode for test execution
 *
 * @param iterations Number of times each test should be repeated
 */
void enable_benchmark_mode(int iterations) {
    benchmark_mode = 1;
    benchmark_iterations = iterations;
    log_message(LOG_INFO, "Benchmark mode enabled: %d iterations per module", iterations);
}

/**
 * @brief Run all test modules with optional benchmarking
 *
 * Executes all registered test modules sequentially, collecting statistics
 * and/or benchmark data depending on the current mode.
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void run_all_tests(VM *vm) {
    log_message(LOG_INFO, "Starting comprehensive FORTH-79 modular test suite...");
    log_message(LOG_INFO, "==============================================");

    /* Reset global statistics */
    global_test_stats.total_tests = 0;
    global_test_stats.total_pass = 0;
    global_test_stats.total_fail = 0;
    global_test_stats.total_skip = 0;
    global_test_stats.total_error = 0;

    /* Run each test module */
    for (int i = 0; test_modules[i].module_name != NULL; i++) {

        TestModule const*module = &test_modules[i];
        log_message(LOG_INFO, "\n=== Testing Module: %s ===", module->module_name);

        if (module->run_module_tests != NULL) {
            module->run_module_tests(vm);
        } else {
            log_message(LOG_WARN, "Module %s: No tests implemented yet", module->module_name);
        }
    }

    /* Print test summary */
    log_message(LOG_INFO, "FINAL TEST SUMMARY:");
    log_message(LOG_INFO, "  Total tests: %d", global_test_stats.total_tests);
    log_message(LOG_INFO, "  Passed: %d", global_test_stats.total_pass);
    log_message(LOG_INFO, "  Failed: %d", global_test_stats.total_fail);
    log_message(LOG_INFO, "  Skipped: %d", global_test_stats.total_skip);
    log_message(LOG_INFO, "  Errors: %d", global_test_stats.total_error);

    if (global_test_stats.total_fail == 0 && global_test_stats.total_error == 0) {
        log_message(LOG_INFO, "ALL IMPLEMENTED TESTS PASSED!");
    } else {
        log_message(LOG_ERROR, "%d tests FAILED or had ERRORS!",
                    global_test_stats.total_fail + global_test_stats.total_error);
    }
    log_message(LOG_INFO, "==============================================");
}

/**
 * @brief Run tests for a specific module
 *
 * @param vm Pointer to the Forth virtual machine instance
 * @param module_name Name of the module to test
 */
void run_module_tests(VM *vm, const char *module_name) {
    for (int i = 0; test_modules[i].module_name != NULL; i++) {
        if (strcmp(test_modules[i].module_name, module_name) == 0) {
            if (benchmark_mode) {
                log_message(LOG_INFO, "Benchmarking module: %s", module_name);
                if (test_modules[i].run_module_tests != NULL) {
                    /* Warmup */
                    test_modules[i].run_module_tests(vm);

                    /* Benchmark */
                    uint64_t start_time = get_time_ns();
                    for (int iter = 0; iter < benchmark_iterations; iter++) {
                        test_modules[i].run_module_tests(vm);
                    }
                    uint64_t end_time = get_time_ns();

                    uint64_t total_time = end_time - start_time;
                    double runs_per_second = (double) benchmark_iterations * 1000000000.0 / total_time;

                    log_message(LOG_INFO, "  %.0f runs/sec | %.1f μs/run",
                                runs_per_second,
                                (double) total_time / benchmark_iterations / 1000.0);
                } else {
                    log_message(LOG_WARN, "No tests implemented for module: %s", module_name);
                }
            } else {
                log_message(LOG_INFO, "Running tests for module: %s", module_name);
                if (test_modules[i].run_module_tests != NULL) {
                    test_modules[i].run_module_tests(vm);
                } else {
                    log_message(LOG_WARN, "No tests implemented for module: %s", module_name);
                }
            }
            return;
        }
    }
    log_message(LOG_ERROR, "Unknown test module: %s", module_name);
}

/**
 * @brief Run tests for a specific Forth word
 *
 * Searches all modules for tests related to the specified word and executes them.
 *
 * @param vm Pointer to the Forth virtual machine instance
 * @param word_name Name of the Forth word to test
 */
void run_word_tests(VM *vm, const char *word_name) {
    log_message(LOG_INFO, "Searching for tests for word: %s", word_name);

    int found = 0;
    for (int i = 0; test_modules[i].module_name != NULL; i++) {
        /* This would need to be implemented in each module */
        /* For now, just run all tests - individual word testing can be added later */
    }

    if (!found) {
        log_message(LOG_WARN, "No tests found for word: %s", word_name);
    }
}