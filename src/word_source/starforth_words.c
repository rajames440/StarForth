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

#include "include/starforth_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"
#include "../../include/version.h"
#include "../../include/physics_metadata.h"
#include "../../include/platform_time.h"
#include "../../include/profiler.h"
#include "../../include/block_subsystem.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __unix__
#include <time.h>
#endif

/* ============================================================================
 * PRNG State - Linear Congruential Generator (Numerical Recipes constants)
 * ============================================================================ */
static uint64_t g_prng_state = 1;

/**
 * @brief Get execution heat count for word at dictionary address
 *
 * Stack effect: ( addr -- n )
 * Returns the execution frequency (execution_heat counter) for a word.
 * Note: Exposed as ENTROPY@ for FORTH compatibility, but measures execution heat.
 * @param vm Pointer to the VM instance
 */
/**
 * @brief Validate that an address is a valid DictEntry pointer
 * @param vm The VM instance
 * @param candidate The address to validate
 * @return 1 if valid, 0 if not
 */
static int is_valid_dict_entry(VM* vm, DictEntry* candidate)
{
    if (!candidate) return 0;

    /* Walk dictionary to verify this is a real entry - must hold dict_lock */
    int found = 0;
    sf_mutex_lock(&vm->dict_lock);
    for (DictEntry* e = vm->latest; e != NULL; e = e->link)
    {
        if (e == candidate) { found = 1; break; }
    }
    sf_mutex_unlock(&vm->dict_lock);
    return found;
}

void starforth_word_execution_heat_fetch(VM* vm)
{
    if (vm->dsp < 0)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY@: data stack underflow");
        return;
    }

    cell_t addr = vm_pop(vm);
    DictEntry* entry = (DictEntry*)(uintptr_t)addr;

    if (!entry)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY@: null dictionary entry");
        return;
    }

    /* Guardrail: Validate the pointer is actually a DictEntry */
    if (!is_valid_dict_entry(vm, entry))
    {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY@: invalid dictionary entry address %p (not in dictionary)", (void*)entry);
        return;
    }

    vm_push(vm, entry->execution_heat);
    log_message(LOG_DEBUG, "ENTROPY@: word execution heat = %ld", (long)entry->execution_heat);
}

/**
 * @brief Set execution heat count for word at dictionary address
 *
 * Stack effect: ( n addr -- )
 * Note: Exposed as ENTROPY! for FORTH compatibility, but sets execution heat.
 * @param vm Pointer to the VM instance
 */
void starforth_word_execution_heat_store(VM* vm)
{
    if (vm->dsp < 1)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY!: data stack underflow");
        return;
    }

    cell_t addr = vm_pop(vm);
    cell_t value = vm_pop(vm);
    DictEntry* entry = (DictEntry*)(uintptr_t)addr;

    if (!entry)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY!: null dictionary entry");
        return;
    }

    /* Guardrail: Validate the pointer is actually a DictEntry */
    if (!is_valid_dict_entry(vm, entry))
    {
        vm->error = 1;
        log_message(LOG_ERROR, "ENTROPY!: invalid dictionary entry address %p (not in dictionary)", (void*)entry);
        return;
    }

    entry->execution_heat = value;
    log_message(LOG_DEBUG, "ENTROPY!: set word execution heat to %ld", (long)value);
}

/**
 * @brief Display execution heat statistics for all words in dictionary
 *
 * Stack effect: ( -- )
 * Shows execution frequency (execution_heat) for each word.
 * @param vm Pointer to the VM instance
 */
void starforth_word_word_execution_heat(VM* vm)
{
    printf("Word Usage Statistics (Execution Heat Counts):\n");
    printf("=============================================\n");

    cell_t total_heat = 0;
    int word_count = 0;

    for (DictEntry* entry = vm->latest; entry; entry = entry->link)
    {
        if (entry->execution_heat > 0)
        {
            printf("%.*s: %ld\n", (int)entry->name_len, entry->name, (long)entry->execution_heat);
            total_heat += entry->execution_heat;
        }
        word_count++;
    }

    printf("-------------------------------------\n");
    printf("Total executions: %ld\n", (long)total_heat);
    printf("Total words: %d\n", word_count);
    if (total_heat > 0)
    {
        printf("Average executions per word: %ld\n", (long)(total_heat / word_count));
    }
}

