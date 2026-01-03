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
 * capsule_run.c - DoE Run Logging and Parity (M7.1)
 *
 * Ring buffer for capsule execution logging and parity record emission.
 * Freestanding - no libc dependency except for console output hooks.
 */

#include "starkernel/capsule_run.h"
#include "starkernel/capsule.h"

/*===========================================================================
 * Run Log Ring Buffer
 *===========================================================================*/

static CapsuleRunRecord run_log[CAPSULE_MAX_RUN_RECORDS];
static uint32_t run_log_head = 0;   /* Next write position */
static uint32_t run_log_count = 0;  /* Total records logged */
static uint64_t run_id_counter = 0; /* Sequential run ID */

void capsule_run_log_init(void) {
    run_log_head = 0;
    run_log_count = 0;
    run_id_counter = 0;

    /* Zero the ring buffer */
    for (uint32_t i = 0; i < CAPSULE_MAX_RUN_RECORDS; i++) {
        run_log[i].run_id = 0;
        run_log[i].vm_id = 0;
        run_log[i].reserved = 0;
        run_log[i].capsule_id = 0;
        run_log[i].capsule_hash = 0;
        run_log[i].pre_dict_hash = 0;
        run_log[i].post_dict_hash = 0;
        run_log[i].started_ns = 0;
        run_log[i].ended_ns = 0;
        run_log[i].result_code = 0;
        run_log[i].flags = 0;
    }
}

uint64_t capsule_run_log_record(const CapsuleRunRecord *record) {
    if (!record) {
        return 0;
    }

    /* Assign run ID */
    uint64_t id = ++run_id_counter;

    /* Copy record to ring buffer */
    CapsuleRunRecord *slot = &run_log[run_log_head];
    *slot = *record;
    slot->run_id = id;

    /* Advance head (circular) */
    run_log_head = (run_log_head + 1) % CAPSULE_MAX_RUN_RECORDS;

    /* Track total count (saturates at buffer size for wrap detection) */
    if (run_log_count < CAPSULE_MAX_RUN_RECORDS) {
        run_log_count++;
    }

    return id;
}

int capsule_run_log_get(uint64_t run_id, CapsuleRunRecord *out) {
    if (!out || run_id == 0 || run_id > run_id_counter) {
        return -1;
    }

    /* Search ring buffer for matching run_id */
    for (uint32_t i = 0; i < CAPSULE_MAX_RUN_RECORDS; i++) {
        if (run_log[i].run_id == run_id) {
            *out = run_log[i];
            return 0;
        }
    }

    return -1;  /* Not found (may have been overwritten) */
}

uint32_t capsule_run_log_count(void) {
    return run_log_count;
}

/*===========================================================================
 * Parity Logging
 *
 * These functions emit structured parity records to the console/serial.
 * Format is designed for deterministic verification and debugging.
 *
 * Output goes through a console hook that can be redirected to:
 * - Serial port (kernel mode)
 * - stdout (hosted mode)
 * - Debug port 0x402 (QEMU)
 *===========================================================================*/

/* Console output hook - to be set by platform layer */
static void (*parity_putc)(char c) = 0;
static void (*parity_puts)(const char *s) = 0;

void capsule_parity_set_output(void (*putc_fn)(char), void (*puts_fn)(const char *)) {
    parity_putc = putc_fn;
    parity_puts = puts_fn;
}

/* Helper: output hex value */
static void parity_put_hex64(uint64_t val) {
    static const char hex[] = "0123456789abcdef";
    char buf[19];  /* "0x" + 16 hex + null */
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        buf[2 + (15 - i)] = hex[(val >> (i * 4)) & 0xF];
    }
    buf[18] = '\0';
    if (parity_puts) parity_puts(buf);
}

static void parity_put_u32(uint32_t val) {
    char buf[12];
    int i = 11;
    buf[i--] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0 && i >= 0) {
            buf[i--] = '0' + (val % 10);
            val /= 10;
        }
    }
    if (parity_puts) parity_puts(&buf[i + 1]);
}

static void parity_put_u64(uint64_t val) {
    char buf[21];
    int i = 20;
    buf[i--] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0 && i >= 0) {
            buf[i--] = '0' + (val % 10);
            val /= 10;
        }
    }
    if (parity_puts) parity_puts(&buf[i + 1]);
}

void capsule_parity_log_birth(
    uint32_t vm_id,
    uint64_t capsule_id,
    uint64_t capsule_hash,
    uint64_t dict_hash)
{
    if (!parity_puts) return;

    parity_puts("PARITY:BIRTH vm_id=");
    parity_put_u32(vm_id);
    parity_puts(" capsule_id=");
    parity_put_hex64(capsule_id);
    parity_puts(" mode=p capsule_hash=");
    parity_put_hex64(capsule_hash);
    parity_puts(" dict_hash=");
    parity_put_hex64(dict_hash);
    parity_puts("\n");
}

void capsule_parity_log_birth_failed(
    uint32_t vm_id,
    uint64_t capsule_id,
    CapsuleRunResult error,
    uint64_t partial_dict_hash)
{
    if (!parity_puts) return;

    parity_puts("PARITY:BIRTH_FAILED vm_id=");
    parity_put_u32(vm_id);
    parity_puts(" capsule_id=");
    parity_put_hex64(capsule_id);
    parity_puts(" error=");
    parity_put_u32((uint32_t)error);
    parity_puts(" partial_dict_hash=");
    parity_put_hex64(partial_dict_hash);
    parity_puts("\n");
}

void capsule_parity_log_run(
    uint32_t vm_id,
    uint64_t run_id,
    uint64_t capsule_id,
    uint64_t pre_dict_hash,
    uint64_t post_dict_hash)
{
    if (!parity_puts) return;

    parity_puts("PARITY:RUN vm_id=");
    parity_put_u32(vm_id);
    parity_puts(" run_id=");
    parity_put_u64(run_id);
    parity_puts(" capsule_id=");
    parity_put_hex64(capsule_id);
    parity_puts(" mode=e pre_dict=");
    parity_put_hex64(pre_dict_hash);
    parity_puts(" post_dict=");
    parity_put_hex64(post_dict_hash);
    parity_puts("\n");
}

/*===========================================================================
 * Mama Init Parity
 *===========================================================================*/

void capsule_parity_log_mama_init(
    uint64_t capsule_id,
    uint64_t capsule_hash,
    uint64_t dict_hash)
{
    if (!parity_puts) return;

    parity_puts("PARITY:MAMA_INIT capsule_id=");
    parity_put_hex64(capsule_id);
    parity_puts(" mode=m capsule_hash=");
    parity_put_hex64(capsule_hash);
    parity_puts(" dict_hash=");
    parity_put_hex64(dict_hash);
    parity_puts("\n");
}
