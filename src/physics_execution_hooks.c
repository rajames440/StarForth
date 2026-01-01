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

/*
 * physics_pre_execute - Called before word execution
 *
 * Consolidates:
 *   - Linear decay application
 *   - Heat increment
 *   - Rolling window recording
 *   - Pipelining hit detection and speculation
 *   - Profiler entry
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

/*
 * physics_post_execute - Called after word execution
 *
 * Consolidates:
 *   - Physics metadata touch
 *   - Profiler exit
 *   - DoE counter increment
 *   - Heartbeat cycle check (when no background thread)
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

/*
 * physics_on_lookup - Called when outer interpreter finds a word
 *
 * Handles physics for the lookup path (not inner threaded execution).
 * Thread-safe: acquires dict_lock.
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
