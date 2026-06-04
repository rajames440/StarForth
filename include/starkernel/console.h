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
 * console.h - Serial console + framebuffer VT100 interface for StarKernel
 *
 * Output policy:
 *   - Serial UART is always active (initialized by console_init).
 *   - When console_fb_init() has been called and the framebuffer is ready,
 *     every character is also rendered through the VT100 terminal on screen.
 *   - Both outputs are always live simultaneously; serial cannot be disabled.
 */

#ifndef STARKERNEL_CONSOLE_H
#define STARKERNEL_CONSOLE_H

#include <stdint.h>
#include "uefi.h"

/**
 * Initialize serial console (UART).
 * Must be called once during early kernel boot.
 */
void console_init(void);

/**
 * Initialize the framebuffer VT100 terminal.
 * Call after UEFI boot services have been exited and the GOP framebuffer
 * address is known (from BootInfo).  Safe to call with info==NULL (no-op).
 * fmt: FB_PIXEL_BGRX32 is correct for most QEMU / real hardware GOP.
 */
#include "framebuffer.h"
void console_fb_init(const FramebufferInfo *info, FbPixelFormat fmt);

/**
 * Write a single character to serial console
 */
void console_putc(char c);

/**
 * Write a null-terminated string to serial console
 */
void console_puts(const char *s);

/**
 * Write a string with newline to serial console
 */
void console_println(const char *s);

/**
 * Read a single character from serial console (non-blocking)
 * Returns -1 if no character available
 */
int console_getc(void);

/**
 * Check if character is available for reading
 */
int console_poll(void);

/**
 * Set the active VM name shown as [Name] prefix on each output line.
 * Pass NULL to suppress the prefix (kernel-only output before any VM).
 * The pointer must remain valid for as long as it is active.
 */
void console_set_vm_name(const char *name);
const char *console_get_vm_name(void);

#endif /* STARKERNEL_CONSOLE_H */
