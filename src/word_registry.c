/*
                                  ***   StarForth   ***

  word_registry.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.963-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/word_registry.c
 */

#include "../include/word_registry.h"
#include "../include/log.h"
#include <string.h>

/* Include all 18 FORTH-79 word module headers */
#include "word_source/include/stack_words.h"
#include "word_source/include/return_stack_words.h"
#include "word_source/include/memory_words.h"
#include "word_source/include/dictionary_words.h"
#include "word_source/include/arithmetic_words.h"
#include "word_source/include/double_words.h"
#include "word_source/include/mixed_arithmetic_words.h"
#include "word_source/include/logical_words.h"
#include "word_source/include/io_words.h"
#include "word_source/include/format_words.h"
#include "word_source/include/string_words.h"
#include "word_source/include/control_words.h"
#include "word_source/include/defining_words.h"
#include "word_source/include/dictionary_manipulation_words.h"
#include "word_source/include/vocabulary_words.h"
#include "word_source/include/block_words.h"
#include "word_source/include/editor_words.h"
#include "word_source/include/system_words.h"
#include "word_source/include/starforth_words.h"

/**
 * @brief Registers a single FORTH word in the virtual machine
 * @param vm Pointer to the virtual machine instance
 * @param name Name of the FORTH word to register
 * @param func Function pointer to the word's implementation
 */
void register_word(VM *vm, const char *name, word_func_t func) {
    DictEntry *entry = vm_create_word(vm, name, strlen(name), func);
    if (entry) {
        log_message(LOG_DEBUG, "Registered word: %s", name);
    } else {
        log_message(LOG_ERROR, "Failed to register word: %s", name);
    }
}

/**
 * @brief Registers all standard FORTH-79 words in the virtual machine
 * @param vm Pointer to the virtual machine instance
 * @details Registers all 18 FORTH-79 word modules in dependency order,
 *          starting with fundamental stack operations and building up to
 *          more complex functionality like control flow and editing.
 */
void register_forth79_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 Standard word set...");

    /* Register all 18 FORTH-79 word modules IN DEPENDENCY ORDER */
    register_stack_words(vm); /* Module 1: Stack Operations - FOUNDATION */
    register_return_stack_words(vm); /* Module 2: Return Stack Operations */
    register_memory_words(vm); /* Module 3: Memory Operations */
    register_arithmetic_words(vm); /* Module 4: Arithmetic Operations */
    register_logical_words(vm); /* Module 5: Logical & Comparison */
    register_mixed_arithmetic_words(vm); /* Module 6: Mixed Arithmetic - needs arithmetic + stack */
    register_double_words(vm); /* Module 7: Double Number Arithmetic */
    register_format_words(vm); /* Module 8: Formatting & Conversion */
    register_string_words(vm); /* Module 9: String & Text Processing */
    register_io_words(vm); /* Module 10: I/O & Terminal */
    register_block_words(vm); /* Module 11: Block & Mass Storage */
    register_dictionary_words(vm); /* Module 12: Dictionary & Compilation */
    register_dictionary_manipulation_words(vm); /* Module 13: Dictionary Manipulation */
    register_vocabulary_words(vm); /* Module 14: Vocabulary System */
    register_system_words(vm); /* Module 15: System & Environment */
    register_editor_words(vm); /* Module 16: Line Editor */
    register_defining_words(vm); /* Module 17: Defining Words */
    register_control_words(vm); /* Module 18: Control Flow */
    register_starforth_words(vm); /* Module 19: StarForth Implementation Extensions */

    log_message(LOG_INFO, "FORTH-79 Standard word set registration complete");
}