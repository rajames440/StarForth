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

#ifndef STARFORTH_PROFILER_H
#define STARFORTH_PROFILER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Forward declarations */
typedef struct VM VM;
typedef struct DictEntry DictEntry;

/**
 * @brief Profiling detail levels
 *
 * Controls the amount of profiling information collected during execution.
 * Higher levels include all features from lower levels.
 */
typedef enum {
    PROFILE_DISABLED = 0, /**< Profiling disabled (zero overhead) */
    PROFILE_BASIC = 1, /**< Word execution frequency tracking (minimal overhead) */
    PROFILE_DETAILED = 2, /**< Basic + word execution timing (5-10% overhead) */
    PROFILE_VERBOSE = 3, /**< Detailed + stack/memory access patterns (15-20% overhead) */
    PROFILE_FULL = 4 /**< Reserved for future use (full instrumentation) */
} ProfileLevel;

/**
 * @brief Profiling measurement categories
 *
 * Different aspects of VM operation that can be profiled
 */
typedef enum {
    PROF_VM_INIT = 0, /**< VM initialization */
    PROF_VM_EXECUTE, /**< VM execution loop */
    PROF_VM_COMPILE, /**< Word compilation */
    PROF_DICT_LOOKUP, /**< Dictionary searches */
    PROF_DICT_ADD, /**< Dictionary additions */
    PROF_STACK_OPS, /**< Stack operations */
    PROF_MEMORY_ACCESS, /**< Memory access operations */
    PROF_WORD_EXEC, /**< Word execution */
    PROF_IO_OPS, /**< I/O operations */
    PROF_CONTROL_FLOW, /**< Control flow operations */
    PROF_CATEGORY_COUNT /**< Total number of categories */
} ProfileCategory;

/**
 * @brief Timer for measuring execution duration
 */
typedef struct {
    uint64_t start_time; /**< Start timestamp */
    uint64_t total_time; /**< Accumulated execution time */
    uint64_t call_count; /**< Number of measurements */
    uint64_t min_time; /**< Minimum recorded time */
    uint64_t max_time; /**< Maximum recorded time */
    const char *name; /**< Timer identifier */
} ProfileTimer;

/**
 * @brief Execution statistics for a single word
 */
typedef struct {
    const char *word_name; /**< Name of the profiled word */
    uint64_t total_time; /**< Total execution time */
    uint64_t call_count; /**< Number of executions */
    uint64_t avg_time; /**< Average execution time */
    uint64_t min_time; /**< Minimum execution time */
    uint64_t max_time; /**< Maximum execution time */
    double percentage; /**< Percentage of total runtime */
} WordProfile;

/* Memory access pattern tracking */
typedef struct {
    uint64_t reads;
    uint64_t writes;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t total_bytes_read;
    uint64_t total_bytes_written;
} MemoryProfile;

/* Main profiler state */
typedef struct {
    ProfileLevel level;
    uint64_t profiling_overhead;
    uint64_t total_execution_time;

    /* Category timers */
    ProfileTimer category_timers[PROF_CATEGORY_COUNT];

    /* Word execution tracking */
    WordProfile *word_profiles;
    size_t word_profile_count;
    size_t word_profile_capacity;

    /* Memory access tracking */
    MemoryProfile memory_stats;

    /* Hot path detection */
    uint64_t instruction_count;
    uint64_t branch_count;
    uint64_t loop_iterations;

    /* Performance counters */
    struct {
        uint64_t vm_cycles;
        uint64_t dict_lookups;
        uint64_t stack_operations;
        uint64_t memory_allocations;
        uint64_t compile_operations;
    } counters;
} Profiler;

/* Global profiler instance */
extern void *g_profiler;

/* Profiler lifecycle */
/**
 * @brief Initialize the profiling system
 * @param level Initial profiling detail level
 * @return 0 on success, non-zero on failure
 */
int profiler_init(ProfileLevel level);

/**
 * @brief Shutdown and cleanup the profiling system
 */
void profiler_shutdown(void);

/**
 * @brief Reset all profiling statistics
 */
void profiler_reset(void);

/**
 * @brief Change the current profiling detail level
 * @param level New profiling level to set
 */

void profiler_set_level(ProfileLevel level);

/* Timing functions */
uint64_t profiler_get_time_ns(void);

void profiler_start_timer(ProfileCategory category);

void profiler_end_timer(ProfileCategory category);

/* Word profiling */
/**
 * @brief Start timing a word execution (PROFILE_DETAILED+)
 * @param entry Dictionary entry of the word being executed
 */
void profiler_word_enter(const DictEntry *entry);

/**
 * @brief End timing a word execution (PROFILE_DETAILED+)
 * @param entry Dictionary entry of the word being executed
 */
void profiler_word_exit(const DictEntry *entry);

/**
 * @brief Track word execution frequency without timing (PROFILE_BASIC+)
 * @param entry Dictionary entry of the word being executed
 *
 * This is a lightweight alternative to profiler_word_enter/exit that only
 * increments call counters without timing overhead. Ideal for identifying
 * hot words (frequently executed) for optimization.
 */
void profiler_word_count(const DictEntry *entry);

/* Memory profiling */
void profiler_memory_read(size_t bytes);

void profiler_memory_write(size_t bytes);

void profiler_memory_alloc(size_t bytes);

/* Counter increments */
void profiler_inc_vm_cycles(uint64_t cycles);

void profiler_inc_dict_lookup(void);

void profiler_inc_stack_op(void);

void profiler_inc_branch(void);

void profiler_inc_loop_iter(void);

/* Analysis and reporting */
void profiler_generate_report(void);

void profiler_print_summary(void);

void profiler_print_word_analysis(void);

void profiler_print_memory_analysis(void);

void profiler_print_hotspots(void);

/* Performance optimization suggestions */
void profiler_analyze_performance(VM *vm);

void profiler_suggest_optimizations(void);

/* Profiler macros for easy integration */
#define PROFILE_ENABLED() (0)

#define PROFILE_START(category) ((void)0)
#define PROFILE_END(category) ((void)0)
#define PROFILE_WORD_ENTER(name) ((void)0)
#define PROFILE_WORD_EXIT(name) ((void)0)
#define PROFILE_MEMORY_READ(bytes) ((void)0)
#define PROFILE_MEMORY_WRITE(bytes) ((void)0)
#define PROFILE_INC_CYCLES(n) ((void)0)
#define PROFILE_INC_DICT_LOOKUP() ((void)0)
#define PROFILE_INC_STACK_OP() ((void)0)

/* Scoped profiling helper */
typedef struct {
    ProfileCategory category;
    const char *word_name;
} ProfileScope;

#endif /* STARFORTH_PROFILER_H */