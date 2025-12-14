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

#ifndef STARFORTH_BLKIO_FACTORY_H
#define STARFORTH_BLKIO_FACTORY_H

/* -----------------------------------------------------------------------------
   StarForth - Block I/O Factory (Public API)
   Chooses FILE backend when a disk image path is provided; otherwise RAM.
   No dynamic allocation; caller supplies backend state memory.

   NOTE: This header avoids referencing blkio typedefs to prevent "unknown type"
   errors when include order or local headers differ. We forward-declare only the
   device struct here. The implementation includes "blkio.h".
   ----------------------------------------------------------------------------- */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration; real definition is in blkio.h */
struct blkio_dev;

/**
 * Open a block device using either a file-backed image (if disk_img_path is
 * non-empty) or a RAM-backed buffer (fallback).
 *
 * Parameters:
 *  - dev            : output device object (caller-allocated)
 *  - disk_img_path  : path to raw image; if NULL/empty => RAM fallback
 *  - read_only      : 1 = open read-only; 0 = read/write
 *  - total_blocks   : FILE: 0 => derive from file size; >0 => size hint
 *                     RAM : must match ram_blocks
 *  - fbs            : forth block size in bytes (0 => default per blkio.h)
 *  - state_mem      : caller-provided backend state buffer (opaque to caller)
 *  - state_len      : size of state_mem in bytes
 *  - ram_base       : RAM storage base pointer (RAM backend only)
 *  - ram_blocks     : number of Forth blocks in ram_base (RAM backend only)
 *  - out_used_file  : optional; set to 1 if FILE backend selected, else 0
 *
 * Returns:
 *   0 on success; negative blkio error code on failure.
 */
int blkio_factory_open(struct blkio_dev *dev,
                       const char *disk_img_path,
                       uint8_t read_only,
                       uint32_t total_blocks,
                       uint32_t fbs,
                       void *state_mem,
                       size_t state_len,
                       uint8_t *ram_base,
                       uint32_t ram_blocks,
                       uint8_t *out_used_file);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STARFORTH_BLKIO_FACTORY_H */