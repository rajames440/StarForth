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
#include "../../include/block_subsystem.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Get entropy count for word at dictionary address
 *
 * Stack effect: ( addr -- n )
 * @param vm Pointer to the VM instance
 */
void starforth_word_entropy_fetch(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY@: data stack underflow");
        return;
    }

    cell_t addr = vm_pop(vm);
    DictEntry *entry = (DictEntry *) (uintptr_t) addr;

    if (!entry) {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY@: null dictionary entry");
        return;
    }

    vm_push(vm, entry->entropy);
    log_message(LOG_DEBUG, "ENTROPY@: word entropy = %ld", (long) entry->entropy);
}

/**
 * @brief Set entropy count for word at dictionary address
 *
 * Stack effect: ( n addr -- )
 * @param vm Pointer to the VM instance
 */
void starforth_word_entropy_store(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY!: data stack underflow");
        return;
    }

    cell_t addr = vm_pop(vm);
    cell_t value = vm_pop(vm);
    DictEntry *entry = (DictEntry *) (uintptr_t) addr;

    if (!entry) {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY!: null dictionary entry");
        return;
    }

    entry->entropy = value;
    log_message(LOG_DEBUG, "ENTROPY!: set word entropy to %ld", (long) value);
}

/**
 * @brief Display entropy statistics for all words in dictionary
 *
 * Stack effect: ( -- )
 * @param vm Pointer to the VM instance
 */
void starforth_word_word_entropy(VM *vm) {
    printf("Word Usage Statistics (Entropy Counts):\n");
    printf("=====================================\n");

    cell_t total_entropy = 0;
    int word_count = 0;

    for (DictEntry *entry = vm->latest; entry; entry = entry->link) {
        if (entry->entropy > 0) {
            printf("%.*s: %ld\n", (int) entry->name_len, entry->name, (long) entry->entropy);
            total_entropy += entry->entropy;
        }
        word_count++;
    }

    printf("-------------------------------------\n");
    printf("Total executions: %ld\n", (long) total_entropy);
    printf("Total words: %d\n", word_count);
    if (total_entropy > 0) {
        printf("Average executions per word: %ld\n", (long) (total_entropy / word_count));
    }
}

/**
 * @brief Reset all entropy counters to zero
 *
 * Stack effect: ( -- )
 * @param vm Pointer to the VM instance
 */
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

/**
 * @brief Display the N most frequently used words
 *
 * Stack effect: ( n -- )
 * @param vm Pointer to the VM instance
 */
void starforth_word_top_words(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "TOP-WORDS: data stack underflow");
        return;
    }

    cell_t n = vm_pop(vm);
    if (n <= 0) {
        printf("TOP-WORDS: invalid count %ld\n", (long) n);
        return;
    }

    /* Simple implementation: just show words with entropy > 0, sorted by entropy */
    printf("Top %ld most frequently used words:\n", (long) n);
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
    int display_count = (word_count < n) ? word_count : (int) n;
    for (int i = 0; i < display_count; i++) {
        DictEntry *entry = words_with_entropy[i];
        printf("%2d. %.*s: %ld\n", i + 1, (int) entry->name_len, entry->name, (long) entry->entropy);
    }
}

/**
 * @brief Shebang-style comment word for init.4th metadata
 *
 * Starts with "(- " and consumes input until closing ")"
 * Used for marking blocks that should be extracted to init.4th
 * Stack effect: ( -- )
 * @param vm Pointer to the VM instance
 */
void starforth_word_paren_dash(VM *vm) {
    /* Consume input until we find closing ")" */
    int depth = 1;

    while (vm->input_pos < vm->input_length && depth > 0) {
        char c = vm->input_buffer[vm->input_pos++];
        if (c == '(') {
            depth++;
        } else if (c == ')') {
            depth--;
        }
    }

    if (depth > 0) {
        log_message(LOG_WARN, "(- comment not terminated");
    }

    /* This is a comment - no stack effect, just consume input */
    log_message(LOG_DEBUG, "(- comment parsed (init.4th metadata marker)");
}

