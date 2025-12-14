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
    LOG_TEST, /**< Test results_run_01_2025_12_08 and all previous levels */
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