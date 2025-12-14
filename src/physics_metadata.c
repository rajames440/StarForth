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

  physics_metadata.c - Phase 1 physics metadata helpers

  Captures thermodynamic-style signals for dictionary entries without
  introducing heavy runtime cost. Future phases can build on these helpers
  to drive physics-aware scheduling and storage placement.
*/

#include "../include/physics_metadata.h"
#include "../include/ssm_jacquard.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* L8 FINAL INTEGRATION: L1 heat tracking is always-on (internal physics signal) */

static uint32_t clamp_u32(uint64_t value) {
    return (value > UINT32_MAX) ? UINT32_MAX : (uint32_t) value;
}

static uint16_t temperature_from_execution_heat(cell_t execution_heat, uint16_t prior_q8) {
    if (execution_heat <= 0) return 0;
    uint64_t scaled = ((uint64_t) execution_heat) << 8; /* convert to Q8 */
    uint16_t target = (scaled > UINT16_MAX) ? UINT16_MAX : (uint16_t) scaled;
    /* Smooth with a simple EMA to avoid jitter */
    return (uint16_t)((3u * prior_q8 + target) / 4u);
}

static uint8_t derive_state_flags(const DictEntry *entry) {
    if (!entry) return 0;
    uint8_t flags = 0;
    if (entry->flags & WORD_IMMEDIATE) flags |= PHYSICS_STATE_IMMEDIATE;
    if (entry->flags & WORD_PINNED) flags |= PHYSICS_STATE_PINNED;
    if (entry->flags & WORD_HIDDEN) flags |= PHYSICS_STATE_HIDDEN;
    if (entry->flags & WORD_COMPILED) flags |= PHYSICS_STATE_COMPILED;
    return flags;
}

void physics_metadata_init(DictEntry *entry, uint32_t header_bytes) {
    if (!entry) return;
    entry->physics.temperature_q8 = 0;
    entry->physics.last_active_ns = 0;
    entry->physics.last_decay_ns = 0;
    entry->physics.mass_bytes = header_bytes;
    entry->physics.avg_latency_ns = 0;
    entry->physics.state_flags = derive_state_flags(entry);
    entry->physics.acl_hint = 0;
    entry->physics.pubsub_mask = 0;
}

void physics_metadata_set_mass(DictEntry *entry, uint32_t total_bytes) {
    if (!entry) return;
    entry->physics.mass_bytes = total_bytes;
}

/* ============================================================================
 * INTENT: Update word's physics metadata after execution
 * FL1: Heat accumulation feedback loop - converts raw execution_heat counter
 *      to smoothed temperature_q8 metric and updates activity timestamps
 * WHY: Atomic heat increment (FL1) happens in execution loop; this function
 *      derives secondary metrics (temperature, timestamps) for observability
 * ============================================================================ */
void physics_metadata_touch(DictEntry *entry, cell_t execution_heat, uint64_t now_ns) {
    if (!entry) return;
    entry->physics.temperature_q8 = temperature_from_execution_heat(execution_heat, entry->physics.temperature_q8);
    entry->physics.last_active_ns = now_ns;
    entry->physics.last_decay_ns = now_ns;
}

void physics_metadata_refresh_state(DictEntry *entry) {
    if (!entry) return;
    entry->physics.state_flags = derive_state_flags(entry);
}

void physics_metadata_record_latency(DictEntry *entry, uint64_t sample_ns) {
    if (!entry) return;
    uint32_t sample = clamp_u32(sample_ns);
    uint32_t prior = entry->physics.avg_latency_ns;
    if (prior == 0) {
        entry->physics.avg_latency_ns = sample;
    } else {
        entry->physics.avg_latency_ns = (uint32_t)((3ull * prior + sample) / 4ull);
    }
}

typedef struct physics_seed_definition {
    const char *name;
    uint16_t temperature_q8;
    uint32_t avg_latency_ns;
    uint8_t acl_hint;
    uint8_t pubsub_mask;
} physics_seed_definition_t;

static const physics_seed_definition_t physics_seed_table[] = {
    {"IF", 0x0100u, 400u, 0, 0},
    {"ELSE", 0x0100u, 400u, 0, 0},
    {"THEN", 0x0100u, 350u, 0, 0},
    {"BEGIN", 0x0120u, 450u, 0, 0},
    {"WHILE", 0x0120u, 450u, 0, 0},
    {"REPEAT", 0x0120u, 450u, 0, 0},
    {"DO", 0x0180u, 500u, 0, 0},
    {"LOOP", 0x0180u, 500u, 0, 0},
    {"+LOOP", 0x0200u, 520u, 0, 0},
    {"LEAVE", 0x0200u, 520u, 0, 0},
    {".", 0x0080u, 300u, 0, 0},
    {"EMIT", 0x0180u, 800u, 0, 0x02u},
    {"TYPE", 0x0180u, 900u, 0, 0x02u},
    {"BLOCK", 0x0200u, 950u, 0, 0x04u},
    {"BUFFER", 0x01C0u, 900u, 0, 0x04u},
    {"SAVE-BUFFERS", 0x0260u, 1400u, 0, 0x04u},
    {"FLUSH", 0x0240u, 1200u, 0, 0x04u},
    {"LOAD", 0x0220u, 1100u, 0, 0x04u},
    {"LIST", 0x0200u, 1000u, 0, 0x04u},
    {"SAVE-SYSTEM", 0x0280u, 2000u, 0x80u, 0x08u},
    {NULL, 0, 0, 0, 0}
};

