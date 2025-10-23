/*
                                  ***   StarForth   ***

  apply_init.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:55:24.669-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/apply_init.c
 */

#define _POSIX_C_SOURCE 200112L
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>

#ifndef FBS_DEFAULT
#define FBS_DEFAULT 1024u
#endif

typedef struct {
    const char *img_path;
    const char *in_path;
    uint32_t fbs;
    int64_t start_block_guard; /* -1 = none; refuse writes below this */
    int64_t end_block_guard; /* -1 = none; refuse writes above this */
    int clip; /* allow truncation of too-long block text */
    int dry_run; /* don't write, just print plan */
    int verify; /* re-read and compare after write */
    int verbose; /* chatty logging */
} opts_t;

typedef struct {
    int64_t blkno;
    uint8_t *data;
    size_t len;
} section_t;

/* ---- utils ---- */
static void die(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

static int parse_u32(const char *s, uint32_t *out) {
    if (!s || !*s) return -1;
    uint64_t v = 0;
    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10u + (uint32_t) (*p - '0');
        if (v > 0xffffffffULL) return -1;
    }
    *out = (uint32_t) v;
    return 0;
}

static int parse_i64(const char *s, int64_t *out) {
    if (!s || !*s) return -1;
    int sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    int64_t v = 0;
    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10 + (*p - '0');
    }
    *out = sign * v;
    return 0;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "Usage: %s --img=<disk.img> --in=./conf/init.4th [options]\n"
            "Options:\n"
            "  --fbs=<bytes>        Forth block size (default 1024)\n"
            "  --start=<blk>        Guard: refuse writes to blocks < start\n"
            "  --end=<blk>          Guard: refuse writes to blocks > end\n"
            "  --clip               Truncate block text if > FBS (default: error)\n"
            "  --dry-run            Parse and print plan; do not write\n"
            "  --verify             After write, re-read block and compare bytes\n"
            "  --verbose            Chatty logging\n"
            "  -h, --help           This help\n",
            argv0);
}

/* Trim trailing CR/LF/NULs to match extractor’s behavior */
static size_t trim_trailing(const uint8_t *buf, size_t n) {
    while (n > 0 && (buf[n - 1] == '\r' || buf[n - 1] == '\n' || buf[n - 1] == '\0')) n--;
    return n;
}

/* Try to parse "Block <num>" at start of line (leading spaces allowed) */
static int try_parse_block_header(const char *line, int64_t *out_blkno) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "Block", 5) != 0) return 0;
    p += 5;
    if (!isspace((unsigned char)*p)) return 0;
    while (isspace((unsigned char)*p)) p++;
    /* Parse signed integer (we’ll error later if negative) */
    char *endp = NULL;
    long long v = strtoll(p, &endp, 10);
    if (endp == p) return 0;
    *out_blkno = (int64_t) v;
    return 1;
}

/* Dynamic buffer append */
static void sb_append(uint8_t **buf, size_t *cap, size_t *len, const uint8_t *src, size_t add) {
    if (*len + add > *cap) {
        size_t ncap = (*cap == 0 ? 1024 : *cap);
        while (ncap < *len + add) ncap *= 2;
        uint8_t *nb = (uint8_t *) realloc(*buf, ncap);
        if (!nb) die("out of memory");
        *buf = nb;
        *cap = ncap;
    }
    memcpy(*buf + *len, src, add);
    *len += add;
}

/* Write a single block to image, with zero-pad / clip handling */
static int write_block(FILE *img, uint32_t fbs, int64_t blkno,
                       const uint8_t *data_in, size_t len_in,
                       int clip, int verify, int verbose) {
    size_t len = trim_trailing(data_in, len_in);
    if (!clip && len > fbs) {
        fprintf(stderr, "Block %" PRId64 ": content %zu > FBS %u (use --clip to truncate)\n",
                blkno, len, fbs);
        return -1;
    }
    if (len > fbs) len = fbs;

    /* Prepare a full block buffer */
    uint8_t *blk = (uint8_t *) calloc(1, fbs);
    if (!blk) die("calloc failed");
    memcpy(blk, data_in, len);

    off_t off = (off_t) blkno * (off_t) fbs;
    if (fseeko(img, off, SEEK_SET) != 0) {
        perror("seek(img)");
        free(blk);
        return -1;
    }
    size_t n = fwrite(blk, 1, fbs, img);
    if (n != fbs) {
        perror("write(img)");
        free(blk);
        return -1;
    }
    if (verbose) {
        fprintf(stderr, "Wrote block %" PRId64 " (%zu/%u bytes used, padded %u)\n",
                blkno, len, fbs, (unsigned) (fbs - len));
    }

    if (verify) {
        if (fseeko(img, off, SEEK_SET) != 0) {
            perror("seek(img)");
            free(blk);
            return -1;
        }
        uint8_t *rb = (uint8_t *) malloc(fbs);
        if (!rb) die("malloc verify");
        size_t rn = fread(rb, 1, fbs, img);
        int ok = (rn == fbs) && (memcmp(rb, blk, fbs) == 0);
        free(rb);
        if (!ok) {
            fprintf(stderr, "Verify failed at block %" PRId64 "\n", blkno);
            free(blk);
            return -1;
        }
    }

    free(blk);
    return 0;
}

