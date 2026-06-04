/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0.
*/

/**
 * vt100.c — Full-color ANSI / VT100 terminal state machine
 *
 * Parser states
 * ─────────────
 *   NORMAL      plain text characters
 *   ESC         received 0x1B, waiting for next byte
 *   CSI         received ESC [ , accumulating parameter digits / intermediates
 *   ESC_HASH    received ESC #  (line attributes — we ignore, eat next byte)
 *   ESC_CHAR    received ESC ( or ESC )  (charset designation — ignored)
 *
 * Supported CSI sequences
 * ───────────────────────
 *   CUU  A   cursor up
 *   CUD  B   cursor down
 *   CUF  C   cursor forward
 *   CUB  D   cursor back
 *   CUP  H   cursor position  (row ; col)
 *   ED   J   erase display    (0=below 1=above 2=all)
 *   EL   K   erase line       (0=right 1=left 2=all)
 *   SGR  m   select graphic rendition  (see header)
 *   SCP  s   save cursor position
 *   RCP  u   restore cursor position
 *
 * ESC sequences (2-char)
 *   ESC 7    save cursor (DEC)
 *   ESC 8    restore cursor (DEC)
 *   ESC M    reverse index (scroll down)
 *   ESC c    full reset
 */

#include "vt100.h"
#include "framebuffer.h"
#include <stdint.h>

/* Maximum CSI parameters */
#define VT100_MAX_PARAMS 16

/* Underline scanline (row 14 of 16) */
#define UNDERLINE_ROW 14u

/* -----------------------------------------------------------------------
 * Parser state enum
 * --------------------------------------------------------------------- */

typedef enum {
    ST_NORMAL = 0,
    ST_ESC,
    ST_CSI,
    ST_ESC_HASH,
    ST_ESC_CHAR,
} ParseState;

/* -----------------------------------------------------------------------
 * Terminal state
 * --------------------------------------------------------------------- */

typedef struct {
    /* Geometry */
    uint32_t cols;
    uint32_t rows;

    /* Cursor */
    uint32_t cx;        /* column (0-based) */
    uint32_t cy;        /* row    (0-based) */
    uint32_t saved_cx;
    uint32_t saved_cy;

    /* Colors & attributes */
    uint32_t fg;        /* packed 0x00RRGGBB */
    uint32_t bg;
    uint32_t def_fg;
    uint32_t def_bg;
    int      bold;
    int      underline;
    int      reverse;

    /* Escape parser */
    ParseState state;
    int32_t    params[VT100_MAX_PARAMS];
    uint32_t   param_count;
    int        param_has_value;  /* 1 if current param digit has been seen */

    int        initialized;
} VT100State;

static VT100State g_vt;

/* -----------------------------------------------------------------------
 * Forward declarations
 * --------------------------------------------------------------------- */
static void put_char(uint8_t ch);
static void advance_cursor(void);
static void scroll_up(uint32_t lines);
static void erase_display(int mode);
static void erase_line(int mode);
static void set_cursor(uint32_t row, uint32_t col);
static void apply_sgr(void);

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

/* Framebuffer cell dimensions (pixels) — reads from framebuffer driver */
static uint32_t cell_w(void) { return fb_cell_w(); }
static uint32_t cell_h(void) { return fb_cell_h(); }

/* Pixel position of the top-left corner of character cell (cx, cy) */
static uint32_t px_of(uint32_t col) { return col * cell_w(); }
static uint32_t py_of(uint32_t row) { return row * cell_h(); }

/* Effective fg / bg accounting for reverse video */
static uint32_t eff_fg(void) { return g_vt.reverse ? g_vt.bg : g_vt.fg; }
static uint32_t eff_bg(void) { return g_vt.reverse ? g_vt.fg : g_vt.bg; }

