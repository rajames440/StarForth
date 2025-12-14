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

/* ============================================================================
 * FREEZE-WORD ( caddr u -- )
 *
 * Freeze a word by name: prevent its execution heat from decaying.
 * Useful for system-critical words that must stay in cache.
 *
 * Example:
 *   S" DUP" FREEZE-WORD
 *   S" DROP" FREEZE-WORD
 * ============================================================================
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

/* ============================================================================
 * UNFREEZE-WORD ( caddr u -- )
 *
 * Unfreeze a word by name: allow its execution heat to decay normally.
 *
 * Example:
 *   S" MY-TEMP-WORD" UNFREEZE-WORD
 * ============================================================================
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

/* ============================================================================
 * FROZEN? ( caddr u -- flag )
 *
 * Query freeze status of a word.
 * Returns: -1 (true) if word is frozen, 0 (false) otherwise.
 *
 * Example:
 *   S" DUP" FROZEN? IF ." DUP is frozen" THEN
 * ============================================================================
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

/* ============================================================================
 * HEAT! ( heat caddr u -- )
 *
 * Set execution heat for a word manually (diagnostic/testing).
 *
 * Example:
 *   100 S" MY-WORD" HEAT!   \ Set MY-WORD heat to 100
 * ============================================================================
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

/* ============================================================================
 * HEAT@ ( caddr u -- heat )
 *
 * Read current execution heat for a word.
 *
 * Example:
 *   S" DUP" HEAT@ . \ Print DUP's current heat
 * ============================================================================
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

/* ============================================================================
 * SHOW-HEAT ( caddr u -- )
 *
 * Display execution heat for a single word.
 * Shows heat value and freeze status.
 *
 * Example:
 *   S" DUP" SHOW-HEAT
 *   Output: DUP: 523 (frozen)
 * ============================================================================
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

/* ============================================================================
 * ALL-HEATS ( -- )
 *
 * Display execution heat for all words in dictionary.
 * Sorted by descending heat (hottest first).
 *
 * Example:
 *   ALL-HEATS
 *   Output:
 *     DUP: 1523 (frozen)
 *     EMIT: 892 (frozen)
 *     TYPE: 456
 *     ...
 * ============================================================================
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

/* ============================================================================
 * DECAY-RATE@ ( -- rate )
 *
 * Get current decay rate (Q48.16 fixed-point format, per microsecond).
 * Returns the scaled decay rate for introspection and diagnostics.
 * ============================================================================
 */
void forth_DECAY_RATE_FETCH(VM *vm) {
    if (vm->error) return;

    /* Stack: ( -- rate ) */
    vm->data_stack[vm->dsp++] = (cell_t)DECAY_RATE_PER_US_Q16;
}

/* ============================================================================
 * FREEZE-CRITICAL ( -- )
 *
 * Freeze all system-critical words that must remain in cache.
 * Recommended to call once at startup.
 *
 * Example:
 *   FREEZE-CRITICAL
 * ============================================================================
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

/* ============================================================================
 * Word Registration: Phase 2 Freeze/Decay Control Words
 * ============================================================================
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

