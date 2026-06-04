/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0.
*/

/**
 * fbtest.c — Host-side render harness for the LithosAnanke framebuffer console.
 *
 * Compiles framebuffer.c + vt100.c + font_8x16.c directly against host gcc.
 * No hardware, no kernel, no QEMU required.
 *
 * What it tests
 * ─────────────
 *  • All 16 ANSI fg/bg palette colors (ESC[30-37, 40-47, 90-97, 100-107)
 *  • 24-bit truecolor fg and bg (ESC[38;2;r;g;b m  /  ESC[48;2;r;g;b m)
 *  • 256-color ramp (ESC[38;5;n m)
 *  • SGR bold, underline, reverse video
 *  • Cursor positioning  ESC[r;cH
 *  • Erase-line  ESC[2K
 *  • Scrolling: N lines past the bottom → content scrolls up
 *  • CP437 box-drawing glyphs (rendered via embedded font)
 *  • Auto hi-res scale: 800×600 (scale=1) and 2560×1440 (scale=2)
 *
 * Each run emits a viewable PNG image and runs pixel-level assertions.
 * Exit code: 0 = all assertions pass; non-zero = failure.
 *
 * Usage (built via Makefile.starkernel `fbtest` target):
 *   ./build/host/fbtest
 *   → build/host/console-800x600.png
 *   → build/host/console-2560x1440.png
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Pull in the three kernel HAL headers.
 * framebuffer.h includes uefi.h for FramebufferInfo — uefi.h uses only
 * <stdint.h>/<stddef.h> and __attribute__((ms_abi)), both fine on host.
 * --------------------------------------------------------------------- */

#include "framebuffer.h"   /* resolved via -Iinclude/starkernel in HOSTCC rule */
#include "vt100.h"

/* Font data accessor declared here; definition comes from font_8x16.c */
extern const uint8_t *font_8x16_glyph(uint8_t ch);

/* -----------------------------------------------------------------------
 * Minimal self-contained PNG writer
 * Encodes as 8-bit RGBA (4 bytes/pixel) using uncompressed zlib blocks.
 * No external zlib/libpng required.
 * --------------------------------------------------------------------- */

