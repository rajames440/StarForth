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

#else
#define SERIAL_SUPPORTED 0

/* Stubs for non-x86 architectures */
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
 * Sets up COM1 at 115200 baud, 8N1
 */
void console_init(void) {
#if SERIAL_SUPPORTED
    /* Disable interrupts */
    outb(SERIAL_INT_ENABLE_PORT, 0x00);

    /* Enable DLAB (set baud rate divisor) */
    outb(SERIAL_LINE_CTRL_PORT, 0x80);

    /* Set divisor to 1 (115200 baud) */
    outb(SERIAL_DATA_PORT, 0x01);        /* Low byte */
    outb(SERIAL_INT_ENABLE_PORT, 0x00);  /* High byte */

    /* 8 bits, no parity, one stop bit */
    outb(SERIAL_LINE_CTRL_PORT, 0x03);

    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(SERIAL_FIFO_CTRL_PORT, 0xC7);

    /* IRQs enabled, RTS/DSR set */
    outb(SERIAL_MODEM_CTRL_PORT, 0x0B);

    /* Test serial chip (loopback test) */
    outb(SERIAL_MODEM_CTRL_PORT, 0x1E);  /* Set in loopback mode */
    outb(SERIAL_DATA_PORT, 0xAE);        /* Test byte */

    /* Check if serial is working */
    if (inb(SERIAL_DATA_PORT) != 0xAE) {
        /* Serial is faulty - we can't do much about it */
        return;
    }

    /* Set in normal operation mode */
    outb(SERIAL_MODEM_CTRL_PORT, 0x0F);
#else
    (void)SERIAL_SUPPORTED;
#endif
}

#if SERIAL_SUPPORTED
/**
 * Check if transmit buffer is empty
 */
static int serial_transmit_empty(void) {
    return inb(SERIAL_LINE_STATUS_PORT) & SERIAL_LSR_THR_EMPTY;
}
#endif

/**
 * Write a single character to serial console
 */
void console_putc(char c) {
#if SERIAL_SUPPORTED
    /* Convert \n to \r\n for proper terminal display */
    if (c == '\n') {
        /* Wait for transmit buffer to be empty */
        while (!serial_transmit_empty()) {
            arch_relax();
        }
        /* Send CR first */
        outb(SERIAL_DATA_PORT, '\r');
    }

    /* Wait for transmit buffer to be empty */
    while (!serial_transmit_empty()) {
        arch_relax();
    }

    /* Send the character */
    outb(SERIAL_DATA_PORT, (uint8_t)c);
#else
    (void)c;
    return;
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
#if SERIAL_SUPPORTED
    return inb(SERIAL_LINE_STATUS_PORT) & SERIAL_LSR_DATA_READY;
#else
    return 0;
#endif
}

/**
 * Read a single character from serial console (non-blocking)
 * Returns -1 if no character available
 */
int console_getc(void) {
#if SERIAL_SUPPORTED
    if (!console_poll()) {
        return -1;
    }
    return inb(SERIAL_DATA_PORT);
#else
    return -1;
#endif
}
