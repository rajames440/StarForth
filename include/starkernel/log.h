/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/**
 * starkernel/log.h — Kernel-native logger for LithosAnanke
 *
 * Provides the same API as the hosted include/log.h so all kernel VM
 * code calling log_message() works without source changes.
 *
 * Output routes through console_puts() → UART → serial log.
 * Implementation lives in src/starkernel/vm/host/shim.c.
 *
 * This header shadows include/log.h for kernel TUs because
 * -Iinclude/starkernel precedes -Iinclude in KERNEL_CFLAGS and
 * LOADER_CFLAGS, so #include "log.h" finds this file first.
 *
 * No <stdio.h> dependency — freestanding safe.
 */

#ifndef STARKERNEL_LOG_H
#define STARKERNEL_LOG_H

/* Forward declaration — matches hosted log.h */
struct VM;

#ifndef LOG_LINE_MAX
#define LOG_LINE_MAX 256
#endif

/**
 * Log levels — identical to the hosted enumeration so kernel VM code
 * compiled against either header produces compatible call sites.
 */
typedef enum {
    LOG_NONE  = -1, /* Disable all logging */
    LOG_ERROR =  0, /* Errors only */
    LOG_WARN,       /* Warnings and errors */
    LOG_INFO,       /* Informational, warnings, and errors */
    LOG_TEST,       /* Test results and all above */
    LOG_DEBUG       /* Full verbosity */
} LogLevel;

typedef enum {
    TEST_PASS  = 0,
    TEST_FAIL,
    TEST_SKIP,
    TEST_ERROR
} TestResult;

void     log_set_level(LogLevel level);
LogLevel log_get_level(void);
void     log_message(LogLevel level, const char *fmt, ...);
void     log_test_result(const char *word_name, TestResult result);

/* TODO: persistent logging via VM-backed ring buffer — stub for now */
void     log_set_vm(struct VM *vm);

#endif /* STARKERNEL_LOG_H */
