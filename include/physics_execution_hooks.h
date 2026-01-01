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

#ifndef STARFORTH_PHYSICS_EXECUTION_HOOKS_H
#define STARFORTH_PHYSICS_EXECUTION_HOOKS_H

/*
 * physics_execution_hooks.h - Clean abstraction for physics-driven execution hooks
 *
 * Purpose: Hide ALL physics machinery behind two simple function calls.
 * The VM execution loop calls these; it doesn't know or care about:
 *   - Heat tracking
 *   - Decay models
 *   - Rolling window
 *   - Pipelining metrics
 *   - Hotwords cache
 *   - Profiling
 *
 * This keeps vm.c clean and focused on execution semantics.
 */

#include "vm.h"

/*
 * Call BEFORE executing a word.
 * Handles: decay, heat increment, rolling window, pipelining speculation check.
 *
 * @param vm       The VM instance
 * @param word     The word about to execute
 * @param prev     The previously executed word (NULL if first in sequence)
 */
void physics_pre_execute(VM* vm, DictEntry* word, DictEntry* prev);

/*
 * Call AFTER executing a word.
 * Handles: physics touch, profiler exit, DoE counter, heartbeat check.
 *
 * @param vm       The VM instance
 * @param word     The word that just executed
 */
void physics_post_execute(VM* vm, DictEntry* word);

/*
 * Call when looking up a word (outer interpreter).
 * Handles: decay + heat for lookup path (not inner loop).
 *
 * @param vm       The VM instance
 * @param entry    The word found
 * @param canon    Canonical dictionary entry (may differ from entry)
 */
void physics_on_lookup(VM* vm, DictEntry* entry, DictEntry* canon);

#endif /* STARFORTH_PHYSICS_EXECUTION_HOOKS_H */
