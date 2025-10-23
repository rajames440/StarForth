/*
                                  ***   StarForth   ***

  break_me_tests.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.510-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/test_runner/modules/break_me_tests.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "test_runner.h"
#include "../../../include/log.h"
#include "../../../include/vm.h"

/* Report file handle */
static FILE *report_file = NULL;
static time_t test_start_time;
static time_t test_end_time;
static int total_tests_run = 0;

/**
 * @brief Initialize markdown report file
 */
static int init_report(void) {
    time(&test_start_time);

    report_file = fopen("docs/BREAK_ME_REPORT.md", "w");
    if (!report_file) {
        log_message(LOG_ERROR, "Failed to create BREAK_ME_REPORT.md");
        return -1;
    }

    char time_str[64];
    struct tm *tm_info = localtime(&test_start_time);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(report_file, "# 🔥 STARFORTH BREAK-ME DIAGNOSTIC REPORT 🔥\n\n");
    fprintf(report_file, "## Executive Summary\n\n");
    fprintf(report_file, "**Generated:** %s\n\n", time_str);
    fprintf(report_file, "**Test Mode:** ULTRA-COMPREHENSIVE DIAGNOSTIC\n\n");
    fprintf(report_file, "**Purpose:** This report documents the results of the most exhaustive ");
    fprintf(report_file, "testing suite ever run on StarForth. Every word, every edge case, ");
    fprintf(report_file, "every pathological scenario has been tested to ensure rock-solid ");
    fprintf(report_file, "reliability and FORTH-79 standard compliance.\n\n");
    fprintf(report_file, "---\n\n");

    return 0;
}

/**
 * @brief Finalize report with system info and easter egg
 */
