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
              Block Subsystem - Layer 2: Mapping & Business Logic (v2)
              ---------------------------------------------------------
   Architecture:
     - Forth blocks 0-1023: RAM (fast, in-memory buffer cache)
     - Forth blocks 1024+:  Disk (persistent storage via blkio)
     - Device devblock 0:   Volume header (4 KiB)
     - Device devblocks 1..B: BAM (1-bit per 1 KiB Forth block), 4 KiB pages
     - Device devblocks (1+B)..end: payload; each 4 KiB packs 3×1 KiB data + 1 KiB metadata

   blkio NOTE:
     - blkio backends operate on 1 KiB units.
     - One 4 KiB “devblock” == 4 consecutive 1 KiB blkio blocks.

   License: See LICENSE file. No warranty.
*/
#ifndef STARFORTH_BLOCK_SUBSYSTEM_H
#define STARFORTH_BLOCK_SUBSYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include "vm.h"
#include "blkio.h"  /* ensure struct blkio_dev is fully visible */

#ifdef __cplusplus
extern "C" {
#endif

/* Core configuration constants */
#define BLK_FORTH_SIZE        1024u   /* Forth block size */
#define BLK_RAM_BLOCKS        1024u   /* RAM 0..1023 */
#define BLK_DISK_START        1024u   /* Disk-backed Forth blocks start */
#define BLK_DEVICE_SECTOR     4096u   /* Physical “devblock” size (4×1 KiB blkio units) */
#define BLK_PACK_RATIO        3u      /* 3× 1 KiB data per 4 KiB devblock (plus 1 KiB metadata) */
#define BLK_META_TOTAL        1024u   /* Last 1 KiB in a 4 KiB devblock is metadata */
#define BLK_META_PER_BLOCK    341u    /* 341×3 ~= 1023, padded to 1024 */

/* Forth-friendly reserved ranges */
#ifndef BLK_FORTH_SYS_RESERVED
#define BLK_FORTH_SYS_RESERVED   33u  /* RAM 0..32 reserved */
#endif
#ifndef BLK_DISK_SYS_RESERVED
#define BLK_DISK_SYS_RESERVED    32u  /* Disk 1024..1055 reserved (first N disk blocks) */
#endif

/* =========================
 * On-disk volume header v2
 * =========================
 * Serialized into device devblock 0 (4 KiB = 4×1 KiB blkio blocks 0..3).
 * BAM lives in external devblocks [bam_start .. bam_start+bam_devblocks-1] (each 4 KiB).
 * All *_devblocks indices are in 4 KiB units.
 */
typedef struct {
    /* Identification & versioning */
    uint32_t magic; /* 0x53544652 "STFR" */
    uint32_t version; /* 2 */

    /* Administrative info */
    uint32_t total_volumes;
    uint32_t flags;
    char label[64];

    /* Physical device geometry (4 KiB devblocks) */
    uint64_t total_devblocks; /* count of 4 KiB devblocks (blkio_info.total_blocks / 4) */

    /* BAM placement (external 1-bit bitmap region, stored in 4 KiB pages) */
    uint32_t bam_start; /* usually 1 */
    uint32_t bam_devblocks; /* number of 4 KiB pages used by BAM */
    uint32_t devblock_base; /* first payload devblock = bam_start + bam_devblocks */

    /* Capacity modeling (Forth 1 KiB blocks tracked/usable) */
    uint64_t tracked_blocks; /* 32768 * bam_devblocks (bits per 4 KiB page) */
    uint64_t total_blocks; /* min(tracked, 3 * (total_devblocks - 1 - bam_devblocks)) */
    uint64_t free_blocks;

    /* Allocation hints */
    uint64_t first_free; /* next free Forth block (>= 1024) */
    uint64_t last_allocated;

    /* Reserved low ranges */
    uint32_t reserved_disk_lo; /* e.g., 32 blocks reserved at 1024.. */
    uint32_t reserved_ram_lo; /* e.g., 33 blocks reserved at 0..32  */

    /* Timestamps (optional) */
    uint64_t created_time;
    uint64_t mounted_time;

    /* Optional integrity (unused yet) */
    uint64_t hdr_crc;

    /* Padding to keep header ≤ 4096 bytes */
    uint8_t _pad[4096 - (
                     4 + 4 + /* magic, version */
                     4 + 4 + 64 + /* total_volumes, flags, label */
                     8 + /* total_devblocks */
                     4 + 4 + 4 + /* bam_start, bam_devblocks, devblock_base */
                     8 + 8 + 8 + /* tracked_blocks, total_blocks, free_blocks */
                     8 + 8 + /* first_free, last_allocated */
                     4 + 4 + /* reserved ranges */
                     8 + 8 + /* timestamps */
                     8 /* hdr_crc */
                 )];
} blk_volume_meta_t;

/* Per-1 KiB block metadata (packed into top 1 KiB region of each 4 KiB sector). */
typedef struct {
    /* Core integrity (16 bytes) */
    uint64_t checksum; /* CRC64 of block data */
    uint64_t magic; /* 0x424C4B5F5354524BULL "BLK_STRK" */

    /* Timestamps (16 bytes) */
    uint64_t created_time; /* Unix timestamp (creation) */
    uint64_t modified_time; /* Unix timestamp (last write) */

    /* Block status (16 bytes) */
    uint64_t flags; /* Status flags */
    uint64_t write_count; /* Number of writes (wear leveling) */

    /* Content identification (32 bytes) */
    uint64_t content_type; /* 0=empty, 1=source, 2=data, ... */
    uint64_t encoding; /* 0=ASCII, 1=UTF-8, 2=binary, ... */
    uint64_t content_length; /* Actual data length (≤ 1024) */
    uint64_t reserved1; /* Alignment/future use */

    /* Cryptographic (64 bytes) */
    uint64_t entropy[4]; /* 256-bit entropy/random seed */
    uint64_t hash[4]; /* SHA-256 (optional) */

    /* Security & ownership (40 bytes) */
    uint64_t owner_id; /* User/process ID */
    uint64_t permissions; /* rwx-style permissions */
    uint64_t acl_block; /* Block number containing ACL (0=none) */
    uint64_t signature[2]; /* 128-bit signature */

    /* Link/chain support (32 bytes) */
    uint64_t prev_block; /* Previous in chain (0=none) */
    uint64_t next_block; /* Next in chain (0=none) */
    uint64_t parent_block; /* Parent/index (0=none) */
    uint64_t chain_length; /* Total blocks in chain */

    /* Application-specific (120 bytes) */
    uint64_t app_data[15]; /* 15×64-bit app-defined fields */

    /* Padding to reach 341-byte slice */
    uint8_t padding[5];
} blk_meta_t;

/* Error codes */
enum {
    BLK_OK = 0,
    BLK_EINVAL = -1,
    BLK_ERANGE = -2,
    BLK_EIO = -3,
    BLK_ENODEV = -4,
    BLK_ERESERVED = -5,
    BLK_EDIRTY = -6,
    BLK_ENOMEM = -7
};

/* ===== Public API ===== */
int blk_subsys_init(VM *vm, uint8_t *ram_base, size_t ram_size);

int blk_subsys_attach_device(struct blkio_dev *dev);

int blk_subsys_shutdown(void);

uint8_t *blk_get_buffer(uint32_t block_num, int writable);

uint8_t *blk_get_empty_buffer(uint32_t block_num);

int blk_flush(uint32_t block_num);

int blk_update(uint32_t block_num);

int blk_get_volume_meta(blk_volume_meta_t *meta);

int blk_set_volume_meta(const blk_volume_meta_t *meta);

int blk_is_valid(uint32_t block_num);

uint32_t blk_get_total_blocks(void);

int blk_get_meta(uint32_t block_num, blk_meta_t *meta);

int blk_set_meta(uint32_t block_num, const blk_meta_t *meta);

int blk_is_allocated(uint32_t block_num);

int blk_mark_allocated(uint32_t block_num);

int blk_mark_free(uint32_t block_num);

int blk_allocate(uint32_t * block_num);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STARFORTH_BLOCK_SUBSYSTEM_H */