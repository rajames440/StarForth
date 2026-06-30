/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/* log_words.c — FORTH words for log level control and string emission */

#include "include/log_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <stdint.h>

/* ── Level constants ─────────────────────────────────────────────────── */

/** @brief LOG-ERROR ( -- n )  Push the @c LOG_ERROR level constant. */
static void log_word_error(VM *vm) { vm_push(vm, (cell_t)LOG_ERROR); }
/** @brief LOG-WARN  ( -- n )  Push the @c LOG_WARN level constant. */
static void log_word_warn (VM *vm) { vm_push(vm, (cell_t)LOG_WARN);  }
/** @brief LOG-INFO  ( -- n )  Push the @c LOG_INFO level constant. */
static void log_word_info (VM *vm) { vm_push(vm, (cell_t)LOG_INFO);  }
/** @brief LOG-TEST  ( -- n )  Push the @c LOG_TEST level constant. */
static void log_word_test (VM *vm) { vm_push(vm, (cell_t)LOG_TEST);  }
/** @brief LOG-DEBUG ( -- n )  Push the @c LOG_DEBUG level constant. */
static void log_word_debug(VM *vm) { vm_push(vm, (cell_t)LOG_DEBUG); }

/* ── LOG-LEVEL! ( n -- ) ─────────────────────────────────────────────── */

/**
 * @brief LOG-LEVEL! ( n -- )
 *
 * Pops @c n from the stack and calls @c log_set_level() to update the active
 * log filter. Values below @c LOG_ERROR are clamped to @c LOG_ERROR; values
 * above @c LOG_DEBUG are clamped to @c LOG_DEBUG.
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
 * Pushes the current log level onto the stack.
 */
static void log_word_get_level(VM *vm)
{
    vm_push(vm, (cell_t)log_get_level());
}

/* ── (do-log-*) runtime words ────────────────────────────────────────── */
/*
 * Called at runtime when a compiled LOG-INFO" etc. executes.
 * Reads the inline counted string from the threaded-code stream (same
 * layout as (do-string)) and routes it through log_message().
 *
 * Inline layout: [1 length byte][n string bytes][padding to cell boundary]
 */

static void log_do_emit(VM *vm, LogLevel level)
{
    if (vm->rsp < 0) { vm->error = 1; return; }
    uint8_t *data = (uint8_t *)(uintptr_t)vm->return_stack[vm->rsp];
    if (!data) { vm->error = 1; return; }
    uint8_t n = data[0];
    /* advance IP before emitting so re-entrant calls see correct state */
    size_t skip   = 1 + (size_t)n;
    size_t padded = (skip + (sizeof(cell_t) - 1)) & ~(sizeof(cell_t) - 1);
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)(data + padded);
    log_message(level, "%.*s", (int)n, (const char *)(data + 1));
}

static void log_do_error(VM *vm) { log_do_emit(vm, LOG_ERROR); }
static void log_do_warn (VM *vm) { log_do_emit(vm, LOG_WARN);  }
static void log_do_info (VM *vm) { log_do_emit(vm, LOG_INFO);  }
static void log_do_test (VM *vm) { log_do_emit(vm, LOG_TEST);  }
static void log_do_debug(VM *vm) { log_do_emit(vm, LOG_DEBUG); }

/* ── Shared parse/compile helper ─────────────────────────────────────── */
/*
 * Parses a string literal "ccc" from the input stream (skipping an optional
 * leading space, consuming up to the closing double-quote).  In interpret
 * mode emits directly via log_message().  In compile mode emits the named
 * runtime word followed by the inline counted string block.
 */
static void log_emit_string(VM *vm, LogLevel level,
                             const char *rt_name, int rt_nlen)
{
    const char *src = vm->input_buffer;
    size_t pos = vm->input_pos;
    size_t end = vm->input_length;

    if (!src || pos > end) { vm->error = 1; return; }
    if (pos < end && src[pos] == ' ') pos++;

    size_t start = pos;
    while (pos < end && src[pos] != '"') pos++;
    if (pos >= end) { vm->error = 1; return; }

    size_t n = pos - start;
    if (n > 255) n = 255;
    vm->input_pos = pos + 1;

    if (vm->mode == MODE_INTERPRET) {
        log_message(level, "%.*s", (int)n, src + start);
        return;
    }

    /* Compilation: emit runtime word then inline [len][chars][pad] */
    DictEntry *rt = vm_find_word(vm, rt_name, (size_t)rt_nlen);
    if (!rt) { vm->error = 1; return; }
    vm_compile_word(vm, rt);

    size_t skip   = 1 + n;
    size_t padded = (skip + (sizeof(cell_t) - 1)) & ~(sizeof(cell_t) - 1);
    uint8_t *raw  = (uint8_t *)vm_allot(vm, padded);
    if (!raw) { vm->error = 1; return; }
    raw[0] = (uint8_t)n;
    for (size_t i = 0; i < n; i++) raw[1 + i] = (uint8_t)src[start + i];
    for (size_t i = 1 + n; i < padded; i++) raw[i] = 0;
}

/* ── LOG-ERROR" LOG-WARN" LOG-INFO" LOG-TEST" LOG-DEBUG" (IMMEDIATE) ── */

