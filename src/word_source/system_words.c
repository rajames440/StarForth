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

/* system_words.c - FORTH-79 System & Environment Words */
#include "include/system_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global system state */
static int system_running = 1;          /* System running flag */
static int forth_79_standard = 1;       /* FORTH-79 standard compliance */

/* Helper function to print word name */
static void print_word_name(const DictEntry *entry) {
    size_t i;
    size_t name_len;
    
    if (entry == NULL) {
        return;
    }
    
    name_len = entry->name_len;
    
    for (i = 0; i < name_len; i++) {
        printf("%c", entry->name[i]);
    }
}

/* Helper function to reset VM to initial state */
static void reset_vm_state(VM *vm, int cold_start) {
    /* Reset stacks */
    vm->dsp = -1;
    vm->rsp = -1;
    
    /* Clear error state */
    vm->error = 0;
    
    /* Set interpretation mode */
    vm->mode = MODE_INTERPRET;
    
    if (cold_start) {
        /* Cold start - reset dictionary to initial state */
        /* In a full implementation, this would restore the base dictionary */
        /* For now, just reset the HERE pointer to a safe state */
        if (vm->here > 1024) {  /* Keep some basic dictionary intact */
            vm->here = 1024;
        }
    }
    
    /* Reset system running flag */
    system_running = 1;
}

/* COLD ( -- )  Cold start system */
void word_cold(VM *vm) {
    printf("FORTH-79 Cold Start\n");
    reset_vm_state(vm, 1);  /* Cold start */
    printf("System initialized.\n");
}

/* WARM ( -- )  Warm start system */
void word_warm(VM *vm) {
    printf("FORTH-79 Warm Start\n");
    reset_vm_state(vm, 0);  /* Warm start */
    printf("System restarted.\n");
}

/* BYE ( -- )  Exit FORTH system */
void word_bye(VM *vm) {
    printf("Goodbye!\n");
    system_running = 0;
    /* In a real implementation, this might call exit() or set a flag */
    /* For testing purposes, we'll just set a flag */
}

/* SAVE-SYSTEM ( -- )  Save system image */
void word_save_system(VM *vm) {
    FILE *save_file;
    size_t bytes_written;
    
    printf("Saving system image...\n");
    
    /* Attempt to save basic VM state to a file */
    save_file = fopen("forth_system.img", "wb");
    if (save_file == NULL) {
        printf("Error: Cannot create system image file\n");
        vm->error = 1;
        return;
    }
    
    /* Save basic VM state */
    bytes_written = fwrite(&vm->here, sizeof(vm->here), 1, save_file);
    if (bytes_written != 1) {
        printf("Error: Failed to save system state\n");
        fclose(save_file);
        vm->error = 1;
        return;
    }
    
    /* Save memory (simplified - in real implementation would save dictionary) */
    bytes_written = fwrite(vm->memory, 1, vm->here, save_file);
    if (bytes_written != vm->here) {
        printf("Error: Failed to save memory image\n");
        fclose(save_file);
        vm->error = 1;
        return;
    }
    
    fclose(save_file);
    printf("System image saved successfully\n");
}

/* WORDS ( -- )  List words in current vocabulary */
void word_words(VM *vm) {
    DictEntry *entry;
    int word_count;
    int words_per_line;
    
    printf("Words in current vocabulary:\n");
    
    entry = vm->latest;
    word_count = 0;
    words_per_line = 0;
    
    while (entry != NULL) {
        print_word_name(entry);
        printf(" ");
        
        word_count++;
        words_per_line++;
        
        /* Print 8 words per line for readability */
        if (words_per_line >= 8) {
            printf("\n");
            words_per_line = 0;
        }
        
        entry = entry->link;
    }
    
    if (words_per_line > 0) {
        printf("\n");
    }
    
    printf("Total: %d words\n", word_count);
}

/* VLIST ( -- )  List all words in vocabulary */
void word_vlist(VM *vm) {
    DictEntry *entry;
    int word_count;
    
    printf("Complete vocabulary listing:\n");
    printf("Name                 Address    Flags\n");
    printf("-------------------- ---------- -----\n");
    
    entry = vm->latest;
    word_count = 0;
    
    while (entry != NULL) {
        /* Print name (padded to 20 characters) */
        print_word_name(entry);
        
        /* Pad name to 20 characters */
        if (entry->name_len < 20) {
            size_t padding;
            padding = 20 - entry->name_len;
            while (padding > 0) {
                printf(" ");
                padding--;
            }
        }
        
        /* Print address and flags */
        printf(" %p %02X\n", (void*)entry, entry->flags);
        
        word_count++;
        entry = entry->link;
    }
    
    printf("Total: %d words\n", word_count);
}

/* ?STACK ( -- )  Check stack depth */
void word_question_stack(VM *vm) {
    printf("Data stack depth: %d\n", vm->dsp + 1);
    printf("Return stack depth: %d\n", vm->rsp + 1);
    
    /* Check for stack underflow/overflow */
    if (vm->dsp < -1) {
        printf("WARNING: Data stack underflow!\n");
    } else if (vm->dsp >= STACK_SIZE - 1) {
        printf("WARNING: Data stack overflow!\n");
    }
    
    if (vm->rsp < -1) {
        printf("WARNING: Return stack underflow!\n");
    } else if (vm->rsp >= STACK_SIZE - 1) {
        printf("WARNING: Return stack overflow!\n");
    }
    
    /* Print stack contents if not empty */
    if (vm->dsp >= 0) {
        int i;
        printf("Data stack: ");
        for (i = 0; i <= vm->dsp; i++) {
            printf("%ld ", (long)vm->data_stack[i]);
        }
        printf("\n");
    }
}

/* PAGE ( -- )  Clear screen/page */
void word_page(VM *vm) {
    /* Clear screen using ANSI escape sequences */
    printf("\033[2J\033[H");
    fflush(stdout);
}

/* NOP ( -- )  No operation */
void word_nop(VM *vm) {
    /* Do nothing - this is intentional */
    (void)vm;  /* Suppress unused parameter warning */
}

/* 79-STANDARD ( -- )  Ensure FORTH-79 compliance */
void word_79_standard(VM *vm) {
    if (forth_79_standard) {
        printf("FORTH-79 Standard compliance: ACTIVE\n");
        printf("System conforms to FORTH-79 specification\n");
    } else {
        printf("FORTH-79 Standard compliance: INACTIVE\n");
        printf("System may have extensions or modifications\n");
    }
    
    /* Push compliance flag onto stack */
    vm_push(vm, forth_79_standard ? -1 : 0);
}

/* Helper function to check if system is still running */
int system_is_running(void) {
    return system_running;
}

/* Helper function to set FORTH-79 standard compliance */
void set_forth_79_compliance(int enabled) {
    forth_79_standard = enabled;
}

/* FORTH-79 System Word Registration and Testing */
void register_system_words(VM *vm) {
    log_message(LOG_INFO, "Registering system & environment words...");
    
    /* Register all system & environment words */
    register_word(vm, "COLD", word_cold);
    register_word(vm, "WARM", word_warm);
    register_word(vm, "BYE", word_bye);
    register_word(vm, "SAVE-SYSTEM", word_save_system);
    register_word(vm, "WORDS", word_words);
    register_word(vm, "VLIST", word_vlist);
    register_word(vm, "?STACK", word_question_stack);
    register_word(vm, "PAGE", word_page);
    register_word(vm, "NOP", word_nop);
    register_word(vm, "79-STANDARD", word_79_standard);

    log_message(LOG_INFO, "Note: WORDS, VLIST, PAGE, and SAVE-SYSTEM require manual verification");
}