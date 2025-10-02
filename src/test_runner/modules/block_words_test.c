/*

                                 ***   StarForth   ***
  block_words_test.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/11/25, 10:13 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/**
 * @brief Test suites for FORTH block words (Module 16)
 *
 * Contains test cases for the following FORTH words:
 * - BLOCK: Block access
 * - BUFFER: Buffer management
 * - UPDATE: Block modification marking
 * - SAVE-BUFFERS: Buffer flushing
 * - EMPTY-BUFFERS: Buffer invalidation
 * - FLUSH: Buffer saving and invalidation
 * - LOAD: Block interpretation
 * - LIST: Block display
 * - SCR: Current block number
 */
static WordTestSuite block_word_suites[] = {
    {
        "BLOCK", {
            {"basic", "1 BLOCK DUP . CR", "Should return block address", TEST_NORMAL, 0, 1},
            {"zero_block", "0 BLOCK", "Should handle block 0", TEST_ERROR_CASE, 1, 1},
            {"large_block", "65536 BLOCK", "Should handle large number", TEST_ERROR_CASE, 1, 1},
            {"negative", "-1 BLOCK", "Should handle negative", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "BLOCK", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "BUFFER", {
            {"basic", "1 BUFFER DUP . CR", "Should return buffer address", TEST_NORMAL, 0, 1},
            {"zero_block", "0 BUFFER", "Should handle block 0", TEST_ERROR_CASE, 1, 1},
            {"flush_dirty", "2 BLOCK 1+ 2 BUFFER", "Should flush if dirty", TEST_NORMAL, 0, 1},
            {"empty_stack", "BUFFER", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "UPDATE", {
            {"basic", "1 BLOCK UPDATE", "Should mark block dirty", TEST_NORMAL, 0, 0},
            {"multiple", "1 BLOCK UPDATE UPDATE", "Should handle multiple updates", TEST_NORMAL, 0, 0},
            {"no_block", "0 SCR ! UPDATE", "Should handle no current block", TEST_ERROR, 0, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "SAVE-BUFFERS", {
            {"basic", "SAVE-BUFFERS", "Should flush all buffers", TEST_NORMAL, 0, 1},
            {"dirty_blocks", "1 BLOCK UPDATE SAVE-BUFFERS", "Should save dirty blocks", TEST_NORMAL, 0, 1},
            {"no_dirty", "SAVE-BUFFERS", "Should handle no dirty blocks", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "EMPTY-BUFFERS", {
            {"basic", "EMPTY-BUFFERS", "Should invalidate all buffers", TEST_NORMAL, 0, 1},
            {"after_use", "1 BLOCK EMPTY-BUFFERS", "Should invalidate used buffers", TEST_NORMAL, 0, 1},
            {"dirty_blocks", "1 BLOCK UPDATE EMPTY-BUFFERS", "Should handle dirty blocks", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "FLUSH", {
            {"basic", "FLUSH", "Should save and invalidate buffers", TEST_NORMAL, 0, 1},
            {"dirty_blocks", "1 BLOCK UPDATE FLUSH", "Should flush dirty blocks", TEST_NORMAL, 0, 1},
            {"after_empty", "EMPTY-BUFFERS FLUSH", "Should handle empty buffers", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "LOAD", {
            {"basic", "1 LOAD", "Should interpret block 1", TEST_NORMAL, 0, 1},
            {"zero_block", "0 LOAD", "Should handle block 0", TEST_ERROR_CASE, 1, 1},
            {"nonexistent", "9999 LOAD", "Should handle missing block", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "LOAD", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "LIST", {
            {"basic", "1 LIST", "Should display block 1", TEST_NORMAL, 0, 1},
            {"zero_block", "0 LIST", "Should handle block 0", TEST_ERROR_CASE, 1, 1},
            {"nonexistent", "9999 LIST", "Should handle missing block", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "LIST", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "SCR", {
            {"basic", "SCR @ . CR", "Should show current block", TEST_NORMAL, 0, 1},
            {"after_list", "1 LIST SCR @ . CR", "Should update after LIST", TEST_NORMAL, 0, 1},
            {"after_load", "1 LOAD SCR @ . CR", "Should update after LOAD", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/**
 * @brief Executes all block words test suites
 * @param vm Pointer to the FORTH virtual machine instance
 *
 * Runs through all test cases for block manipulation words, including
 * block access, buffer management, and block I/O operations. Results
 * are logged and summarized after completion.
 */
void run_block_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Block Words Tests (Module 16)...");

    for (int i = 0; block_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &block_word_suites[i]);
    }

    print_module_summary("Block Words", 0, 0, 0, 0);
}