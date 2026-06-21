/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/* log_words.c — FORTH words for log level control */

#include "include/log_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* ── Level constants ─────────────────────────────────────────────────── */

/** @brief LOG-ERROR ( -- n )  Push the @c LOG_ERROR level constant. Stack: ( -- LOG_ERROR ) */
static void log_word_error(VM *vm) { vm_push(vm, (cell_t)LOG_ERROR); }
/** @brief LOG-WARN  ( -- n )  Push the @c LOG_WARN level constant.  Stack: ( -- LOG_WARN  ) */
static void log_word_warn (VM *vm) { vm_push(vm, (cell_t)LOG_WARN);  }
/** @brief LOG-INFO  ( -- n )  Push the @c LOG_INFO level constant.  Stack: ( -- LOG_INFO  ) */
static void log_word_info (VM *vm) { vm_push(vm, (cell_t)LOG_INFO);  }
/** @brief LOG-TEST  ( -- n )  Push the @c LOG_TEST level constant.  Stack: ( -- LOG_TEST  ) */
static void log_word_test (VM *vm) { vm_push(vm, (cell_t)LOG_TEST);  }
/** @brief LOG-DEBUG ( -- n )  Push the @c LOG_DEBUG level constant. Stack: ( -- LOG_DEBUG ) */
static void log_word_debug(VM *vm) { vm_push(vm, (cell_t)LOG_DEBUG); }

/* ── LOG-LEVEL! ( n -- ) ─────────────────────────────────────────────── */

/**
 * @brief LOG-LEVEL! ( n -- )
 *
 * Pops @c n from the stack and calls @c log_set_level() to update the active
 * log filter. Values below @c LOG_ERROR are clamped to @c LOG_ERROR; values
 * above @c LOG_DEBUG are clamped to @c LOG_DEBUG.
 *
 * Stack effect: ( n -- )
 *
 * @param vm Active VM; sets @c vm->error = 1 on stack underflow
 */
static void log_word_set_level(VM *vm)
{
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t n = vm_pop(vm);
    if (n < (cell_t)LOG_ERROR) n = (cell_t)LOG_ERROR;
    if (n > (cell_t)LOG_DEBUG) n = (cell_t)LOG_DEBUG;
    log_set_level((LogLevel)n);
}

/* ── LOG-LEVEL@ ( -- n ) ─────────────────────────────────────────────── */

/**
 * @brief LOG-LEVEL@ ( -- n )
 *
 * Pushes the current log level (as returned by @c log_get_level()) onto the stack.
 *
 * Stack effect: ( -- current_level )
 *
 * @param vm Active VM
 */
static void log_word_get_level(VM *vm)
{
    vm_push(vm, (cell_t)log_get_level());
}

/* ── Registration ────────────────────────────────────────────────────── */

/**
 * @brief Register all log-level control words with the VM dictionary.
 *
 * Registers: @c LOG-ERROR, @c LOG-WARN, @c LOG-INFO, @c LOG-TEST,
 * @c LOG-DEBUG, @c LOG-LEVEL!, @c LOG-LEVEL@. Called during VM bootstrap.
 *
 * @param vm Active VM to register words into
 */
void register_log_words(VM *vm)
{
    register_word(vm, "LOG-ERROR", log_word_error);
    register_word(vm, "LOG-WARN",  log_word_warn);
    register_word(vm, "LOG-INFO",  log_word_info);
    register_word(vm, "LOG-TEST",  log_word_test);
    register_word(vm, "LOG-DEBUG", log_word_debug);
    register_word(vm, "LOG-LEVEL!", log_word_set_level);
    register_word(vm, "LOG-LEVEL@", log_word_get_level);
}
