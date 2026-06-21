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

  physics_freeze_words.c - Phase 2 Freeze/Decay Control Words

  Implements the FORTH interface for Phase 2 physics model enhancements:
  - FREEZE-WORD: Lock a word's execution heat (prevents decay)
  - UNFREEZE-WORD: Allow heat decay to resume
  - FROZEN?: Query freeze status
  - HEAT!: Set execution heat manually (diagnostics)
  - HEAT@: Read current execution heat
  - SHOW-HEAT: Display heat for a single word
  - ALL-HEATS: Display heat for all words in dictionary

  Semantic Note:
  - WORD_FROZEN (0x04) prevents heat decay via physics_metadata_apply_linear_decay()
  - Works independently from WORD_PINNED (0x08) which locks heat at maximum
  - FROZEN words maintain their heat indefinitely across OS context switches
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "vm.h"
#include "physics_metadata.h"
#include "word_registry.h"

/**
 * @brief FREEZE-WORD ( caddr u -- )
 *
 * Set the @c WORD_FROZEN flag on the named word, preventing @c physics_metadata_apply_linear_decay()
 * from reducing its @c execution_heat. Frozen words stay hot across OS context switches indefinitely.
 * Silently succeeds if the word is not found (lenient policy).
 *
 * Stack effect: ( caddr u -- )
 *
 * @param vm Active VM; @c vm->error is set and returns early on underflow
 */
void forth_FREEZE_WORD(VM *vm) {
    if (vm->error) return;

    /* Stack: ( caddr u -- ) */
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t len = vm->data_stack[--vm->dsp];
    cell_t caddr = vm->data_stack[--vm->dsp];

    if (len <= 0 || len > WORD_NAME_MAX) {
        return; /* Silently fail on invalid length */
    }

    /* Convert VM address to C pointer (if using strict pointer mode) */
    const char *name = (const char *)(uintptr_t)caddr;

    /* Look up the word */
    DictEntry *entry = vm_find_word(vm, name, (size_t)len);

    if (entry) {
        entry->flags |= WORD_FROZEN;
    }
    /* Silently succeed even if word not found (lenient) */
}

/**
 * @brief UNFREEZE-WORD ( caddr u -- )
 *
 * Clears the @c WORD_FROZEN flag on the named word, allowing its @c execution_heat
 * to decay normally via Loop #3. Does not alter @c WORD_PINNED. Silently succeeds
 * if the word is not found or @c u is out of range.
 *
 * Stack effect: ( caddr u -- )
 *
 * @param vm Active VM; @c vm->error is set and returns early on underflow
 */
void forth_UNFREEZE_WORD(VM *vm) {
    if (vm->error) return;

    /* Stack: ( caddr u -- ) */
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t len = vm->data_stack[--vm->dsp];
    cell_t caddr = vm->data_stack[--vm->dsp];

    if (len <= 0 || len > WORD_NAME_MAX) {
        return;
    }

    const char *name = (const char *)(uintptr_t)caddr;
    DictEntry *entry = vm_find_word(vm, name, (size_t)len);

    if (entry) {
        entry->flags &= ~WORD_FROZEN;
    }
}

/**
 * @brief FROZEN? ( caddr u -- flag )
 *
 * Tests whether the @c WORD_FROZEN flag is set on the named word. Pushes
 * -1 (FORTH true) if frozen, 0 otherwise. Also pushes 0 when the word is
 * not found or @c u is out of range, so callers need not distinguish "not frozen"
 * from "not found".
 *
 * Stack effect: ( caddr u -- flag )
 *
 * @param vm Active VM; @c vm->error is set and returns early on underflow
 */
void forth_FROZEN_QUERY(VM *vm) {
    if (vm->error) return;

    /* Stack: ( caddr u -- flag ) */
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t len = vm->data_stack[--vm->dsp];
    cell_t caddr = vm->data_stack[--vm->dsp];

    if (len <= 0 || len > WORD_NAME_MAX) {
        vm->data_stack[vm->dsp++] = 0; /* Not frozen */
        return;
    }

    const char *name = (const char *)(uintptr_t)caddr;
    DictEntry *entry = vm_find_word(vm, name, (size_t)len);

    if (entry && (entry->flags & WORD_FROZEN)) {
        vm->data_stack[vm->dsp++] = -1; /* True (frozen) */
    } else {
        vm->data_stack[vm->dsp++] = 0;  /* False (not frozen) */
    }
}

