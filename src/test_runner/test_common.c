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

#include "../../include/vm.h"
#include "../../include/vm_debug.h"
#include "include/test_common.h"
#include "../../include/log.h"
#include "test_runner.h"

#ifdef __STARKERNEL__
#include "starkernel/hal/hal.h"
#define test_fatal_exit() sk_hal_panic("test failure - fail fast")
#else
#include <stdio.h>
#include <stdlib.h>
#define test_fatal_exit() exit(1)
#endif

/** 
 * @brief Global flag to control test execution behavior
 * @details When set to non-zero, causes test runner to exit immediately on first failure
 */
int fail_fast = 0;

/* ---------- debug helpers (dump on fatal and exit) ---------- */

/**
 * @brief Dumps VM state and terminates execution
 * @param vm Pointer to VM instance
 * @param reason String describing the failure reason
 * @param file Source file where failure occurred
 * @param line Line number where failure occurred
 */
static void dump_and_die(VM* vm, const char* reason, const char* file, int line)
{
    log_message(LOG_ERROR, "[TEST-RUNNER] abort @ %s:%d (%s)",
                file, line, reason ? reason : "no-reason");
    vm_debug_dump_state(vm, reason ? reason : "test-failure");
    test_fatal_exit();
}

/* Convenience macro so call sites get file:line automatically */
#define DIE(vm, why) dump_and_die((vm), (why), __FILE__, __LINE__)

/* Single gate used by fail-fast paths */
/**
 * @brief Handles test failure and exits
 * @param vm Pointer to VM instance
 * @param module Name of the test module
 * @param case_name Name of the test case
 * @param input Test input that caused the failure
 * @param reason Description of failure reason
 */
static void fail_and_exit(VM* vm,
                          const char* module,
                          const char* case_name,
                          const char* input,
                          const char* reason)
{
    if (module && *module && case_name && *case_name)
    {
        log_message(LOG_ERROR, "[TEST-RUNNER] FAIL in %s.%s", module, case_name);
    }
    else if (module && *module)
    {
        log_message(LOG_ERROR, "[TEST-RUNNER] FAIL in %s", module);
    }

    if (input && *input)
    {
        log_message(LOG_ERROR, "[TEST-RUNNER] Input: %s", input);
    }
    if (reason && *reason)
    {
        log_message(LOG_ERROR, "[TEST-RUNNER] Reason: %s", reason);
    }

    /* Dump VM state at point-of-failure and exit */
    DIE(vm, reason ? reason : "assertion failed");
}

/* ---------- VM state save/restore ---------- */

/**
 * @brief Saves current VM state
 * @param vm Pointer to VM instance
 * @param dsp Pointer to store data stack pointer
 * @param rsp Pointer to store return stack pointer
 * @param error Pointer to store error state
 * @param mode Pointer to store VM mode
 */
void save_vm_state(VM* vm, int* dsp, int* rsp, int* error, vm_mode_t* mode)
{
    *dsp = vm->dsp;
    *rsp = vm->rsp;
    *error = vm->error;
    *mode = vm->mode;
}

/**
 * @brief Restores previously saved VM state
 * @param vm Pointer to VM instance
 * @param dsp Data stack pointer to restore
 * @param rsp Return stack pointer to restore
 * @param error Error state to restore
 * @param mode VM mode to restore
 */
void restore_vm_state(VM* vm, int dsp, int rsp, int error, vm_mode_t mode)
{
    /* For stress tests and error recovery, aggressively clear both stacks
     * to prevent any stale state from affecting subsequent tests.
     * This is safer than trying to selectively clear ranges. */
    vm->dsp = -1;
    vm->rsp = -1;
    vm->error = 0;
    vm->mode = MODE_INTERPRET;

    /* Clear control flow flags to prevent stale state between tests */
    vm->exit_colon = 0;
    vm->abort_requested = 0;

    /* Clear compilation state in case test left system in compile mode */
    vm->compiling_word = NULL;
    vm->current_executing_entry = NULL;
}

