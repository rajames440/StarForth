/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  mkcapsule - Generate capsule directory from a capsule tree

  Usage:
    mkcapsule <capsules_dir> <output.c>        build capsule C source
    mkcapsule --lint <capsules_dir>            lint all .4th files
    mkcapsule --manifest <capsules_dir>        print block manifest to stdout
    mkcapsule --manifest <capsules_dir> <out>  write block manifest to file

  Build mode generates a C source file containing:
    - capsule_arena[]        (all payload bytes concatenated)
    - capsule_descriptors[]  (CapsuleDesc, one per file)
    - capsule_names[]        (CapsuleNameEntry, parallel to descriptors)
    - capsule_directory      (CapsuleDirHeader)

  Manifest mode generates a markdown block ownership index:
    - Capsule summary (name, blocks claimed, xxHash64)
    - Block map sorted by LBN with conflict detection
    - Conflict register
  The xxHash64 column is the anchor for future Ed25519 fingerprints (Phase 8).

  Name encoding: relative path from capsules_dir root, '/' replaced by ':'.
  Files whose encoded name would exceed 511 bytes are skipped with a warning.
  The file extension is preserved verbatim and is meaningful only at load time.
*/

#define _DEFAULT_SOURCE  /* For nftw, strdup */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <ftw.h>
#include <time.h>
#include <sys/stat.h>

/*===========================================================================
 * xxHash64 (embedded for build tool — no external dependency)
 *===========================================================================*/

#define XXHASH64_PRIME1  0x9E3779B185EBCA87ULL
#define XXHASH64_PRIME2  0xC2B2AE3D27D4EB4FULL
#define XXHASH64_PRIME3  0x165667B19E3779F9ULL
#define XXHASH64_PRIME4  0x85EBCA77C2B2AE63ULL
#define XXHASH64_PRIME5  0x27D4EB2F165667C5ULL

static inline uint64_t xxh64_rotl(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}

