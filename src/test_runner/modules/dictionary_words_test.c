/*
 * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
 *
 * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 * To the extent possible under law, the author(s) have dedicated all copyright and related
 * and neighboring rights to this software to the public domain worldwide.
 * This software is distributed without any warranty.
 *
 * See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
 */

#include "../include/test_runner.h"
#include "../include/test_common.h"

/* Dictionary Words Test Suites - Module 4 */
static WordTestSuite dictionary_word_suites[] = {
    {
        "HERE", {
            {"basic", "HERE HERE = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"after_comma", "HERE 42 , HERE SWAP - . CR", "Should print: 4", TEST_NORMAL, 0, 1},
            {"after_c_comma", "HERE 65 C, HERE SWAP - . CR", "Should print: 1", TEST_NORMAL, 0, 1},
            {"after_allot", "HERE 10 ALLOT HERE SWAP - . CR", "Should print: 10", TEST_NORMAL, 0, 1},
            {"stability", "HERE DUP HERE = . CR", "Should be stable", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 5
    },
    
    {
        "ALLOT", {
            {"basic", "HERE 10 ALLOT HERE SWAP - . CR", "Should print: 10", TEST_NORMAL, 0, 1},
            {"zero", "HERE 0 ALLOT HERE SWAP - . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"negative", "HERE -4 ALLOT HERE SWAP - . CR", "Should print: -4", TEST_NORMAL, 0, 1},
            {"large", "HERE 100 ALLOT HERE SWAP - . CR", "Should print: 100", TEST_NORMAL, 0, 1},
            {"after_use", "HERE 10 ALLOT 42 OVER ! @ . CR", "Should store and retrieve", TEST_NORMAL, 0, 1},
            {"empty_stack", "ALLOT", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        ",", {
            {"basic", "42 , HERE 4 - @ . CR", "Should compile 42", TEST_NORMAL, 0, 1},
            {"negative", "-999 , HERE 4 - @ . CR", "Should compile -999", TEST_NORMAL, 0, 1},
            {"zero", "0 , HERE 4 - @ . CR", "Should compile 0", TEST_NORMAL, 0, 1},
            {"max_int", "2147483647 , HERE 4 - @ . CR", "Should compile max int", TEST_EDGE_CASE, 0, 1},
            {"min_int", "-2147483648 , HERE 4 - @ . CR", "Should compile min int", TEST_EDGE_CASE, 0, 1},
            {"multiple", "10 , 20 , HERE 8 - @ . HERE 4 - @ . CR", "Should compile multiple", TEST_NORMAL, 0, 1},
            {"empty_stack", ",", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 7
    },
    
    {
        "C,", {
            {"basic", "65 C, HERE 1 - C@ . CR", "Should compile byte 65", TEST_NORMAL, 0, 1},
            {"zero", "0 C, HERE 1 - C@ . CR", "Should compile byte 0", TEST_NORMAL, 0, 1},
            {"high_byte", "255 C, HERE 1 - C@ . CR", "Should compile byte 255", TEST_NORMAL, 0, 1},
            {"truncation", "256 C, HERE 1 - C@ . CR", "Should truncate to 0", TEST_NORMAL, 0, 1},
            {"negative", "-1 C, HERE 1 - C@ . CR", "Should handle negative", TEST_NORMAL, 0, 1},
            {"multiple", "65 C, 66 C, HERE 2 - C@ . HERE 1 - C@ . CR", "Should compile multiple", TEST_NORMAL, 0, 1},
            {"empty_stack", "C,", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 7
    },
    
    {
        "2,", {
            {"basic", "12345 67890 2, HERE 8 - 2@ . . CR", "Should compile double", TEST_NORMAL, 0, 1},
            {"zero", "0 0 2, HERE 8 - 2@ . . CR", "Should compile zero double", TEST_NORMAL, 0, 1},
            {"negative", "-1000 -2000 2, HERE 8 - 2@ . . CR", "Should compile negative double", TEST_NORMAL, 0, 1},
            {"large", "2147483647 -1 2, HERE 8 - 2@ . . CR", "Should compile large double", TEST_EDGE_CASE, 0, 1},
            {"one_item", "42 2,", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "2,", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 6
    },
    
    {
        "PAD", {
            {"basic", "PAD PAD = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"different_from_here", "PAD HERE = . CR", "Should print: 0", TEST_NORMAL, 0, 1},
            {"stability", "PAD DUP PAD = . CR", "Should be stable", TEST_NORMAL, 0, 1},
            {"usable", "PAD 42 OVER ! @ . CR", "Should be usable memory", TEST_NORMAL, 0, 1},
            {"large_store", "PAD 100 0 DO I OVER I + C! LOOP DROP", "Should handle large writes", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 5
    },
    
    {
        "SP@", {
            {"basic", "SP@ SP@ = . CR", "Should print: 0 (different after push)", TEST_NORMAL, 0, 1},
            {"depth_effect", "42 SP@ SWAP DROP SP@ = . CR", "Should show stack effect", TEST_NORMAL, 0, 1},
            {"empty_stack", "DEPTH 0= SP@ AND . CR", "Should work on empty stack", TEST_NORMAL, 0, 1},
            {"multiple_items", "1 2 3 SP@ SWAP DROP SWAP DROP SWAP DROP SP@ = . CR", "Should track changes", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 4
    },
    
    {
        "SP!", {
            {"basic_restore", "SP@ 42 43 44 ROT SP! DEPTH . CR", "Should restore stack", TEST_NORMAL, 0, 1},
            {"empty_restore", "SP@ 1 2 3 4 5 ROT SP! DEPTH . CR", "Should restore to original", TEST_NORMAL, 0, 1},
            {"clear_stack", "1 2 3 4 5 SP@ 5 CELLS - SP! DEPTH . CR", "Should clear stack", TEST_NORMAL, 0, 1},
            {"invalid_addr", "0 SP!", "Should cause error", TEST_ERROR_CASE, 1, 1},
            {"empty_stack", "SP!", "Should cause stack underflow", TEST_ERROR_CASE, 1, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 5
    },
    
    {
        "LATEST", {
            {"basic", "LATEST LATEST = . CR", "Should print: -1", TEST_NORMAL, 0, 1},
            {"stability", "LATEST DUP LATEST = . CR", "Should be stable", TEST_NORMAL, 0, 1},
            {"fetch_test", "LATEST @ . CR", "Should fetch latest word", TEST_NORMAL, 0, 1},
            {"address_test", "LATEST 0<> . CR", "Should be valid address", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 4
    },
    
    /* Dictionary compilation interaction tests */
    {
        "COMPILATION", {
            {"here_comma_sequence", "HERE 10 , 20 , 30 , HERE SWAP - 12 = . CR", "Should advance HERE by 12", TEST_NORMAL, 0, 1},
            {"mixed_compilation", "HERE 42 , 65 C, 100 200 2, HERE SWAP - 13 = . CR", "Should advance correctly", TEST_NORMAL, 0, 1},
            {"allot_comma_combo", "HERE 10 ALLOT 99 , HERE SWAP - 14 = . CR", "Should combine allot and comma", TEST_NORMAL, 0, 1},
            {"pad_isolation", "PAD 999 OVER ! HERE 42 , PAD @ 999 = . CR", "PAD should be isolated", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 4
    },
    
    /* Stack pointer manipulation tests */
    {
        "STACK_MGMT", {
            {"sp_round_trip", "42 43 SP@ >R 44 45 R> SP! . . CR", "Should print: 43 42", TEST_NORMAL, 0, 1},
            {"depth_preservation", "1 2 3 DEPTH >R SP@ >R 4 5 6 R> SP! R> DEPTH = . CR", "Should preserve depth", TEST_NORMAL, 0, 1},
            {"stack_isolation", "SP@ 100 200 300 ROT SP! 42 SP@ SWAP SP! . CR", "Should isolate operations", TEST_NORMAL, 0, 1},
            {NULL, NULL, NULL, TEST_NORMAL, 0, 0}
        }, 3
    },
    
    /* End marker */
    {NULL, {{NULL, NULL, NULL, TEST_NORMAL, 0, 0}}, 0}
};

void run_dictionary_words_tests(VM *vm) {
    log_message(LOG_INFO, "Running Dictionary Words Tests (Module 4)...");
    
    for (int i = 0; dictionary_word_suites[i].word_name != NULL; i++) {
        run_test_suite(vm, &dictionary_word_suites[i]);
    }
    
    print_module_summary("Dictionary Words", 0, 0, 0, 0);
}
