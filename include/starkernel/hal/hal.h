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

/**
 * Kernel HAL surface exposed to the StarForth VM integration layer.
 *
 * Hosted builds never include this header; kernel code funnels all VM-facing
 * services through here so the VM core does not depend on kernel internals.
 */

#ifndef STARKERNEL_HAL_HAL_H
#define STARKERNEL_HAL_HAL_H

#ifdef __STARKERNEL__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct VMHostServices;

void                    sk_hal_init(void);
void                   *sk_hal_alloc(size_t size, size_t align);
void                    sk_hal_free(void *ptr);
uint64_t                sk_hal_time_ns(void);
uint64_t                sk_hal_heartbeat_ticks(void);
size_t                  sk_hal_console_write(const char *buf, size_t len);
int                     sk_hal_console_putc(int c);
void                    sk_hal_panic(const char *message) __attribute__((noreturn));
bool                    sk_hal_is_executable_ptr(const void *ptr);
const struct VMHostServices *sk_hal_host_services(void);
void                    sk_hal_whitelist_exec_region(uint64_t start, uint64_t end, const char *name);
void                    sk_hal_freeze_exec_range(void);

#endif /* __STARKERNEL__ */

#endif /* STARKERNEL_HAL_HAL_H */
