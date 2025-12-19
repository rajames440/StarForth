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
