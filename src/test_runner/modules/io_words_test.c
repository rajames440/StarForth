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
 * @brief Test suites for I/O words (Module 9)
 * @details Contains comprehensive test cases for all I/O related FORTH words
 *          including output formatting, character I/O, and base conversion operations
 */
static WordTestSuite io_word_suites[] = {
    {
        ".", {
            {"positive", "42 . CR", "Should print: 42", TEST_NORMAL, 0, 1},
            {"negative", "-42 . CR", "Should print: -42", TEST_NORMAL, 0, 1},
            {"zero", "0 . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"max_int", "2147483647 . CR", "Should print: 2147483647", TEST_EDGE_CASE, 0, 1},
            {"min_int", "-2147483648 . CR", "Should print: -2147483648", TEST_EDGE_CASE, 0, 1},
            {"multiple", "1 2 3 . . . CR", "Should print: 3 2 1", TEST_NORMAL, 0, 1},
            {"empty_stack", ".", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        "CR", {
            {"basic", "42 . CR 43 . CR", "Should print on separate lines", TEST_NORMAL, 0, 1},
            {"multiple", "CR CR CR", "Should print multiple newlines", TEST_NORMAL, 0, 1},
            {"no_stack_effect", "DEPTH CR DEPTH = . CR", "Should not affect stack", TEST_NORMAL, 0, 1},
            {"after_output", "1 2 3 . . . CR", "Should work after output", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "SPACE", {
            {"basic", "42 . SPACE 43 . CR", "Should add space between numbers", TEST_NORMAL, 0, 1},
            {"multiple", "SPACE SPACE SPACE", "Should print multiple spaces", TEST_NORMAL, 0, 1},
            {"no_stack_effect", "DEPTH SPACE DEPTH = . CR", "Should not affect stack", TEST_NORMAL, 0, 1},
            {"with_text", "65 EMIT SPACE 66 EMIT CR", "Should space between chars", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "SPACES", {
            {"zero", "0 SPACES", "Should print no spaces", TEST_NORMAL, 0, 1},
            {"one", "1 SPACES", "Should print one space", TEST_NORMAL, 0, 1},
            {"multiple", "5 SPACES", "Should print five spaces", TEST_NORMAL, 0, 1},
            {"large", "20 SPACES", "Should print twenty spaces", TEST_NORMAL, 0, 1},
            {"negative", "-5 SPACES", "Should handle negative", TEST_NORMAL, 0, 1},
            {"with_output", "42 . 3 SPACES 43 . CR", "Should space output", TEST_NORMAL, 0, 1},
            {"empty_stack", "SPACES", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        "EMIT", {
            {"letter_A", "65 EMIT CR", "Should print: A", TEST_NORMAL, 0, 1},
            {"letter_Z", "90 EMIT CR", "Should print: Z", TEST_NORMAL, 0, 1},
            {"digit", "48 EMIT CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"space_char", "32 EMIT CR", "Should print space", TEST_NORMAL, 0, 1},
            {"newline", "10 EMIT", "Should print newline", TEST_NORMAL, 0, 1},
            {"high_ascii", "127 EMIT CR", "Should print DEL char", TEST_NORMAL, 0, 1},
            {"truncation", "321 EMIT CR", "Should truncate to low byte", TEST_NORMAL, 0, 1},
            {"negative", "-1 EMIT CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"sequence", "72 EMIT 73 EMIT 10 EMIT", "Should print: HI", TEST_NORMAL, 0, 1},
            {"empty_stack", "EMIT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        10
    },

    {
        "KEY", {
            {"basic_test", "KEY DROP", "Should wait for keypress", TEST_NORMAL, 0, 0},
            {"echo_test", "KEY DUP EMIT CR", "Should echo keypresses", TEST_NORMAL, 0, 0},
            {"multiple", "KEY KEY KEY DROP DROP DROP", "Should read multiple keys", TEST_NORMAL, 0, 0},
            {"no_stack_underflow", "KEY . CR", "Should push key to stack", TEST_NORMAL, 0, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "?KEY", {
            {"available_test", "?KEY . CR", "Should test key availability", TEST_NORMAL, 0, 0},
            {"no_wait", "?KEY IF KEY EMIT THEN CR", "Should not wait", TEST_NORMAL, 0, 0},
            {"multiple_check", "?KEY ?KEY OR . CR", "Should check availability", TEST_NORMAL, 0, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        3
    },

    {
        "TYPE", {
            {
                "basic_string", "HERE S\" Hello\" DUP >R HERE SWAP CMOVE HERE R> TYPE CR", "Should print: Hello",
                TEST_NORMAL, 0, 1
            },
            {"empty_string", "HERE 0 TYPE CR", "Should print nothing", TEST_NORMAL, 0, 1},
            {"single_char", "HERE 65 OVER C! 1 TYPE CR", "Should print: A", TEST_NORMAL, 0, 1},
            {
                "numbers", "HERE S\" 12345\" DUP >R HERE SWAP CMOVE HERE R> TYPE CR", "Should print: 12345",
                TEST_NORMAL, 0, 1
            },
            {"zero_length", "PAD 0 TYPE", "Should handle zero length", TEST_NORMAL, 0, 1},
            {"one_item", "PAD TYPE", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "TYPE", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        "COUNT", {
            {
                "basic", "HERE S\" Test\" DROP C@ HERE 1+ SWAP COUNT . . CR", "Should show length and addr+1",
                TEST_NORMAL, 0, 1
            },
            {"empty", "HERE 0 OVER C! COUNT . . CR", "Should handle empty string", TEST_NORMAL, 0, 1},
            {"max_length", "HERE 255 OVER C! COUNT . . CR", "Should handle max length", TEST_NORMAL, 0, 1},
            {"zero_addr_plus_one", "HERE COUNT SWAP 1- = . CR", "Should increment address", TEST_NORMAL, 0, 1},
            {"empty_stack", "COUNT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "EXPECT", {
            {"basic_input", "PAD 10 EXPECT", "Should read up to 10 chars", TEST_NORMAL, 0, 0},
            {"max_length", "PAD 80 EXPECT", "Should read up to 80 chars", TEST_NORMAL, 0, 0},
            {"zero_length", "PAD 0 EXPECT", "Should handle zero length", TEST_NORMAL, 0, 1},
            {"one_item", "PAD EXPECT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "EXPECT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        5
    },

    {
        "SPAN", {
            {"after_expect", "PAD 10 EXPECT SPAN @ . CR", "Should show chars read", TEST_NORMAL, 0, 0},
            {"zero_input", "PAD 0 EXPECT SPAN @ . CR", "Should show zero", TEST_NORMAL, 0, 1},
            {"variable_test", "SPAN @ SPAN @ = . CR", "Should be consistent", TEST_NORMAL, 0, 1},
            {"address_test", "SPAN 0<> . CR", "Should be valid address", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    {
        "WORD", {
            /* Basic functionality */
            {"basic_parse", "BL WORD COUNT TYPE CR", "Should parse next word", TEST_NORMAL, 0, 0},
            {"delimiter_test", "44 WORD COUNT TYPE CR", "Should use comma delimiter", TEST_NORMAL, 0, 0},
            {"empty_input", "BL WORD COUNT . CR", "Should handle empty input", TEST_NORMAL, 0, 1},

            /* Different delimiters */
            {"space_delim", "32 WORD DROP", "Should use space as delimiter", TEST_NORMAL, 0, 1},
            {"newline_delim", "10 WORD DROP", "Should use newline as delimiter", TEST_NORMAL, 0, 1},
            {"tab_delim", "9 WORD DROP", "Should use tab as delimiter", TEST_NORMAL, 0, 1},
            {"comma_delim", "44 WORD DROP", "Should use comma as delimiter", TEST_NORMAL, 0, 1},

            /* Leading delimiters */
            {"skip_leading", "BL WORD DROP", "Should skip leading spaces", TEST_NORMAL, 0, 1},

            /* Word length tests */
            {"single_char", "BL WORD COUNT . CR", "Should parse single character", TEST_NORMAL, 0, 1},
            {"long_word", "BL WORD COUNT 0 > . CR", "Should parse long word", TEST_NORMAL, 0, 1},

            /* Edge cases */
            {"zero_delim", "0 WORD DROP", "Should handle null delimiter", TEST_NORMAL, 0, 1},
            {"high_ascii", "127 WORD DROP", "Should handle DEL delimiter", TEST_NORMAL, 0, 1},

            /* Count-prefixed string */
            {"count_format", "BL WORD C@ . CR", "Should have count byte", TEST_NORMAL, 0, 1},
            {"count_value", "BL WORD COUNT SWAP DROP . CR", "Should return count", TEST_NORMAL, 0, 1},

            /* Error cases */
            {"empty_stack", "WORD", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},

            /* Consecutive delimiters */
            {"multi_delim", "BL WORD DROP BL WORD DROP", "Should handle multiple parses", TEST_NORMAL, 0, 1},

            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        17
    },

    /* Base conversion and formatting tests */
    {
        "BASE_OUTPUT", {
            {"decimal", "10 BASE ! 255 . CR", "Should print: 255", TEST_NORMAL, 0, 1},
            {"hex", "16 BASE ! 255 . CR", "Should print: FF", TEST_NORMAL, 0, 1},
            {"octal", "8 BASE ! 64 . CR", "Should print: 100", TEST_NORMAL, 0, 1},
            {"binary", "5 2 BASE ! . CR", "Should print: 101", TEST_NORMAL, 0, 1},
            {
                "base_restore", "BASE @  DECIMAL  255  16 BASE !  .  BASE !  CR", "Should restore base", TEST_NORMAL, 0,
                1
            },
            {"reset_decimal", "DECIMAL", "Reset base to DECIMAL for subsequent tests", TEST_NORMAL, 0, 0},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        6
    },

    /* Output formatting combinations */
    {
        "FORMAT_COMBO", {
            {"number_space_number", "DECIMAL 42 . SPACE 43 . CR", "Should format: 42 43", TEST_NORMAL, 0, 1},
            {"multi_line", "DECIMAL 1 . CR 2 . CR 3 . CR", "Should print on separate lines", TEST_NORMAL, 0, 1},
            {"indented", "DECIMAL 5 SPACES 42 . CR", "Should indent number", TEST_NORMAL, 0, 1},
            {"table_format", "DECIMAL 1 . 3 SPACES 10 . 3 SPACES 100 . CR", "Should format table", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        4
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

/**
 * @brief Executes all I/O words test suites
 * @param vm Pointer to the FORTH virtual machine instance
 * @details Runs through all defined test suites for I/O words, logging results_run_01_2025_12_08
 *          and providing a summary of the test execution
 */
void run_io_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running I/O Words Tests (Module 9)...");

    for (int i = 0; io_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &io_word_suites[i]);
    }

    print_module_summary("I/O Words", 0, 0, 0, 0);
}