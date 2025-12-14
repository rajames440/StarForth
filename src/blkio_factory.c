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

static int nonempty(const char *s) { return (s && *s); }

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