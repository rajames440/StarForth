/*
                                  ***   StarForth   ***

  physics_metadata.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T08:16:51.590-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/include/physics_metadata.h
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

void physics_metadata_init(DictEntry *entry, uint32_t header_bytes);

void physics_metadata_set_mass(DictEntry *entry, uint32_t total_bytes);

void physics_metadata_touch(DictEntry *entry, cell_t entropy, uint64_t now_ns);

void physics_metadata_refresh_state(DictEntry * entry);

void physics_metadata_record_latency(DictEntry *entry, uint64_t sample_ns);

void physics_metadata_apply_seed(DictEntry * entry);

#endif /* STARFORTH_PHYSICS_METADATA_H */