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
 * xxhash64.h - xxHash64 Implementation (Freestanding)
 *
 * Fast, deterministic 64-bit hash function for capsule content addressing.
 * No libc dependency - works in kernel context.
 *
 * Based on xxHash by Yann Collet (BSD-2-Clause license).
 * https://github.com/Cyan4973/xxHash
 */

#ifndef STARKERNEL_XXHASH64_H
#define STARKERNEL_XXHASH64_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Default seed for capsule hashing (deterministic) */
#define XXHASH64_CAPSULE_SEED   0ULL

/*===========================================================================
 * One-Shot Hashing
 *===========================================================================*/

/**
 * xxhash64 - Compute xxHash64 of a buffer
 *
 * @param data  Input data
 * @param len   Length in bytes
 * @param seed  Hash seed (use XXHASH64_CAPSULE_SEED for capsules)
 * @return 64-bit hash value
 */
uint64_t xxhash64(const void *data, size_t len, uint64_t seed);

/**
 * xxhash64_capsule - Hash capsule payload with standard seed
 *
 * Convenience wrapper using XXHASH64_CAPSULE_SEED.
 *
 * @param data  Capsule payload bytes
 * @param len   Payload length
 * @return Content hash (capsule_id)
 */
static inline uint64_t xxhash64_capsule(const void *data, size_t len) {
    return xxhash64(data, len, XXHASH64_CAPSULE_SEED);
}

/*===========================================================================
 * Streaming API (for large data)
 *===========================================================================*/

/** Streaming state structure */
typedef struct {
    uint64_t total_len;
    uint64_t v1;
    uint64_t v2;
    uint64_t v3;
    uint64_t v4;
    uint8_t  buffer[32];
    uint32_t buffer_size;
    uint64_t seed;
} XXHash64State;

/**
 * xxhash64_reset - Initialize streaming state
 *
 * @param state  State to initialize
 * @param seed   Hash seed
 */
void xxhash64_reset(XXHash64State *state, uint64_t seed);

/**
 * xxhash64_update - Feed data into streaming hash
 *
 * @param state  Streaming state
 * @param data   Input data
 * @param len    Length in bytes
 */
void xxhash64_update(XXHash64State *state, const void *data, size_t len);

/**
 * xxhash64_digest - Finalize and get hash value
 *
 * @param state  Streaming state
 * @return 64-bit hash value
 */
uint64_t xxhash64_digest(const XXHash64State *state);

#ifdef __cplusplus
}
#endif

#endif /* STARKERNEL_XXHASH64_H */
