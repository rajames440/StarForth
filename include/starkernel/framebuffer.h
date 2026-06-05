/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0.
*/

/**
 * framebuffer.h — UEFI GOP framebuffer driver interface
 *
 * Pixel format: BGRX32 (PixelBlueGreenRedReserved8BitPerColor),
 * the dominant format for QEMU virt GOP.  RGBX32 is also supported.
 *
 * Color values throughout this API are packed as 0x00RRGGBB.
 */

#ifndef STARKERNEL_FRAMEBUFFER_H
#define STARKERNEL_FRAMEBUFFER_H

#include <stdint.h>
#include "uefi.h"

/* Pixel format of the GOP framebuffer */
typedef enum {
    FB_PIXEL_BGRX32 = 0, /* PixelBlueGreenRedReserved8BitPerColor (default) */
    FB_PIXEL_RGBX32 = 1, /* PixelRedGreenBlueReserved8BitPerColor */
} FbPixelFormat;

/* Pack / unpack 24-bit 0x00RRGGBB color */
#define FB_RGB(r, g, b) \
    (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define FB_R(c) (((uint32_t)(c) >> 16) & 0xFFu)
#define FB_G(c) (((uint32_t)(c) >>  8) & 0xFFu)
#define FB_B(c) ( (uint32_t)(c)        & 0xFFu)

/* Standard ANSI 16-color palette (indices 0–15) */
extern const uint32_t FB_ANSI_PALETTE[16];

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

/**
 * Initialize the framebuffer from UEFI bootloader GOP data.
 * Must be called before any other fb_* function.
 * fmt: pixel layout reported by GOP (default FB_PIXEL_BGRX32).
 */
void fb_init(const FramebufferInfo *info, FbPixelFormat fmt);

/** Returns 1 if the framebuffer has been successfully initialized. */
int fb_is_available(void);

/* -----------------------------------------------------------------------
 * Geometry queries
 * --------------------------------------------------------------------- */
uint32_t fb_width(void);
uint32_t fb_height(void);

/** Effective character cell width/height in pixels (8/16 × scale factor). */
uint32_t fb_cell_w(void);
uint32_t fb_cell_h(void);

/* -----------------------------------------------------------------------
 * Pixel-level primitives
 * --------------------------------------------------------------------- */

/** Write a single pixel.  Out-of-bounds writes are silently ignored. */
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t rgb);

/** Fill a rectangle with a solid color. */
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                  uint32_t rgb);

/* -----------------------------------------------------------------------
 * Glyph rendering (8 × 16 font cells)
 * --------------------------------------------------------------------- */

/**
 * Draw one 8×16 character glyph at pixel position (px, py).
 * fg / bg are packed 0x00RRGGBB colors.
 */
void fb_draw_glyph(uint32_t px, uint32_t py, uint8_t ch,
                   uint32_t fg, uint32_t bg);

/* -----------------------------------------------------------------------
 * Scrolling
 * --------------------------------------------------------------------- */

/**
 * Scroll the framebuffer up by `char_rows` character rows (each 16 px).
 * The vacated rows at the bottom are filled with bg.
 */
void fb_scroll_rows(uint32_t char_rows, uint32_t bg);

#endif /* STARKERNEL_FRAMEBUFFER_H */