static inline uint64_t xxh64_read64(const uint8_t *p) {
    return ((uint64_t)p[0])       | ((uint64_t)p[1] << 8)  |
           ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

static inline uint32_t xxh64_read32(const uint8_t *p) {
    return ((uint32_t)p[0])       | ((uint32_t)p[1] << 8)  |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint64_t xxh64_round(uint64_t acc, uint64_t input) {
    acc += input * XXHASH64_PRIME2;
    acc = xxh64_rotl(acc, 31);
    acc *= XXHASH64_PRIME1;
    return acc;
}

static inline uint64_t xxh64_merge_round(uint64_t acc, uint64_t val) {
    val = xxh64_round(0, val);
    acc ^= val;
    acc = acc * XXHASH64_PRIME1 + XXHASH64_PRIME4;
    return acc;
}

static inline uint64_t xxh64_avalanche(uint64_t h) {
    h ^= h >> 33;
    h *= XXHASH64_PRIME2;
    h ^= h >> 29;
    h *= XXHASH64_PRIME3;
    h ^= h >> 32;
    return h;
}

static uint64_t xxhash64(const void *data, size_t len, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *end = p + len;
    uint64_t h64;

    if (len >= 32) {
        const uint8_t *limit = end - 32;
        uint64_t v1 = seed + XXHASH64_PRIME1 + XXHASH64_PRIME2;
        uint64_t v2 = seed + XXHASH64_PRIME2;
        uint64_t v3 = seed + 0;
        uint64_t v4 = seed - XXHASH64_PRIME1;
        do {
            v1 = xxh64_round(v1, xxh64_read64(p)); p += 8;
            v2 = xxh64_round(v2, xxh64_read64(p)); p += 8;
            v3 = xxh64_round(v3, xxh64_read64(p)); p += 8;
            v4 = xxh64_round(v4, xxh64_read64(p)); p += 8;
        } while (p <= limit);
        h64 = xxh64_rotl(v1, 1) + xxh64_rotl(v2, 7) +
              xxh64_rotl(v3, 12) + xxh64_rotl(v4, 18);
        h64 = xxh64_merge_round(h64, v1);
        h64 = xxh64_merge_round(h64, v2);
        h64 = xxh64_merge_round(h64, v3);
        h64 = xxh64_merge_round(h64, v4);
    } else {
        h64 = seed + XXHASH64_PRIME5;
    }
    h64 += (uint64_t)len;
    while (p + 8 <= end) {
        uint64_t k1 = xxh64_round(0, xxh64_read64(p));
        h64 ^= k1;
        h64 = xxh64_rotl(h64, 27) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
        p += 8;
    }
    if (p + 4 <= end) {
        h64 ^= (uint64_t)xxh64_read32(p) * XXHASH64_PRIME1;
        h64 = xxh64_rotl(h64, 23) * XXHASH64_PRIME2 + XXHASH64_PRIME3;
        p += 4;
    }
    while (p < end) {
        h64 ^= (uint64_t)(*p) * XXHASH64_PRIME5;
        h64 = xxh64_rotl(h64, 11) * XXHASH64_PRIME1;
        p++;
    }
    return xxh64_avalanche(h64);
}

/*===========================================================================
 * Capsule Collection
 *===========================================================================*/

#define MAX_CAPSULES          256
#define MAX_PATH_LEN          4096
#define CAPSULE_NAME_MAX      512
#define MAX_BLOCKS_PER_CAPSULE 64

typedef struct {
    char     path[MAX_PATH_LEN];          /* Absolute source file path */
    char     name[CAPSULE_NAME_MAX];      /* Colon-separated capsule name */
    uint8_t *data;                        /* File contents */
    size_t   length;                      /* File size in bytes */
    uint64_t hash;                        /* xxHash64 of contents */
    uint32_t flags;                       /* Capsule flags */
} CapsuleEntry;

static CapsuleEntry capsules[MAX_CAPSULES];
static int          capsule_count = 0;
static const char  *base_dir = NULL;
static size_t       base_dir_len = 0;

typedef struct {
    char     name[CAPSULE_NAME_MAX];
    uint64_t hash;
    int      blocks[MAX_BLOCKS_PER_CAPSULE];
    int      block_count;
} ManifestEntry;

static ManifestEntry manifest_entries[MAX_CAPSULES];
static int           manifest_count = 0;

/* Flag constants — must match capsule.h */
#define FLAG_ACTIVE       0x00000001
#define FLAG_PRODUCTION   0x00000010
#define FLAG_EXPERIMENT   0x00000020
#define FLAG_MAMA_INIT    0x00000040

/*
 * Determine flags from the colon-separated capsule name.
 *
 * init.4th (bare) is Mama's canonical init — gets FLAG_MAMA_INIT only.
 * All other capsules carry both FLAG_PRODUCTION and FLAG_EXPERIMENT so
 * that birth eligibility is not gated on mode type (D2).
 */
static uint32_t flags_from_name(const char *name) {
    uint32_t flags = FLAG_ACTIVE;

    if (strcmp(name, "init.4th") == 0) {
        flags |= FLAG_MAMA_INIT;
    } else {
        flags |= FLAG_PRODUCTION | FLAG_EXPERIMENT;
    }

    return flags;
}

/*
 * Build the colon-separated capsule name from a relative path.
 * Returns 1 on success, 0 if the name would exceed CAPSULE_NAME_MAX-1 bytes.
 */
static int build_capsule_name(const char *relpath, char *out) {
    size_t len = strlen(relpath);
    if (len >= CAPSULE_NAME_MAX) {
        return 0;  /* too long */
    }
    size_t i;
    for (i = 0; i < len; i++) {
        out[i] = (relpath[i] == '/') ? ':' : relpath[i];
    }
    out[len] = '\0';
    return 1;
}

/*
 * validate_forth_blocks: enforce 64-char × 16-line block format on .4th files.
 * Checks: block header "Block N", line length ≤ 64, ≤ 16 content lines/block,
 * and that every line (including the last) is terminated with '\n'.
 * Returns 0 if clean, >0 if violations were found.
 */
static int validate_forth_blocks(const char *fpath,
                                 const uint8_t *data, size_t size)
{
    int errors       = 0;
    int block_num    = -1;
    int line_in_block = 0;
    int file_line    = 0;
    size_t pos       = 0;

    while (pos < size) {
        size_t start = pos;
        while (pos < size && data[pos] != '\n') pos++;

        int has_newline = (pos < size);  /* stopped at '\n', not EOF */
        size_t line_len = pos - start;
        if (line_len > 0 && data[start + line_len - 1] == '\r') line_len--;

        file_line++;

        /* Unterminated: non-empty content with no trailing '\n' */
        if (!has_newline && line_len > 0) {
            fprintf(stderr,
                "mkcapsule: ERROR: %s:%d: unterminated line (missing newline)\n",
                fpath, file_line);
            errors++;
        }

        /* Build a printable snippet for diagnostics (first 40 chars) */
        char snippet[44];
        {
            size_t slen = line_len < 40 ? line_len : 40;
            size_t i;
            for (i = 0; i < slen; i++) {
                unsigned char c = data[start + i];
                snippet[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
            }
            if (line_len > 40) {
                snippet[40] = '.'; snippet[41] = '.';
                snippet[42] = '.'; snippet[43] = '\0';
            } else {
                snippet[slen] = '\0';
            }
        }

        /* Block header: "Block NNN" */
        if (line_len >= 7 &&
            data[start+0]=='B' && data[start+1]=='l' && data[start+2]=='o' &&
            data[start+3]=='c' && data[start+4]=='k' && data[start+5]==' ') {

            if (line_len > 64) {
                fprintf(stderr,
                    "mkcapsule: ERROR: %s:%d: block header %zu chars (max 64)"
                    ": \"%s\"\n",
                    fpath, file_line, line_len, snippet);
                errors++;
            }
            block_num     = (int)strtol((const char *)(data + start + 6), NULL, 10);
            line_in_block = 0;

            /* Block number must be in user space: [2048, 5120) */
            if (block_num < 2048 || block_num >= 5120) {
                fprintf(stderr,
                    "mkcapsule: ERROR: %s:%d: block %d out of user range "
                    "[2048, 5120)\n",
                    fpath, file_line, block_num);
                errors++;
            }

        } else if (block_num >= 0) {
            if (line_len > 64) {
                fprintf(stderr,
                    "mkcapsule: ERROR: %s:%d: block %d line %d: "
                    "%zu chars (max 64): \"%s\"\n",
                    fpath, file_line, block_num, line_in_block + 1,
                    line_len, snippet);
                errors++;
            }
            line_in_block++;
            if (line_in_block == 17) {
                fprintf(stderr,
                    "mkcapsule: ERROR: %s:%d: block %d: "
                    "more than 16 content lines\n",
                    fpath, file_line, block_num);
                errors++;
            }
        }

        if (has_newline) pos++;  /* advance past '\n' */
    }

    return errors;
}

/*
 * nftw callback — called once per filesystem entry.
 * Incorporates every regular file found; skips names that are too long.
 */
static int process_file(const char *fpath, const struct stat *sb,
                        int typeflag, struct FTW *ftwbuf) {
    (void)sb;

    if (typeflag != FTW_F) {
        return 0;  /* directories and symlinks: skip */
    }

    /* Skip hidden files (basename begins with '.') */
    if (fpath[ftwbuf->base] == '.') {
        return 0;
    }

    /* Derive relative path from base_dir */
    const char *relpath;
    if (strlen(fpath) > base_dir_len && fpath[base_dir_len] == '/') {
        relpath = fpath + base_dir_len + 1;
    } else {
        relpath = fpath;
    }

    /* Build colon-separated name; skip with warning if too long */
    char name[CAPSULE_NAME_MAX];
    if (!build_capsule_name(relpath, name)) {
        fprintf(stderr,
            "mkcapsule: WARNING: skipping '%s' — name would exceed %d bytes\n",
            relpath, CAPSULE_NAME_MAX - 1);
        return 0;
    }

    if (capsule_count >= MAX_CAPSULES) {
        fprintf(stderr, "mkcapsule: ERROR: too many capsules (max %d)\n",
                MAX_CAPSULES);
        return -1;
    }

    /* Read file contents */
    FILE *f = fopen(fpath, "rb");
    if (!f) {
        fprintf(stderr, "mkcapsule: ERROR: cannot open '%s'\n", fpath);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        fprintf(stderr, "mkcapsule: WARNING: skipping empty file '%s'\n", fpath);
        return 0;
    }

    uint8_t *data = malloc((size_t)size);
    if (!data) {
        fclose(f);
        fprintf(stderr, "mkcapsule: ERROR: out of memory reading '%s'\n", fpath);
        return -1;
    }

    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        fprintf(stderr, "mkcapsule: ERROR: failed to read '%s'\n", fpath);
        return -1;
    }
    fclose(f);

    /* Validate 64x16 block format for .4th capsule files */
    {
        size_t nlen = strlen(fpath);
        if (nlen >= 4 && strcmp(fpath + nlen - 4, ".4th") == 0) {
            int bv = validate_forth_blocks(fpath, data, (size_t)size);
            if (bv > 0) {
                free(data);
                fprintf(stderr,
                    "mkcapsule: ERROR: %s: %d block format violation(s) "
                    "— fix before building\n", fpath, bv);
                return -1;
            }
        }
    }

    /* Fill entry */
    CapsuleEntry *e = &capsules[capsule_count];
    strncpy(e->path, fpath, MAX_PATH_LEN - 1);
    e->path[MAX_PATH_LEN - 1] = '\0';
    strncpy(e->name, name, CAPSULE_NAME_MAX - 1);
    e->name[CAPSULE_NAME_MAX - 1] = '\0';
    e->data   = data;
    e->length = (size_t)size;
    e->hash   = xxhash64(data, (size_t)size, 0);
    e->flags  = flags_from_name(name);

    capsule_count++;

    char mode = 'e';
    if (e->flags & FLAG_MAMA_INIT)    mode = 'm';
    else if (e->flags & FLAG_PRODUCTION) mode = 'p';

    fprintf(stderr, "  [%c] %s  (%" PRIu64 " bytes, hash=0x%016" PRIx64 ")\n",
            mode, e->name, (uint64_t)e->length, e->hash);

    return 0;
}

/*===========================================================================
 * Standalone Lint Mode
 *===========================================================================*/

static int lint_errors  = 0;
static int lint_files   = 0;
static int lint_failing = 0;

static int lint_file(const char *fpath, const struct stat *sb,
                     int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    if (typeflag != FTW_F) return 0;
    if (fpath[ftwbuf->base] == '.') return 0;

    size_t nlen = strlen(fpath);
    if (nlen < 4 || strcmp(fpath + nlen - 4, ".4th") != 0) return 0;

    FILE *f = fopen(fpath, "rb");
    if (!f) {
        fprintf(stderr, "  ERROR: cannot open '%s'\n", fpath);
        lint_errors++;
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    lint_files++;

    if (sz <= 0) {
        fclose(f);
        fprintf(stderr, "  PASS: %s  (empty)\n", fpath);
        return 0;
    }

    uint8_t *data = malloc((size_t)sz);
    if (!data || fread(data, 1, (size_t)sz, f) != (size_t)sz) {
        free(data);
        fclose(f);
        fprintf(stderr, "  ERROR: read failed '%s'\n", fpath);
        lint_errors++;
        return 0;
    }
    fclose(f);

    int errs = validate_forth_blocks(fpath, data, (size_t)sz);
    free(data);

    if (errs == 0) {
        fprintf(stderr, "  PASS: %s\n", fpath);
    } else {
        fprintf(stderr, "  FAIL: %s  (%d violation%s)\n",
                fpath, errs, errs == 1 ? "" : "s");
        lint_errors  += errs;
        lint_failing++;
    }
    return 0;
}

/*===========================================================================
 * Manifest Mode
 *===========================================================================*/

/*
 * collect_block_numbers: scan file data for "Block N" header lines and
 * accumulate unique, sorted LBNs. Returns the count stored.
 */
static int collect_block_numbers(const uint8_t *data, size_t size,
                                 int *out, int max)
{
    int count = 0;
    size_t pos = 0;

    while (pos < size) {
        size_t start = pos;
        while (pos < size && data[pos] != '\n') pos++;
        size_t line_len = pos - start;
        if (line_len > 0 && data[start + line_len - 1] == '\r') line_len--;

        if (line_len >= 7 &&
            data[start+0]=='B' && data[start+1]=='l' && data[start+2]=='o' &&
            data[start+3]=='c' && data[start+4]=='k' && data[start+5]==' ') {
            int bn = (int)strtol((const char *)(data + start + 6), NULL, 10);
            int dup = 0;
            for (int i = 0; i < count; i++) {
                if (out[i] == bn) { dup = 1; break; }
            }
            if (!dup && count < max)
                out[count++] = bn;
        }

        if (pos < size) pos++;
    }
    return count;
}

static int cmp_int(const void *a, const void *b) {
    return (*(const int *)a) - (*(const int *)b);
}

static int cmp_manifest_name(const void *a, const void *b) {
    return strcmp(((const ManifestEntry *)a)->name,
                  ((const ManifestEntry *)b)->name);
}

/* nftw callback: collect .4th capsules for manifest */
static int manifest_file(const char *fpath, const struct stat *sb,
                         int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    if (typeflag != FTW_F) return 0;
    if (fpath[ftwbuf->base] == '.') return 0;

    size_t nlen = strlen(fpath);
    if (nlen < 4 || strcmp(fpath + nlen - 4, ".4th") != 0) return 0;

    const char *relpath;
    if (strlen(fpath) > base_dir_len && fpath[base_dir_len] == '/')
        relpath = fpath + base_dir_len + 1;
    else
        relpath = fpath;

    char name[CAPSULE_NAME_MAX];
    if (!build_capsule_name(relpath, name)) return 0;

    FILE *f = fopen(fpath, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return 0; }

    uint8_t *data = malloc((size_t)sz);
    if (!data || fread(data, 1, (size_t)sz, f) != (size_t)sz) {
        free(data); fclose(f); return 0;
    }
    fclose(f);

    if (manifest_count >= MAX_CAPSULES) { free(data); return 0; }

    ManifestEntry *e = &manifest_entries[manifest_count++];
    strncpy(e->name, name, CAPSULE_NAME_MAX - 1);
    e->name[CAPSULE_NAME_MAX - 1] = '\0';
    e->hash        = xxhash64(data, (size_t)sz, 0);
    e->block_count = collect_block_numbers(data, (size_t)sz,
                                           e->blocks, MAX_BLOCKS_PER_CAPSULE);
    qsort(e->blocks, (size_t)e->block_count, sizeof(int), cmp_int);

    free(data);
    return 0;
}

/* Flat (LBN, entry-index) pair used for the sorted block map */
typedef struct { int lbn; int idx; } BlockRef;

static int cmp_blockref(const void *a, const void *b) {
    const BlockRef *x = (const BlockRef *)a;
    const BlockRef *y = (const BlockRef *)b;
    if (x->lbn != y->lbn) return x->lbn - y->lbn;
    return strcmp(manifest_entries[x->idx].name,
                  manifest_entries[y->idx].name);
}

static void generate_manifest(FILE *out) {
    time_t now = time(NULL);
    struct tm *tm_ptr = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_ptr);

    qsort(manifest_entries, (size_t)manifest_count,
          sizeof(ManifestEntry), cmp_manifest_name);

    /* Build flat block reference list */
    static BlockRef refs[MAX_CAPSULES * MAX_BLOCKS_PER_CAPSULE];
    int ref_count = 0;
    for (int i = 0; i < manifest_count; i++) {
        for (int j = 0; j < manifest_entries[i].block_count; j++) {
            refs[ref_count].lbn = manifest_entries[i].blocks[j];
            refs[ref_count].idx = i;
            ref_count++;
        }
    }
    qsort(refs, (size_t)ref_count, sizeof(BlockRef), cmp_blockref);

    fprintf(out,
        "# Capsule Block Manifest — Auto-generated\n"
        "<!-- Generated by mkcapsule --manifest  %s -->\n"
        "<!-- DO NOT EDIT — re-run mkcapsule --manifest to refresh.  -->\n"
        "<!-- Hand-written justifications and immutability notes live  -->\n"
        "<!-- in MANIFEST.md alongside this auto-generated index.      -->\n"
        "\n", timestamp);

    /* Capsule summary */
    fprintf(out, "## Capsule Summary\n\n");
    fprintf(out, "| Capsule | Blocks claimed | xxHash64 |\n");
    fprintf(out, "|---------|----------------|----------|\n");
    for (int i = 0; i < manifest_count; i++) {
        ManifestEntry *e = &manifest_entries[i];
        fprintf(out, "| `%s` | ", e->name);
        if (e->block_count == 0) {
            fprintf(out, "*(none — raw code capsule)*");
        } else {
            for (int j = 0; j < e->block_count; j++) {
                if (j > 0) fprintf(out, ", ");
                fprintf(out, "%d", e->blocks[j]);
            }
        }
        fprintf(out, " | `0x%016" PRIx64 "` |\n", e->hash);
    }
    fprintf(out, "\n");

    /* Block map sorted by LBN */
    fprintf(out, "## Block Map (sorted by LBN)\n\n");
    fprintf(out, "| LBN | Capsule | xxHash64 | Status |\n");
    fprintf(out, "|-----|---------|----------|--------|\n");
    for (int i = 0; i < ref_count; i++) {
        int lbn = refs[i].lbn;
        ManifestEntry *e = &manifest_entries[refs[i].idx];
        int conflict = (i > 0             && refs[i-1].lbn == lbn) ||
                       (i < ref_count - 1 && refs[i+1].lbn == lbn);
        fprintf(out, "| %d | `%s` | `0x%016" PRIx64 "` | %s |\n",
                lbn, e->name, e->hash,
                conflict ? "CONFLICT" : "ok");
    }
    fprintf(out, "\n");

    /* Conflict register */
    int any = 0;
    for (int i = 0; i + 1 < ref_count; i++)
        if (refs[i].lbn == refs[i+1].lbn) { any = 1; break; }

    fprintf(out, "## Conflicts\n\n");
    if (!any) {
        fprintf(out, "None.\n\n");
    } else {
        fprintf(out, "| LBN | Capsules |\n");
        fprintf(out, "|-----|----------|\n");
        int i = 0;
        while (i < ref_count) {
            int lbn = refs[i].lbn;
            int j = i;
            while (j < ref_count && refs[j].lbn == lbn) j++;
            if (j - i > 1) {
                fprintf(out, "| %d | ", lbn);
                for (int k = i; k < j; k++) {
                    if (k > i) fprintf(out, ", ");
                    fprintf(out, "`%s`", manifest_entries[refs[k].idx].name);
                }
                fprintf(out, " |\n");
            }
            i = j;
        }
        fprintf(out, "\n");
    }

    fprintf(out, "---\n");
    fprintf(out,
        "*%d capsule(s) scanned.  "
        "Re-run `mkcapsule --manifest <dir>` to refresh.*\n",
        manifest_count);
}

/*===========================================================================
 * C Source Generation
 *===========================================================================*/

static void emit_escaped_name(FILE *out, const char *name) {
    /* Emit name as a C string literal with all non-printable bytes escaped */
    fputc('"', out);
    for (const char *p = name; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') {
            fputc('\\', out);
            fputc(c, out);
        } else if (c < 0x20 || c >= 0x7F) {
            fprintf(out, "\\x%02x", c);
        } else {
            fputc(c, out);
        }
    }
    fputc('"', out);
}

static void generate_output(FILE *out) {
    time_t now = time(NULL);
    struct tm *tm_ptr = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_ptr);

    fprintf(out,
        "/*\n"
        " * capsule_generated.c — Generated Capsule Directory\n"
        " *\n"
        " * Generated by mkcapsule at %s\n"
        " * DO NOT EDIT — this file is auto-generated by the build system.\n"
        " *\n"
        " * Capsule count: %d\n"
        " */\n\n",
        timestamp, capsule_count);

    fprintf(out, "#include \"starkernel/capsule.h\"\n\n");

    /*------------------------------------------------------------------
     * Payload arena
     *------------------------------------------------------------------*/
    fprintf(out,
        "/*===========================================================================\n"
        " * Payload Arena\n"
        " *===========================================================================*/\n\n"
        "const uint8_t capsule_arena[] = {\n");

    size_t arena_offset = 0;
    for (int i = 0; i < capsule_count; i++) {
        CapsuleEntry *e = &capsules[i];
        fprintf(out,
            "    /* [%d] %s  offset=%zu  length=%zu  hash=0x%016" PRIx64 " */\n",
            i, e->name, arena_offset, e->length, e->hash);
        for (size_t j = 0; j < e->length; j++) {
            if (j % 16 == 0) fprintf(out, "    ");
            fprintf(out, "0x%02X,", e->data[j]);
            if (j % 16 == 15 || j == e->length - 1) fputc('\n', out);
            else fputc(' ', out);
        }
        fputc('\n', out);
        arena_offset += e->length;
    }
    fprintf(out, "};\n\n");

    /*------------------------------------------------------------------
     * Descriptor array
     *------------------------------------------------------------------*/
    fprintf(out,
        "/*===========================================================================\n"
        " * Capsule Descriptors\n"
        " *===========================================================================*/\n\n"
        "const CapsuleDesc capsule_descriptors[%d] = {\n", capsule_count);

    size_t offset = 0;
    for (int i = 0; i < capsule_count; i++) {
        CapsuleEntry *e = &capsules[i];
        const char *flags_comment =
            (e->flags & FLAG_MAMA_INIT) ? "MAMA_INIT | ACTIVE" :
                                          "PRODUCTION | EXPERIMENT | ACTIVE";
        fprintf(out,
            "    /* [%d] %s */\n"
            "    {\n"
            "        .magic        = CAPSULE_MAGIC_PACK(CAPSULE_VERSION_0, CAPSULE_HASH_XXHASH64),\n"
            "        .capsule_id   = 0x%016" PRIx64 "ULL,\n"
            "        .content_hash = 0x%016" PRIx64 "ULL,\n"
            "        .offset       = %zuULL,\n"
            "        .length       = %zuULL,\n"
            "        .flags        = 0x%08X,  /* %s */\n"
            "        .owner_vm     = 0,\n"
            "        .birth_count  = 0,\n"
            "        .created_ns   = 0,\n"
            "    },\n",
            i, e->name,
            e->hash, e->hash,
            offset, e->length,
            e->flags, flags_comment);
        offset += e->length;
    }
    fprintf(out, "};\n\n");

    /*------------------------------------------------------------------
     * Name array (parallel to descriptors)
     *------------------------------------------------------------------*/
    fprintf(out,
        "/*===========================================================================\n"
        " * Capsule Names  (colon-separated paths, 512 bytes each)\n"
        " *===========================================================================*/\n\n"
        "const CapsuleNameEntry capsule_names[%d] = {\n", capsule_count);

    for (int i = 0; i < capsule_count; i++) {
        CapsuleEntry *e = &capsules[i];
        fprintf(out, "    /* [%d] */ { .name = ", i);
        emit_escaped_name(out, e->name);
        fprintf(out, " },\n");
    }
    fprintf(out, "};\n\n");

    /*------------------------------------------------------------------
     * Directory header
     *------------------------------------------------------------------*/
    uint64_t dir_hash = 0;
    for (int i = 0; i < capsule_count; i++) {
        dir_hash = xxhash64(&capsules[i].hash, sizeof(uint64_t), dir_hash);
    }

    fprintf(out,
        "/*===========================================================================\n"
        " * Directory Header\n"
        " *===========================================================================*/\n\n"
        "const CapsuleDirHeader capsule_directory = {\n"
        "    .magic         = 0x%016" PRIx64 "ULL,  /* 'CAPD' */\n"
        "    .arena_base    = (uint64_t)(uintptr_t)capsule_arena,\n"
        "    .arena_size    = sizeof(capsule_arena),\n"
        "    .desc_count    = %d,\n"
        "    .desc_capacity = %d,\n"
        "    .name_count    = %d,\n"
        "    .reserved      = 0,\n"
        "    .dir_hash      = 0x%016" PRIx64 "ULL,\n"
        "};\n\n"
        "/*===========================================================================\n"
        " * PE/COFF GOT-indirection fix: accessor functions\n"
        " *\n"
        " * In -fPIC PE builds, cross-TU data references go through GOT and the PE\n"
        " * linker does NOT convert GOTPCREL->LEA as the ELF linker does.  Defining\n"
        " * these accessors in the same TU as the symbols lets the compiler emit\n"
        " * direct RIP-relative addressing; callers reach them via a direct CALL.\n"
        " *===========================================================================*/\n\n"
        "__attribute__((visibility(\"hidden\")))\n"
        "uint32_t capsule_get_desc_count(void) { return capsule_directory.desc_count; }\n\n"
        "__attribute__((visibility(\"hidden\")))\n"
        "const CapsuleDirHeader *capsule_get_directory(void) { return &capsule_directory; }\n\n"
        "__attribute__((visibility(\"hidden\")))\n"
        "const CapsuleDesc *capsule_get_descriptors(void) { return capsule_descriptors; }\n\n"
        "__attribute__((visibility(\"hidden\")))\n"
        "const CapsuleNameEntry *capsule_get_names(void) { return capsule_names; }\n\n"
        "__attribute__((visibility(\"hidden\")))\n"
        "const uint8_t *capsule_get_arena(void) { return capsule_arena; }\n",
        (uint64_t)0x44504143ULL,
        capsule_count, MAX_CAPSULES, capsule_count,
        dir_hash);
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(int argc, char **argv) {
    /* --manifest mode: generate block ownership markdown */
    if (argc >= 3 && strcmp(argv[1], "--manifest") == 0) {
        base_dir     = argv[2];
        base_dir_len = strlen(base_dir);
        while (base_dir_len > 0 && base_dir[base_dir_len - 1] == '/') base_dir_len--;

        if (nftw(base_dir, manifest_file, 20, FTW_PHYS) != 0) {
            fprintf(stderr, "mkcapsule: ERROR: directory scan failed\n");
            return 1;
        }

        FILE *mout = stdout;
        if (argc == 4) {
            mout = fopen(argv[3], "w");
            if (!mout) {
                fprintf(stderr, "mkcapsule: ERROR: cannot create '%s'\n", argv[3]);
                return 1;
            }
        }
        generate_manifest(mout);
        if (mout != stdout) {
            fclose(mout);
            fprintf(stderr, "mkcapsule: manifest written to %s  (%d capsule(s))\n",
                    argv[3], manifest_count);
        }
        return 0;
    }

    /* --lint mode: validate all .4th files without generating C output */
    if (argc == 3 && strcmp(argv[1], "--lint") == 0) {
        base_dir     = argv[2];
        base_dir_len = strlen(base_dir);
        while (base_dir_len > 0 && base_dir[base_dir_len - 1] == '/') base_dir_len--;

        fprintf(stderr, "mkcapsule lint: scanning %s\n", base_dir);
        nftw(base_dir, lint_file, 20, FTW_PHYS);
        fprintf(stderr,
            "mkcapsule lint: %d file(s), %d failing, %d violation(s)  [%s]\n",
            lint_files, lint_failing, lint_errors,
            lint_errors == 0 ? "ALL PASS" : "FAIL");
        return lint_errors > 0 ? 1 : 0;
    }

    if (argc != 3) {
        fprintf(stderr,
            "Usage:\n"
            "  %s <capsules_dir> <output.c>            build capsule C source\n"
            "  %s --lint <capsules_dir>                lint all .4th files\n"
            "  %s --manifest <capsules_dir>            print block manifest (stdout)\n"
            "  %s --manifest <capsules_dir> <out.md>   write block manifest to file\n"
            "\n"
            "Lint checks (per .4th file):\n"
            "  - each block opens with 'Block N'\n"
            "  - every line <= 64 chars\n"
            "  - no more than 16 content lines per block\n"
            "  - every line terminated with newline\n"
            "\n"
            "Manifest output:\n"
            "  - capsule summary (name, blocks claimed, xxHash64)\n"
            "  - block map sorted by LBN with conflict detection\n"
            "  - conflict register\n"
            "  xxHash64 column is the anchor for Ed25519 fingerprints (Phase 8 PKI).\n"
            "\n"
            "Names are colon-separated relative paths (e.g. core:init.4th).\n"
            "Files whose encoded name exceeds %d bytes are skipped with a warning.\n",
            argv[0], argv[0], argv[0], argv[0], CAPSULE_NAME_MAX - 1);
        return 1;
    }

    base_dir     = argv[1];
    base_dir_len = strlen(base_dir);

    /* Strip trailing slash from base_dir length so relpath strips cleanly */
    while (base_dir_len > 0 && base_dir[base_dir_len - 1] == '/') {
        base_dir_len--;
    }

    const char *output_path = argv[2];

    fprintf(stderr, "mkcapsule: scanning %s\n", base_dir);

    if (nftw(base_dir, process_file, 20, FTW_PHYS) != 0) {
        fprintf(stderr, "mkcapsule: ERROR: directory scan failed\n");
        return 1;
    }

    if (capsule_count == 0) {
        fprintf(stderr, "mkcapsule: WARNING: no files found in %s\n", base_dir);
    }

    fprintf(stderr, "mkcapsule: %d capsule(s) collected\n", capsule_count);

    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "mkcapsule: ERROR: cannot create '%s'\n", output_path);
        return 1;
    }

    generate_output(out);
    fclose(out);

    fprintf(stderr, "mkcapsule: wrote %s\n", output_path);

    for (int i = 0; i < capsule_count; i++) {
        free(capsules[i].data);
    }

    return 0;
}
