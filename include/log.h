/*
                                  ***   StarForth   ***

  log.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:55:24.719-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/include/log.h
 */

#ifndef LOG_H
#define LOG_H

/* Maximum log line size for persistent logging */
#ifndef LOG_LINE_MAX
#define LOG_LINE_MAX 256
#endif

/* Forward declaration */
struct VM;


/**
 * @brief Logging level enumeration
 *
 * Defines the available logging levels in order of increasing verbosity.
 */
typedef enum {
    LOG_NONE = -1, /**< Completely disable all logging */
    LOG_ERROR = 0, /**< Error messages only */
    LOG_WARN, /**< Warning and error messages */
    LOG_INFO, /**< Informational, warning and error messages */
    LOG_TEST, /**< Test results and all previous levels */
    LOG_DEBUG /**< Debug messages and all previous levels */
} LogLevel;

/**
 * @brief Test result type enumeration
 *
 * Defines the possible outcomes of test execution.
 */
typedef enum {
    TEST_PASS = 0, /**< Test completed successfully */
    TEST_FAIL, /**< Test failed */
    TEST_SKIP, /**< Test was skipped */
    TEST_ERROR /**< Test encountered an error during execution */
} TestResult;

/**
 * @brief Set the global logging level
 * @param level The new logging level to set
 * @note Default level is LOG_INFO
 */
void log_set_level(LogLevel level);

/**
 * @brief Get the current logging level
 * @return The current logging level
 */
LogLevel log_get_level(void);

/**
 * @brief Log a formatted message at the specified level
 * @param level The logging level for this message
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
void log_message(LogLevel level, const char *fmt, ...);

/**
 * @brief Log a test result with colored output
 * @param word_name Name of the word being tested
 * @param result Result of the test execution
 */
void log_test_result(const char *word_name, TestResult result);

/**
 * @brief Set the VM instance for persistent logging
 * @param vm VM instance to use for persistent logging (NULL to disable)
 */
void log_set_vm(struct VM *vm);

#endif /* LOG_H */