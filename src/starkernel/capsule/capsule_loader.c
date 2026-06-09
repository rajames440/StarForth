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
#ifdef __STARKERNEL__
#include "starkernel/console.h"
#endif

/* Ramdrive starts at this block number; dest = source - CAPSULE_RAM_OFFSET */
#define CAPSULE_RAM_OFFSET   2048u
#define LOADER_BLOCK_SIZE    1024u

/* FORTH block geometry: 16 lines × 64 characters = 1024 bytes */
#define CAPSULE_BLOCK_LINES  16
#define CAPSULE_LINE_COLS    64

/*
 * Execution line buffer — capsule text (the .4th source representation)
 * may use lines longer than the canonical 64-column block storage limit.
 * This buffer covers the execution path; CAPSULE_LINE_COLS covers storage
 * and the 16-bit bitmask geometry.
 */
#define CAPSULE_EXEC_LINE_MAX 256

/*
 * Forward-definition retry budget.
 *
 * When a block line triggers an error (typically UNKNOWN WORD for a
 * forward reference) the loader skips that line and retries the entire
 * block from the top.  Each retry sets one bit in a uint16_t bitmask
 * (one bit per line 0-15).  When the mask reaches 0xFFFF all 16 line
 * slots are exhausted and the block is declared unloadable; the loader
 * panics with a per-line report.
 *
 * 16 skipped lines per 1024-byte block matches the FORTH block geometry
 * exactly — if every line is unresolvable the capsule is fundamentally
 * broken.
 */
#define CAPSULE_MAX_DEFERRED CAPSULE_BLOCK_LINES

#ifdef __STARKERNEL__
/*===========================================================================
 * Kernel ramdrive: a heap-allocated buffer covering blocks
 * CAPSULE_RAM_OFFSET .. CAPSULE_RAM_OFFSET + KRD_MAX_BLOCKS - 1.
 *
 * Bypasses the block subsystem for the ramdrive region so that
 * capsule loading works without a blkio disk device in kernel context.
 *===========================================================================*/
#define KRD_MAX_BLOCKS 1024u
static uint8_t *krd_base;

void capsule_loader_set_ramdrive(uint8_t *buf) { krd_base = buf; }

static uint8_t *krd_ptr(uint32_t block_num)
{
    if (!krd_base) return NULL;
    if (block_num < CAPSULE_RAM_OFFSET) return NULL;
    uint32_t idx = block_num - CAPSULE_RAM_OFFSET;
    if (idx >= KRD_MAX_BLOCKS) return NULL;
    return krd_base + (size_t)idx * LOADER_BLOCK_SIZE;
}

/*
 * capsule_blk_init - Initialize block subsystem + kernel ramdrive.
 *
 * Called from kernel_main before capsule_exec_init.
 * ram_buf: 1MB heap buffer for dedicated RAM blocks (0-991)
 * krd_buf: 1MB heap buffer for ramdrive blocks (2048-3071), must be pre-zeroed
 */
int capsule_blk_init(void *vm, uint8_t *ram_buf, size_t ram_size, uint8_t *krd_buf)
{
    int rc = blk_subsys_init((VM *)vm, ram_buf, ram_size);
    if (rc == 0) capsule_loader_set_ramdrive(krd_buf);
    return rc;
}
#endif /* __STARKERNEL__ */

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
#ifdef __STARKERNEL__
    uint8_t *buf = krd_ptr(block_num);
#else
    uint8_t *buf = blk_get_buffer(block_num, 1);
#endif
    if (!buf) return;

    uint32_t i;
    for (i = 0; i < LOADER_BLOCK_SIZE; i++) buf[i] = 0;

    uint32_t copy_len = (content_len > LOADER_BLOCK_SIZE)
                        ? LOADER_BLOCK_SIZE : content_len;
    for (i = 0; i < copy_len; i++) buf[i] = content[i];

#ifndef __STARKERNEL__
    blk_update(block_num);
#endif
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

#ifdef __STARKERNEL__
    uint8_t *src = krd_ptr(source_block_num);
#else
    uint8_t *src = blk_get_buffer(source_block_num, 0);
