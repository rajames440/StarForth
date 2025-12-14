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

#include "../include/log.h"
#include "../include/platform_time.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* Current log level (default to LOG_INFO) */
static LogLevel current_level = LOG_INFO;

/* Log level names */
static const char *level_names[] = {
    "ERROR",
    "WARN",
    "INFO",
    "TEST",
    "DEBUG"
};

/* ANSI color codes for each log level - conditionally used */
#ifndef STARFORTH_MINIMAL
static const char *level_colors[] = {
    "\x1b[31m", // ERROR - Red
    "\x1b[33m", // WARN  - Yellow
    "\x1b[32m", // INFO  - Green
    "\x1b[35m", // TEST  - Magenta
    "\x1b[34m" // DEBUG - Blue
};

/* Test result colors */
static const char *test_pass_color = "\x1b[32m"; // Green
static const char *test_fail_color = "\x1b[31m"; // Red
static const char *test_skip_color = "\x1b[33m"; // Yellow

/* Reset color */
static const char *color_reset = "\x1b[0m";
#else
/* No colors for minimal mode */
static const char *level_colors[] = {"", "", "", "", ""};
static const char *test_pass_color = "";
static const char *test_fail_color = "";
static const char *test_skip_color = "";
static const char *color_reset = "";
#endif

/* Set global log level */
/**
 * @brief Sets the global logging level
 *
 * @param level The LogLevel to set as current logging level
 */
void log_set_level(LogLevel level) {
    current_level = level;
}

/* Get current log level */
/**
 * @brief Gets the current global logging level
 *
 * @return LogLevel Current logging level
 */
LogLevel log_get_level(void) {
    return current_level;
}

/* Timestamp helper */
/**
 * @brief Helper function to generate timestamp string
 *
 * @param buffer Output buffer for the timestamp string
 * @param size Size of the output buffer
 */
static void get_timestamp(char *buffer, size_t size) {
    /* Use platform abstraction for timestamps */
    sf_time_ns_t now = sf_realtime_ns();
    sf_format_timestamp(now, buffer, 1); /* 24-hour format */
}

/* Generic logger */
/**
 * @brief Logs a message with the specified level and format
 *
 * @param level The LogLevel for this message
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
void log_message(LogLevel level, const char *fmt, ...) {
    /* LOG_NONE (-1) disables ALL logging */
    if (current_level == LOG_NONE || level > current_level)
        return;

    char timestamp[16];
    get_timestamp(timestamp, sizeof(timestamp));

#ifndef STARFORTH_MINIMAL
    fprintf(stderr, "%s[%s] %s: %s", level_colors[level], timestamp, level_names[level], color_reset);
#else
    fprintf(stderr, "[%s] %s: ", timestamp, level_names[level]);
#endif

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

/* Dedicated test result logger (only shown for LOG_TEST or higher) */
/**
 * @brief Logs test results_run_01_2025_12_08 with appropriate formatting and colors
 *
 * @param word_name Name of the word being tested
 * @param result Test result (PASS/FAIL/SKIP)
 */
void log_test_result(const char *word_name, TestResult result) {
    if (current_level == LOG_NONE || current_level < LOG_TEST)
        return;

    char timestamp[16];
    get_timestamp(timestamp, sizeof(timestamp));

    const char *color;
    const char *status;

    switch (result) {
        case TEST_PASS:
            color = test_pass_color;
            status = "PASS";
            break;
        case TEST_FAIL:
            color = test_fail_color;
            status = "FAIL";
            break;
        case TEST_SKIP:
            color = test_skip_color;
            status = "SKIP";
            break;
        default:
            color = color_reset;
            status = "????";
            break;
    }

#ifndef STARFORTH_MINIMAL
    fprintf(stderr, "\x1b[35m[%s] TEST: %sTesting %-12s ... %s%s%s\n",
            timestamp, color_reset, word_name, color, status, color_reset);
#else
    fprintf(stderr, "[%s] TEST: Testing %-12s ... %s\n",
            timestamp, word_name, status);
#endif
}