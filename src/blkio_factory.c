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
// StarForth - Block I/O Factory (Implementation)
// -----------------------------------------------------------------------------

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "blkio.h"
#include "blkio_factory.h"

/* Backends */
extern size_t blkio_file_state_size(void);

extern const blkio_vtable_t *blkio_file_vtable(void);

extern int blkio_file_init_state(void *state_mem, size_t state_len,
                                 const char *path,
                                 uint32_t total_blocks,
                                 uint32_t fbs,
                                 uint8_t read_only,
                                 uint8_t create_if_missing,
                                 uint8_t truncate_to_size,
                                 void **out_opaque);

extern size_t blkio_ram_state_size(void);

extern const blkio_vtable_t *blkio_ram_vtable(void);

extern int blkio_ram_init_state(void *state_mem, size_t state_len,
                                uint8_t *base, uint32_t total_blocks,
                                uint32_t fbs, uint8_t read_only,
                                void **out_opaque);

/** @brief Return 1 if string @c s is non-NULL and non-empty. */
static int nonempty(const char *s) { return (s && *s); }

/**
 * @brief Open a block I/O device using the best available backend.
 *
 * Selects the file backend when @c disk_img_path is non-empty, otherwise
 * falls back to the RAM backend using @c ram_base / @c ram_blocks.
 *
 * File backend behaviour:
 * - Creates the file if it does not exist (unless @c read_only is set).
 * - Derives @c total_blocks from the file size when @c total_blocks == 0.
 * - Sets @c *out_used_file = 1 on success.
 *
 * RAM backend behaviour:
 * - @c ram_base must point to at least @c ram_blocks * @c fbs bytes of
 *   caller-owned memory that outlives the device.
 *
 * @param dev           Output device handle to populate
 * @param disk_img_path Path to disk image file, or NULL/empty for RAM
 * @param read_only     1 = open read-only, 0 = read-write
 * @param total_blocks  Desired block count (0 = derive from file size)
 * @param fbs           FORTH block size in bytes (0 = use @c BLKIO_FORTH_BLOCK_SIZE)
 * @param state_mem     Caller-allocated state buffer (must be large enough)
 * @param state_len     Size of @c state_mem in bytes
 * @param ram_base      Base of RAM block array (used only when no disk path)
 * @param ram_blocks    Number of blocks in @c ram_base
 * @param out_used_file Optional output: set to 1 if the file backend was used
 * @return @c BLKIO_OK on success, or a @c BLKIO_E* error code
 */
int blkio_factory_open(blkio_dev_t *dev,
                       const char *disk_img_path,
                       uint8_t read_only,
                       uint32_t total_blocks,
                       uint32_t fbs,
                       void *state_mem,
                       size_t state_len,
                       uint8_t *ram_base,
                       uint32_t ram_blocks,
                       uint8_t *out_used_file) {
    if (out_used_file) *out_used_file = 0;
    if (!dev || !state_mem || state_len == 0) return BLKIO_EINVAL;

    const uint32_t efbs = (fbs ? fbs : BLKIO_FORTH_BLOCK_SIZE);

    if (nonempty(disk_img_path)) {
        const size_t need = blkio_file_state_size();
        if (state_len < need) return BLKIO_ENOSPACE;

        void *opaque = NULL;
        const uint8_t create_if_missing = read_only ? 0u : 1u;
        const uint8_t truncate_to_size = (!read_only && total_blocks > 0) ? 1u : 0u;

        int rc = blkio_file_init_state(state_mem, state_len,
                                       disk_img_path,
                                       total_blocks,
                                       efbs,
                                       read_only,
                                       create_if_missing,
                                       truncate_to_size,
                                       &opaque);
        if (rc != BLKIO_OK) return rc;

        blkio_params_t p = {
            .forth_block_size = efbs,
            .total_blocks = total_blocks, /* 0 => derive in file_open */
            .opaque = opaque
        };

        rc = blkio_open(dev, blkio_file_vtable(), &p);
        if (rc == BLKIO_OK && out_used_file) *out_used_file = 1u;
        return rc;
    }

    if (!ram_base || ram_blocks == 0) return BLKIO_EINVAL;

    const size_t need_ram = blkio_ram_state_size();
    if (state_len < need_ram) return BLKIO_ENOSPACE;

    void *opaque = NULL;
    int rc = blkio_ram_init_state(state_mem, state_len,
                                  ram_base, ram_blocks,
                                  efbs, read_only,
                                  &opaque);
    if (rc != BLKIO_OK) return rc;

    const blkio_params_t p = {
        .forth_block_size = efbs,
        .total_blocks = ram_blocks,
        .opaque = opaque
    };

    return blkio_open(dev, blkio_ram_vtable(), &p);
}