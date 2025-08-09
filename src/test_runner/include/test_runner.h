/*

                                 ***   StarForth ***
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

/* Test statistics structure */
typedef struct {
    int total_tests;
    int total_pass;
    int total_fail;
    int total_skip;
    int total_error;
} TestStats;

/* Test module structure */
typedef struct {
    const char *module_name;
    void *reserved;
    int reserved_int;
    void (*run_module_tests)(VM *vm);
} TestModule;

/* Global test statistics - extern declaration */
extern TestStats global_test_stats;

/* Function declarations for test modules */
void run_stack_words_tests(VM *vm);
void run_return_stack_words_tests(VM *vm);
void run_memory_words_tests(VM *vm);
void run_arithmetic_words_tests(VM *vm);
void run_logical_words_tests(VM *vm);
void run_mixed_arithmetic_words_tests(VM *vm);
void run_double_words_tests(VM *vm);
void run_dictionary_words_tests(VM *vm);
void run_io_words_tests(VM *vm);
void run_format_words_tests(VM *vm);
void run_string_words_tests(VM *vm);
void run_dictionary_manipulation_words_tests(VM *vm);
void run_vocabulary_words_tests(VM *vm);
void run_block_words_tests(VM *vm);
void run_editor_words_tests(VM *vm);
void run_system_words_tests(VM *vm);
void run_defining_words_tests(VM *vm);
void run_control_words_tests(VM *vm);

/* Add benchmark mode support */
void enable_benchmark_mode(int iterations);

/* Existing functions */
void run_all_tests(VM *vm);
void run_module_tests(VM *vm, const char *module_name);
void run_word_tests(VM *vm, const char *word_name);

#endif /* TEST_RUNNER_H */