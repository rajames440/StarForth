/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0.
*/

/**
 * framebuffer.c — UEFI GOP framebuffer pixel driver
 *
 * Supports BGRX32 and RGBX32 pixel formats.
 * Pixel scale factor allows 1×/2×/4× rendering for hi-res displays.
 *
 * Hi-res mode:
 *   1080p (1920×1080) @ scale=1 → 240 cols × 67 rows  (8×16 cells)
 *   1080p (1920×1080) @ scale=2 → 120 cols × 33 rows  (16×32 cells)
 *   4K    (3840×2160) @ scale=2 → 240 cols × 67 rows  (16×32 cells)
 *   4K    (3840×2160) @ scale=4 → 120 cols × 33 rows  (32×64 cells)
 */

#include "framebuffer.h"
#include <stdint.h>

/* Font API from font_8x16.c */
extern const uint8_t *font_8x16_glyph(uint8_t ch);

/* ANSI 16-color palette (indices 0–15, packed 0x00RRGGBB) */
const uint32_t FB_ANSI_PALETTE[16] = {
    /* 0  Black   */ FB_RGB(0x00, 0x00, 0x00),
    /* 1  Red     */ FB_RGB(0xAA, 0x00, 0x00),
    /* 2  Green   */ FB_RGB(0x00, 0xAA, 0x00),
    /* 3  Yellow  */ FB_RGB(0xAA, 0x55, 0x00),
    /* 4  Blue    */ FB_RGB(0x00, 0x00, 0xAA),
    /* 5  Magenta */ FB_RGB(0xAA, 0x00, 0xAA),
    /* 6  Cyan    */ FB_RGB(0x00, 0xAA, 0xAA),
    /* 7  White   */ FB_RGB(0xAA, 0xAA, 0xAA),
    /* 8  Br.Black   */ FB_RGB(0x55, 0x55, 0x55),
    /* 9  Br.Red     */ FB_RGB(0xFF, 0x55, 0x55),
    /* 10 Br.Green   */ FB_RGB(0x55, 0xFF, 0x55),
    /* 11 Br.Yellow  */ FB_RGB(0xFF, 0xFF, 0x55),
    /* 12 Br.Blue    */ FB_RGB(0x55, 0x55, 0xFF),
    /* 13 Br.Magenta */ FB_RGB(0xFF, 0x55, 0xFF),
    /* 14 Br.Cyan    */ FB_RGB(0x55, 0xFF, 0xFF),
    /* 15 Br.White   */ FB_RGB(0xFF, 0xFF, 0xFF),
};

/* -----------------------------------------------------------------------
 * Module state
 * --------------------------------------------------------------------- */

typedef struct {
    volatile uint32_t *base;       /* GOP framebuffer base (32-bit pixels) */
    uint32_t           stride;     /* pixels_per_scanline */
    uint32_t           width;      /* horizontal resolution in pixels */
    uint32_t           height;     /* vertical resolution in pixels */
    FbPixelFormat      fmt;        /* pixel format (BGRX vs RGBX) */
    uint32_t           scale;      /* pixel scale: 1, 2, or 4 (hi-res) */
    int                ready;
} FbState;

static FbState g_fb;

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

/* Convert our 0x00RRGGBB to the hardware BGRX / RGBX 32-bit word */
static uint32_t pack_pixel(uint32_t rgb)
{
    uint32_t r = FB_R(rgb);
    uint32_t g = FB_G(rgb);
    uint32_t b = FB_B(rgb);
    if (g_fb.fmt == FB_PIXEL_RGBX32) {
        return (r << 16) | (g << 8) | b;       /* RGBX: R@[23:16] */
    }
    /* BGRX (default): B@[23:16] G@[15:8] R@[7:0] */
    return (b << 16) | (g << 8) | r;
}

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

void fb_init(const FramebufferInfo *info, FbPixelFormat fmt)
{
    if (!info || !info->base || info->width == 0 || info->height == 0) {
        return;
    }

    g_fb.base   = (volatile uint32_t *)info->base;
    g_fb.stride = info->pixels_per_scanline;
    g_fb.width  = info->width;
    g_fb.height = info->height;
    g_fb.fmt    = fmt;
    g_fb.ready  = 1;

    /*
     * Auto-select scale factor based on resolution.
     * ≥ 2560 wide   → 2× (gives readable 16px cells on QHD/4K)
     * ≥ 3840 wide   → 4× (gives readable 32px cells on 4K at density)
     * Otherwise     → 1× (1080p and below)
     */
    if (info->width >= 3840) {
        g_fb.scale = 4;
    } else if (info->width >= 2560) {
        g_fb.scale = 2;
    } else {
        g_fb.scale = 1;
    }
}

