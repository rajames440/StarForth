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

#include "vm.h"
#include "log.h"
#include "word_registry.h"
#include "test_runner/include/test_runner.h"
#include <string.h>
#include <signal.h>
#include <stdlib.h>

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
    printf("  --benchmark [N]   Run performance benchmarks (default: 1000 iterations)\n");
    printf("                    (exits after benchmarking, does not start REPL)\n");
    printf("  --log-error       Set logging level to ERROR (only errors)\n");
    printf("  --log-warn        Set logging level to WARN (warnings and errors)\n");
    printf("  --log-info        Set logging level to INFO (default)\n");
    printf("  --log-test        Set logging level to TEST (test results only)\n");
    printf("  --log-debug       Set logging level to DEBUG (all messages)\n");
    printf("  --help, -h        Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s                        # Start REPL with INFO logging\n", program_name);
    printf("  %s --run-tests            # Run tests with TEST logging, then start REPL\n", program_name);
    printf("  %s --benchmark            # Run benchmarks with 1000 iterations\n", program_name);
    printf("  %s --benchmark 5000       # Run benchmarks with 5000 iterations\n", program_name);
    printf("  %s --log-debug --run-tests  # Run tests with DEBUG logging\n", program_name);
}

int main(int argc, char *argv[]) {
    VM vm;
    int run_tests = 0;
    int run_benchmark = 0;
    int benchmark_iterations = 1000;
    LogLevel log_level = LOG_INFO;  /* Default logging level */
    int log_level_explicitly_set = 0;  /* Track if user explicitly set log level */

    /* Set global VM pointer for cleanup */
    global_vm = &vm;

    /* Register cleanup function to run on exit */
    atexit(cleanup_and_exit);

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);   /* CTRL-C */
    signal(SIGTERM, signal_handler);  /* Termination */

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run-tests") == 0) {
            run_tests = 1;
        }
        else if (strcmp(argv[i], "--benchmark") == 0) {
            run_benchmark = 1;
            /* Check if next argument is a number (iterations) */
            if (i + 1 < argc && atoi(argv[i + 1]) > 0) {
                benchmark_iterations = atoi(argv[i + 1]);
                i++;  /* Skip the iterations argument */
            }
        }
        else if (strcmp(argv[i], "--log-error") == 0) {
            log_level = LOG_ERROR;
            log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--log-warn") == 0) {
            log_level = LOG_WARN;
            log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--log-info") == 0) {
            log_level = LOG_INFO;
            log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--log-test") == 0) {
            log_level = LOG_TEST;
            log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--log-debug") == 0) {
            log_level = LOG_DEBUG;
            log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else {
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
    /* Check for initialization failure */
    if (vm.error) {
        fprintf(stderr, "Failed to initialize VM\n");
        return 1;
    }

    /* Handle benchmark mode */
    if (run_benchmark) {
        enable_benchmark_mode(benchmark_iterations);
        run_all_tests(&vm);
        cleanup_and_exit();
        return 0;
    }

    /* Run tests if requested */
    if (run_tests) {
        log_message(LOG_INFO, "Running comprehensive test suite...");
        run_all_tests(&vm);
        log_message(LOG_INFO, "Test run complete. Starting REPL...");
    }

    /* Start the REPL */
    vm_repl(&vm);

    /* Normal cleanup (will also be called by atexit) */
    cleanup_and_exit();

    return 0;
}
