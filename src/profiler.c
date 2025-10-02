/**
 * @file profiler.c
 * @brief Performance profiling implementation for StarForth
 *
 * This module implements performance profiling functionality for the StarForth virtual machine.
 * It tracks execution statistics including:
 * - Word execution counts and timing
 * - Dictionary lookup frequency
 * - Stack operations
 * - Memory usage patterns
 *
 * The profiler supports multiple detail levels from basic to verbose profiling.
 */

#define _POSIX_C_SOURCE 199309L
#include "../include/profiler.h"
#include "../include/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Forward declare DictEntry */
typedef struct DictEntry DictEntry;

/** 
 * @brief Statistics for individual word execution
 */
typedef struct {
    const DictEntry *entry; /**< Pointer to dictionary entry */
    uint64_t call_count; /**< Number of times word was called */
    uint64_t total_time_ns; /**< Total execution time in nanoseconds */
    uint64_t min_time_ns; /**< Minimum execution time in nanoseconds */
    uint64_t max_time_ns; /**< Maximum execution time in nanoseconds */
} WordStats;

/* Global profiler state */
static struct {
    ProfileLevel level;
    int enabled;

    /* Word execution tracking */
    WordStats *word_stats;
    size_t word_count;
    size_t word_capacity;

    /* Current word timing */
    const char *current_word;
    uint64_t current_start_time;

    /* Global counters */
    uint64_t total_dict_lookups;
    uint64_t total_stack_ops;
    uint64_t total_memory_reads;
    uint64_t total_memory_writes;
} profiler_state = {0};

void *g_profiler = &profiler_state;

/**
 * @brief Get current time in nanoseconds
 * @return Current time in nanoseconds from monotonic clock
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;
}

/**
 * @brief Safely print a word name, handling non-null-terminated names
 * @param entry Dictionary entry whose name to print
 */
static void print_word_name_safe(const DictEntry *entry) {
    if (!entry) {
        printf("<null>");
        return;
    }
    for (size_t i = 0; i < entry->name_len && i < 32; i++) {
        putchar((unsigned char) entry->name[i]);
    }
}

/**
 * @brief Initialize the profiler subsystem
 * @param level Profiling detail level to enable
 * @return 0 on success, -1 on failure
 */
int profiler_init(ProfileLevel level) {
    if (level == PROFILE_DISABLED) {
        return 0;
    }

    profiler_state.level = level;
    profiler_state.enabled = 1;

    /* Allocate initial word stats array */
    profiler_state.word_capacity = 256;
    profiler_state.word_stats = calloc(profiler_state.word_capacity, sizeof(WordStats));
    if (!profiler_state.word_stats) {
        profiler_state.enabled = 0;
        return -1;
    }

    profiler_state.word_count = 0;
    profiler_state.total_dict_lookups = 0;
    profiler_state.total_stack_ops = 0;
    profiler_state.total_memory_reads = 0;
    profiler_state.total_memory_writes = 0;

    return 0;
}

/**
 * @brief Shutdown the profiler and free resources
 */
void profiler_shutdown(void) {
    if (profiler_state.word_stats) {
        free(profiler_state.word_stats);
        profiler_state.word_stats = NULL;
    }
    profiler_state.enabled = 0;
    profiler_state.word_count = 0;
}

/**
 * @brief Find or create statistics entry for a word
 * @param entry Dictionary entry to find/create stats for
 * @return Pointer to word stats or NULL if not found/created
 */
static WordStats *get_word_stats(const DictEntry *entry) {
    if (!profiler_state.enabled || !entry) return NULL;

    /* Search for existing entry - compare by pointer address since
       DictEntry pointers are stable for the lifetime of the word */
    for (size_t i = 0; i < profiler_state.word_count; i++) {
        if (profiler_state.word_stats[i].entry == entry) {
            return &profiler_state.word_stats[i];
        }
    }

    /* Create new entry if space available */
    if (profiler_state.word_count < profiler_state.word_capacity) {
        WordStats *stats = &profiler_state.word_stats[profiler_state.word_count++];
        stats->entry = entry;
        stats->call_count = 0;
        stats->total_time_ns = 0;
        stats->min_time_ns = UINT64_MAX;
        stats->max_time_ns = 0;
        return stats;
    }

    return NULL;
}

