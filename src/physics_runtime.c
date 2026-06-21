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

  physics_runtime.c - Phase 1 physics runtime scaffolding

  Implements the host snapshot shim (POSIX + L4Re stubs) and provides a
  fixed-size analytics heap compatible with the HOLA shared-memory layout.
  The implementation favours predictable costs and clear extension points for
  future governance-approved features.
*/

#include "physics_runtime.h"
#include "platform_time.h"

/* STARFORTH_MINIMAL: kernel build — skip Linux /proc runtime monitoring.
 * Provide stub API so vmcore link succeeds; none of these are called. */
#if defined(STARFORTH_MINIMAL)
int  physics_runtime_init(size_t analytics_heap_bytes)
    { (void)analytics_heap_bytes; return 0; }
void physics_runtime_shutdown(void) {}
int  physics_analytics_publish_event(uint32_t channel, const void *payload, uint16_t payload_bytes)
    { (void)channel; (void)payload; (void)payload_bytes; return 0; }
int  physics_host_snapshot(physics_host_snapshot_t *out)
    { (void)out; return -1; }
#else /* !STARFORTH_MINIMAL — full Linux/POSIX implementation below */

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef __l4__
#include <l4/sys/kip.h>
#include <l4/re/env.h>
#endif

#if !defined(__l4__)
#include <sched.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#endif

/* ------------------------------------------------------------------------- */
/* Analytics heap internals                                                  */
/* ------------------------------------------------------------------------- */

typedef struct physics_runtime_state {
    physics_analytics_heap_t heap;
    int heap_ready;
    uint64_t backend_seq;
} physics_runtime_state_t;

static physics_runtime_state_t g_runtime = {0};

/**
 * @brief Round @c value up to the next multiple of @c alignment.
 *
 * @c alignment must be a power of two. Used internally to satisfy
 * cache-line and struct alignment requirements in the analytics heap.
 *
 * @param value     Value to align
 * @param alignment Alignment boundary (must be power of two)
 * @return Smallest multiple of @c alignment that is >= @c value
 */
static size_t align_up(size_t value, size_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

#if !defined(__l4__)

/**
 * @brief Parse a floating-point average field from a Linux PSI file line.
 *
 * Searches @c line for @c tag (e.g. @c "avg10="), reads the following
 * floating-point value, and returns it scaled to millipercentage (×1000).
 * Returns 0 on parse failure or negative value.
 *
 * @param line Raw text line from a @c /proc/pressure/ PSI file (cpu, memory, or io)
 * @param tag  Field prefix to locate (e.g. @c "avg10=")
 * @return Scaled value in milli-units, or 0 on failure
 */
static uint32_t parse_avg_milli(const char *line, const char *tag) {
    const char *p = strstr(line, tag);
    if (!p) return 0;
    p += strlen(tag);
    errno = 0;
    char *end = NULL;
    double value = strtod(p, &end);
    if (errno != 0 || end == p || value < 0.0) {
        return 0;
    }
    double scaled = value * 1000.0;
    if (scaled > (double) UINT32_MAX) scaled = (double) UINT32_MAX;
    return (uint32_t)(scaled + 0.5);
}

/**
 * @brief Read a Linux Pressure Stall Information (PSI) file.
 *
 * Parses @c /proc/pressure/{cpu,io,memory} and populates the six output
 * pointers with avg10/avg60/avg300 milli-values for both the @c some and
 * @c full stall categories. Any output pointer may be NULL to skip that field.
 * Returns a bitmask: bit 0 set if @c some line was found, bit 1 if @c full.
 *
 * @param path   Path to PSI file (e.g. @c "/proc/pressure/cpu")
 * @param some10  avg10 for the @c some stall category (milli-units), or NULL
 * @param some60  avg60 for the @c some stall category, or NULL
 * @param some300 avg300 for the @c some stall category, or NULL
 * @param full10  avg10 for the @c full stall category, or NULL
 * @param full60  avg60 for the @c full stall category, or NULL
 * @param full300 avg300 for the @c full stall category, or NULL
 * @return Bitmask of parsed sections (0 = none, 1 = some, 2 = full, 3 = both)
 */
static int read_psi_file(const char *path,
                         uint32_t *some10, uint32_t *some60, uint32_t *some300,
                         uint32_t *full10, uint32_t *full60, uint32_t *full300) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return 0;
    }
    char line[256];
    int mask = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "some", 4) == 0) {
            if (some10) *some10 = parse_avg_milli(line, "avg10=");
            if (some60) *some60 = parse_avg_milli(line, "avg60=");
            if (some300) *some300 = parse_avg_milli(line, "avg300=");
            mask |= 1;
        } else if (strncmp(line, "full", 4) == 0) {
            if (full10) *full10 = parse_avg_milli(line, "avg10=");
            if (full60) *full60 = parse_avg_milli(line, "avg60=");
            if (full300) *full300 = parse_avg_milli(line, "avg300=");
            mask |= 2;
        }
    }
    fclose(f);
    return mask;
}

