/*
                                  ***   StarForth   ***

  physics_metadata.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T08:16:51.572-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/physics_metadata.c
 */

/*
                                  ***   StarForth   ***

  physics_metadata.c - Phase 1 physics metadata helpers

  Captures thermodynamic-style signals for dictionary entries without
  introducing heavy runtime cost. Future phases can build on these helpers
  to drive physics-aware scheduling and storage placement.
*/

#include "../include/physics_metadata.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t clamp_u32(uint64_t value) {
    return (value > UINT32_MAX) ? UINT32_MAX : (uint32_t) value;
}

static uint16_t temperature_from_entropy(cell_t entropy, uint16_t prior_q8) {
    if (entropy <= 0) return 0;
    uint64_t scaled = ((uint64_t) entropy) << 8; /* convert to Q8 */
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

void physics_metadata_touch(DictEntry *entry, cell_t entropy, uint64_t now_ns) {
    if (!entry) return;
    entry->physics.temperature_q8 = temperature_from_entropy(entropy, entry->physics.temperature_q8);
    entry->physics.last_active_ns = now_ns;
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