static void draw_cursor_glyph(uint8_t ch)
{
    uint32_t f = eff_fg();
    uint32_t b = eff_bg();

    if (g_vt.bold) {
        /* Lighten fg for bold */
        uint32_t r = FB_R(f);
        uint32_t g = FB_G(f);
        uint32_t bv= FB_B(f);
        r = r > 0xAAu ? 0xFFu : (r + 0x55u > 0xFFu ? 0xFFu : r + 0x55u);
        g = g > 0xAAu ? 0xFFu : (g + 0x55u > 0xFFu ? 0xFFu : g + 0x55u);
        bv= bv> 0xAAu ? 0xFFu : (bv+ 0x55u > 0xFFu ? 0xFFu : bv+ 0x55u);
        f = FB_RGB(r, g, bv);
    }

    fb_draw_glyph(px_of(g_vt.cx), py_of(g_vt.cy), ch, f, b);

    if (g_vt.underline) {
        uint32_t scale = cell_h() / 16u;
        uint32_t ul_y  = py_of(g_vt.cy) + UNDERLINE_ROW * scale;
        fb_fill_rect(px_of(g_vt.cx), ul_y, cell_w(), scale, f);
    }
}

static void reset_attrs(void)
{
    g_vt.fg        = g_vt.def_fg;
    g_vt.bg        = g_vt.def_bg;
    g_vt.bold      = 0;
    g_vt.underline = 0;
    g_vt.reverse   = 0;
}

static void reset_parser(void)
{
    uint32_t i;
    g_vt.state            = ST_NORMAL;
    g_vt.param_count      = 0;
    g_vt.param_has_value  = 0;
    for (i = 0; i < VT100_MAX_PARAMS; i++) {
        g_vt.params[i] = -1;  /* -1 = not set (use default) */
    }
}