/** @brief LOG-ERROR" ( "ccc<quote>" -- ) Log string at ERROR level. IMMEDIATE. */
static void log_word_error_quote(VM *vm) {
    log_emit_string(vm, LOG_ERROR, "(do-log-error)", 14);
}
/** @brief LOG-WARN" ( "ccc<quote>" -- ) Log string at WARN level. IMMEDIATE. */
static void log_word_warn_quote(VM *vm) {
    log_emit_string(vm, LOG_WARN,  "(do-log-warn)",  13);
}
/** @brief LOG-INFO" ( "ccc<quote>" -- ) Log string at INFO level. IMMEDIATE. */
static void log_word_info_quote(VM *vm) {
    log_emit_string(vm, LOG_INFO,  "(do-log-info)",  13);
}
/** @brief LOG-TEST" ( "ccc<quote>" -- ) Log string at TEST level. IMMEDIATE. */
static void log_word_test_quote(VM *vm) {
    log_emit_string(vm, LOG_TEST,  "(do-log-test)",  13);
}
/** @brief LOG-DEBUG" ( "ccc<quote>" -- ) Log string at DEBUG level. IMMEDIATE. */
static void log_word_debug_quote(VM *vm) {
    log_emit_string(vm, LOG_DEBUG, "(do-log-debug)", 14);
}

/* ── LOG-*-STR ( c-addr u -- ) ──────────────────────────────────────── */
/*
 * Stack-consuming variants: take a counted string from the stack (as
 * produced by S") and route it through log_message() at the named level.
 * addr is a VM byte offset into vm->memory, identical to how TYPE works.
 */
static void log_str_emit(VM *vm, LogLevel level)
{
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "LOG-*-STR: stack underflow");
        vm->error = 1;
        return;
    }
    cell_t u    = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    if (u <= 0) return;
    if (addr < 0 || (addr + u) > (cell_t)VM_MEMORY_SIZE) {
        log_message(LOG_ERROR, "LOG-*-STR: address out of range");
        vm->error = 1;
        return;
    }
    if (u > LOG_LINE_MAX - 1) u = LOG_LINE_MAX - 1;
    log_message(level, "%.*s", (int)u, (const char *)&vm->memory[addr]);
}

/** @brief LOG-ERROR-STR ( c-addr u -- ) Log string from stack at ERROR level. */
static void log_word_error_str(VM *vm) { log_str_emit(vm, LOG_ERROR); }
/** @brief LOG-WARN-STR  ( c-addr u -- ) Log string from stack at WARN level. */
static void log_word_warn_str (VM *vm) { log_str_emit(vm, LOG_WARN);  }
/** @brief LOG-INFO-STR  ( c-addr u -- ) Log string from stack at INFO level. */
static void log_word_info_str (VM *vm) { log_str_emit(vm, LOG_INFO);  }
/** @brief LOG-TEST-STR  ( c-addr u -- ) Log string from stack at TEST level. */
static void log_word_test_str (VM *vm) { log_str_emit(vm, LOG_TEST);  }
/** @brief LOG-DEBUG-STR ( c-addr u -- ) Log string from stack at DEBUG level. */
static void log_word_debug_str(VM *vm) { log_str_emit(vm, LOG_DEBUG); }

/* ── Registration ────────────────────────────────────────────────────── */

/**
 * @brief Register all log words with the VM dictionary.
 *
 * Level constants:  LOG-ERROR  LOG-WARN  LOG-INFO  LOG-TEST  LOG-DEBUG
 * Level control:    LOG-LEVEL!  LOG-LEVEL@
 * Runtime (hidden): (do-log-error)  (do-log-warn)  (do-log-info)
 *                   (do-log-test)   (do-log-debug)
 * String literals:  LOG-ERROR"  LOG-WARN"  LOG-INFO"  LOG-TEST"  LOG-DEBUG"
 * Stack string:     LOG-ERROR-STR  LOG-WARN-STR  LOG-INFO-STR
 *                   LOG-TEST-STR   LOG-DEBUG-STR
 */
void register_log_words(VM *vm)
{
    /* Level constants (unchanged) */
    register_word(vm, "LOG-ERROR", log_word_error);
    register_word(vm, "LOG-WARN",  log_word_warn);
    register_word(vm, "LOG-INFO",  log_word_info);
    register_word(vm, "LOG-TEST",  log_word_test);
    register_word(vm, "LOG-DEBUG", log_word_debug);
    register_word(vm, "LOG-LEVEL!", log_word_set_level);
    register_word(vm, "LOG-LEVEL@", log_word_get_level);

    /* Runtime words for compiled LOG-*" — registered before the immediates
     * that reference them so vm_find_word() can locate them at compile time */
    register_word(vm, "(do-log-error)", log_do_error);
    register_word(vm, "(do-log-warn)",  log_do_warn);
    register_word(vm, "(do-log-info)",  log_do_info);
    register_word(vm, "(do-log-test)",  log_do_test);
    register_word(vm, "(do-log-debug)", log_do_debug);

    /* String-literal LOG-*" words — IMMEDIATE */
    register_word(vm, "LOG-ERROR\"", log_word_error_quote); vm_make_immediate(vm);
    register_word(vm, "LOG-WARN\"",  log_word_warn_quote);  vm_make_immediate(vm);
    register_word(vm, "LOG-INFO\"",  log_word_info_quote);  vm_make_immediate(vm);
    register_word(vm, "LOG-TEST\"",  log_word_test_quote);  vm_make_immediate(vm);
    register_word(vm, "LOG-DEBUG\"", log_word_debug_quote); vm_make_immediate(vm);

    /* Stack-consuming LOG-*-STR words */
    register_word(vm, "LOG-ERROR-STR", log_word_error_str);
    register_word(vm, "LOG-WARN-STR",  log_word_warn_str);
    register_word(vm, "LOG-INFO-STR",  log_word_info_str);
    register_word(vm, "LOG-TEST-STR",  log_word_test_str);
    register_word(vm, "LOG-DEBUG-STR", log_word_debug_str);
}
