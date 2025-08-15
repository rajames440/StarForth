/*

                                 ***   StarForth   ***
  log.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/15/25, 10:28 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../include/log.h"
#include "../include/platform/starforth_platform.h"
#include <stdarg.h>

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

/* ANSI color codes for each log level */
static const char *level_colors[] = {
    "\x1b[31m", // ERROR - Red
    "\x1b[33m", // WARN  - Yellow
    "\x1b[32m", // INFO  - Green
    "\x1b[35m", // TEST  - Magenta
    "\x1b[34m"  // DEBUG - Blue
};

/* Test result colors */
static const char *test_pass_color = "\x1b[32m";    // Green
static const char *test_fail_color = "\x1b[31m";    // Red
static const char *test_skip_color = "\x1b[33m";    // Yellow

/* Reset color */
static const char *color_reset = "\x1b[0m";

/* Set global log level */
void log_set_level(LogLevel level) {
    current_level = level;
}

/* Get current log level */
LogLevel log_get_level(void) {
    return current_level;
}

/* Timestamp helper */
static void get_timestamp(char *buffer, size_t size) {
    sf_time_t now = sf_time(NULL);
    struct sf_tm *tm_info = sf_localtime(&now);
    sf_strftime(buffer, size, "%H:%M:%S", tm_info);
}

/* Generic logger */
void log_message(LogLevel level, const char *fmt, ...) {
    /* LOG_NONE (-1) disables ALL logging */
    if (current_level == LOG_NONE || level > current_level)
        return;

    char timestamp[16];
    get_timestamp(timestamp, sizeof(timestamp));

    sf_fprintf(sf_stderr, "%s[%s] %s: %s", level_colors[level], timestamp, level_names[level], color_reset);

    va_list args;
    va_start(args, fmt);
    sf_vfprintf(sf_stderr, fmt, args);
    va_end(args);

    sf_fprintf(sf_stderr, "\n");
}

/* Dedicated test result logger (only shown for LOG_TEST or higher) */
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

    sf_fprintf(sf_stderr, "\x1b[35m[%s] TEST: %sTesting %-12s ... %s%s%s\n",
            timestamp, color_reset, word_name, color, status, color_reset);
}