/* Return parameter N, substituting `def` when not set */
static int32_t param(uint32_t n, int32_t def)
{
    if (n >= g_vt.param_count) return def;
    return (g_vt.params[n] < 0) ? def : g_vt.params[n];
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void vt100_init(void)
{
    if (!fb_is_available()) return;

    g_vt.cols    = fb_width()  / cell_w();
    g_vt.rows    = fb_height() / cell_h();
    g_vt.def_fg  = VT100_DEFAULT_FG;
    g_vt.def_bg  = VT100_DEFAULT_BG;
    g_vt.cx = g_vt.cy = 0;
    g_vt.saved_cx = g_vt.saved_cy = 0;
    reset_attrs();
    reset_parser();
    g_vt.initialized = 1;

    erase_display(2);
}

uint32_t vt100_cols(void) { return g_vt.cols; }
uint32_t vt100_rows(void) { return g_vt.rows; }

/* -----------------------------------------------------------------------
 * Cursor movement
 * --------------------------------------------------------------------- */

static void set_cursor(uint32_t row, uint32_t col)
{
    if (row >= g_vt.rows) row = g_vt.rows - 1;
    if (col >= g_vt.cols) col = g_vt.cols - 1;
    g_vt.cy = row;
    g_vt.cx = col;
}

static void advance_cursor(void)
{
    g_vt.cx++;
    if (g_vt.cx >= g_vt.cols) {
        g_vt.cx = 0;
        g_vt.cy++;
        if (g_vt.cy >= g_vt.rows) {
            scroll_up(1);
            g_vt.cy = g_vt.rows - 1;
        }
    }
}

static void scroll_up(uint32_t lines)
{
    fb_scroll_rows(lines, g_vt.def_bg);
}

/* -----------------------------------------------------------------------
 * Erase operations
 * --------------------------------------------------------------------- */

static void erase_line_range(uint32_t row, uint32_t c0, uint32_t c1)
{
    uint32_t c;
    for (c = c0; c < c1; c++) {
        fb_draw_glyph(px_of(c), py_of(row), 0x20, g_vt.fg, g_vt.bg);
    }
}

static void erase_display(int mode)
{
    if (mode == 0) {
        /* erase from cursor to end */
        erase_line_range(g_vt.cy, g_vt.cx, g_vt.cols);
        uint32_t r;
        for (r = g_vt.cy + 1; r < g_vt.rows; r++) {
            erase_line_range(r, 0, g_vt.cols);
        }
    } else if (mode == 1) {
        /* erase from start to cursor */
        uint32_t r;
        for (r = 0; r < g_vt.cy; r++) {
            erase_line_range(r, 0, g_vt.cols);
        }
        erase_line_range(g_vt.cy, 0, g_vt.cx + 1);
    } else {
        /* erase entire screen */
        fb_fill_rect(0, 0, fb_width(), fb_height(), g_vt.def_bg);
        g_vt.cx = g_vt.cy = 0;
    }
}

static void erase_line(int mode)
{
    if (mode == 0) {
        erase_line_range(g_vt.cy, g_vt.cx, g_vt.cols);
    } else if (mode == 1) {
        erase_line_range(g_vt.cy, 0, g_vt.cx + 1);
    } else {
        erase_line_range(g_vt.cy, 0, g_vt.cols);
    }
}

/* -----------------------------------------------------------------------
 * SGR (Select Graphic Rendition) handler
 *
 * The parameter list may contain sub-sequences like:
 *   38 ; 2 ; r ; g ; b   truecolor fg
 *   38 ; 5 ; n            256-color fg
 *   48 ; 2 ; r ; g ; b   truecolor bg
 *   48 ; 5 ; n            256-color bg
 * These are walked in a sub-loop.
 * --------------------------------------------------------------------- */

/* 6×6×6 colour cube + grayscale ramp for 256-color mode */
static uint32_t ansi256_to_rgb(uint32_t idx)
{
    if (idx < 16u) {
        return FB_ANSI_PALETTE[idx];
    }
    if (idx >= 232u) {
        uint32_t v = 8u + (idx - 232u) * 10u;
        return FB_RGB(v, v, v);
    }
    /* 6×6×6 colour cube: indices 16–231 */
    idx -= 16u;
    uint32_t r6 = idx / 36u;
    uint32_t g6 = (idx % 36u) / 6u;
    uint32_t b6 = idx % 6u;
    uint32_t r  = r6 ? (55u + r6 * 40u) : 0u;
    uint32_t g  = g6 ? (55u + g6 * 40u) : 0u;
    uint32_t b  = b6 ? (55u + b6 * 40u) : 0u;
    return FB_RGB(r, g, b);
}

static void apply_sgr(void)
{
    uint32_t i = 0;

    if (g_vt.param_count == 0) {
        reset_attrs();
        return;
    }

    while (i < g_vt.param_count) {
        int32_t p = param(i, 0);

        if (p == 0) {
            reset_attrs();
        } else if (p == 1) {
            g_vt.bold = 1;
        } else if (p == 4) {
            g_vt.underline = 1;
        } else if (p == 7) {
            g_vt.reverse = 1;
        } else if (p == 21 || p == 22) {
            g_vt.bold = 0;
        } else if (p == 24) {
            g_vt.underline = 0;
        } else if (p == 27) {
            g_vt.reverse = 0;
        } else if (p >= 30 && p <= 37) {
            g_vt.fg = FB_ANSI_PALETTE[p - 30];
        } else if (p == 38) {
            int32_t mode = param(i + 1, -1);
            if (mode == 2 && i + 4 < g_vt.param_count) {
                uint32_t r = (uint32_t)param(i + 2, 0);
                uint32_t g = (uint32_t)param(i + 3, 0);
                uint32_t b = (uint32_t)param(i + 4, 0);
                g_vt.fg = FB_RGB(r, g, b);
                i += 4;
            } else if (mode == 5 && i + 2 < g_vt.param_count) {
                g_vt.fg = ansi256_to_rgb((uint32_t)param(i + 2, 0));
                i += 2;
            }
        } else if (p == 39) {
            g_vt.fg = g_vt.def_fg;
        } else if (p >= 40 && p <= 47) {
            g_vt.bg = FB_ANSI_PALETTE[p - 40];
        } else if (p == 48) {
            int32_t mode = param(i + 1, -1);
            if (mode == 2 && i + 4 < g_vt.param_count) {
                uint32_t r = (uint32_t)param(i + 2, 0);
                uint32_t g = (uint32_t)param(i + 3, 0);
                uint32_t b = (uint32_t)param(i + 4, 0);
                g_vt.bg = FB_RGB(r, g, b);
                i += 4;
            } else if (mode == 5 && i + 2 < g_vt.param_count) {
                g_vt.bg = ansi256_to_rgb((uint32_t)param(i + 2, 0));
                i += 2;
            }
        } else if (p == 49) {
            g_vt.bg = g_vt.def_bg;
        } else if (p >= 90 && p <= 97) {
            g_vt.fg = FB_ANSI_PALETTE[p - 90 + 8];
        } else if (p >= 100 && p <= 107) {
            g_vt.bg = FB_ANSI_PALETTE[p - 100 + 8];
        }

        i++;
    }
}

/* -----------------------------------------------------------------------
 * CSI dispatch
 * --------------------------------------------------------------------- */

static void dispatch_csi(char final)
{
    int32_t p0 = param(0, -1);
    int32_t p1 = param(1, -1);
    int32_t n;

    switch (final) {
    case 'A':   /* CUU cursor up */
        n = (p0 < 1) ? 1 : p0;
        g_vt.cy = (g_vt.cy >= (uint32_t)n) ? g_vt.cy - (uint32_t)n : 0;
        break;
    case 'B':   /* CUD cursor down */
        n = (p0 < 1) ? 1 : p0;
        g_vt.cy += (uint32_t)n;
        if (g_vt.cy >= g_vt.rows) g_vt.cy = g_vt.rows - 1;
        break;
    case 'C':   /* CUF cursor forward */
        n = (p0 < 1) ? 1 : p0;
        g_vt.cx += (uint32_t)n;
        if (g_vt.cx >= g_vt.cols) g_vt.cx = g_vt.cols - 1;
        break;
    case 'D':   /* CUB cursor back */
        n = (p0 < 1) ? 1 : p0;
        g_vt.cx = (g_vt.cx >= (uint32_t)n) ? g_vt.cx - (uint32_t)n : 0;
        break;
    case 'H':   /* CUP cursor position  ESC[row;colH  (1-based) */
    case 'f':
        {
            uint32_t row = (p0 <= 0) ? 0 : (uint32_t)(p0 - 1);
            uint32_t col = (p1 <= 0) ? 0 : (uint32_t)(p1 - 1);
            set_cursor(row, col);
        }
        break;
    case 'J':   /* ED erase in display */
        erase_display((p0 < 0) ? 0 : p0);
        break;
    case 'K':   /* EL erase in line */
        erase_line((p0 < 0) ? 0 : p0);
        break;
    case 'm':   /* SGR */
        apply_sgr();
        break;
    case 's':   /* SCP save cursor */
        g_vt.saved_cx = g_vt.cx;
        g_vt.saved_cy = g_vt.cy;
        break;
    case 'u':   /* RCP restore cursor */
        set_cursor(g_vt.saved_cy, g_vt.saved_cx);
        break;
    /* unrecognised sequences are silently ignored */
    default:
        break;
    }
}

/* -----------------------------------------------------------------------
 * Printable character output
 * --------------------------------------------------------------------- */

static void put_char(uint8_t ch)
{
    draw_cursor_glyph(ch);
    advance_cursor();
}

/* -----------------------------------------------------------------------
 * vt100_putc — the main entry point
 * --------------------------------------------------------------------- */

void vt100_putc(char raw)
{
    uint8_t c = (uint8_t)raw;

    if (!g_vt.initialized) return;

    switch (g_vt.state) {

    /* ------------------------------------------------------------------ */
    case ST_NORMAL:
        if (c == 0x1B) {
            g_vt.state = ST_ESC;
        } else if (c == '\r') {
            g_vt.cx = 0;
        } else if (c == '\n') {
            g_vt.cy++;
            if (g_vt.cy >= g_vt.rows) {
                scroll_up(1);
                g_vt.cy = g_vt.rows - 1;
            }
        } else if (c == '\t') {
            /* advance to next 8-column tab stop */
            g_vt.cx = (g_vt.cx + 8u) & ~7u;
            if (g_vt.cx >= g_vt.cols) {
                g_vt.cx = 0;
                g_vt.cy++;
                if (g_vt.cy >= g_vt.rows) {
                    scroll_up(1);
                    g_vt.cy = g_vt.rows - 1;
                }
            }
        } else if (c == '\b') {
            if (g_vt.cx > 0) g_vt.cx--;
        } else if (c == 0x07) {
            /* BEL — ignored */
        } else if (c >= 0x20u) {
            put_char(c);
        }
        break;

    /* ------------------------------------------------------------------ */
    case ST_ESC:
        if (c == '[') {
            reset_parser();
            g_vt.state = ST_CSI;
        } else if (c == '7') {
            g_vt.saved_cx = g_vt.cx;
            g_vt.saved_cy = g_vt.cy;
            g_vt.state = ST_NORMAL;
        } else if (c == '8') {
            set_cursor(g_vt.saved_cy, g_vt.saved_cx);
            g_vt.state = ST_NORMAL;
        } else if (c == 'M') {
            /* RI reverse index: scroll down (insert line at top) */
            if (g_vt.cy == 0) {
                /* Scroll display down: not trivially done without a
                 * full back-buffer; fill top row with bg instead.    */
                fb_fill_rect(0, 0, fb_width(), cell_h(), g_vt.def_bg);
            } else {
                g_vt.cy--;
            }
            g_vt.state = ST_NORMAL;
        } else if (c == 'c') {
            /* RIS full reset */
            reset_attrs();
            g_vt.cx = g_vt.cy = 0;
            erase_display(2);
            g_vt.state = ST_NORMAL;
        } else if (c == '#') {
            g_vt.state = ST_ESC_HASH;
        } else if (c == '(' || c == ')') {
            g_vt.state = ST_ESC_CHAR;
        } else {
            /* Unknown 2-char ESC sequence — absorb and reset */
            g_vt.state = ST_NORMAL;
        }
        break;

    /* ------------------------------------------------------------------ */
    case ST_ESC_HASH:
    case ST_ESC_CHAR:
        /* Eat the designator byte and return to normal */
        g_vt.state = ST_NORMAL;
        break;

    /* ------------------------------------------------------------------ */
    case ST_CSI:
        if (c >= '0' && c <= '9') {
            uint32_t i = g_vt.param_count;
            if (i < VT100_MAX_PARAMS) {
                if (g_vt.params[i] < 0) g_vt.params[i] = 0;
                g_vt.params[i] = g_vt.params[i] * 10 + (int32_t)(c - '0');
                g_vt.param_has_value = 1;
            }
        } else if (c == ';') {
            if (g_vt.param_count < VT100_MAX_PARAMS - 1u) {
                g_vt.param_count++;
            }
            g_vt.param_has_value = 0;
        } else if (c == '?' || c == '>' || c == '<' || c == '!') {
            /* Private / intermediate bytes — swallow, keep parsing */
        } else if (c >= 0x40 && c <= 0x7E) {
            /* Final byte — dispatch and return to normal */
            if (g_vt.param_has_value || g_vt.param_count > 0) {
                g_vt.param_count++;
            }
            dispatch_csi((char)c);
            g_vt.state = ST_NORMAL;
        } else {
            /* Unexpected — abort */
            g_vt.state = ST_NORMAL;
        }
        break;
    }
}

void vt100_puts(const char *s)
{
    if (!s) return;
    while (*s) {
        vt100_putc(*s++);
    }
}
