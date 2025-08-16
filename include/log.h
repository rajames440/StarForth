/*

                                 ***   StarForth   ***
  log.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/15/25, 10:18 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef LOG_H
#define LOG_H

#include "platform/starforth_platform.h"

/* Logging levels */
typedef enum {
    LOG_NONE = -1, /* Completely disable all logging */
    LOG_ERROR = 0,
    LOG_WARN,
    LOG_INFO,
    LOG_TEST, // 🔥 NEW
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