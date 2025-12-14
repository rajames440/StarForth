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

  physics_metadata.h - Physics metadata helpers for dictionary entries

  Phase 1 implements lightweight thermodynamic signals that inform scheduling
  and storage placement while keeping the runtime overhead minimal.
*/

#ifndef STARFORTH_PHYSICS_METADATA_H
#define STARFORTH_PHYSICS_METADATA_H

#include <stdint.h>
#include "vm.h"

/* L8 FINAL INTEGRATION: L1 heat tracking always-on */

/* ============================================================================
 * INTENT: Atomic heat increment for FL1 feedback loop
 * FL1: Heat accumulation - called on every word execution by inner interpreter
 * WHY: Atomic operations prevent race conditions when multiple execution paths
 *      (e.g., heartbeat thread + main interpreter) touch execution_heat
 *      RELAXED memory order is sufficient (no ordering dependencies)
 * ============================================================================ */
static inline void physics_execution_heat_increment(DictEntry *entry)
{
    if (!entry) return;
#if defined(__GNUC__)
    __atomic_fetch_add(&entry->execution_heat, 1, __ATOMIC_RELAXED);
#else
    entry->execution_heat++;
#endif
}

static inline cell_t physics_execution_heat_load(const DictEntry *entry)
{
    if (!entry) return 0;
#if defined(__GNUC__)
    return __atomic_load_n(&entry->execution_heat, __ATOMIC_RELAXED);
#else
    return entry->execution_heat;
#endif
}

static inline uint64_t physics_decay_slope_load(const VM *vm)
{
#if defined(__GNUC__)
    return __atomic_load_n(&vm->decay_slope_q48, __ATOMIC_RELAXED);
#else
    return vm->decay_slope_q48;
#endif
}

void physics_metadata_init(DictEntry *entry, uint32_t header_bytes);

void physics_metadata_set_mass(DictEntry *entry, uint32_t total_bytes);

void physics_metadata_touch(DictEntry *entry, cell_t execution_heat, uint64_t now_ns);

void physics_metadata_refresh_state(DictEntry * entry);

void physics_metadata_record_latency(DictEntry *entry, uint64_t sample_ns);

void physics_metadata_apply_seed(DictEntry * entry);

/* L8 FINAL INTEGRATION: L3 linear decay always-on */
void physics_metadata_apply_linear_decay(DictEntry *entry, uint64_t elapsed_ns, VM *vm);

#endif /* STARFORTH_PHYSICS_METADATA_H */
