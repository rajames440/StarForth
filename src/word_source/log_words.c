/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/* log_words.c — FORTH words for log level control */

#include <string.h>
#include "include/log_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* ── Level constants ─────────────────────────────────────────────────── */

static void log_word_error(VM *vm) { vm_push(vm, (cell_t)LOG_ERROR); }
static void log_word_warn (VM *vm) { vm_push(vm, (cell_t)LOG_WARN);  }
static void log_word_info (VM *vm) { vm_push(vm, (cell_t)LOG_INFO);  }
static void log_word_test (VM *vm) { vm_push(vm, (cell_t)LOG_TEST);  }
static void log_word_debug(VM *vm) { vm_push(vm, (cell_t)LOG_DEBUG); }

/* ── LOG-LEVEL! ( n -- ) ─────────────────────────────────────────────── */

static void log_word_set_level(VM *vm)
{
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t n = vm_pop(vm);
    if (n < (cell_t)LOG_ERROR) n = (cell_t)LOG_ERROR;
    if (n > (cell_t)LOG_DEBUG) n = (cell_t)LOG_DEBUG;
    log_set_level((LogLevel)n);
}

/* ── LOG-LEVEL@ ( -- n ) ─────────────────────────────────────────────── */

static void log_word_get_level(VM *vm)
{
    vm_push(vm, (cell_t)log_get_level());
}

/* ── LOG ( c-addr u level -- ) ──────────────────────────────────────────
   Emit a tagged log line at the given level.
   Usage:  S" message" LOG-INFO LOG
           S" oh no"   LOG-ERROR LOG                                      */

static void log_word_log(VM *vm)
{
    if (vm->dsp < 2) { vm->error = 1; return; }
    cell_t level  = vm_pop(vm);
    cell_t u      = vm_pop(vm);
    cell_t c_addr = vm_pop(vm);

    if (level < (cell_t)LOG_ERROR) level = (cell_t)LOG_ERROR;
    if (level > (cell_t)LOG_DEBUG) level = (cell_t)LOG_DEBUG;

    if (u <= 0 || u >= LOG_LINE_MAX) { vm->error = 1; return; }

    char buf[LOG_LINE_MAX];
    memcpy(buf, (const char *)CELL(c_addr), (size_t)u);
    buf[u] = '\0';

    log_message((LogLevel)level, "%s", buf);
}

/* ── Registration ────────────────────────────────────────────────────── */

void register_log_words(VM *vm)
{
    register_word(vm, "LOG-ERROR", log_word_error);
    register_word(vm, "LOG-WARN",  log_word_warn);
    register_word(vm, "LOG-INFO",  log_word_info);
    register_word(vm, "LOG-TEST",  log_word_test);
    register_word(vm, "LOG-DEBUG", log_word_debug);
    register_word(vm, "LOG-LEVEL!", log_word_set_level);
    register_word(vm, "LOG-LEVEL@", log_word_get_level);
    register_word(vm, "LOG",        log_word_log);
}
