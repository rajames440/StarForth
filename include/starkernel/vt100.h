/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0.
*/

/**
 * vt100.h — Full-color ANSI / VT100 terminal state machine
 *
 * Sits on top of the framebuffer driver.  Call vt100_init() once the
 * framebuffer is ready, then route all character output through vt100_putc().
 *
 * Supported escape sequences
 * ──────────────────────────
 *   Cursor movement   ESC[H  ESC[n;mH  ESC[nA  ESC[nB  ESC[nC  ESC[nD
 *   Cursor save/restore  ESC[s  ESC[u  (also ESC7 / ESC8)
 *   Erase             ESC[2J  ESC[K  ESC[0K  ESC[1K  ESC[2K
 *   SGR attributes    ESC[…m  (see below)
 *
 * SGR codes
 * ─────────
 *   0          reset all attributes
 *   1          bold (doubles fg brightness)
 *   4          underline
 *   7          reverse video
 *   22         normal intensity
 *   24         underline off
 *   27         reverse off
 *   30–37      set fg to standard ANSI color 0–7
 *   38;2;r;g;b set fg to 24-bit RGB truecolor
 *   38;5;n     set fg to 256-color index
 *   39         reset fg to default
 *   40–47      set bg to standard ANSI color 0–7
 *   48;2;r;g;b set bg to 24-bit RGB truecolor
 *   48;5;n     set bg to 256-color index
 *   49         reset bg to default
 *   90–97      set fg to bright ANSI color 8–15
 *   100–107    set bg to bright ANSI color 8–15
 */

#ifndef STARKERNEL_VT100_H
#define STARKERNEL_VT100_H

#include <stdint.h>

/* Default terminal colors */
#define VT100_DEFAULT_FG  FB_RGB(0xAA, 0xAA, 0xAA)  /* light gray */
#define VT100_DEFAULT_BG  FB_RGB(0x00, 0x00, 0x00)  /* black       */

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

/**
 * Initialize the VT100 terminal.
 * Must be called after fb_init().  Clears the screen and homes the cursor.
 */
void vt100_init(void);

/* -----------------------------------------------------------------------
 * Character output
 * --------------------------------------------------------------------- */

/** Feed one byte into the terminal (handles escape sequences transparently). */
void vt100_putc(char c);

/** Feed a null-terminated string. */
void vt100_puts(const char *s);

/* -----------------------------------------------------------------------
 * Queries
 * --------------------------------------------------------------------- */

/** Terminal width in character columns. */
uint32_t vt100_cols(void);

/** Terminal height in character rows. */
uint32_t vt100_rows(void);

#endif /* STARKERNEL_VT100_H */
