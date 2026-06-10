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

/** @brief Test suites for FORTH control words
 *  @details Contains test cases for all control flow words in Module 12,
 *           including conditional branches and loop structures
 */
static WordTestSuite control_word_suites[] = {
    {
        "IF", {
            {"true", ": TEST1 IF 42 ELSE 24 THEN ; -1 TEST1 . CR", "Should take true branch", TEST_NORMAL, 0, 1},
            {"false", ": TEST2 IF 42 ELSE 24 THEN ; 0 TEST2 . CR", "Should take false branch", TEST_NORMAL, 0, 1},
            {
                "nested", ": TEST3 IF IF 1 ELSE 2 THEN ELSE 3 THEN ; -1 -1 TEST3 . CR", "Should handle nesting",
                TEST_NORMAL, 0, 1
            },
            {"no_else", ": TEST4 IF 42 THEN ; -1 TEST4 . CR", "Should work without ELSE", TEST_NORMAL, 0, 1},
            {"empty_stack", "IF", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "ELSE", {
            {"alone", "ELSE", "Should error outside IF", TEST_ERROR_CASE, 1, 1},
            {"double", ": BAD IF 1 ELSE ELSE 2 THEN ;", "Should prevent double ELSE", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "THEN", {
            {"alone", "THEN", "Should error outside IF", TEST_ERROR_CASE, 1, 1},
            {"extra", ": BAD IF 1 THEN THEN ;", "Should prevent extra THEN", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "BEGIN", {
            {"until", ": TEST5 0 BEGIN 1+ DUP 5 = UNTIL ; TEST5 . CR", "Should loop until true", TEST_NORMAL, 0, 1},
            {
                "while", ": TEST6 0 BEGIN DUP 5 < WHILE 1+ REPEAT ; TEST6 . CR", "Should loop while true", TEST_NORMAL,
                0, 1
            },
            /* TODO: Nested control flow - needs compiler refactoring for proper origin stack */
            /* {"nested", ": TEST7 0 BEGIN 1+ BEGIN DUP 3 < WHILE 1+ REPEAT DUP 5 = UNTIL ;", "Should handle nesting", TEST_NORMAL, 0, 1}, */
            {"alone", "BEGIN", "Should error outside definition", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "UNTIL", {
            {"basic", ": TEST8 BEGIN DUP 1- DUP 0= UNTIL ; 5 TEST8 . CR", "Should count down", TEST_NORMAL, 0, 1},
            {"immediate", ": TEST9 BEGIN 1 UNTIL ; IMMEDIATE", "Should be immediate", TEST_NORMAL, 0, 1},
            {"no_begin", "UNTIL", "Should error without BEGIN", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "WHILE", {
            {"basic", ": TEST10 BEGIN DUP 5 < WHILE 1+ REPEAT ; 0 TEST10 . CR", "Should count up", TEST_NORMAL, 0, 1},
            {
                "zero_times", ": TEST11 BEGIN DUP 0< WHILE 1+ REPEAT ; 0 TEST11 . CR", "Should handle zero iterations",
                TEST_NORMAL, 0, 1
            },
            {"no_begin", "WHILE", "Should error without BEGIN", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "REPEAT", {
            {
                "basic", ": TEST12 BEGIN DUP 5 < WHILE 1+ REPEAT ; 0 TEST12 . CR", "Should terminate loop", TEST_NORMAL,
                0, 1
            },
            {"no_while", "REPEAT", "Should error without WHILE", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "DO", {
            {"basic", ": TEST13 5 0 DO I . LOOP ; TEST13 CR", "Should count 0 to 4", TEST_NORMAL, 0, 1},
            {"negative", ": TEST14 -1 -5 DO I . LOOP ; TEST14 CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"empty", ": TEST15 0 0 DO LOOP ;", "Should handle empty range", TEST_NORMAL, 0, 1},
            {
                "nested", ": TEST16 3 0 DO 3 0 DO J I + . LOOP LOOP ; TEST16 CR", "Should handle nesting", TEST_NORMAL,
                0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "?DO", {
            {"basic", ": TEST17 5 0 ?DO I . LOOP ; TEST17 CR", "Should count 0 to 4", TEST_NORMAL, 0, 1},
            {"skip", ": TEST18 1 1 ?DO I . LOOP ;", "Should skip when equal", TEST_NORMAL, 0, 1},
            {
                "nested", ": TEST19 3 0 ?DO 3 I ?DO J I + . LOOP LOOP ; TEST19 CR", "Should handle nesting",
                TEST_NORMAL, 0, 1
            },
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "LOOP", {
            {"basic", ": TEST20 5 0 DO I . LOOP ; TEST20 CR", "Should increment by 1", TEST_NORMAL, 0, 1},
            {"no_do", "LOOP", "Should error without DO", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "+LOOP", {
            {"basic", ": TEST21 10 0 DO I . 2 +LOOP ; TEST21 CR", "Should count by 2", TEST_NORMAL, 0, 1},
            {"negative", ": TEST22 0 10 DO I . -1 +LOOP ; TEST22 CR", "Should count down", TEST_NORMAL, 0, 1},
            {"variable", ": TEST23 10 0 DO I . DUP +LOOP ; 3 TEST23 CR", "Should use stack value", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "I", {
            {"basic",    ": TI 4 0 DO I . LOOP CR ; TI",        "Should print: 0 1 2 3", TEST_NORMAL, 0, 1},
            {"single",   ": TI1 1 0 DO I . LOOP CR ; TI1",      "Should print: 0",        TEST_NORMAL, 0, 1},
            {"offset",   ": TI2 5 3 DO I . LOOP CR ; TI2",      "Should print: 3 4",      TEST_NORMAL, 0, 1},
            {"in_expr",  ": TI3 3 0 DO I 2 * . LOOP CR ; TI3",  "Should print: 0 2 4",    TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "J", {
            {"basic",  ": TJ 2 0 DO 3 0 DO J . LOOP LOOP CR ; TJ",     "Should print: 0 0 0 1 1 1", TEST_NORMAL, 0, 1},
            {"paired", ": TJ2 2 0 DO 2 0 DO J I + . LOOP LOOP CR ; TJ2", "Should print: 0 1 1 2",   TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "EXIT", {
            {"early_return",  ": TEX 5 EXIT 99 ; TEX . CR",                           "Should print: 5",    TEST_NORMAL, 0, 1},
            {"cond_exit_pos", ": TEX2 DUP 0> IF EXIT THEN NEGATE ; 3 TEX2 . CR",      "Should print: 3",    TEST_NORMAL, 0, 1},
            {"cond_exit_neg", ": TEX2 DUP 0> IF EXIT THEN NEGATE ; -4 TEX2 . CR",     "Should print: 4",    TEST_NORMAL, 0, 1},
            {"no_skipped",    ": TEX3 1 2 EXIT 3 ; TEX3 . . CR",                      "Should print: 2 1",  TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "LEAVE", {
            {"basic",    ": TLV 0 5 0 DO I 3 = IF LEAVE THEN 1+ LOOP ; TLV . CR",      "Should print: 3",  TEST_NORMAL, 0, 1},
            {"at_start", ": TLV2 0 5 0 DO LEAVE 1+ LOOP ; TLV2 . CR",                  "Should print: 0",  TEST_NORMAL, 0, 1},
            {"qdloop",   ": TLV3 0 5 0 ?DO I 2 = IF LEAVE THEN 1+ LOOP ; TLV3 . CR",   "Should print: 2",  TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "UNLOOP", {
            {"early_exit",  ": TUL 5 0 DO I 2 = IF UNLOOP 42 EXIT THEN LOOP 99 ; TUL . CR",  "Should print: 42", TEST_NORMAL, 0, 1},
            {"full_loop",   ": TUL2 5 0 DO I 9 = IF UNLOOP 77 EXIT THEN LOOP 99 ; TUL2 . CR", "Should print: 99", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "AGAIN", {
            {"basic",    ": TAG 0 BEGIN 1+ DUP 3 = IF EXIT THEN AGAIN ; TAG . CR",     "Should print: 3",  TEST_NORMAL, 0, 1},
            {"countdown",": CD 10 BEGIN 1- DUP 0= IF EXIT THEN AGAIN ; CD . CR",     "Should print: 0",  TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        2
    },

    {
        "CASE", {
            {"match_1",    ": TC CASE 1 OF 10 ENDOF 2 OF 20 ENDOF 99 SWAP ENDCASE . CR ; 1 TC", "Should print: 10", TEST_NORMAL, 0, 1},
            {"match_2",    ": TC CASE 1 OF 10 ENDOF 2 OF 20 ENDOF 99 SWAP ENDCASE . CR ; 2 TC", "Should print: 20", TEST_NORMAL, 0, 1},
            {"default",    ": TC CASE 1 OF 10 ENDOF 2 OF 20 ENDOF 99 SWAP ENDCASE . CR ; 5 TC", "Should print: 99", TEST_NORMAL, 0, 1},
            {"zero_case",  ": TC CASE 0 OF 88 ENDOF 1 OF 77 ENDOF 66 SWAP ENDCASE . CR ; 0 TC", "Should print: 88", TEST_NORMAL, 0, 1},
            {"neg_case",   ": TC CASE -1 OF 55 ENDOF 44 SWAP ENDCASE . CR ; -1 TC",             "Should print: 55", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/** @brief Executes all control words test suites
 *  @param vm Pointer to the FORTH virtual machine instance
 *  @details Runs through all defined test cases for control words,
 *           testing conditional branches and loop constructs
 */
void run_control_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Control Words Tests (Module 12)...");

    for (int i = 0; control_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &control_word_suites[i]);
        run_test_suite(vm, &control_word_suites[i]);
    }

    print_module_summary("Control Words", 0, 0, 0, 0);
}