/**
 * @brief HEAT! ( heat caddr u -- )
 *
 * Directly writes @c heat into @c entry->execution_heat for the named word.
 * Intended for diagnostics and testing; bypasses Loop #1 accumulation and
 * Loop #3 decay. Silently succeeds if the word is not found or @c u is out
 * of range.
 *
 * Stack effect: ( heat caddr u -- )
 *
 * @param vm Active VM; @c vm->error is set and returns early on underflow
 */
void forth_HEAT_STORE(VM *vm) {
    if (vm->error) return;

    /* Stack: ( heat caddr u -- ) */
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t len = vm->data_stack[--vm->dsp];
    cell_t caddr = vm->data_stack[--vm->dsp];
    cell_t heat = vm->data_stack[--vm->dsp];

    if (len <= 0 || len > WORD_NAME_MAX) {
        return;
    }

    const char *name = (const char *)(uintptr_t)caddr;
    DictEntry *entry = vm_find_word(vm, name, (size_t)len);

    if (entry) {
        entry->execution_heat = heat;
    }
}

/**
 * @brief HEAT@ ( caddr u -- heat )
 *
 * Pushes the current @c execution_heat of the named word. Pushes 0 when the
 * word is not found, @c u is out of range, or the word genuinely has zero heat.
 *
 * Stack effect: ( caddr u -- heat )
 *
 * @param vm Active VM; @c vm->error is set and returns early on underflow
 */
void forth_HEAT_FETCH(VM *vm) {
    if (vm->error) return;

    /* Stack: ( caddr u -- heat ) */
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t len = vm->data_stack[--vm->dsp];
    cell_t caddr = vm->data_stack[--vm->dsp];

    if (len <= 0 || len > WORD_NAME_MAX) {
        vm->data_stack[vm->dsp++] = 0;
        return;
    }

    const char *name = (const char *)(uintptr_t)caddr;
    DictEntry *entry = vm_find_word(vm, name, (size_t)len);

    if (entry) {
        vm->data_stack[vm->dsp++] = entry->execution_heat;
    } else {
        vm->data_stack[vm->dsp++] = 0;
    }
}

/**
 * @brief SHOW-HEAT ( caddr u -- )
 *
 * Prints the @c execution_heat of the named word to @c stdout along with its
 * freeze and pinned status flags. Output format: @c "NAME: HEAT (frozen) (pinned)".
 * Prints @c "Word not found: NAME" when the dictionary lookup fails. Intended for
 * interactive diagnostics.
 *
 * Stack effect: ( caddr u -- )
 *
 * @param vm Active VM; @c vm->error is set and returns early on underflow
 */
void forth_SHOW_HEAT(VM *vm) {
    if (vm->error) return;

    /* Stack: ( caddr u -- ) */
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }

    cell_t len = vm->data_stack[--vm->dsp];
    cell_t caddr = vm->data_stack[--vm->dsp];

    if (len <= 0 || len > WORD_NAME_MAX) {
        return;
    }

    const char *name = (const char *)(uintptr_t)caddr;
    DictEntry *entry = vm_find_word(vm, name, (size_t)len);

    if (entry) {
        printf("%.*s: %ld", (int)len, name, entry->execution_heat);

        if (entry->flags & WORD_FROZEN) {
            printf(" (frozen)");
        }
        if (entry->flags & WORD_PINNED) {
            printf(" (pinned)");
        }
        printf("\n");
    } else {
        printf("Word not found: %.*s\n", (int)len, name);
    }
}

/**
 * @brief ALL-HEATS ( -- )
 *
 * Iterates the entire dictionary, collects up to 1024 entries, sorts them by
 * descending @c execution_heat using an O(n²) bubble sort, then prints a
 * formatted table to @c stdout with columns for word name, heat value, and
 * status (frozen/pinned). Intended for interactive diagnostics; the O(n²) sort
 * is acceptable for dictionary sizes below 1000 words.
 *
 * Stack effect: ( -- )
 *
 * @param vm Active VM; no-op if @c vm->error is set
 */
