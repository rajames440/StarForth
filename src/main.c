/*
                                  ***   StarForth   ***

  main.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.433-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/main.c
 */

/*
                                 ***   StarForth   ***
  main.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 10/02/25, 03:40 PM ET
  Author: Robert A. James (rajames) - StarshipOS Forth Project.

  License: Creative Commons Zero v1.0 Universal
  <http://creativecommons.org/publicdomain/zero/1.0/>
 */

#include "vm.h"
#include "vm_debug.h"
#include "log.h"
#include "profiler.h"
#include "platform_time.h"
#include "test_runner/include/test_runner.h"
#include "test_runner/include/test_common.h"  /* brings in extern int fail_fast; */

#include "blkio.h"
#include "blkio_factory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <stdint.h>

#ifdef __unix__
#include <sys/mman.h>
#include <unistd.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

#ifndef BLKIO_FORTH_BLOCK_SIZE
#define BLKIO_FORTH_BLOCK_SIZE 1024u
#endif

#ifndef STARFORTH_STATE_BYTES
#define STARFORTH_STATE_BYTES 8192u
#endif

/* Optional hook (weak): if your block layer exposes an attach function, we call it. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((weak))
#endif
void blk_layer_attach_device(blkio_dev_t * dev);

/* Global VM pointer for cleanup */
static VM *global_vm = NULL;

/* Global blkio device + RAM backing (for cleanup) */
static blkio_dev_t g_blkio = {0};
static int g_blkio_opened = 0;
static uint8_t *g_ram_base = NULL;
static size_t g_ram_bytes = 0;
static int g_ram_is_mmap = 0;

/* Cleanup function for atexit() and signal handlers */
static void cleanup_blkio(void) {
    if (g_blkio_opened) {
        blkio_flush(&g_blkio);
        blkio_close(&g_blkio);
        g_blkio_opened = 0;
    }
#ifdef __unix__
    if (g_ram_base) {
        if (g_ram_is_mmap) munmap(g_ram_base, g_ram_bytes);
        else free(g_ram_base);
        g_ram_base = NULL;
        g_ram_bytes = 0;
        g_ram_is_mmap = 0;
    }
#else
    if (g_ram_base) {
        free(g_ram_base);
        g_ram_base = NULL;
        g_ram_bytes = 0;
    }
#endif
}

/**
 * @brief Performs cleanup operations before program termination
 */
void cleanup_and_exit(void) {
    /* close blkio & free RAM before VM */
    cleanup_blkio();

    if (global_vm != NULL) {
        vm_cleanup(global_vm);
        global_vm = NULL;
    }
}

/* Signal handler for CTRL-C (SIGINT) and SIGTERM */
void signal_handler(int sig) {
    switch (sig) {
        case SIGINT: printf("\nInterrupted! Cleaning up...\n");
            break;
        case SIGTERM: printf("\nTerminating! Cleaning up...\n");
            break;
        default: break;
    }
    cleanup_and_exit();
    exit(0);
}

/**
 * @brief Prints program usage information
 * @param program_name Name of the executable
 */
void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("StarForth - A lightweight Forth virtual machine\n\n");
    printf("Options:\n");
    printf("  --run-tests       Run the comprehensive test suite before starting REPL\n");
    printf("  --stress-tests    Run stress tests (deep nesting, stack exhaustion, large definitions)\n");
    printf("  --integration     Run integration tests (complete Forth programs)\n");
    printf("  --break-me        🔥 ULTRA-COMPREHENSIVE diagnostic mode - tests EVERYTHING,\n");
    printf("                    generates detailed markdown report, includes easter egg surprise!\n");
    printf("  --benchmark [N]   Run performance benchmarks (default: 1000 iterations)\n");
    printf("                    (exits after benchmarking, does not start REPL)\n");
    printf("  --log-error       Set logging level to ERROR (only errors)\n");
    printf("  --log-warn        Set logging level to WARN (warnings and errors)\n");
    printf("  --log-info        Set logging level to INFO (default)\n");
    printf("  --log-test        Set logging level to TEST (test results only)\n");
    printf("  --log-debug       Set logging level to DEBUG (all messages)\n");
    printf("  --log-none        Disable all logging (maximum performance)\n");
    printf("  --fail-fast       Stop test suite immediately on first failure\n");
    printf("  --profile [0-3]   Enable profiler: 1=basic, 2=detailed, 3=full\n");
    printf("  --profile-report  Emit profiler report on exit\n");
    printf("  --disk-img=<path> Use raw disk image file at <path>\n");
    printf("  --disk-ro         Open disk image read-only\n");
    printf("  --ram-disk=<MB>   RAM fallback size if no --disk-img (default: 1 MB)\n");
    printf("  --fbs=<bytes>     Forth block size (default: 1024)\n");
    printf("  --help, -h        Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s                        # Start REPL with INFO logging\n", program_name);
    printf("  %s --run-tests            # Run tests with TEST logging, then start REPL\n", program_name);
    printf("  %s --benchmark            # Run benchmarks with 1000 iterations\n", program_name);
    printf("  %s --benchmark 5000       # Run benchmarks with 5000 iterations\n", program_name);
    printf("  %s --log-debug --run-tests  # Run tests with DEBUG logging\n", program_name);
    printf("  %s --disk-img=./disks/starship.img  # Use disk image\n", program_name);
    printf("  %s --ram-disk=64 --fbs=1024         # 64 MB RAM fallback\n", program_name);
}

