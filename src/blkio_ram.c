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

// -----------------------------------------------------------------------------
// StarForth - RAM Block I/O backend
// -----------------------------------------------------------------------------

#include "blkio.h"
#include <string.h>

typedef struct {
    uint8_t *base;
    uint32_t total_blocks;
    uint32_t fbs;
    uint8_t read_only;
} blkio_ram_state_t;

/**
 * @brief Return the byte size required for a @c blkio_ram_state_t.
 * @return sizeof(blkio_ram_state_t)
 */
size_t blkio_ram_state_size(void) { return sizeof(blkio_ram_state_t); }

/**
 * @brief Initialise a @c blkio_ram_state_t in caller-provided memory.
 *
 * Stores @c base, @c total_blocks, @c fbs (defaulting to
 * @c BLKIO_FORTH_BLOCK_SIZE if zero), and @c read_only into the state struct
 * at @c state_mem. Sets @c *out_opaque to point to the state. The caller
 * owns the @c base memory and must ensure it remains valid for the device lifetime.
 *
 * @param state_mem    Caller-allocated buffer (must be >= blkio_ram_state_size())
 * @param state_len    Size of @c state_mem in bytes
 * @param base         Pointer to the RAM block array
 * @param total_blocks Number of FORTH blocks in @c base
 * @param fbs          FORTH block size in bytes (0 = use default)
 * @param read_only    1 = deny writes, 0 = allow reads and writes
 * @param out_opaque   Output: set to @c state_mem on success
 * @return @c BLKIO_OK on success, @c BLKIO_EINVAL on bad arguments
 */
int blkio_ram_init_state(void *state_mem, size_t state_len,
                         uint8_t *base, uint32_t total_blocks,
                         uint32_t fbs, uint8_t read_only,
                         void **out_opaque) {
    if (!state_mem || state_len < sizeof(blkio_ram_state_t) || !base || !out_opaque)
        return BLKIO_EINVAL;

    blkio_ram_state_t *st = (blkio_ram_state_t *) state_mem;
    st->base = base;
    st->total_blocks = total_blocks;
    st->fbs = (fbs ? fbs : BLKIO_FORTH_BLOCK_SIZE);
    st->read_only = read_only ? 1u : 0u;
    *out_opaque = (void *) st;
    return BLKIO_OK;
}

/** @brief Attach state to the device handle and validate parameters. */
static int ram_open(blkio_dev_t *dev, const blkio_params_t *p);

/** @brief Close the RAM device (no-op; caller owns the memory). */
static int ram_close(blkio_dev_t * dev);

/** @brief Read one FORTH block from the RAM array into @c dst. */
static int ram_read(blkio_dev_t *dev, uint32_t fblock, void *dst);

/** @brief Write one FORTH block from @c src into the RAM array. */
static int ram_write(blkio_dev_t *dev, uint32_t fblock, const void *src);

/** @brief Flush the RAM device — always a no-op since data is in memory. */
static int ram_flush(blkio_dev_t * dev);

/** @brief Return device info (block size, count, physical size, read-only flag). */
static int ram_info(blkio_dev_t * dev, blkio_info_t * out);

static const blkio_vtable_t BLKIO_RAM_VT = {
    .open = ram_open,
    .close = ram_close,
    .read = ram_read,
    .write = ram_write,
    .flush = ram_flush,
    .info = ram_info
};

/**
 * @brief Return the vtable for the RAM block I/O backend.
 * @return Pointer to the static @c blkio_vtable_t for RAM devices
 */
const blkio_vtable_t *blkio_ram_vtable(void) { return &BLKIO_RAM_VT; }

/**
 * @brief Test whether @c fb is a valid block number for the RAM device.
 * @param st RAM state (may be NULL)
 * @param fb Block number to test
 * @return 1 if in bounds, 0 otherwise
 */
static inline int in_bounds(const blkio_ram_state_t *st, uint32_t fb) {
    return st && (fb < st->total_blocks);
}