static uint32_t png_crc(const uint8_t *buf, size_t len)
{
    static uint32_t table[256];
    static int built = 0;
    uint32_t c;
    size_t i, k;

    if (!built) {
        for (i = 0; i < 256; i++) {
            c = (uint32_t)i;
            for (k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        built = 1;
    }
    c = 0xFFFFFFFFu;
    for (i = 0; i < len; i++) c = table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void png_u32be(uint8_t *p, uint32_t v)
{
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >>  8) & 0xFF;
    p[3] =  v        & 0xFF;
}

static void png_write_chunk(FILE *f, const char type[4],
                            const uint8_t *data, uint32_t len)
{
    uint8_t hdr[4];
    uint8_t crc_buf[4];
    uint8_t type_bytes[4];
    uint32_t crc_val;

    /* length field */
    png_u32be(hdr, len);
    fwrite(hdr, 1, 4, f);

    /* type + data + crc */
    memcpy(type_bytes, type, 4);
    fwrite(type_bytes, 1, 4, f);
    if (len) fwrite(data, 1, len, f);

    /* CRC over type+data */
    {
        uint8_t *tmp = (uint8_t *)malloc(4 + len);
        memcpy(tmp, type_bytes, 4);
        if (len) memcpy(tmp + 4, data, len);
        crc_val = png_crc(tmp, 4 + len);
        free(tmp);
    }
    png_u32be(crc_buf, crc_val);
    fwrite(crc_buf, 1, 4, f);
}

/* Write RGBA framebuffer (32-bit BGRX pixels, as produced by framebuffer.c) to PNG.
 * Converts BGRX → RGBA on the fly. */
static int write_png(const char *path, const uint32_t *pixels,
                     uint32_t w, uint32_t h, uint32_t stride)
{
    FILE *f;
    uint8_t sig[8]   = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
    uint8_t ihdr[13] = {0};
    uint32_t row, col;
    /* IDAT: uncompressed zlib, raw filter bytes included */
    size_t row_bytes = 1 + (size_t)w * 3;  /* filter byte + RGB */
    size_t raw_size  = row_bytes * h;
    /* uncompressed zlib: 2-byte header, per-block (BFINAL|BTYPE, LEN, ~LEN, data),
     * 4-byte Adler32. Block limit is 65535 bytes.                                  */
    size_t total_idat  = 2 + raw_size + ((raw_size / 65535) + 1) * 5 + 4;
    uint8_t *idat      = (uint8_t *)malloc(total_idat);
    uint8_t *raw_rgba  = (uint8_t *)malloc(row_bytes * h);
    size_t   idat_pos  = 0;
    size_t   raw_pos   = 0;
    uint32_t adler_s1  = 1, adler_s2 = 0;
    size_t   rem, blk;

    if (!idat || !raw_rgba) { free(idat); free(raw_rgba); return -1; }

    /* Build filtered scanlines (filter 0 = None) */
    for (row = 0; row < h; row++) {
        raw_rgba[raw_pos++] = 0; /* filter = None */
        for (col = 0; col < w; col++) {
            uint32_t px = pixels[row * stride + col];
            /* BGRX in memory: B@[23:16] G@[15:8] R@[7:0] → PNG RGB */
            raw_rgba[raw_pos++] = (px >>  0) & 0xFF; /* R */
            raw_rgba[raw_pos++] = (px >>  8) & 0xFF; /* G */
            raw_rgba[raw_pos++] = (px >> 16) & 0xFF; /* B */
        }
    }

    /* Adler32 over raw data */
    for (size_t i = 0; i < raw_size; i++) {
        adler_s1 = (adler_s1 + raw_rgba[i]) % 65521;
        adler_s2 = (adler_s2 + adler_s1)   % 65521;
    }

    /* Zlib header: CMF=0x78, FLG=0x01 (DEFLATE, no dict, check bits) */
    idat[idat_pos++] = 0x78;
    idat[idat_pos++] = 0x01;

    /* Emit uncompressed DEFLATE blocks */
    rem = raw_size;
    raw_pos = 0;
    while (rem > 0) {
        blk = (rem > 65535) ? 65535 : rem;
        uint8_t bfinal = (rem == blk) ? 1 : 0;
        idat[idat_pos++] = bfinal; /* BFINAL | BTYPE=00 */
        idat[idat_pos++] = (uint8_t)(blk & 0xFF);
        idat[idat_pos++] = (uint8_t)(blk >> 8);
        idat[idat_pos++] = (uint8_t)(~blk & 0xFF);
        idat[idat_pos++] = (uint8_t)(~blk >> 8);
        memcpy(idat + idat_pos, raw_rgba + raw_pos, blk);
        idat_pos += blk;
        raw_pos  += blk;
        rem      -= blk;
    }

    /* Adler32 big-endian */
    idat[idat_pos++] = (adler_s2 >> 8) & 0xFF;
    idat[idat_pos++] =  adler_s2       & 0xFF;
    idat[idat_pos++] = (adler_s1 >> 8) & 0xFF;
    idat[idat_pos++] =  adler_s1       & 0xFF;

    f = fopen(path, "wb");
    if (!f) { free(idat); free(raw_rgba); return -1; }

    fwrite(sig, 1, 8, f);

    /* IHDR: width, height, 8-bit, RGB (color type 2), deflate, no filter, no interlace */
    png_u32be(ihdr + 0, w);
    png_u32be(ihdr + 4, h);
    ihdr[8]  = 8;   /* bit depth */
    ihdr[9]  = 2;   /* color type: RGB */
    ihdr[10] = 0;   /* compression */
    ihdr[11] = 0;   /* filter method */
    ihdr[12] = 0;   /* interlace */
    png_write_chunk(f, "IHDR", ihdr, 13);

    png_write_chunk(f, "IDAT", idat, (uint32_t)idat_pos);
    png_write_chunk(f, "IEND", NULL, 0);

    fclose(f);
    free(idat);
    free(raw_rgba);
    return 0;
}

/* -----------------------------------------------------------------------
 * Assertion helpers
 * --------------------------------------------------------------------- */

static int g_failures = 0;

#define ASSERT_EQ(label, got, want) do { \
    if ((got) != (want)) { \
        fprintf(stderr, "FAIL  %-50s  got=0x%08X  want=0x%08X\n", \
                label, (unsigned)(got), (unsigned)(want)); \
        g_failures++; \
    } else { \
        printf("PASS  %s\n", label); \
    } \
} while(0)

/* Read pixel from framebuffer buffer.  Returns packed pixel (BGRX format). */
static uint32_t px(const uint32_t *fb, uint32_t stride,
                   uint32_t px_x, uint32_t px_y)
{
    return fb[px_y * stride + px_x] & 0x00FFFFFFu;  /* mask reserved byte */
}

/* Read pixel at *character cell* coordinates (col, row), offset by (dx, dy) pixels
 * within the cell. */
static uint32_t cell_px(const uint32_t *fb, uint32_t stride,
                        uint32_t col, uint32_t row,
                        uint32_t dx, uint32_t dy)
{
    return px(fb, stride,
              col * fb_cell_w() + dx,
              row * fb_cell_h() + dy);
}

/* Pack an 0x00RRGGBB value as BGRX for comparison with framebuffer contents.
 * framebuffer.c writes BGRX32:  B@[23:16] G@[15:8] R@[7:0] */
static uint32_t bgrx(uint32_t rrggbb)
{
    uint32_t r = (rrggbb >> 16) & 0xFF;
    uint32_t g = (rrggbb >>  8) & 0xFF;
    uint32_t b =  rrggbb        & 0xFF;
    return (b << 16) | (g << 8) | r;
}

/* -----------------------------------------------------------------------
 * ANSI test page emitted into the terminal
 * --------------------------------------------------------------------- */

static void emit_test_page(void)
{
    int i;
    char buf[128];

    /* Clear screen, home cursor */
    vt100_puts("\033[2J\033[H");

    /* Title */
    vt100_puts("\033[1;37mLithosAnanke Framebuffer Console Test\033[0m\r\n\r\n");

    /* 8 standard fg colors */
    vt100_puts("FG colors: ");
    for (i = 30; i <= 37; i++) {
        snprintf(buf, sizeof(buf), "\033[%dmABC\033[0m ", i);
        vt100_puts(buf);
    }
    vt100_puts("\r\n");

    /* 8 bright fg colors */
    vt100_puts("Bright FG: ");
    for (i = 90; i <= 97; i++) {
        snprintf(buf, sizeof(buf), "\033[%dmABC\033[0m ", i);
        vt100_puts(buf);
    }
    vt100_puts("\r\n");

    /* 8 standard bg colors */
    vt100_puts("BG colors: ");
    for (i = 40; i <= 47; i++) {
        snprintf(buf, sizeof(buf), "\033[%dm   \033[0m ", i);
        vt100_puts(buf);
    }
    vt100_puts("\r\n");

    /* Truecolor fg gradient (red→green) */
    vt100_puts("Truecolor: ");
    for (i = 0; i <= 31; i++) {
        int r = 255 - i * 8;
        int g = i * 8;
        snprintf(buf, sizeof(buf), "\033[38;2;%d;%d;0m█\033[0m", r, g);
        vt100_puts(buf);
    }
    vt100_puts("\r\n");

    /* Truecolor bg gradient (blue→cyan) */
    vt100_puts("TC-BG    : ");
    for (i = 0; i <= 31; i++) {
        int g = i * 8;
        snprintf(buf, sizeof(buf), "\033[48;2;0;%d;255m \033[0m", g);
        vt100_puts(buf);
    }
    vt100_puts("\r\n");

    /* 256-color index ramp (first 32) */
    vt100_puts("256color:  ");
    for (i = 0; i < 32; i++) {
        snprintf(buf, sizeof(buf), "\033[38;5;%dm█\033[0m", i);
        vt100_puts(buf);
    }
    vt100_puts("\r\n");

    /* SGR attrs */
    vt100_puts("\033[1mBold\033[0m  "
               "\033[4mUnderline\033[0m  "
               "\033[7mReverse\033[0m  "
               "\033[1;4;7mBold+Ul+Rev\033[0m\r\n");

    /* Box-drawing (CP437 chars) */
    vt100_puts("\xC9\xCD\xCD\xCD\xCD\xBB\r\n"
               "\xBA    \xBA\r\n"
               "\xC8\xCD\xCD\xCD\xCD\xBC\r\n");

    /* Cursor positioning + erase test (row 12, col 5) */
    vt100_puts("\033[12;1H\033[2KCursor-positioned line (row 12)\r\n");

    /* Explicitly place a known truecolor pixel for assertion */
    vt100_puts("\033[14;1H\033[48;2;18;52;86m \033[0m");  /* cell (0,13): bg=0x123456 */

    /* Red 'A' for assertion at row 15, col 0 */
    vt100_puts("\033[15;1H\033[31mA\033[0m");

    /* Reverse 'X' at row 16, col 0 */
    vt100_puts("\033[16;1H\033[7mX\033[0m");
}

/* -----------------------------------------------------------------------
 * Scroll test: fill terminal with numbered lines then overflow by 3
 * --------------------------------------------------------------------- */

static void emit_scroll_test(uint32_t rows)
{
    uint32_t i;
    char buf[32];
    vt100_puts("\033[2J\033[H");
    for (i = 0; i < rows + 3; i++) {
        /* Omit trailing \r\n on the last line so the final text stays on the
         * last visible row rather than scrolling it off into the blank row below. */
        if (i < rows + 2)
            snprintf(buf, sizeof(buf), "Line %u\r\n", i);
        else
            snprintf(buf, sizeof(buf), "Line %u", i);
        vt100_puts(buf);
    }
}

/* -----------------------------------------------------------------------
 * Run one full test pass on a framebuffer of given dimensions
 * --------------------------------------------------------------------- */

static int run_test(uint32_t w, uint32_t h, const char *png_path)
{
    size_t   buf_bytes = (size_t)w * h * 4;
    uint32_t *framebuf = (uint32_t *)calloc(buf_bytes, 1);
    FramebufferInfo fi;
    uint32_t cw, ch, cols, rows;
    uint32_t lit_x, lit_y;
    int      found_lit;
    uint32_t c;
    int      prev_failures = g_failures;

    if (!framebuf) { fprintf(stderr, "OOM\n"); return -1; }

    memset(&fi, 0, sizeof(fi));
    fi.base               = framebuf;
    fi.size               = buf_bytes;
    fi.width              = w;
    fi.height             = h;
    fi.pixels_per_scanline = w;  /* tight packing */

    /* Initialise console */
    fb_init(&fi, FB_PIXEL_BGRX32);
    vt100_init();

    cw   = fb_cell_w();
    ch   = fb_cell_h();
    cols = fb_width()  / cw;
    rows = fb_height() / ch;

    printf("\n=== %ux%u  cells=%ux%u  cell_size=%ux%u ===\n",
           w, h, cols, rows, cw, ch);

    /* ------------------------------------------------------------------ */
    /* Assertion 1: after vt100_init (which calls ESC[2J), all pixels == bg */
    {
        uint32_t bg = bgrx(0x000000);
        uint32_t p  = px(framebuf, w, 0, 0);
        ASSERT_EQ("A1: pixel(0,0) == bg after clear", p, bg);
    }

    /* ------------------------------------------------------------------ */
    /* Render the test page */
    emit_test_page();

    /* ------------------------------------------------------------------ */
    /* Assertion 2: red 'A' at row 14 (0-based), col 0.
     * 'A' glyph: font row 0 = 0x18 → bits 7,6,5 = 0,0,0; bit 4=1 (leftmost lit at col3).
     * We just check that at least one pixel in the cell matches the red palette entry
     * AND that a clear pixel matches bg.
     */
    {
        /* row 14 in 0-based terms = ESC[15;1H */
        uint32_t red_fg = bgrx(0xAA0000);
        uint32_t bg     = bgrx(0x000000);
        const uint8_t *glyph = font_8x16_glyph('A');
        uint32_t row_in_cell;

        found_lit = 0;
        for (row_in_cell = 0; row_in_cell < 16 && !found_lit; row_in_cell++) {
            uint8_t bits = glyph[row_in_cell];
            uint32_t bit;
            for (bit = 0; bit < 8 && !found_lit; bit++) {
                if ((bits >> (7u - bit)) & 1u) {
                    lit_x = bit  * (cw / 8);
                    lit_y = row_in_cell * (ch / 16);
                    found_lit = 1;
                }
            }
        }
        if (found_lit) {
            uint32_t got = cell_px(framebuf, w, 0, 14, lit_x, lit_y);
            ASSERT_EQ("A2: red 'A' lit pixel == ANSI red", got, red_fg);
            /* first pixel of cell should be bg (bit7 of row0 of 'A' glyph is 0) */
            uint32_t got_bg = cell_px(framebuf, w, 0, 14, 0, 0);
            ASSERT_EQ("A3: red 'A' clear pixel == bg", got_bg, bg);
        } else {
            printf("SKIP  A2/A3 (could not find lit pixel in 'A' glyph)\n");
        }
    }

    /* ------------------------------------------------------------------ */
    /* Assertion 4: truecolor bg 0x123456 at row 13, col 0 */
    {
        uint32_t want = bgrx(0x123456);
        uint32_t got  = cell_px(framebuf, w, 0, 13, cw/2, ch/2);
        ASSERT_EQ("A4: truecolor bg 0x123456 at cell(0,13)", got, want);
    }

    /* ------------------------------------------------------------------ */
    /* Assertion 5: reverse video 'X' at row 15, col 0.
     * Default fg=0xAAAAAA, bg=0x000000.  Reverse: lit pixel == bg-color,
     * clear pixel == fg-color.
     */
    {
        const uint8_t *glyph = font_8x16_glyph('X');
        uint32_t rev_lit   = bgrx(0x000000);  /* lit pixels get bg color */
        uint32_t rev_clear = bgrx(0xAAAAAA);  /* clear pixels get fg color */
        uint32_t row_in_cell;

        found_lit = 0;
        for (row_in_cell = 0; row_in_cell < 16 && !found_lit; row_in_cell++) {
            uint8_t bits = glyph[row_in_cell];
            uint32_t bit;
            for (bit = 0; bit < 8 && !found_lit; bit++) {
                if ((bits >> (7u - bit)) & 1u) {
                    lit_x = bit  * (cw / 8);
                    lit_y = row_in_cell * (ch / 16);
                    found_lit = 1;
                }
            }
        }
        if (found_lit) {
            uint32_t got_lit   = cell_px(framebuf, w, 0, 15, lit_x, lit_y);
            uint32_t got_clear = cell_px(framebuf, w, 0, 15, 0, 0);
            ASSERT_EQ("A5: reverse 'X' lit pixel == bg-color",   got_lit,   rev_lit);
            ASSERT_EQ("A6: reverse 'X' clear pixel == fg-color", got_clear, rev_clear);
        } else {
            printf("SKIP  A5/A6 (could not find lit pixel in 'X' glyph)\n");
        }
    }

    /* ------------------------------------------------------------------ */
    /* Assertion 7: cursor-positioned line at row 11 ("Cursor-positioned line…")
     * Just verify the 'C' of "Cursor" was placed — its cell should not be bg.
     */
    {
        uint32_t bg  = bgrx(0x000000);
        /* ESC[12;1H → row=11 (0-based), col=0 */
        const uint8_t *glyph = font_8x16_glyph('C');
        uint32_t row_in_cell;
        int non_bg = 0;
        for (row_in_cell = 0; row_in_cell < 16 && !non_bg; row_in_cell++) {
            uint8_t bits = glyph[row_in_cell];
            uint32_t bit;
            for (bit = 0; bit < 8 && !non_bg; bit++) {
                if ((bits >> (7u - bit)) & 1u) {
                    uint32_t got = cell_px(framebuf, w, 0, 11,
                                          bit * (cw/8),
                                          row_in_cell * (ch/16));
                    non_bg = (got != bg);
                }
            }
        }
        if (non_bg) {
            printf("PASS  A7: cursor-positioned 'C' at row 11, col 0 is non-bg\n");
        } else {
            fprintf(stderr, "FAIL  A7: cursor-positioned 'C' at row 11, col 0 — all bg\n");
            g_failures++;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Assertion 8: scroll — after rows+3 newlines the last row visible
     * should contain "Line <rows+2>"; we just verify that at least one
     * non-bg pixel exists on the last character row (something was scrolled there).
     */
    emit_scroll_test(rows);
    {
        uint32_t bg = bgrx(0x000000);
        uint32_t last_row = rows - 1;
        int non_bg = 0;
        uint32_t col;
        for (col = 0; col < cols && !non_bg; col++) {
            uint32_t cpx = cell_px(framebuf, w, col, last_row, cw/2, ch/2);
            non_bg = (cpx != bg);
        }
        if (non_bg) {
            printf("PASS  A8: scroll — last row has content after overflow\n");
        } else {
            fprintf(stderr, "FAIL  A8: scroll — last row is entirely bg after overflow\n");
            g_failures++;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Write PNG */
    if (write_png(png_path, framebuf, w, h, w) == 0) {
        printf("PNG   %s\n", png_path);
    } else {
        fprintf(stderr, "WARN  could not write PNG: %s\n", png_path);
    }

    free(framebuf);

    /* Report assertion count for this resolution */
    c = (uint32_t)(g_failures - prev_failures);
    if (c == 0) {
        printf("OK    All assertions passed for %ux%u\n", w, h);
    } else {
        fprintf(stderr, "FAIL  %u assertion(s) failed for %ux%u\n", c, w, h);
    }
    return (c == 0) ? 0 : -1;
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(void)
{
    int rc = 0;

    printf("StarForth framebuffer console host render test\n");
    printf("==============================================\n");

    /* Test at standard resolution (scale=1) */
    rc |= run_test(800, 600, "build/host/console-800x600.png");

    /* Test at QHD resolution (scale=2) */
    rc |= run_test(2560, 1440, "build/host/console-2560x1440.png");

    printf("\n");
    if (g_failures == 0) {
        printf("ALL PASS — %d failure(s)\n", g_failures);
    } else {
        fprintf(stderr, "FAIL  %d assertion failure(s) total\n", g_failures);
    }
    return (g_failures == 0) ? 0 : 1;
}