void physics_metadata_apply_seed(DictEntry *entry) {
    if (!entry || !entry->name_len) return;
    for (const physics_seed_definition_t *seed = physics_seed_table; seed->name; ++seed) {
        if ((int) strlen(seed->name) != entry->name_len) continue;
        if (strncmp(seed->name, entry->name, entry->name_len) != 0) continue;
        if (seed->temperature_q8) entry->physics.temperature_q8 = seed->temperature_q8;
        if (seed->avg_latency_ns) entry->physics.avg_latency_ns = seed->avg_latency_ns;
        if (seed->acl_hint) entry->physics.acl_hint = seed->acl_hint;
        if (seed->pubsub_mask) entry->physics.pubsub_mask = seed->pubsub_mask;
        break;
    }
}

/*
 * ============================================================================
 * Phase 2: Linear Decay Mechanism
 * ============================================================================
 *
 * Reduces execution_heat over time to model:
 * - OS context switches (old task's heat becomes stale)
 * - Temporal locality (recent access patterns > distant past)
 * - Dissipation (physical analogy: thermal equilibrium)
 *
 * Mathematical Model (Discrete Linear):
 *   H(t) = max(0, H_0 - d*t)
 *
 * Where:
 *   H(t) = heat at time t
 *   H_0  = initial heat
 *   d    = decay rate (heat units per nanosecond)
 *   t    = elapsed time since last execution
 *
 * Frozen words (WORD_FROZEN flag) are exempt from decay.
 * ============================================================================
 */

/* L8 FINAL INTEGRATION: L3 linear decay - runtime gated by Jacquard mode selector */
void physics_metadata_apply_linear_decay(DictEntry *entry, uint64_t elapsed_ns, VM *vm) {
    if (!entry || !vm) {
        return;
    }

    /* Frozen words do not decay */
    if (entry->flags & WORD_FROZEN) {
        return;
    }

    /* Don't bother with insignificant time intervals */
    if (elapsed_ns < DECAY_MIN_INTERVAL) {
        return;
    }

    /* L8 GATE: Check if L3 (linear decay) is enabled by Jacquard mode selector */
    if (vm->ssm_config) {
        ssm_config_t *config = (ssm_config_t*)vm->ssm_config;
        if (!config->L3_linear_decay) {
            return;  /* L3 disabled by L8 mode selector */
        }
    }

    /* ========================================================================
     * INTENT: Calculate decay amount using adaptive Q48.16 fixed-point math
     * FL1: Heat decay - counterbalances FL1 accumulation to prevent runaway
     * FL3: Adaptive slope tuning - inference engine adjusts decay_slope_q48
     *      based on statistical analysis of heat trajectory effectiveness
     * WHY: Microseconds (not nanoseconds) to avoid overflow in multiplication
     *      Q48.16 format provides 16 bits of fractional precision for smooth
     *      decay curves while keeping all math integer-only (no floating point)
     * ======================================================================== */
    uint64_t elapsed_us = elapsed_ns / 1000;
    uint64_t slope_q48 = physics_decay_slope_load(vm);
    uint64_t decay_amount_raw = (elapsed_us * slope_q48) >> 16;

    /* Clamp to cell_t range (avoid overflow) */
    cell_t decay_amount = (decay_amount_raw > (uint64_t)0x7FFFFFFFFFFFFFFF)
                          ? 0x7FFFFFFFFFFFFFFF
                          : (cell_t)decay_amount_raw;

    if (decay_amount == 0) {
        return;
    }

#if defined(__GNUC__)
    cell_t old_heat;
    cell_t new_heat;
    do {
        old_heat = __atomic_load_n(&entry->execution_heat, __ATOMIC_RELAXED);
        if (old_heat == 0) {
            return;
        }
        new_heat = (decay_amount >= old_heat) ? 0 : (old_heat - decay_amount);
    } while (!__atomic_compare_exchange_n(&entry->execution_heat,
                                          &old_heat,
                                          new_heat,
                                          0,
                                          __ATOMIC_RELAXED,
                                          __ATOMIC_RELAXED));
#else
    if (decay_amount >= entry->execution_heat) {
        entry->execution_heat = 0;
    } else {
        entry->execution_heat -= decay_amount;
    }
#endif
}
