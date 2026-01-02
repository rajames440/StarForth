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

#include "vm.h"
#include "vm_debug.h"
#include "log.h"
#include "profiler.h"
#include "physics_runtime.h"
#include "platform_time.h"
#include "version.h"
#include "cli.h"
#include "test_runner/include/test_runner.h"
#include "test_runner/include/test_common.h"  /* brings in extern int fail_fast; */
#include "doe_metrics.h"

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
static VM* global_vm = NULL;

/* Global blkio device + RAM backing (for cleanup) */
static blkio_dev_t g_blkio = {0};
static int g_blkio_opened = 0;
static uint8_t* g_ram_base = NULL;
static size_t g_ram_bytes = 0;
static int g_ram_is_mmap = 0;

/* Cleanup function for atexit() and signal handlers */
static void cleanup_blkio(void)
{
    if (g_blkio_opened)
    {
        blkio_flush(&g_blkio);
        blkio_close(&g_blkio);
        g_blkio_opened = 0;
    }
#ifdef __unix__
    if (g_ram_base)
    {
        if (g_ram_is_mmap) munmap(g_ram_base, g_ram_bytes);
        else free(g_ram_base);
        g_ram_base = NULL;
        g_ram_bytes = 0;
        g_ram_is_mmap = 0;
    }
#else
    if (g_ram_base)
    {
        free(g_ram_base);
        g_ram_base = NULL;
        g_ram_bytes = 0;
    }
#endif
}

/**
 * @brief Performs cleanup operations before program termination
 */
void cleanup_and_exit(void)
{
    /* close blkio & free RAM before VM */
    cleanup_blkio();
    physics_runtime_shutdown();

    if (global_vm != NULL)
    {
        vm_cleanup(global_vm);
        global_vm = NULL;
    }
}

