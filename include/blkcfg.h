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
                         Block Subsystem Configuration API
                         ----------------------------------
   Purpose:
     - Provide a common, reusable configuration interface for selecting
       a "block subsystem" (FILE image vs RAM) and its geometry WITHOUT
       touching any real I/O or backend code.

   Notes:
     - No file system calls; no mmap; no blkio includes.
     - Geometries are computed deterministically from inputs only.
     - FILE backend: we record the path and intent; size/blocks remain 0
       until a real backend attaches later.
     - RAM backend: ram_mb -> size_bytes -> blocks (floor to FBS multiple).

   Typical flow:
     - Parse CLI flags (--disk-img=<path> | --ram-disk=<MB>).
     - Call blkcfg_init_from_options().
     - Use g_blkcfg elsewhere to make decisions (later you'll attach drivers).

   License: See LICENSE file. No warranty.

*/
#ifndef STARFORTH_BLKCFG_H
#define STARFORTH_BLKCFG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BLKCFG_DEFAULT_FBS
#define BLKCFG_DEFAULT_FBS 1024u  /* default Forth Block Size in bytes */
#endif

#ifndef BLKCFG_PATH_MAX
#define BLKCFG_PATH_MAX 4096
#endif

typedef enum {
    BLKCFG_BACKEND_NONE = 0,
    BLKCFG_BACKEND_FILE = 1,
    BLKCFG_BACKEND_RAM = 2
} blkcfg_backend_t;

typedef struct {
    blkcfg_backend_t backend; /* FILE or RAM (or NONE)       */
    uint32_t fbs; /* Forth Block Size (bytes)     */
    uint8_t read_only; /* 1 = intended RO, else 0      */

    /* Geometry (no I/O performed to derive these) */
    uint64_t size_bytes; /* For RAM: computed; FILE: 0   */
    uint32_t blocks; /* For RAM: computed; FILE: 0   */

    /* FILE plan (no I/O): path is recorded only */
    char file_path[BLKCFG_PATH_MAX];
} blkcfg_t;

/* Global config (optional convenience) */
extern blkcfg_t g_blkcfg;

/**
 * Initialize a block configuration from CLI-style options.
 *
 * Inputs:
 *  - disk_img_path: if non-NULL and non-empty => choose FILE backend
 *  - ram_mb:        megabytes for RAM fallback (only used if no disk image)
 *  - fbs:           forth block size (bytes). If 0 => BLKCFG_DEFAULT_FBS
 *  - read_only:     intended read-only flag (advisory)
 *
 * Behavior:
 *  - FILE: record path, set blocks/size_bytes = 0 (unknown until attached)
 *  - RAM : compute size_bytes = ram_mb*MiB; blocks = size_bytes/fbs (floored)
 *
 * Returns:
 *   0 on success; -1 on invalid args.
 */
int blkcfg_init_from_options(blkcfg_t *cfg,
                             const char *disk_img_path,
                             uint64_t ram_mb,
                             uint32_t fbs,
                             uint8_t read_only);

/**
 * Format a single-line summary into 'dst' (capacity 'dst_cap' bytes).
 * Example:
 *   "backend=FILE path=./disks/os.img fbs=1024 ro=0"
 *   "backend=RAM size=1024MB fbs=1024 blocks=1048576 ro=0"
 *
 * Returns number of bytes that would have been written (like snprintf).
 */
int blkcfg_format_summary(const blkcfg_t *cfg, char *dst, size_t dst_cap);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STARFORTH_BLKCFG_H */