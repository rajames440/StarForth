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
 * console.c - Serial console implementation (UART 16550)
 * Supports amd64 via I/O ports
 */

#include "console.h"
#include "arch.h"

#if defined(__x86_64__) || defined(__i386__)
#define SERIAL_SUPPORTED 1

/* UART 16550 I/O ports (COM1) */
#define SERIAL_PORT_BASE 0x3F8

#define SERIAL_DATA_PORT        (SERIAL_PORT_BASE + 0)
#define SERIAL_INT_ENABLE_PORT  (SERIAL_PORT_BASE + 1)
#define SERIAL_FIFO_CTRL_PORT   (SERIAL_PORT_BASE + 2)
#define SERIAL_LINE_CTRL_PORT   (SERIAL_PORT_BASE + 3)
#define SERIAL_MODEM_CTRL_PORT  (SERIAL_PORT_BASE + 4)
#define SERIAL_LINE_STATUS_PORT (SERIAL_PORT_BASE + 5)

/* Line Status Register bits */
#define SERIAL_LSR_DATA_READY    (1 << 0)
#define SERIAL_LSR_THR_EMPTY     (1 << 5)

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#elif defined(__aarch64__)
#define SERIAL_SUPPORTED 1

/* PL011 UART — QEMU virt machine UART0 at 0x09000000 */
#define PL011_BASE       ((volatile unsigned int *)0x09000000UL)
#define PL011_DR         (PL011_BASE + 0)   /* Data Register (offset 0x000) */
#define PL011_FR         (PL011_BASE + 6)   /* Flag Register  (offset 0x018) */
#define PL011_FR_TXFF    (1u << 5)          /* TX FIFO full */

static inline void pl011_putc(char c)
{
    while (*PL011_FR & PL011_FR_TXFF) { }
    *PL011_DR = (unsigned int)(unsigned char)c;
}

#else
#define SERIAL_SUPPORTED 0

/* Stubs for other architectures */
static inline void outb(uint16_t port, uint8_t val) {
    (void)port;
    (void)val;
}

static inline uint8_t inb(uint16_t port) {
    (void)port;
    return 0;
}
#endif

/**
 * Initialize serial console
 * x86: programs 16550 UART.
 * aarch64: PL011 is already initialized by UEFI firmware on QEMU virt.
 */
void console_init(void) {
#if defined(__x86_64__) || defined(__i386__)
    outb(SERIAL_INT_ENABLE_PORT, 0x00);
    outb(SERIAL_LINE_CTRL_PORT, 0x80);
    outb(SERIAL_DATA_PORT, 0x01);
    outb(SERIAL_INT_ENABLE_PORT, 0x00);
    outb(SERIAL_LINE_CTRL_PORT, 0x03);
    outb(SERIAL_FIFO_CTRL_PORT, 0xC7);
    outb(SERIAL_MODEM_CTRL_PORT, 0x0B);
    outb(SERIAL_MODEM_CTRL_PORT, 0x1E);
    outb(SERIAL_DATA_PORT, 0xAE);
    if (inb(SERIAL_DATA_PORT) != 0xAE) {
        return;
    }
    outb(SERIAL_MODEM_CTRL_PORT, 0x0F);
#elif defined(__aarch64__)
    /* PL011 already enabled by AAVMF; nothing to do */
    (void)0;
#else
    (void)SERIAL_SUPPORTED;
#endif
}

#if defined(__x86_64__) || defined(__i386__)
static int serial_transmit_empty(void) {
    return inb(SERIAL_LINE_STATUS_PORT) & SERIAL_LSR_THR_EMPTY;
}
#endif

/**
 * Write a single character to serial console
 */
void console_putc(char c) {
#if defined(__aarch64__)
    if (c == '\n') pl011_putc('\r');
    pl011_putc(c);
#elif defined(__x86_64__) || defined(__i386__)
    if (c == '\n') {
        while (!serial_transmit_empty()) { arch_relax(); }
        outb(SERIAL_DATA_PORT, '\r');
    }
    while (!serial_transmit_empty()) { arch_relax(); }
    outb(SERIAL_DATA_PORT, (uint8_t)c);
#else
    (void)c;
#endif
}

/**
 * Write a null-terminated string to serial console
 */
void console_puts(const char *s) {
    if (!s) return;

    while (*s) {
        console_putc(*s++);
    }
}

/**
 * Write a string with newline to serial console
 */
void console_println(const char *s) {
    console_puts(s);
    console_putc('\n');
}

/**
 * Check if data is available to read
 */
int console_poll(void) {
#if defined(__x86_64__) || defined(__i386__)
    return inb(SERIAL_LINE_STATUS_PORT) & SERIAL_LSR_DATA_READY;
#elif defined(__aarch64__)
    /* PL011_FR bit 4 = RXFE (RX FIFO empty); data available when RXFE == 0 */
    #define PL011_FR_RXFE (1u << 4)
    return !(*PL011_FR & PL011_FR_RXFE);
#else
    return 0;
#endif
}

/**
 * Read a single character from serial console (non-blocking)
 * Returns -1 if no character available
 */
int console_getc(void) {
#if defined(__x86_64__) || defined(__i386__)
    if (!console_poll()) return -1;
    return (int)(unsigned char)inb(SERIAL_DATA_PORT);
#elif defined(__aarch64__)
    if (!console_poll()) return -1;
    return (int)(*PL011_DR & 0xFF);
#else
    return -1;
#endif
}
