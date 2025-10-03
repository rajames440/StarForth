#define _POSIX_C_SOURCE 200112L
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>

#ifndef FBS_DEFAULT
#define FBS_DEFAULT 1024u
#endif

typedef struct {
    const char *img_path;
    const char *out_path;
    uint32_t fbs;
    int64_t start_block;
    int64_t end_block;
    int loose;
    int require_close;
    int64_t max_picks;
} opts_t;

/* ---------- utility ---------- */
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
            "Usage: %s --img <raw.img> --out <init.4th> [--fbs=1024] [--start=0] [--end=-1]\n"
            "                [--loose] [--require-close] [--max=N]\n",
            argv0);
}

/* ---------- block helpers ---------- */
static int header_offset_if_candidate(const uint8_t *blk, uint32_t fbs, int loose) {
    size_t i = 0;
    if (loose) {
        if (fbs >= 3 && i + 3 <= fbs && blk[0] == 0xEF && blk[1] == 0xBB && blk[2] == 0xBF)
            i += 3;
        while (i < fbs && (blk[i] == ' ' || blk[i] == '\t' || blk[i] == '\r' || blk[i] == '\n'))
            i++;
    }
    if (i + 2 <= fbs && blk[i] == '(' && blk[i + 1] == '-') return (int) i;
    return -1;
}

static size_t block_text_len(const uint8_t *blk, uint32_t fbs) {
    size_t n = 0;
    while (n < fbs && blk[n] != '\0') n++;
    while (n > 0 && (blk[n - 1] == '\0' || blk[n - 1] == '\r' || blk[n - 1] == '\n')) n--;
    return n;
}

static int has_required_close(const uint8_t *blk, uint32_t fbs, int hdr_off) {
    size_t plen = block_text_len(blk, fbs);
    if ((size_t) hdr_off >= plen) return 0;
    for (size_t i = (size_t) hdr_off + 2; i < plen; ++i)
        if (blk[i] == ')') return 1;
    return 0;
}

/* ---------- file helpers ---------- */
static int64_t file_size_bytes(FILE *f) {
    if (fseeko(f, 0, SEEK_END) != 0) return -1;
    off_t pos = ftello(f);
    if (pos < 0) return -1;
    if (fseeko(f, 0, SEEK_SET) != 0) return -1;
    return (int64_t) pos;
}

/* ---------- main ---------- */
int main(int argc, char **argv) {
    opts_t o = {0};
    o.fbs = FBS_DEFAULT;
    o.start_block = 0;
    o.end_block = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--loose") == 0) {
            o.loose = 1;
        } else if (strcmp(argv[i], "--require-close") == 0) {
            o.require_close = 1;
        } else if (strncmp(argv[i], "--img=", 6) == 0) {
            o.img_path = argv[i] + 6;
        } else if (strncmp(argv[i], "--out=", 6) == 0) {
            o.out_path = argv[i] + 6;
        } else if (strncmp(argv[i], "--fbs=", 6) == 0) {
            uint32_t tmp;
            if (parse_u32(argv[i] + 6, &tmp) != 0 || tmp == 0) {
                fprintf(stderr, "Invalid --fbs value: %s\n", argv[i] + 6);
                return 2;
            }
            o.fbs = tmp;
        } else if (strncmp(argv[i], "--start=", 8) == 0) {
            int64_t tmp;
            if (parse_i64(argv[i] + 8, &tmp) != 0 || tmp < 0) {
                fprintf(stderr, "Invalid --start value: %s\n", argv[i] + 8);
                return 2;
            }
            o.start_block = tmp;
        } else if (strncmp(argv[i], "--end=", 6) == 0) {
            int64_t tmp;
            if (parse_i64(argv[i] + 6, &tmp) != 0) {
                fprintf(stderr, "Invalid --end value: %s\n", argv[i] + 6);
                return 2;
            }
            o.end_block = tmp;
        } else if (strncmp(argv[i], "--max=", 6) == 0) {
            int64_t tmp;
            if (parse_i64(argv[i] + 6, &tmp) != 0 || tmp < 1) {
                fprintf(stderr, "Invalid --max value: %s\n", argv[i] + 6);
                return 2;
            }
            o.max_picks = tmp;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    if (!o.img_path || !o.out_path) {
        usage(argv[0]);
        return 2;
    }

    FILE *fin = fopen(o.img_path, "rb");
    if (!fin) {
        perror(o.img_path);
        return 1;
    }
    FILE *fout = fopen(o.out_path, "wb");
    if (!fout) {
        perror(o.out_path);
        fclose(fin);
        return 1;
    }

    /* Determine end block if -1 */
    if (o.end_block < 0) {
        int64_t bytes = file_size_bytes(fin);
        if (bytes < 0) die("could not stat image size");
        int64_t total_blocks = bytes / (int64_t) o.fbs;
        o.end_block = total_blocks - 1;
        if (fseeko(fin, (off_t) o.start_block * (off_t) o.fbs, SEEK_SET) != 0)
            die("seek failed");
    } else {
        if (fseeko(fin, (off_t) o.start_block * (off_t) o.fbs, SEEK_SET) != 0)
            die("seek failed");
    }

    uint8_t *buf = malloc(o.fbs);
    if (!buf) die("malloc failed");

    unsigned long picked = 0;
    int64_t blkno = o.start_block;

    fprintf(fout, "(- StarForth INIT export )\n");
    fprintf(fout, "(- Source: %s )\n", o.img_path);
    fprintf(fout, "(- FBS: %u  Range: %" PRId64 " .. %" PRId64 "  Mode: %s  RequireClose: %s  Max: %s )\n\n",
            o.fbs, o.start_block, o.end_block,
            o.loose ? "loose" : "strict",
            o.require_close ? "yes" : "no",
            (o.max_picks > 0 ? "set" : "unlimited"));

    for (; blkno <= o.end_block; ++blkno) {
        size_t n = fread(buf, 1, o.fbs, fin);
        if (n == 0) break;
        if (n < o.fbs) memset(buf + n, 0, o.fbs - n);

        int hdr_off = header_offset_if_candidate(buf, o.fbs, o.loose);
        if (hdr_off < 0) continue;
        if (o.require_close && !has_required_close(buf, o.fbs, hdr_off)) continue;

        size_t plen = block_text_len(buf, o.fbs);
        if (plen == 0) continue;

        fprintf(fout, "Block %" PRId64 "\n", blkno);
        fwrite(buf, 1, plen, fout);
        fputc('\n', fout);
        fputc('\n', fout);

        picked++;
        if (o.max_picks > 0 && (int64_t) picked >= o.max_picks) break;
    }

    free(buf);
    fclose(fin);
    fclose(fout);

    fprintf(stderr, "Picked %lu block(s) → %s\n", picked, o.out_path);
    return 0;
}
