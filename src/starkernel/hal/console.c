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
#include "framebuffer.h"
#include "vt100.h"
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

#elif defined(__riscv)
#define SERIAL_SUPPORTED 1

/* NS16550 UART — QEMU virt machine UART0 at 0x10000000 (MMIO, byte-wide) */
#define NS16550_BASE     ((volatile unsigned char *)0x10000000UL)
#define NS16550_THR      (NS16550_BASE + 0)  /* Transmit Holding Register */
#define NS16550_LSR      (NS16550_BASE + 5)  /* Line Status Register */
#define NS16550_LSR_THRE (1u << 5)           /* Transmit Holding Register Empty */
#define NS16550_LSR_DR   (1u << 0)           /* Data Ready */

static inline void ns16550_putc(char c)
{
    while (!(*NS16550_LSR & NS16550_LSR_THRE)) { }
    *NS16550_THR = (unsigned char)c;
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
#elif defined(__riscv)
    /* NS16550 already enabled by UEFI firmware on QEMU virt; nothing to do */
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

/* Active VM name for [Name] line prefix; NULL = no prefix */
static const char *g_active_vm_name = (void *)0;
static int         g_line_start     = 1;

void console_set_vm_name(const char *name) {
    g_active_vm_name = name;
}

const char *console_get_vm_name(void) {
    return g_active_vm_name;
}

/* Raw single-character write — no prefix logic, called by emit_prefix() */
static void raw_putc(char c) {
#if defined(__aarch64__)
    if (c == '\n') pl011_putc('\r');
    pl011_putc(c);
#elif defined(__riscv)
    if (c == '\n') ns16550_putc('\r');
    ns16550_putc(c);
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

/* Emit "[VMName] " directly via raw_putc — no recursion into console_putc */
static void emit_prefix(void) {
    const char *p;
    raw_putc('[');
    for (p = g_active_vm_name; *p; p++) raw_putc(*p);
    raw_putc(']');
    raw_putc(' ');
}

/**
 * Write a single character to serial console.
 * Emits "[VMName] " at the start of each new line when a VM name is set.
 * Also mirrors output to the framebuffer VT100 terminal when available.
 * Serial output is ALWAYS active regardless of framebuffer state.
 */
void console_putc(char c) {
    /* --- serial UART path (always on) --- */
    if (g_active_vm_name && g_line_start && c != '\n') {
        emit_prefix();
        g_line_start = 0;
    }
    raw_putc(c);
    if (c == '\n') {
        g_line_start = 1;
    }

    /* --- framebuffer VT100 path (when available) --- */
    if (fb_is_available()) {
        vt100_putc(c);
    }
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
#elif defined(__riscv)
    return *NS16550_LSR & NS16550_LSR_DR;
#else
    return 0;
#endif
}

/**
 * Initialize the framebuffer VT100 terminal and attach it to the console.
 * Safe to call with info == NULL or when the framebuffer base is zero (no-op).
 * After this call every console_putc / console_puts output is mirrored to
 * the screen; the serial UART continues to function concurrently.
 */
void console_fb_init(const FramebufferInfo *info, FbPixelFormat fmt)
{
    if (!info || !info->base) return;
    fb_init(info, fmt);
    if (fb_is_available()) {
        vt100_init();
    }
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
#elif defined(__riscv)
    if (!console_poll()) return -1;
    return (int)(*NS16550_THR & 0xFF);
#else
    return -1;
#endif
}
