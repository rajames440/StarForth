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
 * console.h - Serial console interface for StarKernel
 */

#ifndef STARKERNEL_CONSOLE_H
#define STARKERNEL_CONSOLE_H

#include <stdint.h>

/**
 * Initialize serial console (UART)
 */
void console_init(void);

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

#endif /* STARKERNEL_CONSOLE_H */
