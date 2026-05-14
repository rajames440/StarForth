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
 * capsule_loader.c - Block Capsule Loader (M7.1)
 *
 * Scans a capsule payload for "Block <num>" header lines and writes
 * each block's content into the corresponding ramdrive slot.
 *
 * Rules:
 *   - A header line begins with the literal bytes "Block " followed
 *     by one or more decimal digits.  Anything after the digits on
 *     that line is ignored.
 *   - Content runs from the byte after the header's newline up to
 *     (but not including) the next header line, or end of payload.
 *   - Blocks may appear in any order.
 *   - Each ramdrive slot is zeroed before content is copied in.
 *   - Content exceeding 1024 bytes is silently truncated.
 *   - Content may be text or binary; no interpretation is performed.
 *   - Anything before the first header is silently ignored.
 */

#include "starkernel/capsule_loader.h"
#include "block_subsystem.h"

#define LOADER_BLOCK_SIZE 1024u

/*===========================================================================
 * Internal: header detection
 *
 * Tests whether the bytes at p..end begin a "Block <num>" header line.
 * If matched, writes the block number to *out_num and the byte position
 * immediately after the header's newline (or end if no newline) to
 * *out_after.  Returns 1 on match, 0 otherwise.
 *===========================================================================*/

static int is_block_header(const uint8_t *p, const uint8_t *end,
                            uint32_t *out_num, const uint8_t **out_after)
{
    /* Need at least "Block " (6) + one digit (1) = 7 bytes */
    if ((size_t)(end - p) < 7u) return 0;

    if (p[0] != 'B') return 0;
    if (p[1] != 'l') return 0;
    if (p[2] != 'o') return 0;
    if (p[3] != 'c') return 0;
    if (p[4] != 'k') return 0;
    if (p[5] != ' ') return 0;

    const uint8_t *q = p + 6;
    if (q >= end || *q < '0' || *q > '9') return 0;

    uint32_t num = 0;
    while (q < end && *q >= '0' && *q <= '9') {
        num = num * 10u + (uint32_t)(*q - '0');
        q++;
    }

    /* Skip the rest of the header line */
    while (q < end && *q != '\n') q++;
    if (q < end) q++;   /* consume the newline */

    *out_num   = num;
    *out_after = q;
    return 1;
}

/*===========================================================================
 * Internal: write one block to the ramdrive
 *
 * Zeros the 1024-byte slot, copies up to 1024 bytes of content, then
 * marks the block dirty.  content_len > 1024 is silently truncated.
 *===========================================================================*/

static void write_ramdrive_block(uint32_t block_num,
                                 const uint8_t *content,
                                 uint32_t content_len)
{
    uint8_t *buf = blk_get_buffer(block_num, 1);
    if (!buf) return;

    uint32_t i;
    for (i = 0; i < LOADER_BLOCK_SIZE; i++) buf[i] = 0;

    uint32_t copy_len = (content_len > LOADER_BLOCK_SIZE)
                        ? LOADER_BLOCK_SIZE
                        : content_len;
    for (i = 0; i < copy_len; i++) buf[i] = content[i];

    blk_update(block_num);
}

/*===========================================================================
 * capsule_load_blocks
 *===========================================================================*/

int capsule_load_blocks(const uint8_t *payload, uint64_t length)
{
    if (!payload || length == 0) return -1;

    const uint8_t *p          = payload;
    const uint8_t *end        = payload + length;
    int            blocks_written    = 0;
    int            in_block          = 0;
    uint32_t       current_block_num = 0;
    const uint8_t *block_start       = (void *)0;

    while (p < end) {
        uint32_t       block_num;
        const uint8_t *content_start;

        if (is_block_header(p, end, &block_num, &content_start)) {
            /* Flush the block we were accumulating, if any */
            if (in_block) {
                uint32_t clen = (uint32_t)(p - block_start);
                write_ramdrive_block(current_block_num, block_start, clen);
                blocks_written++;
            }

            /* Begin accumulating the new block */
            current_block_num = block_num;
            block_start       = content_start;
            in_block          = 1;
            p                 = content_start;
        } else {
            /* Not a header line — advance to the next line */
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
        }
    }

    /* Flush the final block */
    if (in_block) {
        uint32_t clen = (uint32_t)(end - block_start);
        write_ramdrive_block(current_block_num, block_start, clen);
        blocks_written++;
    }

    return blocks_written;
}