#endif
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
 * exec_block_with_retry — execute one block with forward-reference tolerance
 *
 * Executes the block line by line (max CAPSULE_BLOCK_LINES = 16 lines,
 * each clamped to CAPSULE_LINE_COLS = 64 characters — canonical FORTH
 * block geometry).
 *
 * On any error the offending line is recorded in a uint16_t bitmask
 * (one bit per line 0-15) and the entire block is retried from the top.
 * Deferred lines are skipped in O(1) on every subsequent pass.
 *
 * Terminates when:
 *   a) block executes without error → returns 0 (deferred lines are
 *      forward references to be resolved later; logged but not fatal)
 *   b) deferred_mask == 0xFFFF (all 16 line slots exhausted) → returns -1
 *
 * VM compile/interpret mode is reset to MODE_INTERPRET before each retry
 * so a half-compiled colon definition cannot corrupt the next attempt.
 *===========================================================================*/

static int popcount16(uint16_t v)
{
    int c = 0;
    while (v) { c += (int)(v & 1u); v >>= 1; }
    return c;
}

static int exec_block_with_retry(VM *vm, const uint8_t *block_start,
                                  size_t block_len, uint32_t block_num)
{
    uint16_t deferred_mask = 0u;
    char     deferred_text[CAPSULE_BLOCK_LINES][CAPSULE_LINE_COLS + 1];
    int      j;

    for (j = 0; j < CAPSULE_BLOCK_LINES; j++) deferred_text[j][0] = '\0';

    for (;;) {
        /* Reset interpret/compile state before each attempt */
        vm->error     = 0;
        vm->mode      = MODE_INTERPRET;
        vm->state_var = 0;
        vm_store_cell(vm, vm->state_addr, 0);

        int            error_this_pass = 0;
        int            error_line      = 0;   /* set before use; init silences compiler */
        const uint8_t *lp              = block_start;
        const uint8_t *lend            = block_start + block_len;
        int            line_index      = 0;

        while (lp < lend && line_index < CAPSULE_BLOCK_LINES) {
            const uint8_t *nl = lp;
            while (nl < lend && *nl != '\n') nl++;

            /* O(1) skip check via bitmask */
            if (!(deferred_mask & (1u << (unsigned)line_index))) {
                char   line[CAPSULE_EXEC_LINE_MAX];
                size_t line_len = (size_t)(nl - lp);
                size_t i;

                /* Clamp only for the execution buffer; block geometry enforced
                   at storage time, not here */
                if (line_len >= CAPSULE_EXEC_LINE_MAX)
                    line_len = CAPSULE_EXEC_LINE_MAX - 1u;
                for (i = 0; i < line_len; i++) line[i] = (char)lp[i];
                line[line_len] = '\0';

                if (line_len > 0) {
                    vm_interpret(vm, line);
                    if (vm->error) {
                        vm->error     = 0;
                        vm->mode      = MODE_INTERPRET;
                        vm->state_var = 0;
                        vm_store_cell(vm, vm->state_addr, 0);
                        error_this_pass = 1;
                        error_line      = line_index;
                        /* Save truncated text for diagnostics */
                        if (deferred_text[line_index][0] == '\0') {
                            size_t tlen = line_len < CAPSULE_LINE_COLS
                                          ? line_len : CAPSULE_LINE_COLS;
                            for (i = 0; i < tlen; i++)
                                deferred_text[line_index][i] = line[i];
                            deferred_text[line_index][tlen] = '\0';
                        }
                        break;
                    }
                }
            }

            lp = (nl < lend) ? nl + 1 : lend;
            line_index++;
        }

        if (!error_this_pass) {
            /* Block passed — log any deferred lines as unresolved forward refs */
#ifdef __STARKERNEL__
            if (deferred_mask != 0u) {
                char nbuf[12], dbuf[4];
                u32_to_dec(block_num, nbuf, sizeof(nbuf));
                u32_to_dec((uint32_t)popcount16(deferred_mask), dbuf, sizeof(dbuf));
                console_puts("[CAPSULE][DEFER] block ");
                console_puts(nbuf);
                console_puts(": ");
                console_puts(dbuf);
                console_puts(" forward ref(s) unresolved at load time:\r\n");
                for (j = 0; j < CAPSULE_BLOCK_LINES; j++) {
                    if (deferred_mask & (1u << (unsigned)j)) {
                        char lbuf[4];
                        u32_to_dec((uint32_t)j, lbuf, sizeof(lbuf));
                        console_puts("  line ");
                        console_puts(lbuf);
                        console_puts(": ");
                        console_puts(deferred_text[j]);
                        console_puts("\r\n");
                    }
                }
            }
#endif
            return 0;
        }

        /* Budget exhaustion: all 16 line slots are deferred */
        if (deferred_mask == 0xFFFFu) {
#ifdef __STARKERNEL__
            {
                char nbuf[12];
                u32_to_dec(block_num, nbuf, sizeof(nbuf));
                console_puts("[CAPSULE][PANIC] block ");
                console_puts(nbuf);
                console_puts(": exceeded 16 deferred lines — capsule cannot load\r\n");
                for (j = 0; j < CAPSULE_BLOCK_LINES; j++) {
                    if (deferred_text[j][0] != '\0') {
                        char lbuf[4];
                        u32_to_dec((uint32_t)j, lbuf, sizeof(lbuf));
                        console_puts("  line ");
                        console_puts(lbuf);
                        console_puts(": ");
                        console_puts(deferred_text[j]);
                        console_puts("\r\n");
                    }
                }
            }
#endif
            return -1;
        }

        /* Defer the errored line — set its bit */
        deferred_mask |= (1u << (unsigned)error_line);

#ifdef __STARKERNEL__
        {
            char nbuf[12], lbuf[4], dbuf[4];
            u32_to_dec(block_num, nbuf, sizeof(nbuf));
            u32_to_dec((uint32_t)error_line, lbuf, sizeof(lbuf));
            u32_to_dec((uint32_t)popcount16(deferred_mask), dbuf, sizeof(dbuf));
            console_puts("[CAPSULE][DEFER] block ");
            console_puts(nbuf);
            console_puts(" (");
            console_puts(dbuf);
            console_puts("/16) line ");
            console_puts(lbuf);
            console_puts(": ");
            console_puts(deferred_text[error_line]);
            console_puts("\r\n");
        }
#endif
        /* Retry the block with this line now in the skip bitmask */
    }
}

