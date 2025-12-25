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

#include "include/time_words.h"
#include "../../include/vm.h"
#include "../../include/word_registry.h"
#include "../../include/q48_16.h"

#include <stdio.h>

/* ============================================================================
 * TIME-TICKS ( -- u )
 * Push monotonic heartbeat tick count
 * ============================================================================ */
static void time_word_ticks(VM *vm) {
    vm_push(vm, (cell_t)vm->heartbeat.tick_count);
}

/* ============================================================================
 * TIME-TRUST ( -- q )
 * Push TIME-TRUST as Q48.16 value
 * ============================================================================ */
static void time_word_trust(VM *vm) {
    vm_push(vm, (cell_t)vm->heartbeat.m5_time_trust);
}

/* ============================================================================
 * TIME-TRUST. ( -- )
 * Print TIME-TRUST as decimal
 * ============================================================================ */
static void time_word_trust_dot(VM *vm) {
    (void)vm;
    q48_16_t t = vm->heartbeat.m5_time_trust;
    printf("%s ", q48_to_string(t));
}

/* ============================================================================
 * TIME-VARIANCE ( -- q )
 * Push timing variance as Q48.16 value
 * ============================================================================ */
static void time_word_variance(VM *vm) {
    vm_push(vm, (cell_t)vm->heartbeat.m5_variance);
}

/* ============================================================================
 * TIME-INIT ( -- )
 * Initialize heartbeat subsystem (M5 fields)
 * ============================================================================ */
static void time_word_init(VM *vm) {
    vm->heartbeat.m5_time_trust = q48_from_u64(1);  /* 1.0 = full trust */
    vm->heartbeat.m5_variance = 0;
}

/* ============================================================================
 * Registration
 * ============================================================================ */
void register_time_words(VM *vm) {
    register_word(vm, "TIME-TICKS", time_word_ticks);
    register_word(vm, "TIME-TRUST", time_word_trust);
    register_word(vm, "TIME-TRUST.", time_word_trust_dot);
    register_word(vm, "TIME-VARIANCE", time_word_variance);
    register_word(vm, "TIME-INIT", time_word_init);
}
