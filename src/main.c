/*

                                 ***   StarForth   ***
  main.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/15/25, 10:20 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "vm.h"
#include "vm_debug.h"
#include "log.h"
#include "profiler.h"
#include "test_runner/include/test_runner.h"
#include "test_runner/include/test_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

/* Global VM pointer for cleanup */
static VM *global_vm = NULL;

/* Cleanup function for atexit() and signal handlers */
void cleanup_and_exit(void) {
    if (global_vm != NULL) {
        vm_cleanup(global_vm);
        global_vm = NULL;
    }
}

/* Signal handler for CTRL-C (SIGINT) and CTRL-D equivalent */
void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
            printf("\nInterrupted! Cleaning up...\n");
            break;
        case SIGTERM:
            printf("\nTerminating! Cleaning up...\n");
            break;
    }
    cleanup_and_exit();
    exit(0);
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("StarForth - A lightweight Forth virtual machine\n\n");
    printf("Options:\n");
    printf("  --run-tests       Run the comprehensive test suite before starting REPL\n");
    printf("                    (automatically enables TEST logging if no log level set)\n");
    printf("  --stress-tests    Run stress tests (deep nesting, stack exhaustion, large definitions)\n");
    printf("  --integration     Run integration tests (complete Forth programs)\n");
    printf("  --benchmark [N]   Run performance benchmarks (default: 1000 iterations)\n");
    printf("                    (exits after benchmarking, does not start REPL)\n");
    printf("  --log-error       Set logging level to ERROR (only errors)\n");
    printf("  --log-warn        Set logging level to WARN (warnings and errors)\n");
    printf("  --log-info        Set logging level to INFO (default)\n");
    printf("  --log-test        Set logging level to TEST (test results only)\n");
    printf("  --log-debug       Set logging level to DEBUG (all messages)\n");
    printf("  --log-none        Disable all logging (maximum performance)\n");
    printf("  --fail-fast       Stop test suite immediately on first failure\n");
    printf("  --help, -h        Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s                        # Start REPL with INFO logging\n", program_name);
    printf("  %s --run-tests            # Run tests with TEST logging, then start REPL\n", program_name);
    printf("  %s --benchmark            # Run benchmarks with 1000 iterations\n", program_name);
    printf("  %s --benchmark 5000       # Run benchmarks with 5000 iterations\n", program_name);
    printf("  %s --log-debug --run-tests  # Run tests with DEBUG logging\n", program_name);
}