/* ---------- Assertions ---------- */

/**
 * @brief Verifies VM stack depth matches expected value
 * @param vm Pointer to VM instance
 * @param expected_depth Expected stack depth
 * @return 1 if assertion passes, 0 if it fails
 */
int assert_stack_depth(VM* vm, int expected_depth)
{
    if (vm->dsp == expected_depth)
    {
        return 1;
    }
    log_message(LOG_ERROR, "Stack depth mismatch: expected %d, got %d", expected_depth, vm->dsp);
    return 0;
}

/**
 * @brief Verifies top value on VM stack matches expected value
 * @param vm Pointer to VM instance
 * @param expected_value Expected value on top of stack
 * @return 1 if assertion passes, 0 if it fails
 */
int assert_stack_top(VM* vm, int expected_value)
{
    if (vm->dsp < 1)
    {
        log_message(LOG_ERROR, "Stack underflow when checking top value");
        return 0;
    }
    if (vm->data_stack[vm->dsp - 1] == expected_value)
    {
        return 1;
    }
    log_message(LOG_ERROR, "Stack top mismatch: expected %d, got %d",
                expected_value, vm->data_stack[vm->dsp - 1]);
    return 0;
}

/**
 * @brief Verifies VM error state matches expectation
 * @param vm Pointer to VM instance
 * @param should_have_error Expected error state (1 for error, 0 for no error)
 * @return 1 if assertion passes, 0 if it fails
 */
int assert_vm_error(VM* vm, int should_have_error)
{
    int has_error = (vm->error != 0);
    if (has_error == should_have_error)
    {
        return 1;
    }
    if (should_have_error)
    {
        log_message(LOG_ERROR, "Expected VM error but none occurred");
    }
    else
    {
        log_message(LOG_ERROR, "Unexpected VM error: %d", vm->error);
    }
    return 0;
}

/* ---------- Test execution ---------- */

/* Run a single test case */
/**
 * @brief Executes a single test case
 * @param vm Pointer to VM instance
 * @param word_name Name of the word being tested
 * @param test Pointer to test case definition
 * @return Test result (PASS, FAIL, SKIP, or ERROR)
 */
TestResult run_single_test(VM* vm, const char* word_name, const TestCase* test)
{
    if (!test->implemented)
    {
        log_test_result(word_name, TEST_SKIP);
        return TEST_SKIP;
    }

    /* Save VM state */
    int saved_dsp, saved_rsp, saved_error;
    vm_mode_t saved_mode;
    save_vm_state(vm, &saved_dsp, &saved_rsp, &saved_error, &saved_mode);

    /* Clear error state */
    vm->error = 0;

    log_message(LOG_TEST, "  Running %s.%s: %s", word_name, test->name, test->input);

    /* Execute the test */
    vm_interpret(vm, test->input);

    /* Check results_run_01_2025_12_08 */
    TestResult result;
    if (test->should_error)
    {
        if (vm->error != 0)
        {
            result = TEST_PASS;
            log_message(LOG_TEST, "    Expected error occurred");
        }
        else
        {
            result = TEST_FAIL;
            log_message(LOG_ERROR, "    Expected error but none occurred");
        }
    }
    else
    {
        if (vm->error != 0)
        {
            result = TEST_FAIL;
            log_message(LOG_ERROR, "    Unexpected VM error: %d", vm->error);
        }
        else
        {
            result = TEST_PASS;
            log_message(LOG_TEST, "    Test passed");
        }
    }

    log_test_result(word_name, result);
    if (result == TEST_PASS)
    {
        log_message(LOG_TEST, "    Expected: %s", test->expected);
    }

    /* Restore VM nightly */
    restore_vm_state(vm, saved_dsp, saved_rsp, saved_error, saved_mode);

    if (result == TEST_FAIL)
    {
        log_message(LOG_ERROR, "Test failed: %s.%s", word_name, test->name);
        log_message(LOG_ERROR, "Input: %s", test->input);
        log_message(LOG_ERROR, "Expected: %s", test->expected);

        if (fail_fast != 0)
        {
            /* Fail fast: dump and exit with real names from this scope */
            fail_and_exit(vm, word_name, test->name, test->input, "assertion failed");
        }
    }

    return result;
}

