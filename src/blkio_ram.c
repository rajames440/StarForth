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

/* Public helpers */
size_t blkio_ram_state_size(void) { return sizeof(blkio_ram_state_t); }

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

/* Vtable impl */
static int ram_open(blkio_dev_t *dev, const blkio_params_t *p);

static int ram_close(blkio_dev_t * dev);

static int ram_read(blkio_dev_t *dev, uint32_t fblock, void *dst);

static int ram_write(blkio_dev_t *dev, uint32_t fblock, const void *src);

static int ram_flush(blkio_dev_t * dev);
static int ram_info(blkio_dev_t * dev, blkio_info_t * out);

static const blkio_vtable_t BLKIO_RAM_VT = {
    .open = ram_open,
    .close = ram_close,
    .read = ram_read,
    .write = ram_write,
    .flush = ram_flush,
    .info = ram_info
};

const blkio_vtable_t *blkio_ram_vtable(void) { return &BLKIO_RAM_VT; }

/* Helpers */
static inline int in_bounds(const blkio_ram_state_t *st, uint32_t fb) {
    return st && (fb < st->total_blocks);
}

/* Methods */
static int ram_open(blkio_dev_t *dev, const blkio_params_t *p) {
    if (!dev || !p || !p->opaque) return BLKIO_EINVAL;
    blkio_ram_state_t *st = (blkio_ram_state_t *) p->opaque;
    if (!st->base || st->fbs == 0) return BLKIO_EINVAL;

    dev->state = st;
    dev->forth_block_size = st->fbs;
    dev->total_blocks = st->total_blocks;
    return BLKIO_OK;
}

static int ram_close(blkio_dev_t *dev) {
    if (!dev || !dev->state) return BLKIO_EINVAL;
    return BLKIO_OK; /* caller owns memory */
}

static int ram_read(blkio_dev_t *dev, uint32_t fblock, void *dst) {
    if (!dev || !dst) return BLKIO_EINVAL;
    blkio_ram_state_t *st = (blkio_ram_state_t *) dev->state;
    if (!in_bounds(st, fblock)) return BLKIO_EINVAL;
    const uint8_t *src = st->base + ((size_t) fblock * st->fbs);
    memcpy(dst, src, st->fbs);
    return BLKIO_OK;
}

static int ram_write(blkio_dev_t *dev, uint32_t fblock, const void *src) {
    if (!dev || !src) return BLKIO_EINVAL;
    blkio_ram_state_t *st = (blkio_ram_state_t *) dev->state;
    if (st->read_only) return BLKIO_ENOSUP;
    if (!in_bounds(st, fblock)) return BLKIO_EINVAL;
    uint8_t *dst = st->base + ((size_t) fblock * st->fbs);
    memcpy(dst, src, st->fbs);
    return BLKIO_OK;
}

static int ram_flush(blkio_dev_t *dev) {
    (void) dev;
    return BLKIO_OK;
}

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