int fb_is_available(void) { return g_fb.ready; }
uint32_t fb_width(void)   { return g_fb.width; }
uint32_t fb_height(void)  { return g_fb.height; }

/** Effective character cell dimensions in pixels (accounts for scale). */
uint32_t fb_cell_w(void)  { return 8u  * g_fb.scale; }
uint32_t fb_cell_h(void)  { return 16u * g_fb.scale; }

/* -----------------------------------------------------------------------
 * Pixel-level primitives
 * --------------------------------------------------------------------- */

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t rgb)
{
    if (!g_fb.ready || x >= g_fb.width || y >= g_fb.height) return;
    g_fb.base[y * g_fb.stride + x] = pack_pixel(rgb);
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                  uint32_t rgb)
{
    uint32_t px, py;
    uint32_t packed;
    uint32_t x_end, y_end;

    if (!g_fb.ready) return;

    packed = pack_pixel(rgb);
    x_end  = x + w;
    y_end  = y + h;
    if (x_end > g_fb.width)  x_end = g_fb.width;
    if (y_end > g_fb.height) y_end = g_fb.height;

    for (py = y; py < y_end; py++) {
        volatile uint32_t *row = g_fb.base + py * g_fb.stride + x;
        for (px = 0; px < (x_end - x); px++) {
            row[px] = packed;
        }
    }
}

/* -----------------------------------------------------------------------
 * Glyph rendering
 * --------------------------------------------------------------------- */

/**
 * Draw one 8×16 glyph at pixel position (px, py).
 * Scales by g_fb.scale so the same font works at any resolution.
 */
void fb_draw_glyph(uint32_t px, uint32_t py, uint8_t ch,
                   uint32_t fg, uint32_t bg)
{
    const uint8_t *glyph;
    uint32_t row, col, sr, sc;
    uint32_t pfg, pbg;

    if (!g_fb.ready) return;

    glyph = font_8x16_glyph(ch);
    pfg   = pack_pixel(fg);
    pbg   = pack_pixel(bg);

    for (row = 0; row < 16u; row++) {
        uint8_t bits = glyph[row];
        for (col = 0; col < 8u; col++) {
            uint32_t on     = (bits >> (7u - col)) & 1u;
            uint32_t packed = on ? pfg : pbg;
            uint32_t base_x = px + col  * g_fb.scale;
            uint32_t base_y = py + row  * g_fb.scale;

            /* pixel block for scale > 1 */
            for (sr = 0; sr < g_fb.scale; sr++) {
                uint32_t fy = base_y + sr;
                if (fy >= g_fb.height) continue;
                volatile uint32_t *line =
                    g_fb.base + fy * g_fb.stride + base_x;
                for (sc = 0; sc < g_fb.scale; sc++) {
                    if (base_x + sc < g_fb.width) {
                        line[sc] = packed;
                    }
                }
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * Scrolling
 * --------------------------------------------------------------------- */

/**
 * Scroll the framebuffer up by `char_rows` character rows.
 * Each character row is (16 × scale) pixels tall.
 * The vacated rows at the bottom are filled with bg.
 *
 * Uses a word-wide copy loop — no libc memmove dependency.
 */
void fb_scroll_rows(uint32_t char_rows, uint32_t bg)
{
    uint32_t pixel_rows_to_scroll;
    uint32_t src_y, dst_y;
    uint32_t remaining_rows;
    uint32_t x;
    uint32_t packed_bg;

    if (!g_fb.ready || char_rows == 0) return;

    pixel_rows_to_scroll = char_rows * 16u * g_fb.scale;
    if (pixel_rows_to_scroll >= g_fb.height) {
        fb_fill_rect(0, 0, g_fb.width, g_fb.height, bg);
        return;
    }

    packed_bg = pack_pixel(bg);

    /* Copy pixel rows upward */
    for (dst_y = 0; dst_y + pixel_rows_to_scroll < g_fb.height; dst_y++) {
        src_y = dst_y + pixel_rows_to_scroll;
        volatile uint32_t *src = g_fb.base + src_y * g_fb.stride;
        volatile uint32_t *dst = g_fb.base + dst_y * g_fb.stride;
        for (x = 0; x < g_fb.width; x++) {
            dst[x] = src[x];
        }
    }

    /* Clear the newly exposed rows at the bottom */
    remaining_rows = g_fb.height - (g_fb.height - pixel_rows_to_scroll);
    for (dst_y = g_fb.height - pixel_rows_to_scroll;
         dst_y < g_fb.height; dst_y++) {
        volatile uint32_t *row = g_fb.base + dst_y * g_fb.stride;
        for (x = 0; x < g_fb.width; x++) {
            row[x] = packed_bg;
        }
    }
    (void)remaining_rows;  /* calculated inline above */
}