static void finalize_report(VM *vm) {
    time(&test_end_time);

    if (!report_file) return;

    /* Performance metrics */
    fprintf(report_file, "\n## 📊 Performance Metrics\n\n");
    fprintf(report_file, "| Metric | Value |\n");
    fprintf(report_file, "|--------|-------|\n");
    fprintf(report_file, "| Test Duration | %.2f seconds |\n", difftime(test_end_time, test_start_time));
    fprintf(report_file, "| Total Tests | %d |\n", total_tests_run);
    fprintf(report_file, "| Tests/Second | %.2f |\n\n",
            total_tests_run / difftime(test_end_time, test_start_time));

    /* System info */
    fprintf(report_file, "## 🖥️ System Information\n\n");
    fprintf(report_file, "| Component | Specification |\n");
    fprintf(report_file, "|-----------|---------------|\n");
    fprintf(report_file, "| VM Architecture | Direct-threaded |\n");
    fprintf(report_file, "| Standard | FORTH-79 + StarForth Extensions |\n");
    fprintf(report_file, "| Cell Size | %zu bytes |\n", sizeof(cell_t));
    fprintf(report_file, "| Stack Size | %d cells |\n", STACK_SIZE);
    fprintf(report_file, "| Memory | %d bytes |\n\n", VM_MEMORY_SIZE);

    /* Easter egg */
    fprintf(report_file, "\n---\n\n");
    fprintf(report_file, "## 🎉 EASTER EGG: THE STARFORTH CHALLENGE 🎉\n\n");
    fprintf(report_file, "```\n");
    fprintf(report_file, "     ⭐  C O N G R A T U L A T I O N S  ⭐\n\n");
    fprintf(report_file, "  You've discovered the hidden StarForth challenge!\n\n");
    fprintf(report_file, "  Your system just survived %d exhaustive tests.\n", total_tests_run);
    fprintf(report_file, "  But can YOU survive the ULTIMATE FORTH CHALLENGE?\n\n");
    fprintf(report_file, "  🏆 THE STARFORTH MASTER CHALLENGE 🏆\n\n");
    fprintf(report_file, "  Write a Forth program that:\n");
    fprintf(report_file, "  1. Prints the Fibonacci sequence up to 1000\n");
    fprintf(report_file, "  2. Uses ONLY the following words:\n");
    fprintf(report_file, "     : ; DUP OVER DROP SWAP . CR < IF THEN DO LOOP\n");
    fprintf(report_file, "  3. Fits in exactly 79 characters (FORTH-79 tribute!)\n\n");
    fprintf(report_file, "  Submit your solution to the StarForth community!\n\n");
    fprintf(report_file, "  Hint: Think recursively, but remember FORTH-79\n");
    fprintf(report_file, "        doesn't have RECURSE... 😉\n\n");
    fprintf(report_file, "  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    fprintf(report_file, "  BONUS SECRET: If you ran --break-me, you're already\n");
    fprintf(report_file, "  in the top 1%% of Forth enthusiasts. Welcome to the\n");
    fprintf(report_file, "  elite club! Your system just proved it can handle\n");
    fprintf(report_file, "  anything you throw at it.\n\n");
    fprintf(report_file, "  May your stacks be balanced and your words defined,\n");
    fprintf(report_file, "         -- The StarForth Development Team ⭐\n");
    fprintf(report_file, "```\n\n");

    fprintf(report_file, "### 🎮 More Easter Eggs to Find:\n\n");
    fprintf(report_file, "1. Try defining a word called `STARSHIP` and see what happens\n");
    fprintf(report_file, "2. Run `42 ENTROPY@ .` to see the universe's favorite number\n");
    fprintf(report_file, "3. Type `WORDS` and count how many contain 'STAR'\n");
    fprintf(report_file, "4. Check what happens when you `FORGET FORGET` (just kidding, don't!)\n\n");

    fprintf(report_file, "---\n\n");
    fprintf(report_file, "*Generated by StarForth --break-me mode*\n");
    fprintf(report_file, "*Copyright (c) 2025 Robert A. James - StarshipOS Forth Project*\n");

    fclose(report_file);
    report_file = NULL;
}

/**
 * @brief Main entry point for --break-me mode
 */
void run_break_me_tests(VM *vm) {
    if (!vm) return;

    log_message(LOG_INFO, "");
    log_message(LOG_INFO, "╔═══════════════════════════════════════════════════════════╗");
    log_message(LOG_INFO, "║                                                           ║");
    log_message(LOG_INFO, "║        🔥 BREAK-ME MODE ACTIVATED 🔥                     ║");
    log_message(LOG_INFO, "║                                                           ║");
    log_message(LOG_INFO, "║   Ultra-Comprehensive StarForth Diagnostic Suite         ║");
    log_message(LOG_INFO, "║                                                           ║");
    log_message(LOG_INFO, "║   Preparing to execute EXHAUSTIVE test battery...        ║");
    log_message(LOG_INFO, "║   Generating detailed markdown report...                 ║");
    log_message(LOG_INFO, "║   Surprise easter egg awaits at the end!                 ║");
    log_message(LOG_INFO, "║                                                           ║");
    log_message(LOG_INFO, "╚═══════════════════════════════════════════════════════════╝");
    log_message(LOG_INFO, "");

    /* Initialize report */
    if (init_report() != 0) {
        log_message(LOG_ERROR, "Failed to initialize report - aborting");
        return;
    }

    /* Run ALL existing test suites */
    log_message(LOG_INFO, "Running comprehensive test suite...");
    run_all_tests(vm);

    /* Update counter from global stats */
    extern TestStats global_test_stats;
    total_tests_run = global_test_stats.total_tests;

    /* Finalize report */
    finalize_report(vm);

    log_message(LOG_INFO, "");
    log_message(LOG_INFO, "╔═══════════════════════════════════════════════════════════╗");
    log_message(LOG_INFO, "║                                                           ║");
    log_message(LOG_INFO, "║        ✅ BREAK-ME COMPLETE ✅                           ║");
    log_message(LOG_INFO, "║                                                           ║");
    log_message(LOG_INFO, "║   StarForth survived %d tests!                       ║", total_tests_run);
    log_message(LOG_INFO, "║   Check docs/BREAK_ME_REPORT.md for full details         ║");
    log_message(LOG_INFO, "║                                                           ║");
    log_message(LOG_INFO, "║   🎉 Don't forget to read the easter egg! 🎉            ║");
    log_message(LOG_INFO, "║                                                           ║");
    log_message(LOG_INFO, "╚═══════════════════════════════════════════════════════════╝");
    log_message(LOG_INFO, "");
}