/**
 * @brief Attach the RAM state to the device handle.
 *
 * Validates that @c base and @c fbs are non-zero, then sets
 * @c dev->state, @c dev->forth_block_size, and @c dev->total_blocks.
 *
 * @param dev Uninitialised device handle
 * @param p   Parameters with pre-initialised opaque RAM state pointer
 * @return @c BLKIO_OK on success, @c BLKIO_EINVAL on bad arguments
 */
static int ram_open(blkio_dev_t *dev, const blkio_params_t *p) {
    if (!dev || !p || !p->opaque) return BLKIO_EINVAL;
    blkio_ram_state_t *st = (blkio_ram_state_t *) p->opaque;
    if (!st->base || st->fbs == 0) return BLKIO_EINVAL;

    dev->state = st;
    dev->forth_block_size = st->fbs;
    dev->total_blocks = st->total_blocks;
    return BLKIO_OK;
}

/**
 * @brief Close the RAM device — caller retains ownership of the RAM buffer.
 *
 * @param dev Device handle
 * @return @c BLKIO_OK always (no resources to release)
 */
static int ram_close(blkio_dev_t *dev) {
    if (!dev || !dev->state) return BLKIO_EINVAL;
    return BLKIO_OK; /* caller owns memory */
}

/**
 * @brief Copy one FORTH block from the RAM array to @c dst.
 *
 * @param dev    Open RAM device
 * @param fblock Logical block number (must be < @c total_blocks)
 * @param dst    Destination buffer (must be >= @c fbs bytes)
 * @return @c BLKIO_OK on success, @c BLKIO_EINVAL if out of range or NULL pointer
 */
static int ram_read(blkio_dev_t *dev, uint32_t fblock, void *dst) {
    if (!dev || !dst) return BLKIO_EINVAL;
    blkio_ram_state_t *st = (blkio_ram_state_t *) dev->state;
    if (!in_bounds(st, fblock)) return BLKIO_EINVAL;
    const uint8_t *src = st->base + ((size_t) fblock * st->fbs);
    memcpy(dst, src, st->fbs);
    return BLKIO_OK;
}

/**
 * @brief Copy one FORTH block from @c src into the RAM array.
 *
 * @param dev    Open RAM device (must not be read-only)
 * @param fblock Logical block number (must be < @c total_blocks)
 * @param src    Source buffer (must be >= @c fbs bytes)
 * @return @c BLKIO_OK on success, @c BLKIO_ENOSUP if read-only,
 *         @c BLKIO_EINVAL if out of range or NULL pointer
 */
static int ram_write(blkio_dev_t *dev, uint32_t fblock, const void *src) {
    if (!dev || !src) return BLKIO_EINVAL;
    blkio_ram_state_t *st = (blkio_ram_state_t *) dev->state;
    if (st->read_only) return BLKIO_ENOSUP;
    if (!in_bounds(st, fblock)) return BLKIO_EINVAL;
    uint8_t *dst = st->base + ((size_t) fblock * st->fbs);
    memcpy(dst, src, st->fbs);
    return BLKIO_OK;
}

/**
 * @brief Flush the RAM device — no-op since data is always in memory.
 *
 * @param dev Device handle (unused)
 * @return @c BLKIO_OK always
 */
static int ram_flush(blkio_dev_t *dev) {
    (void) dev;
    return BLKIO_OK;
}

/**
 * @brief Return device info for the RAM backend.
 *
 * Physical sector size equals @c fbs (RAM has no physical sector constraint).
 * Physical size in bytes = @c fbs * @c total_blocks.
 *
 * @param dev Open RAM device
 * @param out Output struct to populate
 * @return @c BLKIO_OK on success, @c BLKIO_EINVAL if @c dev or @c out is NULL
 */
static int ram_info(blkio_dev_t *dev, blkio_info_t *out) {
    if (!dev || !out) return BLKIO_EINVAL;
    blkio_ram_state_t *st = (blkio_ram_state_t *) dev->state;
    out->forth_block_size = st->fbs;
    out->total_blocks = st->total_blocks;
    out->phys_sector_size = st->fbs;
    out->phys_size_bytes = (uint64_t) st->fbs * (uint64_t) st->total_blocks;
    out->read_only = st->read_only;
    return BLKIO_OK;
}