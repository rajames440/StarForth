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
 * - THRU: Load block range
 * - -->: Continue to next block
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
            {"no_block", "0 SCR ! UPDATE", "Should handle no current block", TEST_ERROR_CASE, 0, 0},
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

    {
        "THRU", {
            /* Basic functionality */
            {"single_block", "1 1 THRU", "Should load single block (1-1)", TEST_NORMAL, 0, 1},
            {"ascending", "1 3 THRU", "Should load blocks 1-3 in order", TEST_NORMAL, 0, 1},
            {"descending", "3 1 THRU", "Should auto-swap and load 1-3", TEST_NORMAL, 0, 1},
            {"same_block", "2 2 THRU", "Should load single block (2-2)", TEST_NORMAL, 0, 1},

            /* Error cases - invalid block numbers */
            {"zero_start", "0 3 THRU", "Should reject block 0 as start", TEST_ERROR_CASE, 1, 1},
            {"zero_end", "1 0 THRU", "Should reject block 0 as end", TEST_ERROR_CASE, 1, 1},
            {"both_zero", "0 0 THRU", "Should reject both blocks as 0", TEST_ERROR_CASE, 1, 1},
            {"invalid_start", "9999 10000 THRU", "Should reject invalid start block", TEST_ERROR_CASE, 1, 1},
            {"invalid_end", "1 9999 THRU", "Should reject invalid end block", TEST_ERROR_CASE, 1, 1},
            {"both_invalid", "9998 9999 THRU", "Should reject both invalid blocks", TEST_ERROR_CASE, 1, 1},

            /* Stack underflow */
            {"empty_stack", "THRU", "Should cause stack underflow (no args)", TEST_ERROR_CASE, 1, 1},
            {"one_arg", "1 THRU", "Should cause stack underflow (one arg)", TEST_ERROR_CASE, 1, 1},

            /* Range tests */
            {"small_range", "1 2 THRU", "Should load 2-block range", TEST_NORMAL, 0, 1},
            {"medium_range", "1 5 THRU", "Should load 5-block range", TEST_NORMAL, 0, 1},

            /* Edge cases */
            {"negative_start", "-1 3 THRU", "Should handle negative start", TEST_ERROR_CASE, 1, 1},
            {"negative_end", "1 -3 THRU", "Should handle negative end", TEST_ERROR_CASE, 1, 1},
            {"both_negative", "-3 -1 THRU", "Should handle both negative", TEST_ERROR_CASE, 1, 1},

            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        17
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