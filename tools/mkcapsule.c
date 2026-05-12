/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  mkcapsule - Generate capsule directory from a capsule tree

  Usage: mkcapsule <capsules_dir> <output.c>

  Recursively walks capsules_dir and incorporates every file it finds.
  Generates a C source file containing:
    - capsule_arena[]        (all payload bytes concatenated)
    - capsule_descriptors[]  (CapsuleDesc, one per file)
    - capsule_names[]        (CapsuleNameEntry, parallel to descriptors)
    - capsule_directory      (CapsuleDirHeader)

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

#define MAX_CAPSULES     256
#define MAX_PATH_LEN     4096
#define CAPSULE_NAME_MAX 512

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

/* Flag constants — must match capsule.h */
#define FLAG_ACTIVE       0x00000001
#define FLAG_PRODUCTION   0x00000010
#define FLAG_EXPERIMENT   0x00000020
#define FLAG_MAMA_INIT    0x00000040

/*
 * Determine flags from the colon-separated capsule name.
 *
 * Directory conventions:
 *   core:*           -> (m) Mama's init
 *   production:*     -> (p) production
 *   domains:*        -> (p) production
 *   experiments:*    -> (e) experiment
 *   anything else    -> (e) experiment (safe default)
 */
static uint32_t flags_from_name(const char *name) {
    uint32_t flags = FLAG_ACTIVE;

    if (strncmp(name, "core:", 5) == 0) {
        flags |= FLAG_MAMA_INIT;
    } else if (strncmp(name, "production:", 11) == 0) {
        flags |= FLAG_PRODUCTION;
    } else if (strncmp(name, "domains:", 8) == 0) {
        flags |= FLAG_PRODUCTION;
    } else if (strncmp(name, "experiments:", 12) == 0) {
        flags |= FLAG_EXPERIMENT;
    } else {
        flags |= FLAG_EXPERIMENT;
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
 * nftw callback — called once per filesystem entry.
 * Incorporates every regular file found; skips names that are too long.
 */
static int process_file(const char *fpath, const struct stat *sb,
                        int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)ftwbuf;

    if (typeflag != FTW_F) {
        return 0;  /* directories and symlinks: skip */
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
            (e->flags & FLAG_MAMA_INIT)    ? "MAMA_INIT | ACTIVE" :
            (e->flags & FLAG_PRODUCTION)   ? "PRODUCTION | ACTIVE" :
                                             "EXPERIMENT | ACTIVE";
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
        "};\n",
        (uint64_t)0x44504143ULL,
        capsule_count, MAX_CAPSULES, capsule_count,
        dir_hash);
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr,
            "Usage: %s <capsules_dir> <output.c>\n"
            "\n"
            "Walks capsules_dir recursively, incorporates every file found,\n"
            "and writes a C source file with the compiled capsule directory.\n"
            "\n"
            "Names are colon-separated relative paths (e.g. core:init.4th).\n"
            "Files whose encoded name exceeds %d bytes are skipped with a warning.\n",
            argv[0], CAPSULE_NAME_MAX - 1);
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
