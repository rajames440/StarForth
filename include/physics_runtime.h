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

  physics_runtime.h - Phase 1 physics runtime services

  Provides host snapshot shims (POSIX + L4Re) and analytics heap scaffolding
  for physics-aware scheduling. The interface is intentionally lightweight so
  future governance-approved modules can extend it without ABI churn.

  ============================================================================
  IMPORTANT SYNC NOTE (projects using git hooks / governance mirror):
    • If you change HOLA protocol fields, magic, or version numbers below, also
      update the governance export docs so the L4Re mirror stays aligned.
    • Daemons touching this header can throw a warning when HOLA_PROTOCOL_* or
      HOLA_SHARED_MAGIC change to catch accidental ABI drifts early.
    • Reference: docs/src/internal/GOVERNANCE_EXPORT_NOTES.adoc
  ============================================================================
*/

#ifndef STARFORTH_PHYSICS_RUNTIME_H
#define STARFORTH_PHYSICS_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default analytics heap size (10 MiB) */
#define PHYSICS_ANALYTICS_DEFAULT_HEAP_BYTES (10u * 1024u * 1024u)

/** Magic tag for HOLA analytics heap headers ('HOLA') */
#define HOLA_SHARED_MAGIC 0x484f4c41u

/** HOLA protocol version (major.minor) */
#define HOLA_PROTOCOL_MAJOR 0x0001u
#define HOLA_PROTOCOL_MINOR 0x0000u

/** Host backend identifier */
typedef enum physics_host_backend {
    PHYSICS_HOST_BACKEND_POSIX = 0,
    PHYSICS_HOST_BACKEND_L4RE = 1,
    PHYSICS_HOST_BACKEND_FAKE = 2
} physics_host_backend_t;

/** Analytics channel ids */
#define PHYSICS_ANALYTICS_CHANNEL_HOST_SNAPSHOT 0x00000002u

/** Host snapshot flag bits */
#define PHYSICS_HOST_FLAG_PSI        0x0001u
#define PHYSICS_HOST_FLAG_CGROUP     0x0002u
#define PHYSICS_HOST_FLAG_CPU_STAT   0x0004u

/**
 * @brief Snapshot of scheduler-adjacent host signals.
 *
 * Fields are intentionally fixed width to maximise portability and allow the
 * governance repository to consume binary dumps without per-platform quirks.
 */
typedef struct physics_host_snapshot {
    physics_host_backend_t backend; /**< Selected backend (POSIX/L4Re/other) */
    uint32_t scheduler_policy; /**< OS-specific scheduler policy identifier */
    uint32_t scheduler_priority; /**< Nominal thread priority */
    uint32_t scheduler_quantum_ns; /**< Reported quantum in nanoseconds (0 if unknown) */
    uint32_t runnable_threads; /**< Runnable entities on this CPU set (best effort) */
    uint32_t cpu_count; /**< Online logical CPU count */
    uint32_t load_avg_milli; /**< Smoothed load average (×1000) */
    uint32_t flags; /**< Backend-specific flags */
    uint32_t psi_cpu_some_avg10_milli; /**< CPU PSI some avg10 ×1000 */
    uint32_t psi_cpu_some_avg60_milli; /**< CPU PSI some avg60 ×1000 */
    uint32_t psi_cpu_some_avg300_milli; /**< CPU PSI some avg300 ×1000 */
    uint32_t psi_cpu_full_avg10_milli; /**< CPU PSI full avg10 ×1000 */
    uint32_t psi_cpu_full_avg60_milli; /**< CPU PSI full avg60 ×1000 */
    uint32_t psi_cpu_full_avg300_milli; /**< CPU PSI full avg300 ×1000 */
    uint32_t psi_io_some_avg10_milli; /**< IO PSI some avg10 ×1000 */
    uint32_t psi_io_some_avg60_milli; /**< IO PSI some avg60 ×1000 */
    uint32_t psi_io_some_avg300_milli; /**< IO PSI some avg300 ×1000 */
    uint32_t psi_io_full_avg10_milli; /**< IO PSI full avg10 ×1000 */
    uint32_t psi_io_full_avg60_milli; /**< IO PSI full avg60 ×1000 */
    uint32_t psi_io_full_avg300_milli; /**< IO PSI full avg300 ×1000 */
    uint32_t psi_mem_some_avg10_milli; /**< Memory PSI some avg10 ×1000 */
    uint32_t psi_mem_some_avg60_milli; /**< Memory PSI some avg60 ×1000 */
    uint32_t psi_mem_some_avg300_milli; /**< Memory PSI some avg300 ×1000 */
    uint64_t monotonic_time_ns; /**< Monotonic clock reading */
    uint64_t realtime_ns; /**< Wall-clock timestamp (0 if unavailable) */
    uint64_t backend_seq; /**< Backend-provided sequence counter */
    uint64_t cpu_total_jiffies; /**< /proc/stat total jiffies snapshot */
    uint64_t cpu_idle_jiffies; /**< /proc/stat idle jiffies snapshot */
    uint64_t cgroup_cpu_usage_us; /**< cgroup v2 cpu.stat usage_usec (0 if unavailable) */
    uint64_t cgroup_memory_current_bytes; /**< cgroup v2 memory.current (0 if unavailable) */
} physics_host_snapshot_t;

