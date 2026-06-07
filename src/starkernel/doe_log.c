/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/**
 * doe_log.c — DoE CSV logger for the LithosAnanke kernel
 *
 * Emits 15-column CSV rows to the serial log once per heartbeat tick.
 * Each row is prefixed with a cyan [HADES][DOE ] tag so it can be
 * extracted cleanly:
 *
 *   grep -aP '\[HADES\]\[DOE \]' qemu-amd64-*.log
 *
 * Columns (in order):
 *   1.  tick_number           uint32  — monotonic heartbeat counter
 *   2.  elapsed_ns            uint64  — ns since run start
 *   3.  tick_interval_ns      uint64  — actual interval from prior tick
 *   4.  cache_hits_delta       uint32  — hot-words cache hits this tick (TODO: 0)
 *   5.  bucket_hits_delta      uint32  — bucket hits this tick (TODO: 0)
 *   6.  word_executions_delta  uint32  — words executed this tick (TODO: 0)
 *   7.  hot_word_count         uint64  — words with heat >= threshold
 *   8.  avg_word_heat_q48      uint64  — mean execution heat as raw Q48.16 integer
 *   9.  window_width           uint32  — rolling window effective size
 *   10. actual_window_size     uint32  — ring buffer fill level
 *   11. predicted_label_hits   uint32  — context prediction hits (TODO: 0)
 *   12. jitter_bits            uint64  — estimated_jitter_ns IEEE 754 raw bits (union-punned)
 *   13. apic_ticks             uint64  — APIC timer monotonic tick count
 *   14. time_trust_q48         uint64  — TIME-TRUST in Q48.16 format
 *   15. variance_q48           uint64  — timing variance in Q48.16 format
 *
 * Note: avg_word_heat and estimated_jitter_ns are stored as double in the
 * snapshot but the freestanding snprintf has no %%f support. avg_word_heat
 * is emitted as raw Q48.16 (multiply by 65536); jitter is union-punned to its
 * IEEE 754 uint64 bit pattern to avoid UB from out-of-range cast.
 */

#include "starkernel/doe_log.h"
#include "starkernel/console.h"
#include "starkernel/timer.h"
#include "freestanding/stdio.h"

#define DOE_PREFIX   "\x1b[36m[HADES][DOE ]\x1b[0m "
#define DOE_BUF_SIZE 512

static const char *doe_header =
    DOE_PREFIX
    "tick_number,elapsed_ns,tick_interval_ns,"
    "cache_hits_delta,bucket_hits_delta,word_executions_delta,"
    "hot_word_count,avg_word_heat_q48,"
    "window_width,actual_window_size,predicted_label_hits,jitter_bits,"
    "apic_ticks,time_trust_q48,variance_q48";

void doe_log_tick_row(VM *vm, const HeartbeatTickSnapshot *snap)
{
    (void)vm;

    if (!snap)
        return;

    /* Print header once before the first data row */
    static int header_printed = 0;
    if (!header_printed) {
        console_puts(doe_header);
        console_puts("\r\n");
        header_printed = 1;
    }

    /* Pull APIC timer trust state */
    const TimeTrustState *ts = heartbeat_state();
    uint64_t apic_ticks     = ts ? ts->ticks              : 0;
    uint64_t time_trust_q48 = ts ? (uint64_t)ts->trust    : 0;
    uint64_t variance_q48   = ts ? (uint64_t)ts->variance : 0;

    /* Convert doubles to integer-safe representations (freestanding snprintf has no %f).
     * Use union punning for jitter to avoid UB from out-of-range double→int64 cast. */
    uint64_t avg_heat_q48 = (uint64_t)(snap->avg_word_heat * 65536.0);
    union { double d; uint64_t u; } jitter_bits;
    jitter_bits.d = snap->estimated_jitter_ns;
    uint64_t jitter_raw = jitter_bits.u;

    /* Format the 15-column CSV row into a local buffer.
     * Use %llu (unsigned long long) for all uint64_t fields — %lu is unreliable
     * for values > 2^32 on aarch64 due to mixed-width varargs ABI behaviour. */
    char buf[DOE_BUF_SIZE];
    snprintf(buf, sizeof(buf),
             "%u,%llu,%llu,%u,%u,%u,%llu,%llu,%u,%u,%u,%llu,%llu,%llu,%llu",
             snap->tick_number,
             (unsigned long long)snap->elapsed_ns,
             (unsigned long long)snap->tick_interval_ns,
             snap->cache_hits_delta,
             snap->bucket_hits_delta,
             snap->word_executions_delta,
             (unsigned long long)snap->hot_word_count,
             (unsigned long long)avg_heat_q48,
             snap->window_width,
             snap->actual_window_size,
             snap->predicted_label_hits,
             (unsigned long long)jitter_raw,
             (unsigned long long)apic_ticks,
             (unsigned long long)time_trust_q48,
             (unsigned long long)variance_q48);

    console_puts(DOE_PREFIX);
    console_puts(buf);
    console_puts("\r\n");
}