/* Parse helpers */
static int parse_u32(const char *s, uint32_t *out) {
    if (!s || !*s) return -1;
    uint64_t v = 0;
    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10u + (uint32_t)(*p - '0');
        if (v > 0xffffffffULL) return -1;
    }
    *out = (uint32_t) v;
    return 0;
}

static int parse_i(const char *s, int *out) {
    if (!s || !*s) return -1;
    int sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    long v = 0;
    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10 + (*p - '0');
    }
    *out = (int) (sign * v);
    return 0;
}

/**
 * @brief Main entry point for StarForth
 */
int main(int argc, char *argv[]) {
    VM vm = {0}; /* Zero-init */
    global_vm = &vm;

    int run_tests = 0;
    int run_benchmark = 0;
    int do_stress_tests = 0;
    int do_integration_tests = 0;
    int do_break_me = 0;
    int benchmark_iterations = 1000;

    LogLevel log_level = LOG_INFO; /* Default logging level */
    int log_level_explicitly_set = 0; /* Track if user explicitly set log level */
    ProfileLevel profile_level = PROFILE_DISABLED;
    int profile_report = 0;

    /* NEW: blkio CLI */
    const char *disk_img_path = NULL;
    uint8_t disk_read_only = 0;
    uint32_t fbs = BLKIO_FORTH_BLOCK_SIZE;
    /* default fallback per instruction: **1 MB** */
    uint32_t ram_disk_mb = 1;

    /* Initialize platform time subsystem FIRST */
    sf_time_init();

    /* Register cleanup function to run on exit */
    atexit(cleanup_and_exit);

    /* Set up signal handlers */
    signal(SIGINT, signal_handler); /* CTRL-C */
    signal(SIGTERM, signal_handler); /* Termination */

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run-tests") == 0) {
            run_tests = 1;
        } else if (strcmp(argv[i], "--stress-tests") == 0) {
            do_stress_tests = 1;
        } else if (strcmp(argv[i], "--integration") == 0) {
            do_integration_tests = 1;
        } else if (strcmp(argv[i], "--break-me") == 0) {
            do_break_me = 1;
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            run_benchmark = 1;
            if (i + 1 < argc && atoi(argv[i + 1]) > 0) {
                benchmark_iterations = atoi(argv[i + 1]);
                i++;
            }
        } else if (strcmp(argv[i], "--log-error") == 0) {
            log_level = LOG_ERROR;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--log-warn") == 0) {
            log_level = LOG_WARN;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--log-info") == 0) {
            log_level = LOG_INFO;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--log-test") == 0) {
            log_level = LOG_TEST;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--log-debug") == 0) {
            log_level = LOG_DEBUG;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--log-none") == 0) {
            log_level = LOG_NONE;
            log_level_explicitly_set = 1;
        } else if (strcmp(argv[i], "--fail-fast") == 0) {
            /* Use the GLOBAL flag declared in test_common.c */
            fail_fast = 1;
        } else if (strcmp(argv[i], "--profile") == 0) {
            if (i + 1 < argc && isdigit((unsigned char) argv[i + 1][0])) {
                int lvl_tmp = 0;
                if (parse_i(argv[i + 1], &lvl_tmp) == 0) {
                    profile_level = (ProfileLevel) lvl_tmp;
                    if (profile_level < PROFILE_DISABLED) profile_level = PROFILE_DISABLED;
                    if (profile_level > PROFILE_FULL) profile_level = PROFILE_FULL;
                    i++;
                } else {
                    profile_level = PROFILE_BASIC;
                }
            } else {
                profile_level = PROFILE_BASIC;
            }
        } else if (strcmp(argv[i], "--profile-report") == 0) {
            profile_report = 1;
            if (profile_level == PROFILE_DISABLED) profile_level = PROFILE_DETAILED;
            /* NEW: disk flags */
        } else if (strncmp(argv[i], "--disk-img=", 11) == 0) {
            disk_img_path = argv[i] + 11;
        } else if (strcmp(argv[i], "--disk-ro") == 0) {
            disk_read_only = 1;
        } else if (strncmp(argv[i], "--ram-disk=", 11) == 0) {
            uint32_t tmp = 0;
            if (parse_u32(argv[i] + 11, &tmp) != 0 || tmp == 0) {
                fprintf(stderr, "Invalid --ram-disk value: %s (positive MB expected)\n", argv[i] + 11);
                return 1;
            }
            ram_disk_mb = tmp;
        } else if (strncmp(argv[i], "--fbs=", 6) == 0) {
            uint32_t tmp = 0;
            if (parse_u32(argv[i] + 6, &tmp) != 0 || tmp == 0) {
                fprintf(stderr, "Invalid --fbs value: %s\n", argv[i] + 6);
                return 1;
            }
            fbs = tmp;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* If running tests and no explicit log level was set, default to TEST */
    if (run_tests && !log_level_explicitly_set && !run_benchmark) {
        log_level = LOG_TEST;
        log_message(LOG_INFO, "Test mode enabled - using LOG_TEST level for diagnostics");
    }

    /* Set the logging level */
    log_set_level(log_level);

    /* ---------- Open block device BEFORE vm_init so the VM can discover it ---------- */
    {
        const int requested_file = (disk_img_path && *disk_img_path) ? 1 : 0;
        int using_file = requested_file;

        /* If a file path was requested, verify accessibility; WARN + fallback to RAM if not */
        if (requested_file) {
            FILE *chk = fopen(disk_img_path, "rb");
            if (!chk) {
                log_message(LOG_WARN,
                            "Disk image '%s' not found or not accessible; falling back to RAM disk",
                            (disk_img_path ? disk_img_path : "(null)"));
                using_file = 0;
            } else {
                fclose(chk);
            }
        }

        /* Compute RAM geometry (used when using_file==0, or for RAM fallback default) */
        uint64_t ram_bytes_u64 = (uint64_t) ram_disk_mb * 1024ull * 1024ull;

        if (fbs == 0) {
            fbs = BLKIO_FORTH_BLOCK_SIZE;
        }

        uint32_t ram_blocks = (uint32_t)(ram_bytes_u64 / (uint64_t) fbs);
        g_ram_bytes = (size_t) ram_blocks * (size_t) fbs;

        /* Allocate RAM backing iff we're NOT using a file */
        if (!using_file) {
            if (g_ram_bytes == 0) {
                /* Force at least one block (edge-case: tiny MB vs large fbs) */
                g_ram_bytes = (size_t) fbs;
            }
#ifdef __unix__
            void *p = mmap(NULL, g_ram_bytes, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS
#ifdef MAP_NORESERVE
                           | MAP_NORESERVE
#endif
                           , -1, 0);
            if (p != MAP_FAILED) {
                g_ram_is_mmap = 1;
                g_ram_base = (uint8_t *) p;
            }
#endif
            if (!g_ram_base) {
                g_ram_base = (uint8_t *) malloc(g_ram_bytes);
                if (!g_ram_base) {
                    fprintf(stderr, "Failed to allocate RAM-disk (%zu bytes)\n", g_ram_bytes);
                    cleanup_and_exit();
                    return 1;
                }
            }
        }

        static uint8_t state_buf[STARFORTH_STATE_BYTES];
        uint8_t used_file = 0;
        const uint32_t total_blocks_hint = 0; /* derive from file size when using file */

        int rc = blkio_factory_open(&g_blkio,
                                    using_file ? disk_img_path : NULL,
                                    (uint8_t) disk_read_only,
                                    total_blocks_hint,
                                    fbs,
                                    state_buf, sizeof(state_buf),
                                    g_ram_base,
                                    (uint32_t)(g_ram_bytes / (size_t) fbs),
                                    &used_file);
        if (rc != BLKIO_OK) {
            if (using_file)
                fprintf(stderr, "Failed to open disk image '%s' (rc=%d)\n", disk_img_path, rc);
            else
                fprintf(stderr, "Failed to initialize RAM backend (rc=%d)\n", rc);
            cleanup_and_exit();
            return 1;
        }

        g_blkio_opened = 1;

        /* Log geometry so we know what we attached */
        blkio_info_t info = {0};
        if (blkio_info(&g_blkio, &info) == BLKIO_OK) {
            log_message(LOG_INFO,
                        "blkio: backend=%s fbs=%u blocks=%u size=%lluB ro=%u",
                        used_file ? "FILE" : "RAM",
                        (unsigned) info.forth_block_size,
                        (unsigned) info.total_blocks,
                        (unsigned long long) info.phys_size_bytes,
                        (unsigned) info.read_only);
        }
    }
    /* ---------- end block-device open ---------- */

    /* Initialize the VM */
    vm_init(&vm);
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);

    /* Check for initialization failure */
    if (vm.error) {
        fprintf(stderr, "Failed to initialize VM\n");
        cleanup_and_exit();
        return 1;
    }

    /* Initialize block subsystem with RAM backing */
    {
        extern int blk_subsys_init(VM *vm, uint8_t *ram_base, size_t ram_size);
        /* Allocate RAM for blocks 0-1023 (1024 blocks × 1024 bytes) */
        static uint8_t blk_ram[1024 * 1024];
        int rc = blk_subsys_init(&vm, blk_ram, sizeof(blk_ram));
        if (rc != 0) {
            fprintf(stderr, "Failed to initialize block subsystem (rc=%d)\n", rc);
            cleanup_and_exit();
            return 1;
        }

        /* Attach blkio device to block subsystem */
        if (g_blkio_opened && blk_layer_attach_device) {
            blk_layer_attach_device(&g_blkio);
        }
    }

    /* Initialize profiler if enabled */
    if (profile_level > PROFILE_DISABLED) {
        if (profiler_init(profile_level) != 0) {
            fprintf(stderr, "Warning: Failed to initialize profiler\n");
        }
    }

    /* Install VM dump hooks (segfault/abort + manual dumps) */
    vm_debug_set_current_vm(&vm);
    vm_debug_install_signal_handlers();

    /* Handle benchmark mode */
    if (run_benchmark) {
        log_message(LOG_INFO, "==============================================");
        log_message(LOG_INFO, "   StarForth Performance Benchmark Suite");
        log_message(LOG_INFO, "==============================================\n");

        /* Run compute-intensive benchmarks */
        run_compute_benchmarks(&vm);

        log_message(LOG_INFO, "\n==============================================");
        log_message(LOG_INFO, "   Full Test Suite Benchmark");
        log_message(LOG_INFO, "   (Running %d iterations per module)", benchmark_iterations);
        log_message(LOG_INFO, "==============================================\n");

        /* Run full test suite with iterations */
        enable_benchmark_mode(benchmark_iterations);
        run_all_tests(&vm);

        if (profile_report && profile_level > PROFILE_DISABLED) {
            profiler_generate_report();
        }
        if (profile_level > PROFILE_DISABLED) {
            profiler_shutdown();
        }
        cleanup_and_exit();
        return 0;
    }

    /* Run tests if requested */
    if (run_tests) {
        log_message(LOG_INFO, "Running comprehensive test suite...");
        run_all_tests(&vm);
        log_message(LOG_INFO, "Test run complete. Starting REPL...");
    }

    if (do_stress_tests) {
        run_stress_tests(&vm);
        log_message(LOG_INFO, "Stress tests complete.");
    }

    if (do_integration_tests) {
        run_integration_tests(&vm);
        log_message(LOG_INFO, "Integration tests complete.");
    }

    /* Handle --break-me mode */
    if (do_break_me) {
        run_break_me_tests(&vm);
        cleanup_and_exit();
        return 0;
    }

    /* Start the REPL if not running tests that exit */
    if (!do_stress_tests && !do_integration_tests) {
        /* Run INIT to load system initialization from init.4th */
        log_message(LOG_INFO, "Running system initialization (INIT)...");
        vm_interpret(&vm, "INIT");
        if (vm.error) {
            log_message(LOG_ERROR, "System initialization failed - cannot start REPL");
            cleanup_and_exit();
            return 1;
        }

        /* Set dictionary fence after INIT to protect foundational words from FORGET */
        vm.dict_fence_latest = vm.latest;
        vm.dict_fence_here = vm.here;
        log_message(LOG_INFO, "Dictionary fence set - init words protected from FORGET");

        vm_repl(&vm);
    }

    if (profile_report && profile_level > PROFILE_DISABLED) {
        profiler_generate_report();
    }
    if (profile_level > PROFILE_DISABLED) {
        profiler_shutdown();
    }

    /* Normal cleanup (will also be called by atexit) */
    cleanup_and_exit();
    return 0;
}