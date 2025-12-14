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

#ifndef STARFORTH_CLI_H
#define STARFORTH_CLI_H

#include <stdint.h>
#include "log.h"

/**
 * @brief Heartbeat logging modes for DoE experiments
 */
typedef enum {
    HEARTBEAT_LOG_OFF = 0,     /* No heartbeat output, only final summary */
    HEARTBEAT_LOG_SUMMARY = 1, /* Bucket aggregates every N ticks */
    HEARTBEAT_LOG_FULL = 2     /* Per-tick CSV output to hb/run-<id>.csv */
} HeartbeatLogMode;

/**
 * @brief Configuration structure from command-line arguments
 * Consolidates all parsed CLI options into a single, cleanly organized struct
 */
typedef struct
{
    /* Test modes */
    int run_tests;
    int break_me;
    int fail_fast;

    /* Benchmark mode */
    int benchmark;
    int benchmark_iterations;

    /* Design of Experiments mode */
    int doe_experiment;  /* 1 if --doe-experiment flag set, run harness once and collect metrics */
    HeartbeatLogMode heartbeat_log_mode; /* off|summary|full for DoE time-series data */

    /* REPL mode */
    int script_mode;  /* -s: silent/script mode (no prompts, no "ok" output) */

    /* Logging */
    LogLevel log_level;
    int log_level_explicitly_set;

    /* Block I/O */
    const char* disk_img_path;
    uint32_t ram_disk_mb;

    /* Legacy flags (internal use) */
    int run_tests_flag_observed;
} CLIConfig;

/**
 * @brief Parse command-line arguments into CLIConfig
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return CLIConfig struct with parsed options, or exits on --version/--help
 */
CLIConfig cli_parse(int argc, char* argv[]);

/**
 * @brief Print usage/help information
 * @param program_name Name of the executable
 */
void cli_print_usage(const char* program_name);

#endif /* STARFORTH_CLI_H */