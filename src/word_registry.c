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
#include "word_source/include/physics_benchmark_words.h"
#include "word_source/include/physics_pipelining_diagnostic_words.h"
#include "word_source/include/physics_freeze_words.h"
#include "word_source/include/dictionary_heat_diagnostic_words.h"
#include "word_source/include/time_words.h"
#ifdef __STARKERNEL__
#include "starkernel/console.h"
#include "starkernel/hal/hal.h"
#include "starkernel/vm/arena.h"
#include "starkernel/vm/vm_internal.h"
#define REGISTER_CHECK_ARENA(vm, tag) sk_vm_arena_assert_guards(tag)
static uint32_t register_word_counter = 0;
#else
#define REGISTER_CHECK_ARENA(vm, tag) ((void)0)
#endif

/**
 * @brief Registers a single FORTH word in the virtual machine
 * @param vm Pointer to the virtual machine instance
 * @param name Name of the FORTH word to register
 * @param func Function pointer to the word's implementation
 */
void register_word(VM *vm, const char *name, word_func_t func) {
#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
    /* Log address of the function we are registering */
    if ((uintptr_t)func < 0x1000000) {
        console_puts("[REG] SUSPICIOUS func ptr for ");
        console_puts(name);
        console_puts(": ");
        char buf[19];
        uint64_t v = (uint64_t)(uintptr_t)func;
        buf[0] = '0'; buf[1] = 'x'; buf[18] = '\0';
        for (int i = 0; i < 16; ++i) {
            uint8_t nibble = (uint8_t)((v >> ((15 - i) * 4)) & 0xF);
            buf[i + 2] = (nibble < 10) ? (char)('0' + nibble) : (char)('a' + nibble - 10);
        }
        console_println(buf);
    }
#endif
#endif
    DictEntry *entry = vm_create_word(vm, name, strlen(name), func);
    if (entry) {
        log_message(LOG_DEBUG, "Registered word: %s", name);
#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
        console_puts("[REG] ");
        console_println(name);
#endif
        register_word_counter++;
        if ((register_word_counter & 0xF) == 0) {
            REGISTER_CHECK_ARENA(vm, name);
        }
#endif
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
#ifdef __STARKERNEL__
    REGISTER_CHECK_ARENA(vm, "pre-register_arithmetic_words");
#endif
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
    register_physics_benchmark_words(vm); /* Module 20: Physics Benchmark Words */
    register_physics_pipelining_diagnostic_words(vm); /* Module 21: Pipelining Diagnostics */
    register_physics_freeze_words(vm); /* Module 22: Phase 2 Freeze/Decay Control Words */
    register_dictionary_heat_diagnostic_words(vm); /* Module 23: Dictionary Heat Optimization */
    register_time_words(vm); /* Module 24: M5 Time & Heartbeat Words */

#ifdef __STARKERNEL__
#if SK_PARITY_DEBUG
    console_println("[REG] register_forth79_words complete");
#endif
#endif
    log_message(LOG_INFO, "FORTH-79 Standard word set registration complete");
#ifdef __STARKERNEL__
    REGISTER_CHECK_ARENA(vm, "post-register_forth79_words");
#endif
}
