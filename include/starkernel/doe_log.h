/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/**
 * starkernel/doe_log.h — DoE (Design of Experiments) CSV logger
 *
 * Emits per-heartbeat-tick CSV rows to the serial log, tagged with
 * [HADES][DOE ] so they can be grepped cleanly from the QEMU log.
 *
 * Each row contains the 12 standard heartbeat snapshot fields plus
 * 3 APIC timer fields from TimeTrustState:
 *   apic_ticks, time_trust_q48, variance_q48
 *
 * A header row is printed automatically before the first data row.
 *
 * Compiled in when HEARTBEAT_DOE_LOG=1 (default in KERNEL_CFLAGS).
 * Call sites in vm_runtime.c are gated with the same macro.
 */

#ifndef STARKERNEL_DOE_LOG_H
#define STARKERNEL_DOE_LOG_H

#include "vm.h"

/**
 * Emit one CSV row for the current heartbeat tick.
 * Pulls APIC TimeTrustState via heartbeat_state().
 * Prints the column header automatically on the first call.
 *
 * @param vm      VM instance (used for snapshot data)
 * @param snap    Populated HeartbeatTickSnapshot from heartbeat_capture_tick_snapshot()
 */
void doe_log_tick_row(VM *vm, const HeartbeatTickSnapshot *snap);

#endif /* STARKERNEL_DOE_LOG_H */