/** Event header written into the analytics ring buffer. */
typedef struct physics_analytics_event_header {
    uint32_t channel; /**< Logical channel identifier */
    uint16_t payload_bytes; /**< Size of event payload */
    uint16_t reserved; /**< Reserved for alignment */
    uint64_t timestamp_ns; /**< Producer timestamp */
} physics_analytics_event_header_t;

/** Analytics heap header seen by both producer (VM) and consumer (HOLA). */
typedef struct physics_analytics_header {
    uint32_t magic; /**< HOLA_SHARED_MAGIC */
    uint16_t version_major; /**< Protocol major version */
    uint16_t version_minor; /**< Protocol minor version */
    uint32_t heap_bytes; /**< Total heap size */
    uint32_t ring_offset; /**< Offset to event ring */
    uint32_t ring_bytes; /**< Event ring length */
    uint32_t summary_offset; /**< Offset to summary/scratch region */
    uint32_t summary_bytes; /**< Summary region length */
    uint32_t scratch_offset; /**< Offset to reserved scratch area */
    uint32_t scratch_bytes; /**< Scratch area length */
    uint64_t produce_seq; /**< Monotonic sequence for producers */
    uint64_t consume_seq; /**< Consumer-visible sequence */
    uint32_t write_offset; /**< Producer write cursor */
    uint32_t read_offset; /**< Consumer read cursor */
    uint32_t dropped_events; /**< Count of dropped events */
    uint32_t flags; /**< Heap flags (see docs) */
} physics_analytics_header_t;

/** Simple view onto the analytics heap layout. */
typedef struct physics_analytics_heap {
    uint8_t *base; /**< Base pointer (owned by runtime) */
    size_t bytes; /**< Total bytes reserved */
    physics_analytics_header_t *header; /**< Shared header */
    uint8_t *ring; /**< Event ring base */
    size_t ring_bytes; /**< Event ring span */
    uint8_t *summary; /**< Summary/statistics region */
    size_t summary_bytes; /**< Summary region span */
    uint8_t *scratch; /**< Scratch/reserved region */
    size_t scratch_bytes; /**< Scratch region span */
} physics_analytics_heap_t;

/**
 * @brief Initialise physics runtime services.
 * @param analytics_heap_bytes Desired heap size (0 -> default 10 MiB)
 * @return 0 on success, -1 on failure
 */
int physics_runtime_init(size_t analytics_heap_bytes);

/**
 * @brief Tear down physics runtime services and release heap storage.
 */
void physics_runtime_shutdown(void);

/**
 * @brief Capture a host snapshot.
 * @param out Output buffer
 * @return 0 on success, -1 on failure
 */
int physics_host_snapshot(physics_host_snapshot_t *out);

/**
 * @brief Publish an event into the analytics ring buffer.
 * @param channel Logical event channel
 * @param payload Pointer to payload bytes
 * @param payload_bytes Payload length (<= ring size)
 * @return 0 on success, -1 if the payload could not be enqueued
 */
int physics_analytics_publish_event(uint32_t channel, const void *payload, uint16_t payload_bytes);

/**
 * @brief Retrieve the analytics heap descriptor.
 * @return Pointer to heap descriptor (NULL if runtime not initialised)
 */
const physics_analytics_heap_t *physics_analytics_heap_info(void);

#ifdef __cplusplus
}
#endif

#endif /* STARFORTH_PHYSICS_RUNTIME_H */