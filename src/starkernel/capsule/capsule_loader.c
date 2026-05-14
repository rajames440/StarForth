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
 * capsule_loader.c - Block Capsule Loader and Executor (M7.1)
 *
 * Parses a capsule payload for "Block <num>" headers and implements
 * the full init sequence:
 *
 *   1. Write blocks to ramdrive (2048+) — establishes device content
 *   2. Copy from ramdrive N to dedicated RAM N-2048
 *   3. Execute entry block from dedicated RAM via LOAD
 *   4. Zero dedicated RAM blocks to free them for userspace
 *
 * The ramdrive (2048+) is the fallback device when no physical block
 * device is mounted.  Execution always runs from the dedicated RAM
 * region (0–2047).
 *
 * Block number mapping: dest = source - CAPSULE_RAM_OFFSET (2048).
 * Conflicts and out-of-range sources are silently swallowed.
 * TODO: proper conflict and bounds handling in a future enhancement.
 */

#include "starkernel/capsule_loader.h"
#include "starkernel/capsule.h"
#include "starkernel/capsule_run.h"
#include "block_subsystem.h"
#include "vm.h"

/* Ramdrive starts at this block number; dest = source - CAPSULE_RAM_OFFSET */
#define CAPSULE_RAM_OFFSET  2048u
#define LOADER_BLOCK_SIZE   1024u

/*===========================================================================
 * Internal: header detection
 *
 * Tests whether the bytes at p begin a "Block <num>" header line.
 * On match writes the block number to *out_num and the byte position
 * immediately after the header's newline to *out_after.
 * Returns 1 on match, 0 otherwise.
 *===========================================================================*/

static int is_block_header(const uint8_t *p, const uint8_t *end,
                            uint32_t *out_num, const uint8_t **out_after)
{
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

    while (q < end && *q != '\n') q++;
    if (q < end) q++;

    *out_num   = num;
    *out_after = q;
    return 1;
}

/*===========================================================================
 * Internal: write one block to the ramdrive (device region, 2048+)
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
                        ? LOADER_BLOCK_SIZE : content_len;
    for (i = 0; i < copy_len; i++) buf[i] = content[i];

    blk_update(block_num);
}

/*===========================================================================
 * Internal: copy one ramdrive block to its dedicated RAM slot (N-2048)
 *
 * Source block N must be > CAPSULE_RAM_OFFSET.
 * Conflicts and failures are silently swallowed.
 * TODO: proper conflict and bounds handling.
 *===========================================================================*/

static void copy_block_to_ram(uint32_t source_block_num)
{
    if (source_block_num <= CAPSULE_RAM_OFFSET) return; /* TODO */

    uint32_t dest = source_block_num - CAPSULE_RAM_OFFSET;

    uint8_t *src = blk_get_buffer(source_block_num, 0);
    uint8_t *dst = blk_get_buffer(dest, 1);
    if (!src || !dst) return; /* TODO */

    uint32_t i;
    for (i = 0; i < LOADER_BLOCK_SIZE; i++) dst[i] = 0;
    for (i = 0; i < LOADER_BLOCK_SIZE; i++) dst[i] = src[i];

    blk_update(dest);
}

/*===========================================================================
 * Internal: uint32 to decimal string
 * Returns number of characters written (not including null terminator).
 *===========================================================================*/

static size_t u32_to_dec(uint32_t val, char *buf, size_t size)
{
    char tmp[11];
    size_t len = 0;
    if (size < 2u) return 0;
    if (val == 0u) {
        tmp[len++] = '0';
    } else {
        while (val > 0u && len < 10u) {
            tmp[len++] = '0' + (char)(val % 10u);
            val /= 10u;
        }
    }
    size_t out = 0;
    while (len > 0u && out < size - 1u) buf[out++] = tmp[--len];
    buf[out] = '\0';
    return out;
}

/*===========================================================================
 * capsule_load_blocks
 *===========================================================================*/

int capsule_load_blocks(const uint8_t *payload, uint64_t length,
                        uint32_t *out_entry_block)
{
    if (!payload || length == 0) return -1;

    if (out_entry_block) *out_entry_block = 0;

    const uint8_t *p          = payload;
    const uint8_t *end        = payload + length;
    int            blocks_written    = 0;
    int            in_block          = 0;
    int            entry_set         = 0;
    uint32_t       current_block_num = 0;
    const uint8_t *block_start       = (void *)0;

    while (p < end) {
        uint32_t       block_num;
        const uint8_t *content_start;

        if (is_block_header(p, end, &block_num, &content_start)) {
            if (in_block) {
                uint32_t clen = (uint32_t)(p - block_start);
                write_ramdrive_block(current_block_num, block_start, clen);
                blocks_written++;
            }
            current_block_num = block_num;
            block_start       = content_start;
            in_block          = 1;
            if (!entry_set && out_entry_block) {
                *out_entry_block = block_num;
                entry_set = 1;
            }
            p = content_start;
        } else {
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
        }
    }

    if (in_block) {
        uint32_t clen = (uint32_t)(end - block_start);
        write_ramdrive_block(current_block_num, block_start, clen);
        blocks_written++;
    }

    return blocks_written;
}