int main(int argc, char *argv[]) {
    VM vm = {0}; /* Zero-initialize to prevent crashes if cleanup called before vm_init */
    int run_tests = 0;
    int run_benchmark = 0;
    int do_stress_tests = 0;
    int do_integration_tests = 0;
    int benchmark_iterations = 1000;
    LogLevel log_level = LOG_INFO; /* Default logging level */
    int log_level_explicitly_set = 0; /* Track if user explicitly set log level */
    ProfileLevel profile_level = PROFILE_DISABLED;
    int profile_report = 0;

    /* Set global VM pointer for cleanup */
    global_vm = &vm;

    /* Register cleanup function to run on exit */
    atexit(cleanup_and_exit);

    /* Set up signal handlers */
    signal(SIGINT, signal_handler); /* CTRL-C */
    signal(SIGTERM, signal_handler); /* Termination */

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run-tests") == 0) {
            run_tests = 1;
        } else if (strcmp(argv[i], "--stress-tests") == 0) {
            do_stress_tests = 1;
        } else if (strcmp(argv[i], "--integration") == 0) {
            do_integration_tests = 1;
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            run_benchmark = 1;
            /* Check if next argument is a number (iterations) */
            if (i + 1 < argc && atoi(argv[i + 1]) > 0) {
                benchmark_iterations = atoi(argv[i + 1]);
                i++; /* Skip the iterations argument */
            }
        } else if (strcmp(argv[i], "--log-error") == 0) {
            log_level = LOG_ERROR;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--log-warn") == 0) {
            log_level = LOG_WARN;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--log-info") == 0) {
            log_level = LOG_INFO;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--log-test") == 0) {
            log_level = LOG_TEST;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--log-debug") == 0) {
            log_level = LOG_DEBUG;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--log-none") == 0) {
            log_level = LOG_NONE;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--fail-fast") == 0) {
            fail_fast = 1;
        } else if (strcmp(argv[i], "--profile") == 0) {
            if (i + 1 < argc && isdigit(argv[i + 1][0])) {
                profile_level = (ProfileLevel)(argv[++i][0] - '0');
                if (profile_level > PROFILE_FULL) profile_level = PROFILE_FULL;
            } else {
                profile_level = PROFILE_BASIC;
            }
        } else if (strcmp(argv[i], "--profile-report") == 0) {
            profile_report = 1;
            if (profile_level == PROFILE_DISABLED) profile_level = PROFILE_DETAILED;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* If running tests and no explicit log level was set, default to TEST */
    if (run_tests && !log_level_explicitly_set && !run_benchmark) {
        log_level = LOG_TEST;
        log_message(LOG_INFO, "Test mode enabled - using LOG_TEST level for diagnostics");
    }

    /* Set the logging level */
    log_set_level(log_level);

    /* Initialize the VM */
    vm_init(&vm);
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);

    /* Check for initialization failure */
    if (vm.error) {
        fprintf(stderr, "Failed to initialize VM\n");
        return 1;
    }

    /* Initialize profiler if enabled */
    if (profile_level > PROFILE_DISABLED) {
        if (profiler_init(profile_level) != 0) {
            fprintf(stderr, "Warning: Failed to initialize profiler\n");
        }
    }
    /* Install VM dump hooks (segfault/abort + manual dumps) */
    vm_debug_set_current_vm(&vm);
    vm_debug_install_signal_handlers();
    /* Handle benchmark mode */
    if (run_benchmark) {
        log_message(LOG_INFO, "==============================================");
        log_message(LOG_INFO, "   StarForth Performance Benchmark Suite");
        log_message(LOG_INFO, "==============================================\n");

        /* Run compute-intensive benchmarks */
        run_compute_benchmarks(&vm);

        log_message(LOG_INFO, "\n==============================================");
        log_message(LOG_INFO, "   Full Test Suite Benchmark");
        log_message(LOG_INFO, "   (Running %d iterations per module)", benchmark_iterations);
        log_message(LOG_INFO, "==============================================\n");

        /* Run full test suite with iterations */
        enable_benchmark_mode(benchmark_iterations);
        run_all_tests(&vm);

        /* Generate profiling report if requested */
        if (profile_report && profile_level > PROFILE_DISABLED) {
            profiler_generate_report();
        }

        /* Shutdown profiler */
        if (profile_level > PROFILE_DISABLED) {
            profiler_shutdown();
        }

        cleanup_and_exit();
        return 0;
    }

    /* Run tests if requested */
    if (run_tests) {
        log_message(LOG_INFO, "Running comprehensive test suite...");
        run_all_tests(&vm);
        log_message(LOG_INFO, "Test run complete. Starting REPL...");
    }

    if (do_stress_tests) {
        run_stress_tests(&vm);
        log_message(LOG_INFO, "Stress tests complete.");
    }

    if (do_integration_tests) {
        run_integration_tests(&vm);
        log_message(LOG_INFO, "Integration tests complete.");
    }

    /* Start the REPL if not running tests that exit */
    if (!do_stress_tests && !do_integration_tests) {
        vm_repl(&vm);
    }

    /* Generate profiling report if requested */
    if (profile_report && profile_level > PROFILE_DISABLED) {
        profiler_generate_report();
    }

    /* Shutdown profiler */
    if (profile_level > PROFILE_DISABLED) {
        profiler_shutdown();
    }

    /* Normal cleanup (will also be called by atexit) */
    cleanup_and_exit();

    return 0;
}