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

#ifndef STARFORTH_VM_INTERNAL_H
#define STARFORTH_VM_INTERNAL_H

/*
 * vm_internal.h - Shared private declarations for VM implementation
 *
 * This header is ONLY included by VM .c files (vm.c, vm_time.c, vm_bootstrap.c).
 * NOT part of the public API.
 */

#include "../include/vm.h"
#include "../include/platform_time.h"

#include <stdint.h>

#if HEARTBEAT_THREAD_ENABLED && !defined(L4RE_TARGET)
#include <pthread.h>
#define HEARTBEAT_HAS_THREADS 1
#else
#define HEARTBEAT_HAS_THREADS 0
#endif

/* Heartbeat decay batch size */
#define HEARTBEAT_DECAY_BATCH 64u

/* HeartbeatWorker - background thread state for heartbeat */
typedef struct HeartbeatWorker
{
#if HEARTBEAT_HAS_THREADS
    pthread_t thread;
#endif
    uint64_t tick_ns;
    int running;
    int stop_requested;
} HeartbeatWorker;

/* ====================== Atomic helpers ======================= */

static inline uint32_t heartbeat_snapshot_index_load(const volatile uint32_t *ptr)
{
#if defined(__GNUC__)
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
#else
    return *ptr;
#endif
}

static inline void heartbeat_snapshot_index_store(volatile uint32_t *ptr, uint32_t value)
{
#if defined(__GNUC__)
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
#else
    *ptr = value;
#endif
}

/* ====================== Cross-file function declarations ======================= */

/* vm.c (core) - execution spine */
void execute_colon_word(VM *vm);

/* vm_time.c - heartbeat/physics */
void vm_tick(VM *vm);
void vm_tick_window_tuner(VM *vm);
void vm_tick_slope_validator(VM *vm);
void vm_tick_inference_engine(VM *vm);
void vm_heartbeat_run_cycle(VM *vm);
void vm_snapshot_read(const VM *vm, HeartbeatSnapshot *out_snapshot);
void vm_heartbeat_start_thread(VM *vm);
void vm_heartbeat_stop_thread(VM *vm);
void vm_heartbeat_publish_snapshot(VM *vm);

/* vm_bootstrap.c - initialization */
void vm_init(VM *vm);
void vm_cleanup(VM *vm);

/* vm.c (core) - base helpers used by bootstrap */
unsigned vm_get_base(const VM *vm);
void vm_set_base(VM *vm, unsigned b);

#endif /* STARFORTH_VM_INTERNAL_H */