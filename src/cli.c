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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli.h"
#include "version.h"

/* Parse helper for --ram-disk */
static int parse_u32(const char* s, uint32_t* out)
{
    if (!s || !*s) return -1;
    uint64_t v = 0;
    for (const char* p = s; *p; ++p)
    {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10u + (uint32_t)(*p - '0');
        if (v > 0xffffffffULL) return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

/**
 * @brief Print usage/help information
 */
void cli_print_usage(const char* program_name)
{
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("StarForth - A lightweight Forth virtual machine\n\n");
    printf("Options:\n");
    printf("  -s                Script/silent mode - suppress prompts and 'ok' output (for piped input)\n");
    printf("  --break-me        🔥 ULTRA-COMPREHENSIVE diagnostic mode - tests EVERYTHING\n");
    printf("  --benchmark [N]   Run performance benchmarks (default: 1000 iterations)\n");
    printf("  --log-error       Set logging level to ERROR (only errors)\n");
    printf("  --log-warn        Set logging level to WARN (warnings and errors)\n");
    printf("  --log-info        Set logging level to INFO (default)\n");
    printf("  --log-test        Set logging level to TEST (test results_run_01_2025_12_08 only)\n");
    printf("  --log-debug       Set logging level to DEBUG (all messages)\n");
    printf("  --log-none        Disable all logging (maximum performance)\n");
    printf("  --fail-fast       Stop test suite immediately on first failure\n");
    printf("  --doe             Run DoE experiment: output CSV metrics to stdout and exit\n");
    printf("  --heartbeat-log=<mode>  Heartbeat logging mode (off|summary|full)\n");
    printf("                          off:     No heartbeat output (default)\n");
    printf("                          summary: Bucket aggregates in main CSV\n");
    printf("                          full:    Per-tick CSV to hb/run-<id>.csv\n");
    printf("  --disk-img=<path> Use raw disk image file at <path>\n");
    printf("  --ram-disk=<MB>   RAM fallback size if no --disk-img (default: 1 MB)\n");
    printf("  --version, -v     Show version information and exit\n");
    printf("  --help, -h        Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s                        # Start REPL with INFO logging\n", program_name);
    printf("  %s -s < script.forth      # Run script from stdin (no prompts)\n", program_name);
    printf("  %s --benchmark            # Run benchmarks with 1000 iterations\n", program_name);
    printf("  %s --disk-img=./disks/starship.img  # Use disk image\n", program_name);
}

/**
 * @brief Parse command-line arguments into CLIConfig
 */
CLIConfig cli_parse(int argc, char* argv[])
{
    CLIConfig config = {
        /* Test modes - all default to 0 */
        .run_tests = 0,
        .break_me = 0,
        .fail_fast = 0,
        .run_tests_flag_observed = 0,

        /* Benchmark */
        .benchmark = 0,
        .benchmark_iterations = 1000,

        /* Design of Experiments */
        .doe_experiment = 0,
        .heartbeat_log_mode = HEARTBEAT_LOG_OFF,

        /* REPL mode */
        .script_mode = 0,

        /* Logging */
        .log_level = LOG_INFO,
        .log_level_explicitly_set = 0,

        /* Block I/O */
        .disk_img_path = NULL,
        .ram_disk_mb = 1,
    };

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--run-tests") == 0)
        {
            config.run_tests_flag_observed = 1;
        }
        else if (strcmp(argv[i], "--break-me") == 0)
        {
            config.break_me = 1;
        }
        else if (strcmp(argv[i], "--benchmark") == 0)
        {
            config.benchmark = 1;
            if (i + 1 < argc && atoi(argv[i + 1]) > 0)
            {
                config.benchmark_iterations = atoi(argv[i + 1]);
                i++;
            }
        }
        else if (strcmp(argv[i], "--log-error") == 0)
        {
            config.log_level = LOG_ERROR;
            config.log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--log-warn") == 0)
        {
            config.log_level = LOG_WARN;
            config.log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--log-info") == 0)
        {
            config.log_level = LOG_INFO;
            config.log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--log-test") == 0)
        {
            config.log_level = LOG_TEST;
            config.log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--log-debug") == 0)
        {
            config.log_level = LOG_DEBUG;
            config.log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--log-none") == 0)
        {
            config.log_level = LOG_NONE;
            config.log_level_explicitly_set = 1;
        }
        else if (strcmp(argv[i], "--fail-fast") == 0)
        {
            config.fail_fast = 1;
        }
        else if (strcmp(argv[i], "--doe-experiment") == 0 || strcmp(argv[i], "--doe") == 0)
        {
            config.doe_experiment = 1;
        }
        else if (strncmp(argv[i], "--heartbeat-log=", 16) == 0)
        {
            const char* mode_str = argv[i] + 16;
            if (strcmp(mode_str, "off") == 0) {
                config.heartbeat_log_mode = HEARTBEAT_LOG_OFF;
            } else if (strcmp(mode_str, "summary") == 0) {
                config.heartbeat_log_mode = HEARTBEAT_LOG_SUMMARY;
            } else if (strcmp(mode_str, "full") == 0) {
                config.heartbeat_log_mode = HEARTBEAT_LOG_FULL;
            } else {
                fprintf(stderr, "Invalid --heartbeat-log value: %s (expected off|summary|full)\n", mode_str);
                exit(1);
            }
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            config.script_mode = 1;
        }
        else if (strncmp(argv[i], "--disk-img=", 11) == 0)
        {
            config.disk_img_path = argv[i] + 11;
        }
        else if (strncmp(argv[i], "--ram-disk=", 11) == 0)
        {
            uint32_t tmp = 0;
            if (parse_u32(argv[i] + 11, &tmp) != 0 || tmp == 0)
            {
                fprintf(stderr, "Invalid --ram-disk value: %s (positive MB expected)\n", argv[i] + 11);
                exit(1);
            }
            config.ram_disk_mb = tmp;
        }
        else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0)
        {
            printf("%s\n", STARFORTH_VERSION_FULL);
            exit(0);
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            cli_print_usage(argv[0]);
            exit(0);
        }
        else
        {
            printf("Unknown option: %s\n", argv[i]);
            cli_print_usage(argv[0]);
            exit(1);
        }
    }

    return config;
}