/*===========================================================================
 * Internal: walk payload headers and copy each ramdrive block to RAM
 *===========================================================================*/

static void copy_device_to_ram(const uint8_t *payload, uint64_t length)
{
    const uint8_t *p   = payload;
    const uint8_t *end = payload + length;
    int      in_block          = 0;
    uint32_t current_block_num = 0;

    while (p < end) {
        uint32_t       block_num;
        const uint8_t *content_start;

        if (is_block_header(p, end, &block_num, &content_start)) {
            if (in_block) copy_block_to_ram(current_block_num);
            current_block_num = block_num;
            in_block          = 1;
            p                 = content_start;
        } else {
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
        }
    }

    if (in_block) copy_block_to_ram(current_block_num);
}

/*===========================================================================
 * capsule_clear_blocks — zero dedicated RAM slots (N-2048) for userspace
 *===========================================================================*/

void capsule_clear_blocks(const uint8_t *payload, uint64_t length)
{
    if (!payload || length == 0) return;

    const uint8_t *p   = payload;
    const uint8_t *end = payload + length;

    while (p < end) {
        uint32_t       block_num;
        const uint8_t *content_start;

        if (is_block_header(p, end, &block_num, &content_start)) {
            if (block_num > CAPSULE_RAM_OFFSET) {
                uint32_t dest = block_num - CAPSULE_RAM_OFFSET;
                uint8_t *buf  = blk_get_buffer(dest, 1);
                if (buf) {
                    uint32_t i;
                    for (i = 0; i < LOADER_BLOCK_SIZE; i++) buf[i] = 0;
                    blk_update(dest);
                }
            }
            p = content_start;
        } else {
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
        }
    }
}

/*===========================================================================
 * capsule_exec_init — full load / execute / clear sequence
 *===========================================================================*/

CapsuleRunResult capsule_exec_init(
    void                   *vm_opaque,
    const char             *capsule_name,
    const CapsuleDirHeader *dir,
    const CapsuleDesc      *descs,
    const CapsuleNameEntry *names,
    const uint8_t          *arena)
{
    if (!vm_opaque || !capsule_name || !dir || !descs || !names || !arena)
        return CAPSULE_RUN_ERR_INVALID;

    VM *vm = (VM *)vm_opaque;

    /* Locate and validate capsule */
    const CapsuleDesc *cap = capsule_find_by_name(dir, descs, names, capsule_name);
    if (!cap) return CAPSULE_RUN_ERR_INVALID;

    CapsuleValidateResult vr = capsule_validate(cap, arena, dir->arena_size, 1);
    if (vr != CAPSULE_VALID) return CAPSULE_RUN_ERR_INVALID;

    const uint8_t *payload = capsule_get_payload(cap, arena);
    if (!payload) return CAPSULE_RUN_ERR_INVALID;

    /* Step 1: write to ramdrive (establishes device content) */
    uint32_t entry_block = 0;
    int n = capsule_load_blocks(payload, cap->length, &entry_block);
    if (n < 1 || entry_block == 0 || entry_block <= CAPSULE_RAM_OFFSET)
        return CAPSULE_RUN_ERR_INVALID;

    /* Step 2: copy from ramdrive (N) to dedicated RAM (N-2048) */
    /* TODO: conflict handling */
    copy_device_to_ram(payload, cap->length);

    /* Step 3: execute entry block from dedicated RAM */
    uint32_t ram_entry = entry_block - CAPSULE_RAM_OFFSET;

    char   load_cmd[32];
    size_t nc = u32_to_dec(ram_entry, load_cmd, sizeof(load_cmd) - 6u);
    load_cmd[nc++] = ' ';
    load_cmd[nc++] = 'L';
    load_cmd[nc++] = 'O';
    load_cmd[nc++] = 'A';
    load_cmd[nc++] = 'D';
    load_cmd[nc]   = '\0';

    vm->error = 0;
    vm_interpret(vm, load_cmd);
    CapsuleRunResult result = vm->error ? CAPSULE_RUN_ERR_EXEC_FAIL
                                        : CAPSULE_RUN_OK;
    vm->error = 0;

    /* Step 4: zero dedicated RAM blocks — free for userspace */
    capsule_clear_blocks(payload, cap->length);

    return result;
}
