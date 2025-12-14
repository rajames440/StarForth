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

/*
                                  ***   StarForth   ***

  physics_pipelining_diagnostic_words.c - Diagnostic words for pipelining metrics

  Phase 1 diagnostics: words to display word-to-word transition metrics
  collected during normal execution.

*/

#include "../include/vm.h"
#include "../include/physics_pipelining_metrics.h"
#include "../include/word_registry.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * PIPELINING-SHOW-STATS ( <wordname> -- )
 *
 * Display transition metrics for a given word.
 * Typical usage:
 *   PIPELINING-SHOW-STATS EXIT       ( show where EXIT typically goes )
 *   PIPELINING-SHOW-STATS BENCH-DICT  ( show where BENCH-DICT transitions )
 *
 * Requires: word must exist in dictionary
 * Effect: Prints transition statistics to stdout
 */
static void forth_PIPELINING_SHOW_STATS(VM *vm) {
    if (!vm || vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    /* Pop word name from stack (as address + length) */
    cell_t len = vm_pop(vm);
    if (vm->error) return;

    cell_t addr = vm_pop(vm);
    if (vm->error) return;

    /* Convert VM address to C pointer */
    const char *word_name = (const char *)vm_ptr(vm, (vaddr_t)(uint64_t)addr);
    if (!word_name) {
        vm->error = 1;
        return;
    }

    /* Find the word */
    DictEntry *entry = vm_find_word(vm, word_name, (size_t)len);
    if (!entry) {
        printf("(word not found: ");
        fwrite(word_name, 1, (size_t)len, stdout);
        printf(")\n");
        return;
    }

    if (!entry->transition_metrics) {
        printf("(no transition metrics for: ");
        fwrite(word_name, 1, (size_t)len, stdout);
        printf(")\n");
        return;
    }

    /* Display word name */
    printf("\nTransition Metrics for: ");
    fwrite(word_name, 1, (size_t)len, stdout);
    printf("\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");

    /* Display metrics */
    WordTransitionMetrics *m = entry->transition_metrics;

    printf("Total Transitions Observed: %lu\n", m->total_transitions);
    printf("Prefetch Attempts:          %lu\n", m->prefetch_attempts);
    printf("Prefetch Hits:              %lu\n", m->prefetch_hits);
    printf("Prefetch Misses:            %lu\n", m->prefetch_misses);
    printf("Latency Saved (ns):         %ld\n", m->prefetch_latency_saved_q48 >> 16);
    printf("Misprediction Cost (ns):    %ld\n", m->misprediction_cost_q48 >> 16);

    if (m->total_transitions > 0) {
        double hit_rate = (100.0 * m->prefetch_hits) / m->prefetch_attempts;
        printf("Hit Rate:                   %.1f%%\n", (m->prefetch_attempts > 0) ? hit_rate : 0.0);
    }

    printf("Most Likely Next Word ID:   %u\n", m->most_likely_next_word_id);
    printf("Probability (Q48.16):       0x%lx\n", m->max_transition_probability_q48);
    printf("\n");
}

/**
 * PIPELINING-SHOW-TOP-TRANSITIONS ( <wordname> N -- )
 *
 * Display the top N words that typically follow a given word.
 * Useful for understanding hot execution paths.
 *
 * Stack: <wordname> N --
 * Example:
 *   " EXIT" 5 PIPELINING-SHOW-TOP-TRANSITIONS
 *   ( Show top 5 words that usually follow EXIT )
 */
static void forth_PIPELINING_SHOW_TOP_TRANSITIONS(VM *vm) {
    if (!vm || vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    /* Pop count (how many top transitions to show) */
    cell_t top_count = vm_pop(vm);
    if (vm->error) return;

    /* Pop word name */
    cell_t len = vm_pop(vm);
    if (vm->error) return;

    cell_t addr = vm_pop(vm);
    if (vm->error) return;

    if (top_count < 1 || top_count > 20) {
        printf("(invalid count: must be 1-20)\n");
        return;
    }

    const char *word_name = (const char *)vm_ptr(vm, (vaddr_t)(uint64_t)addr);
    if (!word_name) {
        vm->error = 1;
        return;
    }

    DictEntry *entry = vm_find_word(vm, word_name, (size_t)len);
    if (!entry || !entry->transition_metrics) {
        printf("(word not found or no metrics)\n");
        return;
    }

    WordTransitionMetrics *m = entry->transition_metrics;

    if (m->total_transitions == 0) {
        printf("(no transitions recorded yet)\n");
        return;
    }

    printf("\nTop %ld transitions from: ", top_count);
    fwrite(word_name, 1, (size_t)len, stdout);
    printf("\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    printf("Rank  Next Word ID  Count    Probability\n");
    printf("-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "\n");

    if (!m->transition_heat) {
        printf("(no transition data)\n");
        return;
    }

    /* Simple bubble sort to find top N (inefficient but diagnostic code) */
    typedef struct {
        uint32_t word_id;
        uint64_t count;
    } TransitionPair;

    TransitionPair *top = (TransitionPair *)malloc(sizeof(TransitionPair) * top_count);
    if (!top) return;

    memset(top, 0, sizeof(TransitionPair) * top_count);

    /* Scan all words and keep top N */
    for (uint32_t i = 0; i < DICTIONARY_SIZE; i++) {
        uint64_t count = m->transition_heat[i];
        if (count == 0) continue;

        /* Find position in top list */
        int pos = (int)top_count - 1;
        while (pos >= 0 && count > top[pos].count) {
            pos--;
        }
        pos++;

        if (pos < (int)top_count) {
            /* Shift entries to make room */
            for (int j = (int)top_count - 1; j > pos; j--) {
                top[j] = top[j - 1];
            }
            top[pos].word_id = i;
            top[pos].count = count;
        }
    }

    /* Display results_run_01_2025_12_08 */
    for (int i = 0; i < top_count; i++) {
        if (top[i].count == 0) break;

        double prob_pct = 100.0 * top[i].count / m->total_transitions;

        printf("%2d.   %4u         %5lu    %.1f%%\n",
               i + 1, top[i].word_id, top[i].count, prob_pct);
    }
    printf("\n");

    free(top);
}

/**
 * PIPELINING-RESET-ALL ( -- )
 *
 * Clear all transition metrics from the dictionary.
 * Useful for starting fresh measurements.
 */
static void forth_PIPELINING_RESET_ALL(VM *vm) {
    if (!vm) return;

    int count = 0;
    DictEntry *entry = vm->latest;

    while (entry) {
        if (entry->transition_metrics) {
            transition_metrics_reset(entry->transition_metrics);
            count++;
        }
        entry = entry->link;
    }

    printf("(reset %d word metrics)\n", count);
}

/**
 * PIPELINING-ENABLE ( -- )
 *
 * Enable pipelining metrics collection (sets ENABLE_PIPELINING flag).
 * Note: This is compile-time on/off for now; this word is a placeholder.
 */
static void forth_PIPELINING_ENABLE(VM *vm) {
    if (!vm) return;
    printf("(pipelining metrics: ");
#if ENABLE_PIPELINING
    printf("enabled at compile-time)\n");
#else
    printf("disabled - recompile with ENABLE_PIPELINING=1)\n");
#endif
}

/**
 * PIPELINING-STATS ( -- )
 *
 * Display aggregate pipelining statistics across entire dictionary.
 */
static void forth_PIPELINING_STATS(VM *vm) {
    if (!vm) return;

    printf("\n");
    printf("Pipelining Statistics (Aggregate)\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");

    uint64_t total_transitions = 0;
    uint64_t total_attempts = 0;
    uint64_t total_hits = 0;
    uint64_t total_misses = 0;
    int word_count = 0;
    int instrumented_count = 0;

    DictEntry *entry = vm->latest;

    while (entry) {
        word_count++;
        if (entry->transition_metrics) {
            instrumented_count++;
            WordTransitionMetrics *m = entry->transition_metrics;
            total_transitions += m->total_transitions;
            total_attempts += m->prefetch_attempts;
            total_hits += m->prefetch_hits;
            total_misses += m->prefetch_misses;
        }
        entry = entry->link;
    }

    printf("Total Words in Dictionary:     %d\n", word_count);
    printf("Words with Metrics:            %d (%.1f%%)\n",
           instrumented_count,
           (100.0 * instrumented_count) / ((word_count > 0) ? word_count : 1));
    printf("Total Transitions Observed:    %lu\n", total_transitions);
    printf("Total Prefetch Attempts:       %lu\n", total_attempts);
    printf("Total Prefetch Hits:           %lu\n", total_hits);
    printf("Total Prefetch Misses:         %lu\n", total_misses);

    if (total_attempts > 0) {
        double hit_rate = (100.0 * total_hits) / total_attempts;
        printf("Overall Hit Rate:              %.1f%%\n", hit_rate);
    }

    printf("\n");
}

/**
 * PIPELINING-ANALYZE-WORD ( <wordname> -- )
 *
 * Comprehensive analysis of a single word's transition pattern.
 * Shows raw data and interpretation hints.
 */
static void forth_PIPELINING_ANALYZE_WORD(VM *vm) {
    if (!vm || vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t len = vm_pop(vm);
    if (vm->error) return;

    cell_t addr = vm_pop(vm);
    if (vm->error) return;

    const char *word_name = (const char *)vm_ptr(vm, (vaddr_t)(uint64_t)addr);
    if (!word_name) {
        vm->error = 1;
        return;
    }

    DictEntry *entry = vm_find_word(vm, word_name, (size_t)len);
    if (!entry) {
        printf("(word not found)\n");
        return;
    }

    WordTransitionMetrics *m = entry->transition_metrics;
    if (!m || m->total_transitions == 0) {
        printf("(no transition data)\n");
        return;
    }

    printf("\nAnalysis: ");
    fwrite(word_name, 1, (size_t)len, stdout);
    printf("\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");

    /* Calculate entropy / distribution spread */
    double avg_count = (double)m->total_transitions / DICTIONARY_SIZE;
    double max_count = 0;
    for (uint32_t i = 0; i < DICTIONARY_SIZE; i++) {
        if (m->transition_heat && m->transition_heat[i] > max_count) {
            max_count = (double)m->transition_heat[i];
        }
    }

    printf("Execution Pattern:\n");
    if (max_count > 10 * avg_count) {
        printf("  → Highly predictable (strong preferred successor)\n");
    } else if (max_count > 5 * avg_count) {
        printf("  → Moderately predictable (clear pattern)\n");
    } else if (max_count > 2 * avg_count) {
        printf("  → Somewhat variable (multiple paths)\n");
    } else {
        printf("  → Highly variable (many different successors)\n");
    }

    printf("Max Transition Count:  %lu (%.1f%% of total)\n",
           (uint64_t)max_count,
           (100.0 * max_count) / m->total_transitions);

    printf("Word ID with Max:      %u\n", m->most_likely_next_word_id);
    printf("Predictability (IQR):  %.1f%%\n",
           (m->total_transitions > 0) ? (100.0 * m->max_transition_probability_q48 / (1LL << 16)) : 0.0);

    if (m->prefetch_attempts > 0) {
        printf("\nPrefetch Analysis:\n");
        printf("  Hit Rate: %.1f%% (%lu / %lu)\n",
               (100.0 * m->prefetch_hits) / m->prefetch_attempts,
               m->prefetch_hits,
               m->prefetch_attempts);

        int64_t net_ns = (m->prefetch_latency_saved_q48 - m->misprediction_cost_q48) >> 16;
        if (net_ns > 0) {
            printf("  Net Benefit: +%ld ns\n", net_ns);
        } else {
            printf("  Net Loss: %ld ns\n", net_ns);
        }
    } else {
        printf("\nNo prefetch attempts yet (Phase 3 implementation pending)\n");
    }

    printf("\n");
}

/* ============================================================================
 * Word Registry Entry
 * ============================================================================
 */

/**
 * Register all pipelining diagnostic words
 */
void register_physics_pipelining_diagnostic_words(VM *vm) {
    if (!vm) return;

    /* Declare words (forward declarations for compilation) */
    extern void forth_PIPELINING_SHOW_STATS(VM *vm);
    extern void forth_PIPELINING_SHOW_TOP_TRANSITIONS(VM *vm);
    extern void forth_PIPELINING_RESET_ALL(VM *vm);
    extern void forth_PIPELINING_ENABLE(VM *vm);
    extern void forth_PIPELINING_STATS(VM *vm);
    extern void forth_PIPELINING_ANALYZE_WORD(VM *vm);

    /* Register diagnostic words */
    vm_create_word(vm, "PIPELINING-SHOW-STATS", strlen("PIPELINING-SHOW-STATS"), forth_PIPELINING_SHOW_STATS);
    vm_create_word(vm, "PIPELINING-SHOW-TOP-TRANSITIONS", strlen("PIPELINING-SHOW-TOP-TRANSITIONS"), forth_PIPELINING_SHOW_TOP_TRANSITIONS);
    vm_create_word(vm, "PIPELINING-RESET-ALL", strlen("PIPELINING-RESET-ALL"), forth_PIPELINING_RESET_ALL);
    vm_create_word(vm, "PIPELINING-ENABLE", strlen("PIPELINING-ENABLE"), forth_PIPELINING_ENABLE);
    vm_create_word(vm, "PIPELINING-STATS", strlen("PIPELINING-STATS"), forth_PIPELINING_STATS);
    vm_create_word(vm, "PIPELINING-ANALYZE-WORD", strlen("PIPELINING-ANALYZE-WORD"), forth_PIPELINING_ANALYZE_WORD);
}