/**
 * @brief Initialize system from init.4th configuration file
 *
 * Reads ./conf/init.4th (or ROMFS in L4Re), parses Block headers,
 * copies blocks sequentially starting at block 1, then executes them.
 * Stack effect: ( -- )
 * @param vm Pointer to the VM instance
 */
void starforth_word_init(VM *vm) {
    log_message(LOG_INFO, "INIT: Starting system initialization from init.4th");

    /* Read init.4th file */
    char *file_content = NULL;
    size_t file_size = 0;

#ifdef L4RE_TARGET
    /* TODO: L4Re ROMFS implementation */
    log_message(LOG_ERROR, "INIT: L4Re ROMFS not yet implemented");
    vm->error = 1;
    vm->halted = 1;
    return;
#else
    /* Linux/POSIX: read from filesystem */
    const char *init_path = "./conf/init.4th";
    FILE *fp = fopen(init_path, "r");
    if (!fp) {
        log_message(LOG_ERROR, "INIT: Failed to open %s", init_path);
        vm->error = 1;
        vm->halted = 1;
        return;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    file_size = (size_t) ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Allocate buffer */
    file_content = (char *) malloc(file_size + 1);
    if (!file_content) {
        log_message(LOG_ERROR, "INIT: Failed to allocate memory for init.4th (%zu bytes)", file_size);
        fclose(fp);
        vm->error = 1;
        vm->halted = 1;
        return;
    }

    /* Read entire file */
    size_t bytes_read = fread(file_content, 1, file_size, fp);
    fclose(fp);

    if (bytes_read != file_size) {
        log_message(LOG_ERROR, "INIT: Failed to read init.4th (expected %zu, got %zu bytes)", file_size, bytes_read);
        free(file_content);
        vm->error = 1;
        vm->halted = 1;
        return;
    }
    file_content[file_size] = '\0';
    log_message(LOG_DEBUG, "INIT: Read %zu bytes from %s", file_size, init_path);
#endif

    /* First pass: build mapping of original block numbers to sequential numbers */
    typedef struct {
        int original;
        int sequential;
    } BlockMapping;

    BlockMapping block_map[256]; /* Support up to 256 init blocks */
    int block_count = 0;

    char *scan_start = file_content;
    for (size_t i = 0; i <= file_size; i++) {
        char c = (i < file_size) ? file_content[i] : '\n';

        if (c == '\n' || i == file_size) {
            size_t line_len = (size_t) (&file_content[i] - scan_start);

            /* Check if this line starts with "Block " */
            if (line_len >= 6 && strncmp(scan_start, "Block ", 6) == 0) {
                /* Parse the block number */
                int orig_block_num = 0;
                if (sscanf(scan_start + 6, "%d", &orig_block_num) == 1) {
                    if (block_count < 256) {
                        block_map[block_count].original = orig_block_num;
                        block_map[block_count].sequential = block_count + 1;
                        log_message(LOG_DEBUG, "INIT: Block mapping: %d -> %d",
                                    orig_block_num, block_count + 1);
                        block_count++;
                    }
                }
            }

            scan_start = &file_content[i + 1];
        }
    }

    /* Second pass: copy blocks with LOAD reference rewriting */
    int current_dest_block = 1;
    char *line_start = file_content;
    char *block_content_start = NULL;
    int in_block = 0;

    for (size_t i = 0; i <= file_size; i++) {
        char c = (i < file_size) ? file_content[i] : '\n';

        /* End of line or end of file */
        if (c == '\n' || i == file_size) {
            size_t line_len = (size_t) (&file_content[i] - line_start);

            /* Check if this line starts with "Block " */
            if (line_len >= 6 && strncmp(line_start, "Block ", 6) == 0) {
                /* If we were already in a block, write the previous block */
                if (in_block && block_content_start) {
                    size_t block_len = (size_t) (line_start - block_content_start);

                    /* Get buffer for destination block */
                    uint8_t *buf = blk_get_buffer((uint32_t) current_dest_block, 1);
                    if (!buf) {
                        log_message(LOG_ERROR, "INIT: Failed to get buffer for block %d", current_dest_block);
                        free(file_content);
                        vm->error = 1;
                        vm->halted = 1;
                        return;
                    }

                    /* Clear block and copy with LOAD rewriting */
                    memset(buf, 0, 1024);
                    size_t copy_len = (block_len > 1024) ? 1024 : block_len;

                    /* Simple approach: scan and replace "NNNN LOAD" patterns */
                    size_t src = 0, dst = 0;
                    while (src < copy_len && dst < 1024) {
                        /* Check for digit start */
                        if (block_content_start[src] >= '0' && block_content_start[src] <= '9') {
                            /* Parse number */
                            int num = 0;
                            size_t num_start = src;
                            while (src < copy_len && block_content_start[src] >= '0' &&
                                   block_content_start[src] <= '9') {
                                num = num * 10 + (block_content_start[src++] - '0');
                            }

                            /* Check for whitespace + LOAD */
                            size_t ws_start = src;
                            while (src < copy_len && (block_content_start[src] == ' ' ||
                                                      block_content_start[src] == '\t')) {
                                src++;
                            }

                            if (src + 4 <= copy_len && strncmp(&block_content_start[src], "LOAD", 4) == 0) {
                                /* Remap block number */
                                int new_num = num;
                                for (int m = 0; m < block_count; m++) {
                                    if (block_map[m].original == num) {
                                        new_num = block_map[m].sequential;
                                        log_message(LOG_INFO, "INIT: Rewrote %d LOAD -> %d LOAD", num, new_num);
                                        break;
                                    }
                                }

                                /* Write remapped number + whitespace + LOAD */
                                char rewrite[32];
                                int rewrite_len = snprintf(rewrite, sizeof(rewrite), "%d", new_num);
                                if (dst + rewrite_len < 1024) {
                                    memcpy(&buf[dst], rewrite, rewrite_len);
                                    dst += rewrite_len;
                                }

                                /* Copy whitespace */
                                size_t ws_len = src - ws_start;
                                if (dst + ws_len < 1024) {
                                    memcpy(&buf[dst], &block_content_start[ws_start], ws_len);
                                    dst += ws_len;
                                }

                                /* Copy LOAD */
                                if (dst + 4 < 1024) {
                                    memcpy(&buf[dst], "LOAD", 4);
                                    dst += 4;
                                }
                                src += 4;
                            } else {
                                /* Not LOAD, copy number as-is */
                                size_t num_len = src - num_start;
                                if (dst + num_len < 1024) {
                                    memcpy(&buf[dst], &block_content_start[num_start], num_len);
                                    dst += num_len;
                                }
                            }
                        } else {
                            /* Regular char */
                            buf[dst++] = block_content_start[src++];
                        }
                    }

                    log_message(LOG_INFO, "INIT: Copied block content to block %d (%zu bytes)",
                                current_dest_block, dst);
                    current_dest_block++;
                }

                /* Start new block - content begins on next line */
                in_block = 1;
                block_content_start = &file_content[i + 1];
            }

            line_start = &file_content[i + 1];
        }
    }

    /* Write final block if we were in one */
    if (in_block && block_content_start) {
        size_t block_len = (size_t) (&file_content[file_size] - block_content_start);

        uint8_t *buf = blk_get_buffer((uint32_t) current_dest_block, 1);
        if (!buf) {
            log_message(LOG_ERROR, "INIT: Failed to get buffer for final block %d", current_dest_block);
            free(file_content);
            vm->error = 1;
            vm->halted = 1;
            return;
        }

        memset(buf, 0, 1024);
        size_t copy_len = (block_len > 1024) ? 1024 : block_len;
        memcpy(buf, block_content_start, copy_len);

        log_message(LOG_INFO, "INIT: Copied block content to block %d (%zu bytes)",
                    current_dest_block, copy_len);
        current_dest_block++;
    }

    free(file_content);

    int total_blocks = current_dest_block - 1;
    log_message(LOG_INFO, "INIT: Loaded %d blocks from init.4th", total_blocks);

    /* Execute all blocks sequentially */
    log_message(LOG_INFO, "INIT: Executing initialization blocks...");
    for (int i = 1; i < current_dest_block; i++) {
        log_message(LOG_DEBUG, "INIT: Executing block %d (LOAD)", i);

        /* Push block number and execute LOAD */
        vm_push(vm, (cell_t) i);

        /* Find and execute LOAD word */
        DictEntry *load_word = vm_find_word(vm, "LOAD", 4);
        if (!load_word) {
            log_message(LOG_ERROR, "INIT: LOAD word not found in dictionary");
            vm->error = 1;
            vm->halted = 1;
            return;
        }

        load_word->func(vm);

        /* Check for errors after each block */
        if (vm->error) {
            log_message(LOG_ERROR, "INIT: Error executing block %d - system halted", i);
            vm->halted = 1;
            return;
        }
    }

    /* Switch back to FORTH vocabulary */
    log_message(LOG_INFO, "INIT: Switching to FORTH vocabulary");
    vm_interpret(vm, "FORTH DEFINITIONS");
    if (vm->error) {
        log_message(LOG_ERROR, "INIT: Failed to switch to FORTH vocabulary - system halted");
        vm->halted = 1;
        return;
    }

    /* Zero all init blocks to free them for userspace */
    log_message(LOG_INFO, "INIT: Zeroing %d init blocks for userspace use", total_blocks);
    for (int i = 1; i < current_dest_block; i++) {
        uint8_t *buf = blk_get_buffer((uint32_t) i, 1);
        if (buf) {
            memset(buf, 0, 1024);
            log_message(LOG_DEBUG, "INIT: Zeroed block %d", i);
        } else {
            log_message(LOG_WARN, "INIT: Failed to zero block %d (non-critical)", i);
        }
    }

    log_message(LOG_INFO, "INIT: System initialization complete - blocks freed, FORTH context active");
}

/**
 * @brief Register StarForth vocabulary words with the VM
 *
 * Registers all StarForth-specific words and creates the STARFORTH vocabulary
 * @param vm Pointer to the VM instance
 */
void register_starforth_words(VM *vm) {
    log_message(LOG_INFO, "Registering StarForth implementation vocabulary...");

    register_word(vm, "ENTROPY@", starforth_word_entropy_fetch);
    register_word(vm, "ENTROPY!", starforth_word_entropy_store);
    register_word(vm, "WORD-ENTROPY", starforth_word_word_entropy);
    register_word(vm, "RESET-ENTROPY", starforth_word_reset_entropy);
    register_word(vm, "TOP-WORDS", starforth_word_top_words);
    register_word(vm, "(-", starforth_word_paren_dash);
    register_word(vm, "INIT", starforth_word_init);

    /* Create the STARFORTH vocabulary */
    /* This would need vocabulary_word_vocabulary to be called */
    vm_interpret(vm, "VOCABULARY STARFORTH");
    vm_interpret(vm, "STARFORTH DEFINITIONS");

    /* Re-register the words in the STARFORTH vocabulary context */
    register_word(vm, "ENTROPY@", starforth_word_entropy_fetch);
    register_word(vm, "ENTROPY!", starforth_word_entropy_store);
    register_word(vm, "WORD-ENTROPY", starforth_word_word_entropy);
    register_word(vm, "RESET-ENTROPY", starforth_word_reset_entropy);
    register_word(vm, "TOP-WORDS", starforth_word_top_words);
    register_word(vm, "(-", starforth_word_paren_dash);
    register_word(vm, "INIT", starforth_word_init);

    /* Return to FORTH vocabulary */
    vm_interpret(vm, "FORTH DEFINITIONS");

    log_message(LOG_INFO, "StarForth implementation vocabulary registered");
}