/*===========================================================================
 * capsule_exec_payload — unified block parse + populate + execute
 *
 * For each "Block <num>" section in the payload:
 *   1. Write content to block device; warn and truncate if > 1KB
 *   2. Execute content line-by-line via exec_block_with_retry
 *
 * Block numbers are arbitrary; order is irrelevant.
 * No LOAD is injected — FORTH code calls LOAD itself if needed.
 *===========================================================================*/

int capsule_exec_payload(void *vm_opaque, const uint8_t *payload, uint64_t length)
{
    VM *vm = (VM *)vm_opaque;
    if (!vm || !payload || length == 0) return -1;

    const uint8_t *p   = payload;
    const uint8_t *end = payload + length;

    int            in_block          = 0;
    uint32_t       current_block_num = 0;
    const uint8_t *block_start       = (void *)0;

    for (;;) {
        uint32_t       next_num      = 0;
        const uint8_t *content_start = (void *)0;
        int            at_end        = (p >= end);
        int            is_hdr        = !at_end &&
                                       is_block_header(p, end, &next_num, &content_start);

        if (in_block && (is_hdr || at_end)) {
            /* Flush completed block */
            const uint8_t *block_end = at_end ? end : p;
            size_t         block_len = (size_t)(block_end - block_start);

            if (block_len > LOADER_BLOCK_SIZE) {
#ifdef __STARKERNEL__
                char nbuf[12];
                u32_to_dec(current_block_num, nbuf, sizeof(nbuf));
                console_puts("WARN: block ");
                console_puts(nbuf);
                console_puts(" exceeds 1KB, truncating\n");
#endif
                block_len = LOADER_BLOCK_SIZE;
            }

            /* 1. Populate block device */
            write_ramdrive_block(current_block_num, block_start, (uint32_t)block_len);
            copy_block_to_ram(current_block_num);

            /* 2. Execute with forward-reference retry (up to 16 deferred lines) */
            if (exec_block_with_retry(vm, block_start, block_len,
                                      current_block_num) != 0)
                return -1;
        }

        if (at_end) break;

        if (is_hdr) {
            current_block_num = next_num;
            block_start       = content_start;
            in_block          = 1;
            p                 = content_start;
        } else {
            /* Pre-block or unrecognised line — skip */
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
        }
    }

    return 0;
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

    /* Unified: populate block device + execute; then free slots for userspace */
    int rc = capsule_exec_payload(vm, payload, cap->length);
    capsule_clear_blocks(payload, cap->length);
    return (rc == 0) ? CAPSULE_RUN_OK : CAPSULE_RUN_ERR_EXEC_FAIL;
}