/* Run a complete test suite for one word */
/**
 * @brief Executes a complete test suite for a Forth word
 * @param vm Pointer to VM instance
 * @param suite Pointer to test suite definition
 */
void run_test_suite(VM* vm, const WordTestSuite* suite)
{
    log_message(LOG_TEST, "Testing word: %s", suite->word_name);
    log_message(LOG_TEST, "------------------------");

    /* Save dictionary state before test suite */
    DictEntry* saved_latest;
    size_t saved_here;
    save_dict_state(vm, &saved_latest, &saved_here);

    int suite_pass = 0;
    int suite_fail = 0;
    int suite_skip = 0;
    int suite_error = 0;

    for (int j = 0; j < suite->test_count && suite->tests[j].name != NULL; j++)
    {
        const TestCase* test = &suite->tests[j];

        if (!test->implemented)
        {
            suite_skip++;
            continue;
        }

        TestResult result = run_single_test(vm, suite->word_name, test);

        switch (result)
        {
        case TEST_PASS: suite_pass++;
            break;
        case TEST_FAIL: suite_fail++;
            break;
        case TEST_SKIP: suite_skip++;
            break;
        case TEST_ERROR: suite_error++;
            break;
        }
    }

    /* Restore dictionary state after test suite to remove test-created words */
    restore_dict_state(vm, saved_latest, saved_here);

    global_test_stats.total_tests += suite->test_count;
    global_test_stats.total_pass += suite_pass;
    global_test_stats.total_fail += suite_fail;
    global_test_stats.total_skip += suite_skip;
    global_test_stats.total_error += suite_error;

    log_message(LOG_TEST, "  %s: %d passed, %d failed, %d skipped, %d errors",
                suite->word_name, suite_pass, suite_fail, suite_skip, suite_error);
}

/* Print module summary */
/**
 * @brief Prints test module execution summary
 * @param module_name Name of the test module
 * @param pass Number of passed tests
 * @param fail Number of failed tests
 * @param skip Number of skipped tests
 * @param error Number of tests with errors
 */
void print_module_summary(const char* module_name, int pass, int fail, int skip, int error)
{
    log_message(LOG_TEST, "=== %s Summary: %d passed, %d failed, %d skipped, %d errors ===",
                module_name, pass, fail, skip, error);
}

/* ---------- Dictionary State Management ---------- */

/**
 * @brief Saves current dictionary state before test execution
 * @param vm Pointer to VM instance
 * @param latest Pointer to store latest dictionary entry
 * @param here Pointer to store HERE pointer
 * @details Captures dictionary state so tests can be cleaned up after execution
 */
void save_dict_state(VM* vm, DictEntry** latest, size_t* here)
{
    *latest = vm->latest;
    *here = vm->here;
}

/**
 * @brief Restores dictionary state after test execution
 * @param vm Pointer to VM instance
 * @param latest Latest dictionary entry to restore
 * @param here HERE pointer to restore
 * @details Removes all words added during test by restoring dictionary pointers.
 *          This prevents test pollution of the dictionary and ensures each test
 *          starts with a clean slate.
 */
void restore_dict_state(VM* vm, DictEntry* latest, size_t here)
{
    /* Restore dictionary pointers to state before test */
    vm->latest = latest;
    vm->here = here;

    /* NOTE: We don't free memory here because the dictionary uses a contiguous
     * memory region. The memory is still allocated but unreachable. This is
     * safe because tests run in a controlled environment and memory is reclaimed
     * when the VM terminates. For long-running test suites, consider periodically
     * reinitializing the VM. */
}