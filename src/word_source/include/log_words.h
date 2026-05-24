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
 * @defgroup log_words Log Level Words
 * @{
 *
 * @brief FORTH words for querying and setting the kernel log level.
 *
 * @par LOG-ERROR ( -- 0 )
 * Push the LOG_ERROR constant (errors only).
 *
 * @par LOG-WARN ( -- 1 )
 * Push the LOG_WARN constant (warnings and errors).
 *
 * @par LOG-INFO ( -- 2 )
 * Push the LOG_INFO constant (informational, warnings, errors).
 *
 * @par LOG-TEST ( -- 3 )
 * Push the LOG_TEST constant (test results and all above).
 *
 * @par LOG-DEBUG ( -- 4 )
 * Push the LOG_DEBUG constant (full verbosity).
 *
 * @par LOG-LEVEL! ( n -- )
 * Set the current log level to n.  Values outside 0–4 clamp to LOG-DEBUG.
 *
 * @par LOG-LEVEL@ ( -- n )
 * Push the current log level.
 * @}
 */

void register_log_words(VM *vm);

#endif /* LOG_WORDS_H */
