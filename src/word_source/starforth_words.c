/*

                                 ***   StarForth   ***
  starforth_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/15/25, 9:44 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "include/starforth_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"
#include <string.h>
#include <stdio.h>

/* ENTROPY@ ( addr -- n )  Get entropy count for word at dictionary address */
void starforth_word_entropy_fetch(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY@: data stack underflow");
        return;
    }

    cell_t addr = vm_pop(vm);
    DictEntry *entry = (DictEntry*)(uintptr_t)addr;

    if (!entry) {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY@: null dictionary entry");
        return;
    }

    vm_push(vm, entry->entropy);
    log_message(LOG_DEBUG, "ENTROPY@: word entropy = %ld", (long)entry->entropy);
}

/* ENTROPY! ( n addr -- )  Set entropy count for word at dictionary address */
void starforth_word_entropy_store(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY!: data stack underflow");
        return;
    }

    cell_t addr = vm_pop(vm);
    cell_t value = vm_pop(vm);
    DictEntry *entry = (DictEntry*)(uintptr_t)addr;

    if (!entry) {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY!: null dictionary entry");
        return;
    }

    entry->entropy = value;
    log_message(LOG_DEBUG, "ENTROPY!: set word entropy to %ld", (long)value);
}

/* WORD-ENTROPY ( -- )  Display entropy stats for all words in dictionary */
void starforth_word_word_entropy(VM *vm) {
    printf("Word Usage Statistics (Entropy Counts):\n");
    printf("=====================================\n");

    cell_t total_entropy = 0;
    int word_count = 0;

    for (DictEntry *entry = vm->latest; entry; entry = entry->link) {
        if (entry->entropy > 0) {
            printf("%.*s: %ld\n", (int)entry->name_len, entry->name, (long)entry->entropy);
            total_entropy += entry->entropy;
        }
        word_count++;
    }

    printf("-------------------------------------\n");
    printf("Total executions: %ld\n", (long)total_entropy);
    printf("Total words: %d\n", word_count);
    if (total_entropy > 0) {
        printf("Average executions per word: %ld\n", (long)(total_entropy / word_count));
    }
}

/* RESET-ENTROPY ( -- )  Reset all entropy counters to zero */
void starforth_word_reset_entropy(VM *vm) {
    int reset_count = 0;

    for (DictEntry *entry = vm->latest; entry; entry = entry->link) {
        if (entry->entropy > 0) {
            entry->entropy = 0;
            reset_count++;
        }
    }

    printf("Reset entropy counters for %d words\n", reset_count);
    log_message(LOG_INFO, "RESET-ENTROPY: cleared %d word counters", reset_count);
}

/* TOP-WORDS ( n -- )  Display the N most frequently used words */
void starforth_word_top_words(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "TOP-WORDS: data stack underflow");
        return;
    }

    cell_t n = vm_pop(vm);
    if (n <= 0) {
        printf("TOP-WORDS: invalid count %ld\n", (long)n);
        return;
    }

    /* Simple implementation: just show words with entropy > 0, sorted by entropy */
    printf("Top %ld most frequently used words:\n", (long)n);
    printf("==================================\n");

    /* Collect all words with entropy > 0 */
    DictEntry *words_with_entropy[1000]; /* Reasonable limit for now */
    int word_count = 0;

    for (DictEntry *entry = vm->latest; entry && word_count < 1000; entry = entry->link) {
        if (entry->entropy > 0) {
            words_with_entropy[word_count++] = entry;
        }
    }

    /* Simple bubble sort by entropy (descending) */
    for (int i = 0; i < word_count - 1; i++) {
        for (int j = 0; j < word_count - i - 1; j++) {
            if (words_with_entropy[j]->entropy < words_with_entropy[j + 1]->entropy) {
                DictEntry *temp = words_with_entropy[j];
                words_with_entropy[j] = words_with_entropy[j + 1];
                words_with_entropy[j + 1] = temp;
            }
        }
    }

    /* Display top N */
    int display_count = (word_count < n) ? word_count : (int)n;
    for (int i = 0; i < display_count; i++) {
        DictEntry *entry = words_with_entropy[i];
        printf("%2d. %.*s: %ld\n", i + 1, (int)entry->name_len, entry->name, (long)entry->entropy);
    }
}

/* Registration function for StarForth vocabulary words */
void register_starforth_words(VM *vm) {
    log_message(LOG_INFO, "Registering StarForth implementation vocabulary...");

    register_word(vm, "ENTROPY@",     starforth_word_entropy_fetch);
    register_word(vm, "ENTROPY!",     starforth_word_entropy_store);
    register_word(vm, "WORD-ENTROPY", starforth_word_word_entropy);
    register_word(vm, "RESET-ENTROPY", starforth_word_reset_entropy);
    register_word(vm, "TOP-WORDS",    starforth_word_top_words);

    /* Create the STARFORTH vocabulary */
    /* This would need vocabulary_word_vocabulary to be called */
    vm_interpret(vm, "VOCABULARY STARFORTH");
    vm_interpret(vm, "STARFORTH DEFINITIONS");

    /* Re-register the words in the STARFORTH vocabulary context */
    register_word(vm, "ENTROPY@",     starforth_word_entropy_fetch);
    register_word(vm, "ENTROPY!",     starforth_word_entropy_store);
    register_word(vm, "WORD-ENTROPY", starforth_word_word_entropy);
    register_word(vm, "RESET-ENTROPY", starforth_word_reset_entropy);
    register_word(vm, "TOP-WORDS",    starforth_word_top_words);

    /* Return to FORTH vocabulary */
    vm_interpret(vm, "FORTH DEFINITIONS");

    log_message(LOG_INFO, "StarForth implementation vocabulary registered");
}