/* Signal handler for CTRL-C (SIGINT) and SIGTERM */
void signal_handler(int sig)
{
    switch (sig)
    {
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
void print_usage(const char* program_name)
{
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("StarForth - A lightweight Forth virtual machine\n\n");
    printf("Options:\n");
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
    printf("  --version, -v     Show version information and exit\n");
    printf("  --help, -h        Show this help message\n\n");
    printf("Tests are executed automatically before the REPL starts.\n\n");
    printf("Examples:\n");
    printf("  %s                        # Start REPL with INFO logging\n", program_name);
    printf("  %s --benchmark            # Run benchmarks with 1000 iterations\n", program_name);
    printf("  %s --benchmark 5000       # Run benchmarks with 5000 iterations\n", program_name);
    printf("  %s --disk-img=./disks/starship.img  # Use disk image\n", program_name);
    printf("  %s --ram-disk=64 --fbs=1024         # 64 MB RAM fallback\n", program_name);
}

/* ============================================================================
 * STATIC HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize block I/O from CLI configuration
 */
static int init_blkio(const CLIConfig* config)
{
    const int requested_file = (config->disk_img_path && *config->disk_img_path) ? 1 : 0;
    int using_file = requested_file;

    /* Verify file accessibility if requested */
    if (requested_file)
    {
        FILE* chk = fopen(config->disk_img_path, "rb");
        if (!chk)
        {
            log_message(LOG_WARN,
                        "Disk image '%s' not found or not accessible; falling back to RAM disk",
                        config->disk_img_path);
            using_file = 0;
        }
        else
        {
            fclose(chk);
        }
    }

    /* Compute RAM geometry (FBS fixed at 1024 bytes) */
    uint64_t ram_bytes_u64 = (uint64_t)config->ram_disk_mb * 1024ull * 1024ull;
    uint32_t fbs = BLKIO_FORTH_BLOCK_SIZE;
    uint32_t ram_blocks = (uint32_t)(ram_bytes_u64 / (uint64_t)fbs);
    g_ram_bytes = (size_t)ram_blocks * (size_t)fbs;

    /* Allocate RAM backing if not using file */
    if (!using_file)
    {
        if (g_ram_bytes == 0)
        {
            g_ram_bytes = (size_t)fbs;
        }
#ifdef __unix__
        void* p = mmap(NULL, g_ram_bytes, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS
#ifdef MAP_NORESERVE
                       | MAP_NORESERVE
#endif
                       , -1, 0);
        if (p != MAP_FAILED)
        {
            g_ram_is_mmap = 1;
            g_ram_base = (uint8_t*)p;
        }
#endif
        if (!g_ram_base)
        {
            g_ram_base = (uint8_t*)malloc(g_ram_bytes);
            if (!g_ram_base)
            {
                fprintf(stderr, "Failed to allocate RAM-disk (%zu bytes)\n", g_ram_bytes);
                return 1;
            }
        }
    }

    /* Open block device */
    static uint8_t state_buf[STARFORTH_STATE_BYTES];
    uint8_t used_file = 0;
    const uint32_t total_blocks_hint = 0;

    int rc = blkio_factory_open(&g_blkio,
                                using_file ? config->disk_img_path : NULL,
                                0,  /* disk_read_only: always false */
                                total_blocks_hint,
                                fbs,
                                state_buf, sizeof(state_buf),
                                g_ram_base,
                                (uint32_t)(g_ram_bytes / (size_t)fbs),
                                &used_file);
    if (rc != BLKIO_OK)
    {
        if (using_file)
            fprintf(stderr, "Failed to open disk image '%s' (rc=%d)\n", config->disk_img_path, rc);
        else
            fprintf(stderr, "Failed to initialize RAM backend (rc=%d)\n", rc);
        return 1;
    }

    g_blkio_opened = 1;

    /* Log geometry */
    blkio_info_t info = {0};
    if (blkio_info(&g_blkio, &info) == BLKIO_OK)
    {
        log_message(LOG_INFO,
                    "blkio: backend=%s fbs=%u blocks=%u size=%lluB ro=%u",
                    used_file ? "FILE" : "RAM",
                    (unsigned)info.forth_block_size,
                    (unsigned)info.total_blocks,
                    (unsigned long long)info.phys_size_bytes,
                    (unsigned)info.read_only);
    }

    return 0;
}

/**
 * @brief Initialize VM core and block subsystem
 */
static int init_vm_and_subsystem(VM* vm)
{
    /* Initialize VM */
    vm_init(vm);
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);

    if (vm->error)
    {
        fprintf(stderr, "Failed to initialize VM\n");
        return 1;
    }

    /* Initialize block subsystem */
    extern int blk_subsys_init(VM* vm, uint8_t* ram_base, size_t ram_size);
    static uint8_t blk_ram[1024 * 1024];
    int rc = blk_subsys_init(vm, blk_ram, sizeof(blk_ram));
    if (rc != 0)
    {
        fprintf(stderr, "Failed to initialize block subsystem (rc=%d)\n", rc);
        return 1;
    }

    /* Attach blkio device */
    if (g_blkio_opened && blk_layer_attach_device)
    {
        blk_layer_attach_device(&g_blkio);
    }

    return 0;
}

/**
 * @brief Run benchmark mode and exit
 */
static void run_benchmark_mode(VM* vm, const CLIConfig* config)
{
    log_message(LOG_INFO, "==============================================");
    log_message(LOG_INFO, "   StarForth Performance Benchmark Suite");
    log_message(LOG_INFO, "==============================================\n");

    /* run_compute_benchmarks(vm); */

    log_message(LOG_INFO, "\n==============================================");
    log_message(LOG_INFO, "   Full Test Suite Benchmark");
    log_message(LOG_INFO, "   (Running %d iterations per module)", config->benchmark_iterations);
    log_message(LOG_INFO, "==============================================\n");

    enable_benchmark_mode(config->benchmark_iterations);
    run_all_tests(vm);
}

/**
 * @brief Run optional test modes after system initialization
 */
static void run_optional_tests(VM* vm, const CLIConfig* config)
{
    if (config->break_me)
    {
        run_break_me_tests(vm);
    }
}

/**
 * @brief Run Design of Experiments experiment
 *
 * Executes the full test harness N times, collecting metrics after each run.
 * Writes results to CSV file for statistical analysis.
 */
static void run_doe_experiment(VM* vm)
{
    /* Reset statistics for clean start */
    vm_interpret(vm, "PHYSICS-RESET-STATS");

    /* Capture CPU state before run */
    // TODO Check on l4re/POSIX seperation of concern
    int32_t cpu_temp_before = metrics_get_cpu_temp_c();
    int32_t cpu_freq_before = metrics_get_cpu_freq_mhz();

    /* Capture monotonic time before workload */
    sf_time_ns_t workload_start_ns = sf_monotonic_ns();

    /* Run test harness once (all 936+ tests) */
    run_all_tests(vm);

    /* Capture monotonic time after workload */
    sf_time_ns_t workload_end_ns = sf_monotonic_ns();

    /* Calculate actual VM workload execution time in nanoseconds */
    uint64_t workload_duration_ns = workload_end_ns - workload_start_ns;

    /* Capture CPU state after run */
    int32_t cpu_temp_after = metrics_get_cpu_temp_c();
    int32_t cpu_freq_after = metrics_get_cpu_freq_mhz();

    /* Calculate deltas and convert to Q48.16 */
    int32_t cpu_temp_delta = cpu_temp_after - cpu_temp_before;
    int32_t cpu_freq_delta = cpu_freq_after - cpu_freq_before;

    /* DEBUG: Log final rolling window state before metrics extraction */
    log_message(LOG_INFO, "DoE FINAL STATE: rolling_window.total_executions=%lu, is_warm=%d, effective_window_size=%u",
                vm->rolling_window.total_executions,
                vm->rolling_window.is_warm,
                vm->rolling_window.effective_window_size);

    /* Extract metrics from VM with deltas */
    DoeMetrics metrics = metrics_from_vm(vm, workload_duration_ns,
                                         cpu_temp_delta, cpu_freq_delta);

    /* CSV output suppressed - metrics collected but not printed to stdout.
     * Rationale: DoE experiments now rely on internal VM metrics and test output only.
     * The CSV row is redundant and interferes with clean test harness output.
     * If CSV export is needed in the future, consider writing to a file instead
     * of stdout, or add a --csv-export flag. */
    /* metrics_write_csv_row(stdout, &metrics); */
    (void)metrics;  /* Suppress unused variable warning */
}

/* ============================================================================
 * MAIN ENTRY POINT
 * ============================================================================ */

/**
 * @brief Main entry point for StarForth
 */
int main(int argc, char* argv[])
{
    /* Parse command line arguments */
    CLIConfig config = cli_parse(argc, argv);

    /* Initialize platform time subsystem FIRST */
    sf_time_init();

    /* Register cleanup function and signal handlers */
    atexit(cleanup_and_exit);
    signal(SIGINT, signal_handler); /* CTRL-C */
    signal(SIGTERM, signal_handler); /* Termination */

    /* DoE mode: force LOG_NONE immediately to suppress all output except CSV */
    if (config.doe_experiment)
    {
        config.log_level = LOG_NONE;
    }

    /* Initialize logging and profiling */
    if (config.run_tests_flag_observed && !config.log_level_explicitly_set && !config.benchmark && !config.doe_experiment)
    {
        config.log_level = LOG_TEST;
    }

    log_set_level(config.log_level);

    if (physics_runtime_init(0) != 0)
    {
        log_message(LOG_WARN, "Physics runtime failed to allocate analytics heap; proceeding degraded");
    }

    if (config.run_tests_flag_observed)
    {
        log_message(LOG_WARN, "--run-tests flag is obsolete; tests now run automatically.");
        if (!config.log_level_explicitly_set && !config.benchmark)
        {
            log_message(LOG_INFO, "Test mode enabled - using LOG_TEST level for diagnostics");
        }
    }

    /* Initialize block I/O device */
    if (init_blkio(&config) != 0)
    {
        cleanup_and_exit();
        return 1;
    }

    /* Initialize VM and block subsystem */
    VM vm = {0};
    global_vm = &vm;
    if (init_vm_and_subsystem(&vm) != 0)
    {
        cleanup_and_exit();
        return 1;
    }

    /* Install VM debug hooks */
    vm_debug_set_current_vm(&vm);
    vm_debug_install_signal_handlers();

    /* Handle DoE experiment mode (exits after) */
    if (config.doe_experiment)
    {
        /* Force LOG_NONE so only CSV goes to stdout */
        log_set_level(LOG_NONE);

        /* Run DoE experiment - returns non-zero on runtime failure */
        run_doe_experiment(&vm);

        /* Check for VM errors (runtime instability) */
        if (vm.error)
        {
            cleanup_and_exit();
            return 2;  /* Runtime failure */
        }

        cleanup_and_exit();
        return 0;  /* Success */
    }

    /* Handle benchmark mode (exits after) */
    if (config.benchmark)
    {
        run_benchmark_mode(&vm, &config);
        cleanup_and_exit();
        return 0;
    }

    /* Apply global fail-fast flag to test framework */
    if (config.fail_fast)
    {
        fail_fast = 1;
    }

    /* Protect foundational words from FORGET */
    vm.dict_fence_latest = vm.latest;
    vm.dict_fence_here = vm.here;
    log_message(LOG_INFO, "Dictionary fence set - init words protected from FORGET");

    /* Run optional test modes */
    run_optional_tests(&vm, &config);

    /* Start REPL unless --break-me was specified */
    if (!config.break_me)
    {
        /* Always run the full test suite immediately prior to REPL */
        log_message(LOG_INFO, "Running comprehensive test suite...");
        run_all_tests(&vm);
        log_message(LOG_INFO, "Test run complete.");

        /* Run system initialization AFTER POST completes */
        log_message(LOG_INFO, "Running system initialization (INIT)...");
        vm_interpret(&vm, "INIT");
        if (vm.error)
        {
            log_message(LOG_ERROR, "System initialization failed - cannot continue");
            cleanup_and_exit();
            return 1;
        }
        log_message(LOG_INFO, "System initialization complete.");

        vm_repl(&vm, config.script_mode);
    }

    cleanup_and_exit();
    return 0;
}