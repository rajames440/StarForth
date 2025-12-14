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

/* Mixed Arithmetic Words Test Suites - Module 7 */
static WordTestSuite mixed_arithmetic_word_suites[] = {
    {
        "*/", {
            // ( n1 n2 n3 -- n4 )  n1 * n2 / n3 (integer math)
            {"basic", "6 7 4 */ . CR", "Should print: 10 (6*7/4)", TEST_NORMAL, 0, 1},
            {"exact_division", "12 5 3 */ . CR", "Should print: 20 (12*5/3)", TEST_NORMAL, 0, 1},
            {"zero_multiply", "0 999 123 */ . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 5 1 */ . CR", "Should print: 210 (42*5/1)", TEST_NORMAL, 0, 1},
            {"truncation", "7 3 2 */ . CR", "Should print: 10 (21/2 truncated)", TEST_NORMAL, 0, 1},
            {"negative_multiply", "-6 7 4 */ . CR", "Should print: -10", TEST_NORMAL, 0, 1},
            {"negative_divisor", "6 7 -4 */ . CR", "Should print: -10", TEST_NORMAL, 0, 1},
            {"div_by_zero", "6 7 0 */", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "*/", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"two_items", "1 2 */", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        10
    },

    {
        "*/MOD", {
            // ( n1 n2 n3 -- rem quot )  (n1*n2)/n3 -> quot, rem
            {"basic", "17 3 5 */MOD . . CR", "Should print: 1 10 (51/5 = 10 rem 1)", TEST_NORMAL, 0, 1},
            {"exact_division", "15 4 3 */MOD . . CR", "Should print: 0 20 (60/3 = 20 rem 0)", TEST_NORMAL, 0, 1},
            {"zero_multiply", "0 999 123 */MOD . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 5 1 */MOD . . CR", "Should print: 0 210", TEST_NORMAL, 0, 1},
            {"remainder", "7 8 5 */MOD . . CR", "Should print: 1 11 (56/5 = 11 rem 1)", TEST_NORMAL, 0, 1},
            {"negative", "-7 3 4 */MOD . . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"div_by_zero", "6 7 0 */MOD", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "*/MOD", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        8
    },

    {
        "M+", {
            // ( d_lo d_hi n -- d_lo' d_hi' )  Add n to double d (low first, then high, then n)
            {"basic", "0 100 200 M+ . . CR", "Should print: 200 100", TEST_NORMAL, 0, 1},
            {"overflow", "0 2147483647 1 M+ . . CR", "Should handle overflow", TEST_EDGE_CASE, 0, 1},
            {"negative", "0 -100 50 M+ . . CR", "Should print: 50 -100", TEST_NORMAL, 0, 1},
            {"zero_add", "0 42 0 M+ . . CR", "Should print: 0 42", TEST_NORMAL, 0, 1},
            {"both_negative", "0 -100 -200 M+ . . CR", "Should print: -200 -100", TEST_NORMAL, 0, 1},
            {"empty_stack", "M+", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 M+", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    {
        "M*", {
            // ( n1 n2 -- d_lo d_hi )  signed multiply, returns double (lo first, hi second)
            {"basic", "123 456 M* . . CR", "Should print: 56088 0", TEST_NORMAL, 0, 1},
            {"by_zero", "999 0 M* . . CR", "Should print: 0 0", TEST_NORMAL, 0, 1},
            {"by_one", "42 1 M* . . CR", "Should print: 42 0", TEST_NORMAL, 0, 1},
            {"negative", "-123 456 M* . . CR", "Should print: -56088 -1", TEST_NORMAL, 0, 1},
            {"large_multiply", "65536 65536 M* . . CR", "Should handle large result", TEST_EDGE_CASE, 0, 1},
            {"both_negative", "-100 -200 M* . . CR", "Should print: 20000 0", TEST_NORMAL, 0, 1},
            {"empty_stack", "M*", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"one_item", "42 M*", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        8
    },

    {
        "M/MOD", {
            // ( d_lo d_hi n -- rem quot )  double/single signed division (lo first, then hi, then n)
            {"basic", "0 1000 100 M/MOD . . CR", "Should print: 0 10 (1000/100)", TEST_NORMAL, 0, 1},
            {"with_remainder", "0 1234 100 M/MOD . . CR", "Should handle remainder", TEST_NORMAL, 0, 1},
            {"double_dividend", "5001 0 10 M/MOD . . CR", "Should print: 1 500 (5001/10)", TEST_NORMAL, 0, 1},
            {"by_one", "0 42 1 M/MOD . . CR", "Should print: 0 42", TEST_NORMAL, 0, 1},
            {"negative_dividend", "-1 1000 100 M/MOD . . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"div_by_zero", "0 100 0 M/MOD", "Should cause division by zero", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "M/MOD", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        },
        7
    },

    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_mixed_arithmetic_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Mixed Arithmetic Words Tests (Module 7)...");

    for (int i = 0; mixed_arithmetic_word_suites[i].word_name != NULL; i++) {
        log_message(LOG_TEST, "▶ Testing module: %s", __FILE__);
        run_test_suite(vm, &mixed_arithmetic_word_suites[i]);
    }

    print_module_summary("Mixed Arithmetic Words", 0, 0, 0, 0);
}