/* ---- main ---- */
int main(int argc, char **argv) {
    opts_t o;
    memset(&o, 0, sizeof(o));
    o.fbs = FBS_DEFAULT;
    o.start_block_guard = -1;
    o.end_block_guard = -1;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--img=", 6) == 0) o.img_path = argv[i] + 6;
        else if (strncmp(argv[i], "--in=", 5) == 0) o.in_path = argv[i] + 5;
        else if (strncmp(argv[i], "--fbs=", 6) == 0) {
            uint32_t t;
            if (parse_u32(argv[i] + 6, &t) || !t) die("bad --fbs");
            o.fbs = t;
        } else if (strncmp(argv[i], "--start=", 8) == 0) {
            int64_t t;
            if (parse_i64(argv[i] + 8, &t) || t < 0) die("bad --start");
            o.start_block_guard = t;
        } else if (strncmp(argv[i], "--end=", 6) == 0) {
            int64_t t;
            if (parse_i64(argv[i] + 6, &t) || t < 0) die("bad --end");
            o.end_block_guard = t;
        } else if (strcmp(argv[i], "--clip") == 0) o.clip = 1;
        else if (strcmp(argv[i], "--dry-run") == 0) o.dry_run = 1;
        else if (strcmp(argv[i], "--verify") == 0) o.verify = 1;
        else if (strcmp(argv[i], "--verbose") == 0) o.verbose = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }
    if (!o.img_path || !o.in_path) {
        usage(argv[0]);
        return 2;
    }

    FILE *fin = fopen(o.in_path, "rb");
    if (!fin) {
        perror(o.in_path);
        return 1;
    }
    FILE *fimg = fopen(o.img_path, o.dry_run ? "rb" : "r+b");
    if (!fimg) {
        perror(o.img_path);
        fclose(fin);
        return 1;
    }

    /* Parser state */
    char line[8192];
    int in_section = 0;
    int64_t cur_blk = -1;
    uint8_t *buf = NULL;
    size_t cap = 0, len = 0;
    unsigned long sections = 0, written = 0;

    /* Helper to finalize a section */
    auto int finalize(void) {
        if (!in_section) return 0;
        if (o.start_block_guard >= 0 && cur_blk < o.start_block_guard) {
            fprintf(stderr, "Refusing to write block %" PRId64 " (< start guard %" PRId64 ")\n",
                    cur_blk, o.start_block_guard);
            return -1;
        }
        if (o.end_block_guard >= 0 && cur_blk > o.end_block_guard) {
            fprintf(stderr, "Refusing to write block %" PRId64 " (> end guard %" PRId64 ")\n",
                    cur_blk, o.end_block_guard);
            return -1;
        }
        sections++;
        if (o.verbose || o.dry_run) {
            size_t tl = trim_trailing(buf, len);
            fprintf(stderr, "%s Block %" PRId64 "  (%zu byte payload)\n",
                    o.dry_run ? "[plan]" : "[apply]", cur_blk, tl);
        }
        if (!o.dry_run) {
            if (write_block(fimg, o.fbs, cur_blk, buf, len, o.clip, o.verify, o.verbose) != 0)
                return -1;
            written++;
        }
        /* reset */
        in_section = 0;
        cur_blk = -1;
        len = 0;
        return 0;
    }

    while (fgets(line, sizeof(line), fin)) {
        int64_t blkno = -1;
        if (try_parse_block_header(line, &blkno)) {
            if (blkno < 0) {
                fprintf(stderr, "Ignoring negative block number: %lld\n", (long long) blkno);
                blkno = -1;
            }
            if (in_section) {
                if (finalize() != 0) {
                    free(buf);
                    fclose(fin);
                    fclose(fimg);
                    return 1;
                }
            }
            if (blkno >= 0) {
                in_section = 1;
                cur_blk = blkno;
                len = 0;
            }
            continue;
        }

        /* Outside a section: ignore comments like "(- ... )" and blank lines */
        if (!in_section) continue;

        /* Inside a section: append raw line bytes */
        size_t L = strlen(line);
        sb_append(&buf, &cap, &len, (const uint8_t *) line, L);
    }

    /* EOF: flush last */
    if (in_section) {
        if (finalize() != 0) {
            free(buf);
            fclose(fin);
            fclose(fimg);
            return 1;
        }
    }

    free(buf);
    fclose(fin);
    fclose(fimg);

    fprintf(stderr, "Sections parsed: %lu; %s %lu block(s)\n",
            sections, o.dry_run ? "would write" : "wrote", o.dry_run ? sections : written);
    return 0;
}
