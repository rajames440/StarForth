/*

                                 ***   StarForth   ***
  profiler.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/14/25, 9:05 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef STARFORTH_PROFILER_H
#define STARFORTH_PROFILER_H

#include "platform/starforth_platform.h"
#include <stdint.h>
#include <stddef.h>

/* Forward declarations */
typedef struct VM VM;
typedef struct DictEntry DictEntry;

/* Profiling levels */
typedef enum {
    PROFILE_DISABLED = 0,
    PROFILE_BASIC    = 1,    /* VM operations only */
    PROFILE_DETAILED = 2,    /* + word execution times */
    PROFILE_VERBOSE  = 3,    /* + memory access patterns */
    PROFILE_FULL     = 4     /* + instruction-level timing */
} ProfileLevel;

/* Profile categories */
typedef enum {
    PROF_VM_INIT = 0,
    PROF_VM_EXECUTE,
    PROF_VM_COMPILE,
    PROF_DICT_LOOKUP,
    PROF_DICT_ADD,
    PROF_STACK_OPS,
    PROF_MEMORY_ACCESS,
    PROF_WORD_EXEC,
    PROF_IO_OPS,
    PROF_CONTROL_FLOW,
    PROF_CATEGORY_COUNT
} ProfileCategory;

/* Timing measurement structure */
typedef struct {
    uint64_t start_time;
    uint64_t total_time;
    uint64_t call_count;
    uint64_t min_time;
    uint64_t max_time;
    const char *name;
} ProfileTimer;

/* Word execution profile */
typedef struct {
    const char *word_name;
    uint64_t total_time;
    uint64_t call_count;
    uint64_t avg_time;
    uint64_t min_time;
    uint64_t max_time;
    double percentage;
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
int profiler_init(ProfileLevel level);
void profiler_shutdown(void);
void profiler_reset(void);
void profiler_set_level(ProfileLevel level);

/* Timing functions */
uint64_t profiler_get_time_ns(void);
void profiler_start_timer(ProfileCategory category);
void profiler_end_timer(ProfileCategory category);

/* Word profiling */
void profiler_word_enter(const char *word_name);
void profiler_word_exit(const char *word_name);

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
/*
                                 ***   StarForth   ***
  profiler.h - Basic profiler definitions
 Last modified - 8/14/25
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
*/

#ifndef STARFORTH_PROFILER_H
#define STARFORTH_PROFILER_H

#include <stdint.h>

/* Profiling levels */
typedef enum {
    PROFILE_DISABLED = 0,
    PROFILE_BASIC    = 1,
    PROFILE_DETAILED = 2,
    PROFILE_VERBOSE  = 3,
    PROFILE_FULL     = 4
} ProfileLevel;

/* Forward declarations for VM */
typedef struct VM VM;

/* Basic profiler functions - stubs for now */
int profiler_init(ProfileLevel level);
void profiler_shutdown(void);
void profiler_generate_report(void);

/* Profiler macros - disabled for now to avoid build complexity */
#define PROFILE_START(category) ((void)0)
#define PROFILE_END(category) ((void)0)
#define PROFILE_WORD_ENTER(name) ((void)0)
#define PROFILE_WORD_EXIT(name) ((void)0)
#define PROFILE_INC_DICT_LOOKUP() ((void)0)
#define PROFILE_INC_STACK_OP() ((void)0)

#endif /* STARFORTH_PROFILER_H */
#define PROFILE_SCOPE_START(cat) \
    ProfileScope _prof_scope = {cat, NULL}; \
    if (PROFILE_ENABLED()) profiler_start_timer(cat);

#define PROFILE_SCOPE_END() \
    if (PROFILE_ENABLED()) profiler_end_timer(_prof_scope.category);

#define PROFILE_WORD_SCOPE(name) \
    ProfileScope _word_scope = {PROF_WORD_EXEC, name}; \
    if (PROFILE_ENABLED()) profiler_word_enter(name);

#define PROFILE_WORD_SCOPE_END() \
    if (PROFILE_ENABLED() && _word_scope.word_name) profiler_word_exit(_word_scope.word_name);

#endif /* STARFORTH_PROFILER_H */