/**
 * @brief Read aggregate CPU jiffy totals from @c /proc/stat.
 *
 * Parses the first @c cpu line from @c /proc/stat and sums user, nice, system,
 * idle, iowait, irq, softirq, and steal columns into @c *total. The idle
 * estimate is @c idle_j + @c iowait. Either output pointer may be NULL.
 *
 * @param total Output: total jiffy count across all categories
 * @param idle  Output: idle + iowait jiffies (effective idle)
 * @return 0 on success, -1 on file open or parse failure
 */
static int read_proc_stat_totals(uint64_t *total, uint64_t *idle) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) {
        return -1;
    }
    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    unsigned long long user = 0, nice = 0, system = 0, idle_j = 0;
    unsigned long long iowait = 0, irq = 0, softirq = 0, steal = 0;
    int scanned = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                         &user, &nice, &system, &idle_j, &iowait, &irq, &softirq, &steal);
    if (scanned < 4) {
        return -1;
    }
    unsigned long long total_j = user + nice + system + idle_j + iowait + irq + softirq + steal;
    if (total) *total = total_j;
    if (idle) *idle = idle_j + iowait;
    return 0;
}

/**
 * @brief Resolve the cgroup v2 filesystem path for the current process.
 *
 * Reads @c /proc/self/cgroup, finds the cgroup2 hierarchy entry (the line
 * with @c "::" separator), and constructs the full sysfs path under
 * @c /sys/fs/cgroup. The root cgroup ("/") maps to @c /sys/fs/cgroup itself.
 *
 * @param out    Output buffer for the resolved path
 * @param out_sz Size of @c out in bytes
 * @return 0 on success, -1 if the path could not be resolved
 */
static int resolve_cgroup_path(char *out, size_t out_sz) {
    FILE *f = fopen("/proc/self/cgroup", "r");
    if (!f) {
        return -1;
    }
    char line[512];
    int rc = -1;
    while (fgets(line, sizeof(line), f)) {
        char *sep = strstr(line, "::");
        if (!sep) continue;
        char *path = sep + 2;
        char *newline = strchr(path, '\n');
        if (newline) *newline = '\0';
        if (*path == '\0' || strcmp(path, "/") == 0) {
            if (snprintf(out, out_sz, "%s", "/sys/fs/cgroup") < (int) out_sz) {
                rc = 0;
                break;
            }
        } else {
            if (snprintf(out, out_sz, "/sys/fs/cgroup%s", path) < (int) out_sz) {
                rc = 0;
                break;
            }
        }
    }
    fclose(f);
    return rc;
}

/**
 * @brief Read the cumulative CPU usage in microseconds from a cgroup's @c cpu.stat.
 *
 * Opens @c <cgpath>/cpu.stat and extracts the @c usage_usec field.
 * Returns 0 if the file cannot be opened or the field is absent.
 *
 * @param cgpath Absolute path to the cgroup directory
 * @return Cumulative CPU usage in microseconds, or 0 on failure
 */
static uint64_t read_cgroup_cpu_usage_us(const char *cgpath) {
    char path[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/cpu.stat", cgpath) >= (int) sizeof(path)) {
        return 0;
    }
    FILE *f = fopen(path, "r");
    if (!f) {
        return 0;
    }
    char line[256];
    uint64_t value = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "usage_usec %" SCNu64, &value) == 1) {
            break;
        }
    }
    fclose(f);
    return value;
}

