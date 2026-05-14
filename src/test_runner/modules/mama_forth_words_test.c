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
*/

/**
 * @file mama_forth_words_test.c
 * @brief Test module for Mama FORTH vocabulary (M7.1 Capsule System)
 *
 * Tests for the MAMA vocabulary words. These words are kernel-only
 * (__STARKERNEL__), so in hosted builds all tests are marked as
 * not implemented and will be skipped.
 *
 * In kernel builds, these tests validate:
 *   - Capsule directory enumeration
 *   - Descriptor field access
 *   - VM birth protocol
 *   - Experiment execution
 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/*
 * For hosted builds, all tests are marked implemented=0 (skipped).
 * For kernel builds, tests would be implemented=1.
 */
#ifdef __STARKERNEL__
#define MAMA_IMPLEMENTED 1
#else
#define MAMA_IMPLEMENTED 0
#endif

/* Mama FORTH Words Test Suites */
static WordTestSuite mama_word_suites[] = {
    {
        "CAPSULE-COUNT",
        {
            {
                "basic",
                "CAPSULE-COUNT . CR",
                "Should print number of capsules",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {
                "non_negative",
                "CAPSULE-COUNT 0 >= . CR",
                "Should be >= 0",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "CAPSULE@",
        {
            {
                "first_capsule",
                "0 CAPSULE@ . CR",
                "Should return first descriptor address",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {
                "out_of_bounds",
                "9999 CAPSULE@ . CR",
                "Should return 0 for invalid index",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {
                "empty_stack",
                "CAPSULE@",
                "Should cause stack underflow",
                TEST_ERROR_CASE,
                1,
                MAMA_IMPLEMENTED
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "CAPSULE-FLAGS@",
        {
            {
                "basic",
                "0 CAPSULE@ CAPSULE-FLAGS@ . CR",
                "Should return flags from descriptor",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {
                "null_desc",
                "0 CAPSULE-FLAGS@ . CR",
                "Should handle null descriptor",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {
                "empty_stack",
                "CAPSULE-FLAGS@",
                "Should cause stack underflow",
                TEST_ERROR_CASE,
                1,
                MAMA_IMPLEMENTED
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "CAPSULE-LEN@",
        {
            {
                "basic",
                "0 CAPSULE@ CAPSULE-LEN@ . CR",
                "Should return payload length",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {
                "empty_stack",
                "CAPSULE-LEN@",
                "Should cause stack underflow",
                TEST_ERROR_CASE,
                1,
                MAMA_IMPLEMENTED
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "CAPSULE-HASH@",
        {
            {
                "basic",
                "0 CAPSULE@ CAPSULE-HASH@ . CR",
                "Should return content hash",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {
                "empty_stack",
                "CAPSULE-HASH@",
                "Should cause stack underflow",
                TEST_ERROR_CASE,
                1,
                MAMA_IMPLEMENTED
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "MAMA-VM-ID",
        {
            {
                "always_zero",
                "MAMA-VM-ID . CR",
                "Should always print 0",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {
                "equals_zero",
                "MAMA-VM-ID 0 = . CR",
                "Should equal 0",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "VM-COUNT",
        {
            {
                "at_least_one",
                "VM-COUNT . CR",
                "Should be at least 1 (Mama)",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {
                "positive",
                "VM-COUNT 0 > . CR",
                "Should be positive",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "CAPSULE-BIRTH",
        {
            {
                "invalid_id",
                "9999999 CAPSULE-BIRTH . CR",
                "Should return -1 for invalid capsule ID",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {
                "empty_stack",
                "CAPSULE-BIRTH",
                "Should cause stack underflow",
                TEST_ERROR_CASE,
                1,
                MAMA_IMPLEMENTED
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "CAPSULE-RUN",
        {
            {
                "empty_stack",
                "CAPSULE-RUN",
                "Should cause stack underflow",
                TEST_ERROR_CASE,
                1,
                MAMA_IMPLEMENTED
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        1
    },

    {
        "CAPSULE-TEST",
        {
            {
                "basic",
                "CAPSULE-TEST",
                "Should print diagnostic message",
                TEST_NORMAL,
                0,
                MAMA_IMPLEMENTED
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        1
    },

    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}  /* End marker */
};

/**
 * @brief Executes all Mama FORTH vocabulary test suites
 *
 * In hosted builds, all tests will be skipped since MAMA_IMPLEMENTED=0.
 * In kernel builds, tests will execute against the capsule system.
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void run_mama_forth_words_tests(VM *vm)
{
    log_message(LOG_INFO, "Running Mama FORTH Words Tests (Capsule System M7.1)...");

#ifndef __STARKERNEL__
    log_message(LOG_INFO, "  (Hosted build - Mama words not available, tests will be skipped)");
#endif

    for (int i = 0; mama_word_suites[i].word_name != NULL; i++) {
        run_test_suite(vm, &mama_word_suites[i]);
    }

    print_module_summary("Mama FORTH Words", 0, 0, 0, 0);
}
