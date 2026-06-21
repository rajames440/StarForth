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
 * physics_execution_hooks.c - All physics machinery in one place
 *
 * This file absorbs ALL the physics-related code that was scattered
 * throughout execute_colon_word and vm_interpret_word.
 *
 * The VM execution loop is now clean; this file is where physics lives.
 */

#include "../include/physics_execution_hooks.h"
#include "../include/platform_time.h"
#include "../include/profiler.h"
#include "../include/physics_metadata.h"
#include "../include/physics_hotwords_cache.h"
#include "../include/physics_pipelining_metrics.h"
#include "../include/rolling_window_of_truth.h"

/**
 * @brief Pre-execution physics hook — called before every word dispatch.
 *
 * Consolidates all physics work that must happen before a word runs:
 * - Loop #3: applies linear decay based on elapsed time since last execution
 * - Loop #1: increments @c execution_heat for the word
 * - Loop #2: records the word-id into the rolling window of truth
 * - Loop #4: detects a pipelining prefetch hit (if @c prev's speculation
 *            matched @c word), records the transition from @c prev to @c word,
 *            updates the probability cache, and issues a speculative hotwords
 *            cache promotion for the most likely next word
 *
 * @param vm   Pointer to the VM structure
 * @param word DictEntry being executed
 * @param prev DictEntry executed immediately before @c word (may be NULL)
 */
void physics_pre_execute(VM* vm, DictEntry* word, DictEntry* prev)
{
    if (!vm || !word) return;

    uint64_t now_ns = sf_monotonic_ns();

    /* Loop #3: Apply linear decay before accumulating new heat */
    uint64_t elapsed_ns = now_ns - word->physics.last_active_ns;
    physics_metadata_apply_linear_decay(word, elapsed_ns, vm);
    word->physics.last_active_ns = now_ns;
    word->physics.last_decay_ns = now_ns;

    /* Loop #1: Heat accumulation */
    physics_execution_heat_increment(word);

    uint32_t word_id = word->word_id;
    if (word_id >= DICTIONARY_SIZE) return;

    /* Loop #2: Rolling Window of Truth */
    rolling_window_record_execution(&vm->rolling_window, word_id);

    /* Loop #4: Pipelining Transition Metrics */
    if (!prev || !prev->transition_metrics || !ENABLE_PIPELINING) return;

    /* Check if current word was speculatively promoted by previous word */
    uint32_t prev_speculation_target = prev->transition_metrics->most_likely_next_word_id;
    if (prev_speculation_target == word_id && prev->transition_metrics->prefetch_attempts > 0)
    {
        /* PREFETCH HIT: Current word matches previous word's speculation! */
        transition_metrics_record_prefetch_hit(prev->transition_metrics, 0);
        vm->pipeline_metrics.prefetch_hits++;
    }

    /* Record transition from previous word to current word */
    transition_metrics_record(prev->transition_metrics, word_id, DICTIONARY_SIZE);

    /* Update probability cache to find most likely next word */
    transition_metrics_update_cache(prev->transition_metrics, DICTIONARY_SIZE);

    uint32_t speculated_word_id = prev->transition_metrics->most_likely_next_word_id;

    /* Speculative prefetch of most likely next word */
    if (speculated_word_id < DICTIONARY_SIZE &&
        transition_metrics_should_speculate(prev->transition_metrics, speculated_word_id))
    {
        DictEntry* spec_entry = vm_dictionary_lookup_by_word_id(vm, speculated_word_id);

        if (spec_entry && vm->hotwords_cache && ENABLE_HOTWORDS_CACHE)
        {
            spec_entry->execution_heat = HOTWORDS_EXECUTION_HEAT_THRESHOLD + 1;
            hotwords_cache_promote(vm->hotwords_cache, spec_entry);

            prev->transition_metrics->prefetch_attempts++;
            vm->pipeline_metrics.prefetch_attempts++;
        }
    }
}

/**
 * @brief Post-execution physics hook — called after every word dispatch.
 *
 * Consolidates all physics work that must happen after a word completes:
 * - Updates physics metadata (@c temperature_q8, @c last_active_ns) via
 *   @c physics_metadata_touch()
 * - Notifies the profiler (@c profiler_word_exit())
 * - Increments @c vm->heartbeat.words_executed
 * - When running without a background heartbeat thread, checks whether
 *   enough words have been executed to trigger a heartbeat cycle
 *   (@c vm_heartbeat_run_cycle())
 *
 * @param vm   Pointer to the VM structure
 * @param word DictEntry that just finished executing
 */
void physics_post_execute(VM* vm, DictEntry* word)
{
    if (!vm || !word) return;

    physics_metadata_touch(word, word->execution_heat, sf_monotonic_ns());
    profiler_word_exit(word);
    vm->heartbeat.words_executed++;

    /* Heartbeat: Periodic time-driven tuning (Loop #3 & #5) */
    if (!vm->heartbeat.worker && ++vm->heartbeat.check_counter >= HEARTBEAT_CHECK_FREQUENCY)
    {
        extern void vm_heartbeat_run_cycle(VM* vm);
        vm_heartbeat_run_cycle(vm);
        vm->heartbeat.check_counter = 0;
    }
}

/**
 * @brief Physics hook for the outer interpreter word-lookup path.
 *
 * Called when the outer interpreter successfully finds a word by name,
 * before executing it. Applies linear decay and increments heat on both
 * the found entry and the canonical entry (if different), and updates
 * @c last_active_ns / @c last_decay_ns on both. Acquires @c vm->dict_lock
 * for the entire operation to ensure thread safety.
 *
 * @param vm    Pointer to the VM structure
 * @param entry DictEntry found by name search
 * @param canon Canonical DictEntry (may equal @c entry or be the smudge target)
 */
void physics_on_lookup(VM* vm, DictEntry* entry, DictEntry* canon)
{
    if (!vm || !entry) return;

    uint64_t lookup_ns = sf_monotonic_ns();

    sf_mutex_lock(&vm->dict_lock);

    /* Apply decay and heat to found entry */
    uint64_t elapsed_entry = lookup_ns - entry->physics.last_active_ns;
    physics_metadata_apply_linear_decay(entry, elapsed_entry, vm);
    entry->physics.last_active_ns = lookup_ns;
    entry->physics.last_decay_ns = lookup_ns;
    physics_execution_heat_increment(entry);

    /* Also handle canonical entry if different */
    if (canon && canon != entry)
    {
        uint64_t elapsed_canon = lookup_ns - canon->physics.last_active_ns;
        physics_metadata_apply_linear_decay(canon, elapsed_canon, vm);
        canon->physics.last_active_ns = lookup_ns;
        canon->physics.last_decay_ns = lookup_ns;
        physics_execution_heat_increment(canon);
        physics_metadata_touch(canon, canon->execution_heat, lookup_ns);
    }

    sf_mutex_unlock(&vm->dict_lock);
}
