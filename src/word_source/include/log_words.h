/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

#ifndef LOG_WORDS_H
#define LOG_WORDS_H

#include "vm.h"

/**
 * @defgroup log_words Log Words
 * @{
 *
 * @brief FORTH words for log-level control and structured string emission.
 *
 * @par Level constants ( -- n )
 * Push the corresponding LogLevel integer onto the stack.
 *   LOG-ERROR ( -- 0 )
 *   LOG-WARN  ( -- 1 )
 *   LOG-INFO  ( -- 2 )
 *   LOG-TEST  ( -- 3 )
 *   LOG-DEBUG ( -- 4 )
 *
 * @par LOG-LEVEL! ( n -- )
 * Set the current log level to n.  Values outside 0–4 clamp to LOG-DEBUG.
 *
 * @par LOG-LEVEL@ ( -- n )
 * Push the current log level.
 *
 * @par LOG-INFO" ( "ccc<quote>" -- )   IMMEDIATE
 * @par LOG-WARN" ( "ccc<quote>" -- )   IMMEDIATE
 * @par LOG-ERROR" ( "ccc<quote>" -- )  IMMEDIATE
 * @par LOG-TEST" ( "ccc<quote>" -- )   IMMEDIATE
 * @par LOG-DEBUG" ( "ccc<quote>" -- )  IMMEDIATE
 * Parse the string literal up to the closing double-quote and emit it
 * via log_message() at the corresponding level.  Works in both interpret
 * and compile mode (like .").  The level filter is applied at runtime so
 * DEBUG messages are free when the level is INFO.
 *
 * @par LOG-INFO-STR  ( c-addr u -- )
 * @par LOG-WARN-STR  ( c-addr u -- )
 * @par LOG-ERROR-STR ( c-addr u -- )
 * @par LOG-TEST-STR  ( c-addr u -- )
 * @par LOG-DEBUG-STR ( c-addr u -- )
 * Consume a counted string from the stack (as produced by S") and emit
 * it via log_message() at the corresponding level.
 *
 * @par Runtime words (not for direct use)
 * (do-log-error)  (do-log-warn)  (do-log-info)
 * (do-log-test)   (do-log-debug)
 * Threaded-code runtime helpers compiled by the LOG-*" immediates.
 * @}
 */

void register_log_words(VM *vm);

#endif /* LOG_WORDS_H */
