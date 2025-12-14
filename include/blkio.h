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

#ifndef STARFORTH_BLKIO_H
#define STARFORTH_BLKIO_H

/* -----------------------------------------------------------------------------
   StarForth - Block I/O Core API
   - C99, deterministic core (no hidden malloc)
   - Public types / errors / thin inline wrapper
   - Backends provide a vtable implementing: open, close, read, write, flush, info

   License: CC0-1.0 / Public Domain. No warranty.
----------------------------------------------------------------------------- */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BLKIO_FORTH_BLOCK_SIZE
#define BLKIO_FORTH_BLOCK_SIZE 1024u
#endif

/* Error codes (negative = error) */
enum {
    BLKIO_OK = 0,
    BLKIO_EINVAL = -1,
    BLKIO_ENOSUP = -2,
    BLKIO_EIO = -3,
    BLKIO_ENOSPACE = -4,
    BLKIO_ECLOSED = -5
};

/* Forward decl */
struct blkio_dev;

/* Open parameters (generic) */
typedef struct {
    uint32_t forth_block_size; /* 0 => BLKIO_FORTH_BLOCK_SIZE */
    uint32_t total_blocks; /* number of Forth blocks; FILE may derive when 0 */
    void *opaque; /* backend-specific */
} blkio_params_t;

/* Device info */
typedef struct {
    uint32_t forth_block_size;
    uint32_t total_blocks;
    uint32_t phys_sector_size; /* best-effort; 0 if N/A */
    uint64_t phys_size_bytes; /* best-effort; 0 if N/A */
    uint8_t read_only;
} blkio_info_t;

/* Backend vtable */
typedef struct {
    int (*open)(struct blkio_dev *dev, const blkio_params_t *p);

    int (*close)(struct blkio_dev *dev);

    int (*read)(struct blkio_dev *dev, uint32_t fblock, void *dst);

    int (*write)(struct blkio_dev *dev, uint32_t fblock, const void *src);

    int (*flush)(struct blkio_dev *dev);

    int (*info)(struct blkio_dev *dev, blkio_info_t *out);
} blkio_vtable_t;

/* Device (public) */
typedef struct blkio_dev {
    const blkio_vtable_t *vt;
    uint32_t forth_block_size;
    uint32_t total_blocks;
    void *state; /* backend private */
    uint8_t is_open;
} blkio_dev_t;

/* Core wrappers */
static inline int blkio_open(blkio_dev_t *dev, const blkio_vtable_t *vt, const blkio_params_t *p) {
    if (!dev || !vt || !vt->open || !vt->close || !vt->read || !vt->write || !vt->flush || !vt->info)
        return BLKIO_EINVAL;
    dev->vt = vt;
    dev->is_open = 0u;
    dev->state = 0;
    dev->forth_block_size = (p && p->forth_block_size) ? p->forth_block_size : BLKIO_FORTH_BLOCK_SIZE;
    if (dev->forth_block_size == 0) return BLKIO_EINVAL;
    dev->total_blocks = p ? p->total_blocks : 0u;
    const int rc = vt->open(dev, p);
    if (rc == BLKIO_OK) dev->is_open = 1u;
    return rc;
}

static inline int blkio_close(blkio_dev_t *dev) {
    if (!dev || !dev->is_open) return BLKIO_ECLOSED;
    const int rc = dev->vt->close(dev);
    dev->is_open = 0u;
    return rc;
}

static inline int blkio_read(blkio_dev_t *dev, uint32_t fblock, void *dst) {
    if (!dev || !dev->is_open || !dst) return BLKIO_EINVAL;
    if (fblock >= dev->total_blocks) return BLKIO_EINVAL;
    return dev->vt->read(dev, fblock, dst);
}

static inline int blkio_write(blkio_dev_t *dev, uint32_t fblock, const void *src) {
    if (!dev || !dev->is_open || !src) return BLKIO_EINVAL;
    if (fblock >= dev->total_blocks) return BLKIO_EINVAL;
    return dev->vt->write(dev, fblock, src);
}

static inline int blkio_flush(blkio_dev_t *dev) {
    if (!dev || !dev->is_open) return BLKIO_ECLOSED;
    return dev->vt->flush(dev);
}

static inline int blkio_info(blkio_dev_t *dev, blkio_info_t *out) {
    if (!dev || !dev->is_open || !out) return BLKIO_EINVAL;
    const int rc = dev->vt->info(dev, out);
    if (rc == BLKIO_OK) {
        if (!out->forth_block_size) out->forth_block_size = dev->forth_block_size;
        if (!out->total_blocks) out->total_blocks = dev->total_blocks;
    }
    return rc;
}

/* Utilities */
static inline void blkio_bzero(void *p, uint32_t fbs) {
    uint8_t *q = (uint8_t *) p;
    for (uint32_t i = 0; i < fbs; i++) q[i] = 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STARFORTH_BLKIO_H */