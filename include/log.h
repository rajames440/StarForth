/*
* Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
 * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 * See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>

/* Logging levels */
typedef enum {
    LOG_ERROR = 0,
    LOG_WARN,
    LOG_INFO,
    LOG_TEST,   // 🔥 NEW
    LOG_DEBUG
} LogLevel;

/* Test result types */
typedef enum {
    TEST_PASS = 0,
    TEST_FAIL,
    TEST_SKIP,
    TEST_ERROR
} TestResult;

/* Set the global logging level (default LOG_INFO) */
void log_set_level(LogLevel level);

/* Get the current logging level */
LogLevel log_get_level(void);

/* Log a formatted message at the specified level */
void log_message(LogLevel level, const char *fmt, ...);

/* Log a test result with colored output */
void log_test_result(const char *word_name, TestResult result);

#endif /* LOG_H */
