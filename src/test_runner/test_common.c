/*

                                 ***   StarForth   ***
  test_common.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/11/25, 1:57 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../../include/vm.h"
#include "../../include/vm_debug.h"
#include "include/test_common.h"
#include "../../include/log.h"
#include "test_runner.h"

#include <stdio.h>
#include <stdlib.h>

/* fail fast (set by --fail-fast) */
int fail_fast = 0;

/* ---------- debug helpers (dump on fatal and exit) ---------- */

static void dump_and_die(VM *vm, const char *reason, const char *file, int line) {
    fprintf(stderr, "\n[TEST-RUNNER] abort @ %s:%d (%s)\n",
            file, line, reason ? reason : "no-reason");
    vm_debug_dump_state(vm, reason ? reason : "test-failure");
    exit(1);
}

/* Convenience macro so call sites get file:line automatically */
#define DIE(vm, why) dump_and_die((vm), (why), __FILE__, __LINE__)

/* Single gate used by fail-fast paths */
static void fail_and_exit(VM *vm,
                          const char *module,
                          const char *case_name,
                          const char *input,
                          const char *reason)
{
    if (module && *module)   fprintf(stderr, "\n[TEST-RUNNER] FAIL in %s", module);
    if (case_name && *case_name) fprintf(stderr, ".%s", case_name);
    fputc('\n', stderr);

    if (input && *input)   fprintf(stderr, "[TEST-RUNNER] Input: %s\n", input);
    if (reason && *reason) fprintf(stderr, "[TEST-RUNNER] Reason: %s\n", reason);

    /* Dump VM state at point-of-failure and exit */
    DIE(vm, reason ? reason : "assertion failed");
}

/* ---------- VM state save/restore ---------- */

void save_vm_state(VM *vm, int *dsp, int *rsp, int *error, vm_mode_t *mode) {
    *dsp  = vm->dsp;
    *rsp  = vm->rsp;
    *error = vm->error;
    *mode  = vm->mode;
}

void restore_vm_state(VM *vm, int dsp, int rsp, int error, vm_mode_t mode) {
    vm->dsp  = dsp;
    vm->rsp  = rsp;
    vm->error = error;
    vm->mode  = mode;
}

/* ---------- Assertions ---------- */

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
    if (vm->data_stack[vm->dsp - 1] == expected_value) {
        return 1;
    }
    log_message(LOG_ERROR, "Stack top mismatch: expected %d, got %d",
                expected_value, vm->data_stack[vm->dsp - 1]);
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

/* ---------- Test execution ---------- */

/* Run a single test case */
TestResult run_single_test(VM *vm, const char *word_name, const TestCase *test) {
    if (!test->implemented) {
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

    /* Check results */
    TestResult result;
    if (test->should_error) {
        if (vm->error != 0) {
            result = TEST_PASS;
            log_message(LOG_TEST, "    Expected error occurred");
        } else {
            result = TEST_FAIL;
            log_message(LOG_ERROR, "    Expected error but none occurred");
        }
    } else {
        if (vm->error != 0) {
            result = TEST_FAIL;
            log_message(LOG_ERROR, "    Unexpected VM error: %d", vm->error);
        } else {
            result = TEST_PASS;
            log_message(LOG_TEST, "    Test passed");
        }
    }

    log_test_result(word_name, result);
    if (result == TEST_PASS) {
        log_message(LOG_TEST, "    Expected: %s", test->expected);
    }

    /* Restore VM baseline */
    restore_vm_state(vm, saved_dsp, saved_rsp, saved_error, saved_mode);

    if (result == TEST_FAIL) {
        log_message(LOG_ERROR, "Test failed: %s.%s", word_name, test->name);
        log_message(LOG_ERROR, "Input: %s", test->input);
        log_message(LOG_ERROR, "Expected: %s", test->expected);

        if (fail_fast != 0) {
            /* Fail fast: dump and exit with real names from this scope */
            fail_and_exit(vm, word_name, test->name, test->input, "assertion failed");
        }
    }

    return result;
}

/* Run a complete test suite for one word */
void run_test_suite(VM *vm, const WordTestSuite *suite) {
    log_message(LOG_TEST, "Testing word: %s", suite->word_name);
    log_message(LOG_TEST, "------------------------");

    int suite_pass = 0;
    int suite_fail = 0;
    int suite_skip = 0;
    int suite_error = 0;

    for (int j = 0; j < suite->test_count && suite->tests[j].name != NULL; j++) {
        const TestCase *test = &suite->tests[j];

        if (!test->implemented) {
            suite_skip++;
            continue;
        }

        TestResult result = run_single_test(vm, suite->word_name, test);

        switch (result) {
            case TEST_PASS:  suite_pass++; break;
            case TEST_FAIL:  suite_fail++; break;
            case TEST_SKIP:  suite_skip++; break;
            case TEST_ERROR: suite_error++; break;
        }
    }

    global_test_stats.total_tests += suite->test_count;
    global_test_stats.total_pass  += suite_pass;
    global_test_stats.total_fail  += suite_fail;
    global_test_stats.total_skip  += suite_skip;
    global_test_stats.total_error += suite_error;

    log_message(LOG_TEST, "  %s: %d passed, %d failed, %d skipped, %d errors",
                suite->word_name, suite_pass, suite_fail, suite_skip, suite_error);
}

/* Print module summary */
void print_module_summary(const char *module_name, int pass, int fail, int skip, int error) {
    log_message(LOG_TEST, "=== %s Summary: %d passed, %d failed, %d skipped, %d errors ===",
                module_name, pass, fail, skip, error);
}