/**
 * @brief Record entry into a word for profiling
 * @param entry Dictionary entry being entered
 */
void profiler_word_enter(const DictEntry *entry) {
    if (!profiler_state.enabled || profiler_state.level < PROFILE_DETAILED) return;
    if (!entry) return;

    profiler_state.current_word = (const char *) entry;
    profiler_state.current_start_time = get_time_ns();
}

/**
 * @brief Record exit from a word for profiling
 * @param entry Dictionary entry being exited
 */
void profiler_word_exit(const DictEntry *entry) {
    if (!profiler_state.enabled || profiler_state.level < PROFILE_DETAILED) return;
    if (!entry || !profiler_state.current_word) return;

    uint64_t elapsed = get_time_ns() - profiler_state.current_start_time;

    WordStats *stats = get_word_stats(entry);
    if (stats) {
        stats->call_count++;
        stats->total_time_ns += elapsed;
        if (elapsed < stats->min_time_ns) stats->min_time_ns = elapsed;
        if (elapsed > stats->max_time_ns) stats->max_time_ns = elapsed;
    }

    profiler_state.current_word = NULL;
}

/* Increment counters */
void profiler_inc_dict_lookup(void) {
    if (profiler_state.enabled && profiler_state.level >= PROFILE_BASIC) {
        profiler_state.total_dict_lookups++;
    }
}

void profiler_inc_stack_op(void) {
    if (profiler_state.enabled && profiler_state.level >= PROFILE_VERBOSE) {
        profiler_state.total_stack_ops++;
    }
}

void profiler_memory_read(size_t bytes) {
    if (profiler_state.enabled && profiler_state.level >= PROFILE_VERBOSE) {
        profiler_state.total_memory_reads += bytes;
    }
}

void profiler_memory_write(size_t bytes) {
    if (profiler_state.enabled && profiler_state.level >= PROFILE_VERBOSE) {
        profiler_state.total_memory_writes += bytes;
    }
}

/**
 * @brief Compare two WordStats entries by total execution time
 * @param a First WordStats entry to compare
 * @param b Second WordStats entry to compare
 * @return -1, 0, or 1 for qsort ordering
 */
static int compare_by_time(const void *a, const void *b) {
    const WordStats *wa = (const WordStats *) a;
    const WordStats *wb = (const WordStats *) b;
    if (wb->total_time_ns > wa->total_time_ns) return 1;
    if (wb->total_time_ns < wa->total_time_ns) return -1;
    return 0;
}

/**
 * @brief Compare two WordStats entries by call count
 * @param a First WordStats entry to compare
 * @param b Second WordStats entry to compare
 * @return -1, 0, or 1 for qsort ordering
 */
static int compare_by_calls(const void *a, const void *b) {
    const WordStats *wa = (const WordStats *) a;
    const WordStats *wb = (const WordStats *) b;
    if (wb->call_count > wa->call_count) return 1;
    if (wb->call_count < wa->call_count) return -1;
    return 0;
}

/**
 * @brief Generate and print detailed profiling report
 * 
 * Generates a report including:
 * - Global statistics (dictionary lookups, stack ops, memory usage)
 * - Top words by execution time
 * - Most frequently called words
 */