void forth_ALL_HEATS(VM *vm) {
    if (vm->error) return;

    printf("\n=== Execution Heat (Phase 2) ===\n");
    printf("%-16s %10s %s\n", "Word", "Heat", "Status");
    printf("%-16s %10s %s\n", "----", "----", "------");

    /* Simple O(n²) bubble sort by heat (acceptable for <1000 words) */
    int count = 0;
    DictEntry *entries[1024];

    /* Collect all entries */
    for (DictEntry *w = vm->latest; w && count < 1024; w = w->link) {
        entries[count++] = w;
    }

    /* Sort by descending heat */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (entries[j]->execution_heat < entries[j + 1]->execution_heat) {
                DictEntry *tmp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = tmp;
            }
        }
    }

    /* Display sorted by heat */
    for (int i = 0; i < count; i++) {
        DictEntry *w = entries[i];

        printf("%-16.*s %10ld", w->name_len, w->name, w->execution_heat);

        const char *status = "";
        if (w->flags & WORD_FROZEN) {
            status = " (frozen)";
        } else if (w->flags & WORD_PINNED) {
            status = " (pinned)";
        }

        printf("%s\n", status);
    }

    printf("\n");
}

/**
 * @brief DECAY-RATE@ ( -- rate )
 *
 * Pushes the compile-time constant @c DECAY_RATE_PER_US_Q16 as a Q48.16
 * fixed-point value. This is the base heat decay rate per microsecond used
 * by Loop #3 before adaptive tuning by Loop #6. Intended for introspection
 * and diagnostics.
 *
 * Stack effect: ( -- rate )
 *
 * @param vm Active VM; no-op if @c vm->error is set
 */
void forth_DECAY_RATE_FETCH(VM *vm) {
    if (vm->error) return;

    /* Stack: ( -- rate ) */
    vm->data_stack[vm->dsp++] = (cell_t)DECAY_RATE_PER_US_Q16;
}

/**
 * @brief FREEZE-CRITICAL ( -- )
 *
 * Sets @c WORD_FROZEN on a hard-coded list of 21 system-critical words
 * (DUP, DROP, SWAP, OVER, ROT, @, !, C@, C!, EXECUTE, IF, THEN, ELSE, DO,
 * LOOP, BEGIN, UNTIL, REPEAT, ., EMIT, CR). Silently skips any word not found
 * in the current dictionary. Intended to be called once at startup to prevent
 * essential words from decaying out of the hot-words cache.
 *
 * Stack effect: ( -- )
 *
 * @param vm Active VM; no-op if @c vm->error is set
 */
void forth_FREEZE_CRITICAL(VM *vm) {
    if (vm->error) return;

    /* List of critical words that should never decay */
    const char *critical_words[] = {
        "DUP",
        "DROP",
        "SWAP",
        "OVER",
        "ROT",
        "@",
        "!",
        "C@",
        "C!",
        "EXECUTE",
        "IF",
        "THEN",
        "ELSE",
        "DO",
        "LOOP",
        "BEGIN",
        "UNTIL",
        "REPEAT",
        ".",
        "EMIT",
        "CR",
        NULL
    };

    for (int i = 0; critical_words[i] != NULL; i++) {
        const char *name = critical_words[i];
        size_t len = strlen(name);
        DictEntry *entry = vm_find_word(vm, name, len);

        if (entry) {
            entry->flags |= WORD_FROZEN;
        }
    }
}

/**
 * @brief Register all Phase 2 freeze/decay control words with the VM dictionary.
 *
 * Registers: @c FREEZE-WORD, @c UNFREEZE-WORD, @c FROZEN?, @c HEAT!,
 * @c HEAT@, @c SHOW-HEAT, @c ALL-HEATS, @c DECAY-RATE@, @c FREEZE-CRITICAL.
 * Called during VM bootstrap by the word registration subsystem.
 *
 * @param vm Active VM to register words into
 */
void register_physics_freeze_words(VM *vm) {
    register_word(vm, "FREEZE-WORD", forth_FREEZE_WORD);
    register_word(vm, "UNFREEZE-WORD", forth_UNFREEZE_WORD);
    register_word(vm, "FROZEN?", forth_FROZEN_QUERY);
    register_word(vm, "HEAT!", forth_HEAT_STORE);
    register_word(vm, "HEAT@", forth_HEAT_FETCH);
    register_word(vm, "SHOW-HEAT", forth_SHOW_HEAT);
    register_word(vm, "ALL-HEATS", forth_ALL_HEATS);
    register_word(vm, "DECAY-RATE@", forth_DECAY_RATE_FETCH);
    register_word(vm, "FREEZE-CRITICAL", forth_FREEZE_CRITICAL);
}

