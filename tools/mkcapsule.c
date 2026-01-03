/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  mkcapsule - Build tool to generate capsule directory from .4th files

  Usage: mkcapsule <capsules_dir> <output.c>

  Recursively scans capsules_dir for *.4th files and generates a C source
  file containing the CapsuleDirHeader, CapsuleDesc array, and payload arena.
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
 * xxHash64 (embedded for build tool)
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

#define MAX_CAPSULES 256
#define MAX_PATH_LEN 1024

typedef struct {
    char path[MAX_PATH_LEN];     /* Source file path */
    char relpath[MAX_PATH_LEN];  /* Relative path (for comments) */
    uint8_t *data;               /* File contents */
    size_t length;               /* File size */
    uint64_t hash;               /* xxHash64 of contents */
    uint32_t flags;              /* Capsule flags */
} CapsuleEntry;

static CapsuleEntry capsules[MAX_CAPSULES];
static int capsule_count = 0;
static const char *base_dir = NULL;
static size_t base_dir_len = 0;

/* Flag constants (must match capsule.h) */
#define FLAG_ACTIVE       0x00000001
#define FLAG_PRODUCTION   0x00000010
#define FLAG_EXPERIMENT   0x00000020
#define FLAG_MAMA_INIT    0x00000040

/**
 * Determine flags based on path
 * - Files in "core/" are (m) Mama's init - MAMA_INIT flag
 * - Files in "production/" are (p) production
 * - Files in "experiments/" are (e) experiment
 * - Files in "domains/" are (p) production
 * - Everything else is (e) by default
 */
static uint32_t flags_from_path(const char *relpath) {
    uint32_t flags = FLAG_ACTIVE;

    if (strstr(relpath, "core/") != NULL) {
        /* Mama's init capsule - special flag, no (p) or (e) */
        flags |= FLAG_MAMA_INIT;
    } else if (strstr(relpath, "production/") != NULL) {
        flags |= FLAG_PRODUCTION;
    } else if (strstr(relpath, "domains/") != NULL) {
        /* Domain capsules are production */
        flags |= FLAG_PRODUCTION;
    } else if (strstr(relpath, "experiments/") != NULL) {
        flags |= FLAG_EXPERIMENT;
    } else {
        /* Default to experiment for safety */
        flags |= FLAG_EXPERIMENT;
    }

    return flags;
}

/**
 * nftw callback - process each file
 */
static int process_file(const char *fpath, const struct stat *sb,
                        int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)ftwbuf;

    if (typeflag != FTW_F) {
        return 0;  /* Skip non-files */
    }

    /* Check for .4th extension */
    size_t len = strlen(fpath);
    if (len < 4 || strcmp(fpath + len - 4, ".4th") != 0) {
        return 0;  /* Skip non-.4th files */
    }

    if (capsule_count >= MAX_CAPSULES) {
        fprintf(stderr, "Error: Too many capsules (max %d)\n", MAX_CAPSULES);
        return -1;
    }

    /* Read file */
    FILE *f = fopen(fpath, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open %s\n", fpath);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        fprintf(stderr, "Warning: Skipping empty file %s\n", fpath);
        return 0;
    }

    uint8_t *data = malloc((size_t)size);
    if (!data) {
        fclose(f);
        fprintf(stderr, "Error: Out of memory reading %s\n", fpath);
        return -1;
    }

    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        fprintf(stderr, "Error: Failed to read %s\n", fpath);
        return -1;
    }
    fclose(f);

    /* Store entry */
    CapsuleEntry *e = &capsules[capsule_count];
    strncpy(e->path, fpath, MAX_PATH_LEN - 1);
    e->path[MAX_PATH_LEN - 1] = '\0';

    /* Compute relative path */
    if (strlen(fpath) > base_dir_len && fpath[base_dir_len] == '/') {
        strncpy(e->relpath, fpath + base_dir_len + 1, MAX_PATH_LEN - 1);
    } else {
        strncpy(e->relpath, fpath, MAX_PATH_LEN - 1);
    }
    e->relpath[MAX_PATH_LEN - 1] = '\0';

    e->data = data;
    e->length = (size_t)size;
    e->hash = xxhash64(data, (size_t)size, 0);
    e->flags = flags_from_path(e->relpath);

    capsule_count++;

    char mode_char = 'e';
    if (e->flags & FLAG_MAMA_INIT) mode_char = 'm';
    else if (e->flags & FLAG_PRODUCTION) mode_char = 'p';

    fprintf(stderr, "  [%c] %s (%" PRIu64 " bytes, hash=0x%016llx)\n",
            mode_char,
            e->relpath,
            (uint64_t)e->length,
            (unsigned long long)e->hash);

    return 0;
}


/*===========================================================================
 * Code Generation
 *===========================================================================*/