void profiler_generate_report(void) {
    if (!profiler_state.enabled) {
        printf("Profiler not enabled\n");
        return;
    }

    printf("\n");
    printf("========================================\n");
    printf("     StarForth Profiler Report\n");
    printf("========================================\n\n");

    /* Global statistics */
    if (profiler_state.level >= PROFILE_BASIC) {
        printf("Global Statistics:\n");
        printf("  Dictionary lookups:  %lu\n", (unsigned long) profiler_state.total_dict_lookups);

        if (profiler_state.level >= PROFILE_VERBOSE) {
            printf("  Stack operations:    %lu\n", (unsigned long) profiler_state.total_stack_ops);
            printf("  Memory reads:        %lu bytes\n", (unsigned long) profiler_state.total_memory_reads);
            printf("  Memory writes:       %lu bytes\n", (unsigned long) profiler_state.total_memory_writes);
        }
        printf("\n");
    }

    /* Word execution statistics */
    if (profiler_state.level >= PROFILE_DETAILED && profiler_state.word_count > 0) {
        printf("Top Words by Execution Time:\n");
        printf("%-20s %10s %15s %12s %12s\n", "Word", "Calls", "Total (µs)", "Avg (ns)", "Max (ns)");
        printf("--------------------------------------------------------------------------------\n");

        /* Sort by total time */
        qsort(profiler_state.word_stats, profiler_state.word_count,
              sizeof(WordStats), compare_by_time);

        /* Show top 20 */
        size_t limit = profiler_state.word_count < 20 ? profiler_state.word_count : 20;
        for (size_t i = 0; i < limit; i++) {
            WordStats *s = &profiler_state.word_stats[i];
            if (s->call_count > 0) {
                uint64_t avg_ns = s->total_time_ns / s->call_count;
                print_word_name_safe(s->entry);
                /* Pad to 20 characters */
                size_t name_len = s->entry ? s->entry->name_len : 0;
                for (size_t j = name_len; j < 20; j++) putchar(' ');
                printf(" %10lu %15.2f %12lu %12lu\n",
                       (unsigned long) s->call_count,
                       s->total_time_ns / 1000.0,
                       (unsigned long) avg_ns,
                       (unsigned long) s->max_time_ns);
            }
        }
        printf("\n");

        /* Sort by call count and show hot words */
        printf("Most Frequently Called Words:\n");
        printf("%-20s %10s\n", "Word", "Calls");
        printf("----------------------------------------\n");

        qsort(profiler_state.word_stats, profiler_state.word_count,
              sizeof(WordStats), compare_by_calls);

        limit = profiler_state.word_count < 15 ? profiler_state.word_count : 15;
        for (size_t i = 0; i < limit; i++) {
            WordStats *s = &profiler_state.word_stats[i];
            if (s->call_count > 0) {
                print_word_name_safe(s->entry);
                /* Pad to 20 characters */
                size_t name_len = s->entry ? s->entry->name_len : 0;
                for (size_t j = name_len; j < 20; j++) putchar(' ');
                printf(" %10lu\n", (unsigned long) s->call_count);
            }
        }
        printf("\n");
    }

    printf("========================================\n\n");
}

/* Stub implementations for unused functions */
void profiler_reset(void) {
    if (!profiler_state.enabled) return;

    /* Reset all counters */
    for (size_t i = 0; i < profiler_state.word_count; i++) {
        profiler_state.word_stats[i].call_count = 0;
        profiler_state.word_stats[i].total_time_ns = 0;
        profiler_state.word_stats[i].min_time_ns = UINT64_MAX;
        profiler_state.word_stats[i].max_time_ns = 0;
    }

    profiler_state.total_dict_lookups = 0;
    profiler_state.total_stack_ops = 0;
    profiler_state.total_memory_reads = 0;
    profiler_state.total_memory_writes = 0;
}

void profiler_set_level(ProfileLevel level) {
    profiler_state.level = level;
}

uint64_t profiler_get_time_ns(void) {
    return get_time_ns();
}

void profiler_start_timer(ProfileCategory category) { (void) category; }
void profiler_end_timer(ProfileCategory category) { (void) category; }
void profiler_memory_alloc(size_t bytes) { (void) bytes; }
void profiler_inc_vm_cycles(uint64_t cycles) { (void) cycles; }

void profiler_inc_branch(void) {
}

void profiler_inc_loop_iter(void) {
}

void profiler_print_summary(void) { profiler_generate_report(); }

void profiler_print_word_analysis(void) {
}

void profiler_print_memory_analysis(void) {
}

void profiler_print_hotspots(void) {
}

void profiler_analyze_performance(VM *vm) { (void) vm; }

void profiler_suggest_optimizations(void) {
}