/**
 * @brief Read current memory usage in bytes from a cgroup's @c memory.current.
 *
 * Opens @c <cgpath>/memory.current and reads the single integer value.
 * Returns 0 if the file cannot be opened or parsed.
 *
 * @param cgpath Absolute path to the cgroup directory
 * @return Current memory usage in bytes, or 0 on failure
 */
static uint64_t read_cgroup_memory_current(const char *cgpath) {
    char path[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/memory.current", cgpath) >= (int) sizeof(path)) {
        return 0;
    }
    FILE *f = fopen(path, "r");
    if (!f) {
        return 0;
    }
    unsigned long long value = 0;
    if (fscanf(f, "%llu", &value) != 1) {
        value = 0;
    }
    fclose(f);
    return value;
}

#endif /* !__l4__ */
/**
 * @brief Allocate and initialise the HOLA analytics heap.
 *
 * Allocates a contiguous block of at least @c requested_bytes (or
 * @c PHYSICS_ANALYTICS_DEFAULT_HEAP_BYTES if 0), enforcing a minimum that
 * accommodates the header plus 512 KB ring buffer. Partitions the block into
 * header (cache-line aligned), ring (60%), summary (30%), and scratch regions,
 * then populates the @c physics_analytics_header_t with magic, version, and
 * offsets. Safe to call repeatedly — returns 0 immediately if already ready.
 *
 * @param requested_bytes Desired heap size in bytes (0 = use default)
 * @return 0 on success, -1 if allocation failed
 */
static int analytics_heap_allocate(size_t requested_bytes) {
    if (g_runtime.heap_ready) {
        return 0;
    }

    size_t bytes = requested_bytes;
    if (bytes == 0) {
        bytes = PHYSICS_ANALYTICS_DEFAULT_HEAP_BYTES;
    }

    /* Enforce minimum viable size: header + ring + summary */
    const size_t min_bytes = align_up(sizeof(physics_analytics_header_t), 64) + (512u * 1024u);
    if (bytes < min_bytes) {
        bytes = min_bytes;
    }

    uint8_t *base = NULL;

#if !defined(__l4__)
    base = (uint8_t *) malloc(bytes);
    if (!base) {
        return -1;
    }
#else
    /* L4Re path: simple malloc is fine for now; future work can switch to dataspace */
    base = (uint8_t *) malloc(bytes);
    if (!base) {
        return -1;
    }
#endif

    memset(base, 0, bytes);

    physics_analytics_heap_t heap = {0};
    heap.base = base;
    heap.bytes = bytes;

    size_t header_bytes = align_up(sizeof(physics_analytics_header_t), 64);
    heap.header = (physics_analytics_header_t *) base;

    /* Split remaining space into ring (60%), summary (30%), scratch (remainder) */
    size_t remaining = bytes - header_bytes;
    size_t ring_bytes = align_up((remaining * 6u) / 10u, 64);
    if (ring_bytes > remaining) ring_bytes = remaining;
    size_t summary_bytes = remaining > ring_bytes ? align_up((remaining - ring_bytes) * 3u / 4u, 64) : 0;
    if (summary_bytes > remaining - ring_bytes) summary_bytes = remaining - ring_bytes;
    size_t scratch_bytes = remaining - ring_bytes - summary_bytes;

    heap.ring = base + header_bytes;
    heap.ring_bytes = ring_bytes;
    heap.summary = heap.ring + ring_bytes;
    heap.summary_bytes = summary_bytes;
    heap.scratch = heap.summary + summary_bytes;
    heap.scratch_bytes = scratch_bytes;

    /* Populate header */
    physics_analytics_header_t *hdr = heap.header;
    hdr->magic = HOLA_SHARED_MAGIC;
    hdr->version_major = HOLA_PROTOCOL_MAJOR;
    hdr->version_minor = HOLA_PROTOCOL_MINOR;
    hdr->heap_bytes = (uint32_t) bytes;
    hdr->ring_offset = (uint32_t) header_bytes;
    hdr->ring_bytes = (uint32_t) ring_bytes;
    hdr->summary_offset = (uint32_t)(heap.summary - heap.base);
    hdr->summary_bytes = (uint32_t) summary_bytes;
    hdr->scratch_offset = (uint32_t)(heap.scratch - heap.base);
    hdr->scratch_bytes = (uint32_t) scratch_bytes;
    hdr->produce_seq = 0;
    hdr->consume_seq = 0;
    hdr->write_offset = 0;
    hdr->read_offset = 0;
    hdr->dropped_events = 0;
    hdr->flags = 0;

    g_runtime.heap = heap;
    g_runtime.heap_ready = 1;
    g_runtime.backend_seq = 0;
    return 0;
}