static void generate_output(FILE *out) {
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm);

    fprintf(out, "/*\n");
    fprintf(out, " * capsule_directory.c - Generated Capsule Directory\n");
    fprintf(out, " *\n");
    fprintf(out, " * Generated by mkcapsule at %s\n", timestamp);
    fprintf(out, " * DO NOT EDIT - This file is auto-generated\n");
    fprintf(out, " *\n");
    fprintf(out, " * Capsule count: %d\n", capsule_count);
    fprintf(out, " */\n\n");

    fprintf(out, "#include \"starkernel/capsule.h\"\n\n");

    /* Generate arena (all payload bytes concatenated) */
    fprintf(out, "/*===========================================================================\n");
    fprintf(out, " * Payload Arena\n");
    fprintf(out, " *===========================================================================*/\n\n");

    fprintf(out, "const uint8_t capsule_arena[] = {\n");

    size_t total_size = 0;
    for (int i = 0; i < capsule_count; i++) {
        CapsuleEntry *e = &capsules[i];
        fprintf(out, "    /* [%d] %s (offset=%zu, length=%zu, hash=0x%016llx) */\n",
                i, e->relpath, total_size, e->length, (unsigned long long)e->hash);

        for (size_t j = 0; j < e->length; j++) {
            if (j % 16 == 0) {
                fprintf(out, "    ");
            }
            fprintf(out, "0x%02X,", e->data[j]);
            if (j % 16 == 15 || j == e->length - 1) {
                fprintf(out, "\n");
            } else {
                fprintf(out, " ");
            }
        }
        fprintf(out, "\n");
        total_size += e->length;
    }

    fprintf(out, "};\n\n");

    /* Generate descriptor array */
    fprintf(out, "/*===========================================================================\n");
    fprintf(out, " * Capsule Descriptors\n");
    fprintf(out, " *===========================================================================*/\n\n");

    fprintf(out, "const CapsuleDesc capsule_descriptors[%d] = {\n", capsule_count);

    size_t offset = 0;
    for (int i = 0; i < capsule_count; i++) {
        CapsuleEntry *e = &capsules[i];

        fprintf(out, "    /* [%d] %s */\n", i, e->relpath);
        fprintf(out, "    {\n");
        fprintf(out, "        .magic        = CAPSULE_MAGIC_PACK(CAPSULE_VERSION_0, CAPSULE_HASH_XXHASH64),\n");
        fprintf(out, "        .capsule_id   = 0x%016llxULL,\n", (unsigned long long)e->hash);
        fprintf(out, "        .content_hash = 0x%016llxULL,\n", (unsigned long long)e->hash);
        fprintf(out, "        .offset       = %zuULL,\n", offset);
        fprintf(out, "        .length       = %zuULL,\n", e->length);
        const char *flags_comment;
        if (e->flags & FLAG_MAMA_INIT) flags_comment = "MAMA_INIT | ACTIVE";
        else if (e->flags & FLAG_PRODUCTION) flags_comment = "PRODUCTION | ACTIVE";
        else flags_comment = "EXPERIMENT | ACTIVE";

        fprintf(out, "        .flags        = 0x%08X,  /* %s */\n",
                e->flags, flags_comment);
        fprintf(out, "        .owner_vm     = 0,\n");
        fprintf(out, "        .birth_count  = 0,\n");
        fprintf(out, "        .created_ns   = 0,\n");
        fprintf(out, "    },\n");

        offset += e->length;
    }

    fprintf(out, "};\n\n");

    /* Generate directory header */
    fprintf(out, "/*===========================================================================\n");
    fprintf(out, " * Directory Header\n");
    fprintf(out, " *===========================================================================*/\n\n");

    /* Compute directory hash (hash of all descriptor hashes) */
    uint64_t dir_hash = 0;
    for (int i = 0; i < capsule_count; i++) {
        dir_hash = xxhash64(&capsules[i].hash, sizeof(uint64_t), dir_hash);
    }

    fprintf(out, "const CapsuleDirHeader capsule_directory = {\n");
    fprintf(out, "    .magic         = 0x%016llxULL,  /* 'CAPD' */\n",
            (unsigned long long)(0x44504143ULL));
    fprintf(out, "    .arena_base    = (uint64_t)(uintptr_t)capsule_arena,\n");
    fprintf(out, "    .arena_size    = sizeof(capsule_arena),\n");
    fprintf(out, "    .desc_count    = %d,\n", capsule_count);
    fprintf(out, "    .desc_capacity = %d,\n", MAX_CAPSULES);
    fprintf(out, "    .dir_hash      = 0x%016llxULL,\n", (unsigned long long)dir_hash);
    fprintf(out, "};\n");
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <capsules_dir> <output.c>\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Recursively scans capsules_dir for *.4th files and generates\n");
        fprintf(stderr, "a C source file with CapsuleDirHeader, CapsuleDesc[], and arena.\n");
        return 1;
    }

    base_dir = argv[1];
    base_dir_len = strlen(base_dir);
    const char *output_path = argv[2];

    fprintf(stderr, "mkcapsule: Scanning %s\n", base_dir);

    /* Recursively scan directory */
    if (nftw(base_dir, process_file, 20, FTW_PHYS) != 0) {
        fprintf(stderr, "Error: Failed to scan directory\n");
        return 1;
    }

    if (capsule_count == 0) {
        fprintf(stderr, "Warning: No .4th files found\n");
    }

    fprintf(stderr, "mkcapsule: Found %d capsules\n", capsule_count);

    /* Generate output */
    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot create %s\n", output_path);
        return 1;
    }

    generate_output(out);
    fclose(out);

    fprintf(stderr, "mkcapsule: Generated %s\n", output_path);

    /* Cleanup */
    for (int i = 0; i < capsule_count; i++) {
        free(capsules[i].data);
    }

    return 0;
}