/**
 * @brief Reset all execution heat counters to zero
 *
 * Stack effect: ( -- )
 * @param vm Pointer to the VM instance
 */
void starforth_word_reset_execution_heat(VM* vm)
{
    int reset_count = 0;

    for (DictEntry* entry = vm->latest; entry; entry = entry->link)
    {
        if (entry->execution_heat > 0)
        {
            entry->execution_heat = 0;
            entry->physics.temperature_q8 = 0;
            entry->physics.avg_latency_ns = 0;
            entry->physics.last_active_ns = 0;
            reset_count++;
        }
    }
}

/**
 * @brief Display the N most frequently used words
 *
 * Stack effect: ( n -- )
 * @param vm Pointer to the VM instance
 */
void starforth_word_top_words(VM* vm)
{
    if (vm->dsp < 0)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "TOP-WORDS: data stack underflow");
        return;
    }

    cell_t n = vm_pop(vm);
    if (n <= 0)
    {
        printf("TOP-WORDS: invalid count %ld\n", (long)n);
        return;
    }

    /* Simple implementation: just show words with execution_heat > 0, sorted by execution_heat */
    printf("Top %ld most frequently used words:\n", (long)n);
    printf("==================================\n");

    /* Collect all words with execution_heat > 0 */
    DictEntry* words_with_heat[1000]; /* Reasonable limit for now */
    int word_count = 0;

    for (DictEntry* entry = vm->latest; entry && word_count < 1000; entry = entry->link)
    {
        if (entry->execution_heat > 0)
        {
            words_with_heat[word_count++] = entry;
        }
    }

    /* Simple bubble sort by execution_heat (descending) */
    for (int i = 0; i < word_count - 1; i++)
    {
        for (int j = 0; j < word_count - i - 1; j++)
        {
            if (words_with_heat[j]->execution_heat < words_with_heat[j + 1]->execution_heat)
            {
                DictEntry* temp = words_with_heat[j];
                words_with_heat[j] = words_with_heat[j + 1];
                words_with_heat[j + 1] = temp;
            }
        }
    }

    /* Display top N */
    int display_count = (word_count < n) ? word_count : (int)n;
    for (int i = 0; i < display_count; i++)
    {
        DictEntry* entry = words_with_heat[i];
        printf("%2d. %.*s: %ld\n", i + 1, (int)entry->name_len, entry->name, (long)entry->execution_heat);
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
void starforth_word_paren_dash(VM* vm)
{
    /* Consume input until we find closing ")" */
    int depth = 1;

    while (vm->input_pos < vm->input_length && depth > 0)
    {
        char c = vm->input_buffer[vm->input_pos++];
        if (c == '(')
        {
            depth++;
        }
        else if (c == ')')
        {
            depth--;
        }
    }

    if (depth > 0)
    {
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
void starforth_word_init(VM* vm)
{
    log_message(LOG_INFO, "INIT: Starting system initialization from init.4th");

    /* Read init.4th file */
    char* file_content = NULL;
    size_t file_size = 0;

#ifdef L4RE_TARGET
    /* TODO: L4Re ROMFS implementation */
    log_message(LOG_ERROR, "INIT: L4Re ROMFS not yet implemented");
    vm->error = 1;
    vm->halted = 1;
    return;
#else
    /* Linux/POSIX: read from filesystem */
    const char* init_path = "./conf/init.4th";
    FILE* fp = fopen(init_path, "r");
    if (!fp)
    {
        log_message(LOG_ERROR, "INIT: Failed to open %s", init_path);
        vm->error = 1;
        vm->halted = 1;
        return;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    file_size = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Allocate buffer */
    file_content = (char*)malloc(file_size + 1);
    if (!file_content)
    {
        log_message(LOG_ERROR, "INIT: Failed to allocate memory for init.4th (%zu bytes)", file_size);
        fclose(fp);
        vm->error = 1;
        vm->halted = 1;
        return;
    }

    /* Read entire file */
    size_t bytes_read = fread(file_content, 1, file_size, fp);
    fclose(fp);

    if (bytes_read != file_size)
    {
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
    typedef struct
    {
        int original;
        int sequential;
    } BlockMapping;

    BlockMapping block_map[256]; /* Support up to 256 init blocks */
    int block_count = 0;

    char* scan_start = file_content;
    for (size_t i = 0; i <= file_size; i++)
    {
        char c = (i < file_size) ? file_content[i] : '\n';

        if (c == '\n' || i == file_size)
        {
            size_t line_len = (size_t)(&file_content[i] - scan_start);

            /* Check if this line starts with "Block " */
            if (line_len >= 6 && strncmp(scan_start, "Block ", 6) == 0)
            {
                /* Parse the block number */
                int orig_block_num = 0;
                if (sscanf(scan_start + 6, "%d", &orig_block_num) == 1)
                {
                    if (block_count < 256)
                    {
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
    char* line_start = file_content;
    char* block_content_start = NULL;
    int in_block = 0;

    for (size_t i = 0; i <= file_size; i++)
    {
        char c = (i < file_size) ? file_content[i] : '\n';

        /* End of line or end of file */
        if (c == '\n' || i == file_size)
        {
            size_t line_len = (size_t)(&file_content[i] - line_start);

            /* Check if this line starts with "Block " */
            if (line_len >= 6 && strncmp(line_start, "Block ", 6) == 0)
            {
                /* If we were already in a block, write the previous block */
                if (in_block && block_content_start)
                {
                    size_t block_len = (size_t)(line_start - block_content_start);

                    /* Get buffer for destination block */
                    uint8_t* buf = blk_get_buffer((uint32_t)current_dest_block, 1);
                    if (!buf)
                    {
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
                    while (src < copy_len && dst < 1024)
                    {
                        /* Check for digit start */
                        if (block_content_start[src] >= '0' && block_content_start[src] <= '9')
                        {
                            /* Parse number */
                            int num = 0;
                            size_t num_start = src;
                            while (src < copy_len && block_content_start[src] >= '0' &&
                                block_content_start[src] <= '9')
                            {
                                num = num * 10 + (block_content_start[src++] - '0');
                            }

                            /* Check for whitespace + LOAD */
                            size_t ws_start = src;
                            while (src < copy_len && (block_content_start[src] == ' ' ||
                                block_content_start[src] == '\t'))
                            {
                                src++;
                            }

                            if (src + 4 <= copy_len && strncmp(&block_content_start[src], "LOAD", 4) == 0)
                            {
                                /* Remap block number */
                                int new_num = num;
                                for (int m = 0; m < block_count; m++)
                                {
                                    if (block_map[m].original == num)
                                    {
                                        new_num = block_map[m].sequential;
                                        log_message(LOG_INFO, "INIT: Rewrote %d LOAD -> %d LOAD", num, new_num);
                                        break;
                                    }
                                }

                                /* Write remapped number + whitespace + LOAD */
                                char rewrite[32];
                                int rewrite_len = snprintf(rewrite, sizeof(rewrite), "%d", new_num);
                                if (dst + rewrite_len < 1024)
                                {
                                    memcpy(&buf[dst], rewrite, rewrite_len);
                                    dst += rewrite_len;
                                }

                                /* Copy whitespace */
                                size_t ws_len = src - ws_start;
                                if (dst + ws_len < 1024)
                                {
                                    memcpy(&buf[dst], &block_content_start[ws_start], ws_len);
                                    dst += ws_len;
                                }

                                /* Copy LOAD */
                                if (dst + 4 < 1024)
                                {
                                    memcpy(&buf[dst], "LOAD", 4);
                                    dst += 4;
                                }
                                src += 4;
                            }
                            else
                            {
                                /* Not LOAD, copy number as-is */
                                size_t num_len = src - num_start;
                                if (dst + num_len < 1024)
                                {
                                    memcpy(&buf[dst], &block_content_start[num_start], num_len);
                                    dst += num_len;
                                }
                            }
                        }
                        else
                        {
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
    if (in_block && block_content_start)
    {
        size_t block_len = (size_t)(&file_content[file_size] - block_content_start);

        uint8_t* buf = blk_get_buffer((uint32_t)current_dest_block, 1);
        if (!buf)
        {
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
    for (int i = 1; i < current_dest_block; i++)
    {
        log_message(LOG_DEBUG, "INIT: Executing block %d (LOAD)", i);

        /* Push block number and execute LOAD */
        vm_push(vm, (cell_t)i);

        /* Find and execute LOAD word */
        DictEntry* load_word = vm_find_word(vm, "LOAD", 4);
        if (!load_word)
        {
            log_message(LOG_ERROR, "INIT: LOAD word not found in dictionary");
            vm->error = 1;
            vm->halted = 1;
            return;
        }

        vm->current_executing_entry = load_word;
        physics_execution_heat_increment(load_word);
        profiler_word_count(load_word);
        profiler_word_enter(load_word);
        load_word->func(vm);
        physics_metadata_touch(load_word, load_word->execution_heat, sf_monotonic_ns());
        profiler_word_exit(load_word);
        vm->current_executing_entry = NULL;

        /* Check for errors after each block */
        if (vm->error)
        {
            log_message(LOG_ERROR, "INIT: Error executing block %d - system halted", i);
            vm->halted = 1;
            return;
        }
    }

    /* Switch back to FORTH vocabulary */
    log_message(LOG_INFO, "INIT: Switching to FORTH vocabulary");
    vm_interpret(vm, "FORTH DEFINITIONS");
    if (vm->error)
    {
        log_message(LOG_ERROR, "INIT: Failed to switch to FORTH vocabulary - system halted");
        vm->halted = 1;
        return;
    }

    /* Zero all init blocks to free them for userspace */
    log_message(LOG_INFO, "INIT: Zeroing %d init blocks for userspace use", total_blocks);
    for (int i = 1; i < current_dest_block; i++)
    {
        uint8_t* buf = blk_get_buffer((uint32_t)i, 1);
        if (buf)
        {
            memset(buf, 0, 1024);
            log_message(LOG_DEBUG, "INIT: Zeroed block %d", i);
        }
        else
        {
            log_message(LOG_WARN, "INIT: Failed to zero block %d (non-critical)", i);
        }
    }

    log_message(LOG_INFO, "INIT: System initialization complete - blocks freed, FORTH context active");
}

/* ============================================================================
 * PRNG / Utility Words
 * ============================================================================ */

/**
 * @brief Internal PRNG step using Linear Congruential Generator
 *
 * Uses Numerical Recipes LCG constants for good statistical properties.
 * @return Next pseudo-random 64-bit value
 */
static uint64_t prng_next(void)
{
    /* LCG: state = (a * state + c) mod m, where m = 2^64 (implicit) */
    g_prng_state = g_prng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return g_prng_state;
}

/**
 * @brief Set the PRNG seed
 *
 * Stack effect: ( n -- )
 * Sets the internal PRNG state for reproducible random sequences.
 * @param vm Pointer to the VM instance
 */
void starforth_word_seed(VM* vm)
{
    if (vm->dsp < 0)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "SEED: data stack underflow");
        return;
    }

    cell_t seed = vm_pop(vm);
    g_prng_state = (uint64_t)seed;

    /* Ensure non-zero state (LCG weakness) */
    if (g_prng_state == 0)
    {
        g_prng_state = 1;
    }

    log_message(LOG_DEBUG, "SEED: PRNG seeded with %lu", (unsigned long)g_prng_state);
}

/**
 * @brief Generate bounded random number
 *
 * Stack effect: ( lo hi -- n )
 * Returns a pseudo-random number in the inclusive range [lo, hi].
 * @param vm Pointer to the VM instance
 */
void starforth_word_random(VM* vm)
{
    if (vm->dsp < 1)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "RANDOM: data stack underflow (need lo hi)");
        return;
    }

    cell_t hi = vm_pop(vm);
    cell_t lo = vm_pop(vm);

    /* Handle inverted range */
    if (lo > hi)
    {
        cell_t tmp = lo;
        lo = hi;
        hi = tmp;
    }

    /* Generate random value in range [lo, hi] inclusive */
    uint64_t range = (uint64_t)(hi - lo) + 1;
    uint64_t raw = prng_next();

    /* Modulo bias reduction: use upper bits which have better randomness in LCG */
    cell_t result = lo + (cell_t)((raw >> 16) % range);

    vm_push(vm, result);
    log_message(LOG_DEBUG, "RANDOM: [%ld, %ld] -> %ld", (long)lo, (long)hi, (long)result);
}

/**
 * @brief Wait/sleep for specified milliseconds
 *
 * Stack effect: ( n -- )
 * Pauses execution for n milliseconds. Useful for:
 * - Allowing VM/system to cool between benchmark phases
 * - General-purpose timing in FORTH programs
 * @param vm Pointer to the VM instance
 */
void starforth_word_wait(VM* vm)
{
    if (vm->dsp < 0)
    {
        vm->error = 1;
        log_message(LOG_ERROR, "WAIT: data stack underflow");
        return;
    }

    cell_t ms = vm_pop(vm);

    if (ms <= 0)
    {
        log_message(LOG_DEBUG, "WAIT: zero or negative ms (%ld), no delay", (long)ms);
        return;
    }

    log_message(LOG_DEBUG, "WAIT: sleeping for %ld ms", (long)ms);

#ifdef __unix__
    /* POSIX nanosleep for precise, interruptible sleep */
    struct timespec req, rem;
    req.tv_sec = (time_t)(ms / 1000);
    req.tv_nsec = (long)((ms % 1000) * 1000000);

    while (nanosleep(&req, &rem) == -1)
    {
        /* Interrupted by signal, continue sleeping */
        req = rem;
    }
#else
    /* Fallback: busy-wait using platform time (less efficient but portable) */
    sf_time_ns_t start = sf_monotonic_ns();
    sf_time_ns_t target = start + (sf_time_ns_t)ms * 1000000ULL;

    while (sf_monotonic_ns() < target)
    {
        /* Busy wait - could add yield() on platforms that support it */
    }
#endif

    log_message(LOG_DEBUG, "WAIT: sleep complete");
}

/**
 * @brief Print StarForth version information (compliance)
 *
 * Prints: StarForth v<version> <architecture> <variant> <timestamp>
 * Stack effect: ( -- )
 * @param vm Pointer to the VM instance
 */
void starforth_word_version(VM* vm)
{
    (void)vm; /* Unused parameter */
    printf("%s\n", STARFORTH_VERSION_FULL);
}

/**
 * @brief Register StarForth vocabulary words with the VM
 *
 * Registers all StarForth-specific words and creates the STARFORTH vocabulary
 * @param vm Pointer to the VM instance
 */
void register_starforth_words(VM* vm)
{
    // register_word(vm, "ENTROPY@", starforth_word_execution_heat_fetch);
    // register_word(vm, "ENTROPY!", starforth_word_execution_heat_store);
    register_word(vm, "WORD-ENTROPY", starforth_word_word_execution_heat);
    register_word(vm, "RESET-ENTROPY", starforth_word_reset_execution_heat);
    register_word(vm, "TOP-WORDS", starforth_word_top_words);
    register_word(vm, "(-", starforth_word_paren_dash);
    register_word(vm, "INIT", starforth_word_init);
    register_word(vm, "VERSION", starforth_word_version);
    register_word(vm, "SEED", starforth_word_seed);
    register_word(vm, "RANDOM", starforth_word_random);
    register_word(vm, "WAIT", starforth_word_wait);

    /* Create the STARFORTH vocabulary */
    /* This would need vocabulary_word_vocabulary to be called */
    vm_interpret(vm, "VOCABULARY STARFORTH");
    vm_interpret(vm, "STARFORTH DEFINITIONS");

    /* Re-register the words in the STARFORTH vocabulary context */
    register_word(vm, "ENTROPY@", starforth_word_execution_heat_fetch);
    register_word(vm, "ENTROPY!", starforth_word_execution_heat_store);
    register_word(vm, "WORD-ENTROPY", starforth_word_word_execution_heat);
    register_word(vm, "RESET-ENTROPY", starforth_word_reset_execution_heat);
    register_word(vm, "TOP-WORDS", starforth_word_top_words);
    register_word(vm, "(-", starforth_word_paren_dash);
    register_word(vm, "INIT", starforth_word_init);
    register_word(vm, "VERSION", starforth_word_version);
    register_word(vm, "SEED", starforth_word_seed);
    register_word(vm, "RANDOM", starforth_word_random);
    register_word(vm, "WAIT", starforth_word_wait);

    /* Return to FORTH vocabulary */
    vm_interpret(vm, "FORTH DEFINITIONS");
}
