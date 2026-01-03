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
*/

/**
 * xxhash64.c - Freestanding xxHash64 Implementation
 *
 * Based on xxHash by Yann Collet (BSD-2-Clause).
 * Adapted for freestanding kernel use - no libc dependency.
 *
 * xxHash is an extremely fast non-cryptographic hash algorithm.
 * https://github.com/Cyan4973/xxHash
 */

#include "starkernel/xxhash64.h"

/*===========================================================================
 * xxHash64 Constants
 *===========================================================================*/

#define XXHASH64_PRIME1  0x9E3779B185EBCA87ULL
#define XXHASH64_PRIME2  0xC2B2AE3D27D4EB4FULL
#define XXHASH64_PRIME3  0x165667B19E3779F9ULL
#define XXHASH64_PRIME4  0x85EBCA77C2B2AE63ULL
#define XXHASH64_PRIME5  0x27D4EB2F165667C5ULL

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * Rotate left 64-bit
 */
static inline uint64_t xxh64_rotl(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}

/**
 * Read 64-bit little-endian (unaligned safe)
 */
static inline uint64_t xxh64_read64(const uint8_t *p) {
    return ((uint64_t)p[0])       |
           ((uint64_t)p[1] << 8)  |
           ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

/**
 * Read 32-bit little-endian (unaligned safe)
 */
static inline uint32_t xxh64_read32(const uint8_t *p) {
    return ((uint32_t)p[0])       |
           ((uint32_t)p[1] << 8)  |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/**
 * Round function
 */
static inline uint64_t xxh64_round(uint64_t acc, uint64_t input) {
    acc += input * XXHASH64_PRIME2;
    acc = xxh64_rotl(acc, 31);
    acc *= XXHASH64_PRIME1;
    return acc;
}

/**
 * Merge accumulator
 */
static inline uint64_t xxh64_merge_round(uint64_t acc, uint64_t val) {
    val = xxh64_round(0, val);
    acc ^= val;
    acc = acc * XXHASH64_PRIME1 + XXHASH64_PRIME4;
    return acc;
}

/**
 * Avalanche finalization
 */
static inline uint64_t xxh64_avalanche(uint64_t h) {
    h ^= h >> 33;
    h *= XXHASH64_PRIME2;
    h ^= h >> 29;
    h *= XXHASH64_PRIME3;
    h ^= h >> 32;
    return h;
}

/*===========================================================================
 * One-Shot Hashing
 *===========================================================================*/

uint64_t xxhash64(const void *data, size_t len, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *end = p + len;
    uint64_t h64;

    if (len >= 32) {
        const uint8_t *limit = end - 32;
        uint64_t v1 = seed + XXHASH64_PRIME1 + XXHASH64_PRIME2;
        uint64_t v2 = seed + XXHASH64_PRIME2;
        uint64_t v3 = seed + 0;
        uint64_t v4 = seed - XXHASH64_PRIME1;

        do {
            v1 = xxh64_round(v1, xxh64_read64(p));
            p += 8;
            v2 = xxh64_round(v2, xxh64_read64(p));
            p += 8;
            v3 = xxh64_round(v3, xxh64_read64(p));
            p += 8;
            v4 = xxh64_round(v4, xxh64_read64(p));
            p += 8;
        } while (p <= limit);

        h64 = xxh64_rotl(v1, 1) + xxh64_rotl(v2, 7) +
              xxh64_rotl(v3, 12) + xxh64_rotl(v4, 18);

        h64 = xxh64_merge_round(h64, v1);
        h64 = xxh64_merge_round(h64, v2);
        h64 = xxh64_merge_round(h64, v3);
        h64 = xxh64_merge_round(h64, v4);
    } else {
        h64 = seed + XXHASH64_PRIME5;
    }

    h64 += (uint64_t)len;

    /* Process remaining 8-byte chunks */
    while (p + 8 <= end) {
        uint64_t k1 = xxh64_round(0, xxh64_read64(p));
        h64 ^= k1;
        h64 = xxh64_rotl(h64, 27) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
        p += 8;
    }

    /* Process remaining 4-byte chunk */
    if (p + 4 <= end) {
        h64 ^= (uint64_t)xxh64_read32(p) * XXHASH64_PRIME1;
        h64 = xxh64_rotl(h64, 23) * XXHASH64_PRIME2 + XXHASH64_PRIME3;
        p += 4;
    }

    /* Process remaining bytes */
    while (p < end) {
        h64 ^= (uint64_t)(*p) * XXHASH64_PRIME5;
        h64 = xxh64_rotl(h64, 11) * XXHASH64_PRIME1;
        p++;
    }

    return xxh64_avalanche(h64);
}

/*===========================================================================
 * Streaming API
 *===========================================================================*/

void xxhash64_reset(XXHash64State *state, uint64_t seed) {
    state->total_len = 0;
    state->v1 = seed + XXHASH64_PRIME1 + XXHASH64_PRIME2;
    state->v2 = seed + XXHASH64_PRIME2;
    state->v3 = seed + 0;
    state->v4 = seed - XXHASH64_PRIME1;
    state->buffer_size = 0;
    state->seed = seed;
}

void xxhash64_update(XXHash64State *state, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *end = p + len;

    state->total_len += len;

    /* Fill buffer if we have leftover data */
    if (state->buffer_size + len < 32) {
        /* Not enough to process, just buffer it */
        while (p < end) {
            state->buffer[state->buffer_size++] = *p++;
        }
        return;
    }

    /* Process buffered data first */
    if (state->buffer_size > 0) {
        uint32_t fill = 32 - state->buffer_size;
        for (uint32_t i = 0; i < fill; i++) {
            state->buffer[state->buffer_size + i] = p[i];
        }
        p += fill;

        state->v1 = xxh64_round(state->v1, xxh64_read64(state->buffer));
        state->v2 = xxh64_round(state->v2, xxh64_read64(state->buffer + 8));
        state->v3 = xxh64_round(state->v3, xxh64_read64(state->buffer + 16));
        state->v4 = xxh64_round(state->v4, xxh64_read64(state->buffer + 24));

        state->buffer_size = 0;
    }

    /* Process 32-byte blocks */
    if (p + 32 <= end) {
        const uint8_t *limit = end - 32;
        do {
            state->v1 = xxh64_round(state->v1, xxh64_read64(p));
            p += 8;
            state->v2 = xxh64_round(state->v2, xxh64_read64(p));
            p += 8;
            state->v3 = xxh64_round(state->v3, xxh64_read64(p));
            p += 8;
            state->v4 = xxh64_round(state->v4, xxh64_read64(p));
            p += 8;
        } while (p <= limit);
    }

    /* Buffer remaining bytes */
    while (p < end) {
        state->buffer[state->buffer_size++] = *p++;
    }
}

uint64_t xxhash64_digest(const XXHash64State *state) {
    const uint8_t *p = state->buffer;
    const uint8_t *end = p + state->buffer_size;
    uint64_t h64;

    if (state->total_len >= 32) {
        h64 = xxh64_rotl(state->v1, 1) + xxh64_rotl(state->v2, 7) +
              xxh64_rotl(state->v3, 12) + xxh64_rotl(state->v4, 18);

        h64 = xxh64_merge_round(h64, state->v1);
        h64 = xxh64_merge_round(h64, state->v2);
        h64 = xxh64_merge_round(h64, state->v3);
        h64 = xxh64_merge_round(h64, state->v4);
    } else {
        h64 = state->seed + XXHASH64_PRIME5;
    }

    h64 += state->total_len;

    /* Process remaining 8-byte chunks */
    while (p + 8 <= end) {
        uint64_t k1 = xxh64_round(0, xxh64_read64(p));
        h64 ^= k1;
        h64 = xxh64_rotl(h64, 27) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
        p += 8;
    }

    /* Process remaining 4-byte chunk */
    if (p + 4 <= end) {
        h64 ^= (uint64_t)xxh64_read32(p) * XXHASH64_PRIME1;
        h64 = xxh64_rotl(h64, 23) * XXHASH64_PRIME2 + XXHASH64_PRIME3;
        p += 4;
    }

    /* Process remaining bytes */
    while (p < end) {
        h64 ^= (uint64_t)(*p) * XXHASH64_PRIME5;
        h64 = xxh64_rotl(h64, 11) * XXHASH64_PRIME1;
        p++;
    }

    return xxh64_avalanche(h64);
}