/**
 * @brief Free the analytics heap and reset all global heap state.
 *
 * Safe to call when the heap is not ready. After this call,
 * @c g_runtime.heap_ready is 0 and the heap pointer is NULL.
 */
static void analytics_heap_release(void) {
    if (!g_runtime.heap_ready) {
        return;
    }
    free(g_runtime.heap.base);
    g_runtime.heap = (physics_analytics_heap_t) {
        0
    };
    g_runtime.heap_ready = 0;
    g_runtime.backend_seq = 0;
}

/**
 * @brief Compute free bytes available in the analytics ring buffer.
 *
 * Assumes single-producer, single-consumer access with no concurrent writers.
 * Clamps out-of-range @c write_offset and @c read_offset before computing.
 *
 * @param hdr Pointer to the analytics heap header
 * @return Number of free bytes available for a new event write
 */
static size_t analytics_ring_free(const physics_analytics_header_t *hdr) {
    uint32_t write = hdr->write_offset;
    uint32_t read = hdr->read_offset;
    uint32_t size = hdr->ring_bytes;
    if (write >= size) write = 0;
    if (read >= size) read = 0;

    if (write >= read) {
        return (size_t) (size - write + read - 1u);
    }
    return (size_t) (read - write - 1u);
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Initialise the physics runtime and analytics heap.
 *
 * Must be called before any other physics_runtime_* function.
 * Idempotent — safe to call multiple times; subsequent calls return 0
 * immediately without re-allocating.
 *
 * @param analytics_heap_bytes Desired analytics heap size (0 = default)
 * @return 0 on success, -1 on allocation failure
 */
int physics_runtime_init(size_t analytics_heap_bytes) {
    return analytics_heap_allocate(analytics_heap_bytes);
}

/**
 * @brief Shut down the physics runtime and release the analytics heap.
 *
 * Safe to call when the runtime was never initialised. After this call
 * the analytics heap is gone and no events can be published until
 * @c physics_runtime_init() is called again.
 */
void physics_runtime_shutdown(void) {
    analytics_heap_release();
}

/**
 * @brief Return a pointer to the live analytics heap descriptor.
 *
 * @return Pointer to the heap descriptor, or NULL if not yet initialised
 */
const physics_analytics_heap_t *physics_analytics_heap_info(void) {
    return g_runtime.heap_ready ? &g_runtime.heap : NULL;
}

/**
 * @brief Publish an analytics event into the ring buffer.
 *
 * Writes a @c physics_analytics_event_header_t followed by the payload
 * (8-byte padded) into the ring at the current write offset. If insufficient
 * space is available, increments @c dropped_events and returns -1. The event
 * is timestamped with @c sf_monotonic_ns() at write time.
 *
 * @param channel       Event channel identifier (e.g. @c PHYSICS_ANALYTICS_CHANNEL_HOST_SNAPSHOT)
 * @param payload       Event payload bytes (must not be NULL)
 * @param payload_bytes Length of payload in bytes
 * @return 0 on success, -1 if the heap is not ready, payload is NULL, or ring is full
 */
int physics_analytics_publish_event(uint32_t channel, const void *payload, uint16_t payload_bytes) {
    if (!g_runtime.heap_ready || !payload) {
        return -1;
    }

    physics_analytics_header_t *hdr = g_runtime.heap.header;
    uint8_t *ring = g_runtime.heap.ring;
    uint32_t ring_bytes = hdr->ring_bytes;
    if (ring_bytes == 0) {
        return -1;
    }

    size_t padded_payload = align_up(payload_bytes, 8);
    size_t total_bytes = sizeof(physics_analytics_event_header_t) + padded_payload;
    if (total_bytes >= ring_bytes) {
        /* Payload cannot fit even into an empty ring */
        hdr->dropped_events++;
        return -1;
    }

    size_t free_bytes = analytics_ring_free(hdr);
    if (total_bytes > free_bytes) {
        hdr->dropped_events++;
        return -1;
    }

    uint32_t write = hdr->write_offset;
    if (write >= ring_bytes) {
        write = 0;
    }

    if (write + total_bytes > ring_bytes) {
        size_t tail = ring_bytes - write;
        memset(ring + write, 0, tail);
        write = 0;
    }

    physics_analytics_event_header_t *evt = (physics_analytics_event_header_t *) (ring + write);
    evt->channel = channel;
    evt->payload_bytes = payload_bytes;
    evt->reserved = 0;
    evt->timestamp_ns = sf_monotonic_ns();

    memcpy((uint8_t *) (evt + 1), payload, payload_bytes);
    if (padded_payload > payload_bytes) {
        memset(((uint8_t *) (evt + 1)) + payload_bytes, 0, padded_payload - payload_bytes);
    }

    write += (uint32_t) total_bytes;
    if (write >= ring_bytes) {
        write = 0;
    }

    hdr->write_offset = write;
    hdr->produce_seq++;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Host snapshot shim                                                        */
/* ------------------------------------------------------------------------- */

/**
 * @brief Collect a host system snapshot using POSIX / Linux interfaces.
 *
 * Populates @c out with monotonic and realtime timestamps, CPU count,
 * scheduler policy and priority, load average (scaled × 1000), runnable
 * thread estimate, PSI pressure readings, @c /proc/stat CPU jiffy totals,
 * and cgroup v2 CPU usage and memory current values. Sets flags in
 * @c out->flags to indicate which fields were successfully populated.
 * Not available on L4Re (@c __l4__ defined) — returns -1 in that case.
 *
 * @param out Output snapshot struct; zeroed and populated by this function
 * @return 0 on success, -1 if the platform is not POSIX/Linux
 */
static int snapshot_posix(physics_host_snapshot_t *out) {
#if !defined(__l4__)
    memset(out, 0, sizeof(*out));
    out->backend = PHYSICS_HOST_BACKEND_POSIX;
    out->monotonic_time_ns = sf_monotonic_ns();
    out->realtime_ns = sf_realtime_ns();

    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    out->cpu_count = (cpu_count > 0) ? (uint32_t) cpu_count : 1u;

    /* Scheduler information */
    int policy = sched_getscheduler(0);
    if (policy >= 0) {
        out->scheduler_policy = (uint32_t) policy;
        struct sched_param param = {0};
        if (sched_getparam(0, &param) == 0) {
            out->scheduler_priority = (uint32_t) param.sched_priority;
        }
#if defined(_POSIX_PRIORITY_SCHEDULING)
        if (policy == SCHED_RR) {
            struct timespec ts = {0};
            if (sched_rr_get_interval(0, &ts) == 0) {
                out->scheduler_quantum_ns = (uint32_t)((ts.tv_sec * 1000000000LL) + ts.tv_nsec);
            }
        }
#endif
    }

    /* Load estimate (scaled by 1000) */
#if defined(_GNU_SOURCE)
    double loads[3] = {0.0, 0.0, 0.0};
    if (getloadavg(loads, 3) > 0) {
        out->load_avg_milli = (uint32_t)(loads[0] * 1000.0);
    }
#endif

    /* Runnable thread estimate: best effort via load average */
    if (out->load_avg_milli > 0) {
        out->runnable_threads = (out->load_avg_milli + 999u) / 1000u;
    }

    int psi_mask = 0;
    psi_mask |= read_psi_file("/proc/pressure/cpu",
                              &out->psi_cpu_some_avg10_milli,
                              &out->psi_cpu_some_avg60_milli,
                              &out->psi_cpu_some_avg300_milli,
                              &out->psi_cpu_full_avg10_milli,
                              &out->psi_cpu_full_avg60_milli,
                              &out->psi_cpu_full_avg300_milli);
    psi_mask |= read_psi_file("/proc/pressure/io",
                              &out->psi_io_some_avg10_milli,
                              &out->psi_io_some_avg60_milli,
                              &out->psi_io_some_avg300_milli,
                              &out->psi_io_full_avg10_milli,
                              &out->psi_io_full_avg60_milli,
                              &out->psi_io_full_avg300_milli);
    psi_mask |= read_psi_file("/proc/pressure/memory",
                              &out->psi_mem_some_avg10_milli,
                              &out->psi_mem_some_avg60_milli,
                              &out->psi_mem_some_avg300_milli,
                              NULL, NULL, NULL);
    if (psi_mask) {
        out->flags |= PHYSICS_HOST_FLAG_PSI;
    }

    uint64_t total_jiffies = 0;
    uint64_t idle_jiffies = 0;
    if (read_proc_stat_totals(&total_jiffies, &idle_jiffies) == 0) {
        out->cpu_total_jiffies = total_jiffies;
        out->cpu_idle_jiffies = idle_jiffies;
        out->flags |= PHYSICS_HOST_FLAG_CPU_STAT;
    }

    char cgpath[PATH_MAX];
    if (resolve_cgroup_path(cgpath, sizeof(cgpath)) == 0) {
        uint64_t usage = read_cgroup_cpu_usage_us(cgpath);
        uint64_t mem_cur = read_cgroup_memory_current(cgpath);
        out->cgroup_cpu_usage_us = usage;
        out->cgroup_memory_current_bytes = mem_cur;
        if (usage != 0 || mem_cur != 0) {
            out->flags |= PHYSICS_HOST_FLAG_CGROUP;
        }
    }

    out->backend_seq = ++g_runtime.backend_seq;
    return 0;
#else
    (void) out;
    return -1;
#endif
}

#ifdef __l4__
/**
 * @brief Collect a host system snapshot using L4Re kernel interfaces.
 *
 * Provides a minimal snapshot (timestamps, CPU count = 1) using the L4Re
 * kernel info page. Future work may query the actual processor count from
 * the KIP once that interface stabilises.
 *
 * @param out Output snapshot struct; zeroed and populated by this function
 * @return 0 on success
 */
static int snapshot_l4re(physics_host_snapshot_t *out) {
    memset(out, 0, sizeof(*out));
    out->backend = PHYSICS_HOST_BACKEND_L4RE;
    out->monotonic_time_ns = sf_monotonic_ns();
    out->realtime_ns = sf_realtime_ns();

    l4_kernel_info_t *kip = l4re_kip();
    (void) kip;
    out->cpu_count = 1u; /* TODO: query processor count from KIP once available */

    out->backend_seq = ++g_runtime.backend_seq;
    return 0;
}
#endif

/**
 * @brief Capture a host environment snapshot and publish it to the analytics ring.
 *
 * Selects the best available backend (L4Re if available, else POSIX). On
 * success also publishes the snapshot as a @c PHYSICS_ANALYTICS_CHANNEL_HOST_SNAPSHOT
 * event to the analytics ring buffer (if the heap is initialised).
 *
 * @param out Output snapshot struct to populate (must not be NULL)
 * @return 0 on success, -1 if @c out is NULL or all backends fail
 */
int physics_host_snapshot(physics_host_snapshot_t *out) {
    if (!out) {
        return -1;
    }
    int rc;
#ifdef __l4__
    rc = snapshot_l4re(out);
    if (rc == 0) {
        goto publish;
    }
#endif
    rc = snapshot_posix(out);
    if (rc != 0) {
        return rc;
    }
#ifdef __l4__
publish:
#endif
    if (g_runtime.heap_ready) {
        physics_analytics_publish_event(PHYSICS_ANALYTICS_CHANNEL_HOST_SNAPSHOT,
                                        out,
                                        (uint16_t) sizeof(*out));
    }
    return 0;
}

#endif /* STARFORTH_MINIMAL */

/* ------------------------------------------------------------------------- */
/* End                                                                       */
/* ------------------------------------------------------------------------- */