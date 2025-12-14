/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

/*
                                 ***   StarForth   ***
                 Block Subsystem v2 — LBN→PBN Mapping + External BAM
                 ----------------------------------------------------
   Goals:
     - Present a contiguous logical block space (LBN) 0..MAX to user code.
     - Keep system-reserved physical ranges hidden.
     - Preserve FORTH-79 semantics (UPDATE marks dirty, FLUSH persists).
     - Keep v2 on-disk format: devblock0=header, then BAM pages, then payload.
     - Maintain 4KiB device packing: 3×1KiB data + 1KiB metadata per devblock.

   Mapping:
     Reserve:
       - RAM physical 0..31 (hidden from user)
       - DISK physical 1024..1055 (hidden from user)
     Logical:
       - LBN 0..991  -> RAM PBN 32..1023 (volatile, NOT persisted)
       - LBN 992..   -> DISK PBN 1056..  (persistent)

   License: CC0-1.0 / Public Domain. No warranty.
*/

#include "../include/blkio.h"              /* ensure struct blkio_dev is visible */
#include "../include/block_subsystem.h"
#include "../include/log.h"
#include "../include/platform_time.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ===== compile-time config ===== */
#ifndef BLK_FORTH_SYS_RESERVED
# define BLK_FORTH_SYS_RESERVED   32  /* RAM: hide 0..31; user RAM LBN 0..991 */
#endif
#ifndef BLK_DISK_SYS_RESERVED
# define BLK_DISK_SYS_RESERVED    32  /* DISK: hide 1024..1055 */
#endif

#define META_REGION_OFFSET   (BLK_PACK_RATIO * BLK_FORTH_SIZE)       /* 3072 */
#define META_REGION_SIZE     (BLK_DEVICE_SECTOR - META_REGION_OFFSET)/* 1024 */
#define META_PER_BLOCK       (META_REGION_SIZE / BLK_PACK_RATIO)     /* 341 */

static inline size_t minzu(size_t a, size_t b) { return a < b ? a : b; }
static inline uint32_t udiv_floor(uint32_t a, uint32_t b) { return a / b; }

/* ===== CRC64-ISO (runtime table) ===== */
static uint64_t crc64_table[256];
static int crc64_inited = 0;

static void crc64_init(void) {
    if (crc64_inited) return;
    const uint64_t poly = 0x42F0E1EBA9EA3693ULL; /* ISO polynomial */
    for (int i = 0; i < 256; ++i) {
        uint64_t crc = (uint64_t) i;
        for (int j = 0; j < 8; ++j)
            crc = (crc & 1) ? ((crc >> 1) ^ poly) : (crc >> 1);
        crc64_table[i] = crc;
    }
    crc64_inited = 1;
}

static inline uint64_t compute_crc64(const uint8_t *data, size_t len) {
    if (!crc64_inited) crc64_init();
    uint64_t crc = 0xFFFFFFFFFFFFFFFFULL;
    for (size_t i = 0; i < len; i++) {
        uint8_t idx = (uint8_t)(crc ^ data[i]);
        crc = crc64_table[idx] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFFFFFFFFFULL;
}

/* ===== bitset helpers for external BAM ===== */
static inline int bam_test(const uint8_t *bm, uint64_t idx) { return (bm[idx >> 3] >> (idx & 7)) & 1; }
static inline void bam_set(uint8_t *bm, uint64_t idx) { bm[idx >> 3] |= (uint8_t)(1u << (idx & 7)); }
static inline void bam_clr(uint8_t *bm, uint64_t idx) { bm[idx >> 3] &= (uint8_t) ~(1u << (idx & 7)); }

/* ===== cache slot ===== */
typedef struct {
    uint32_t devblock; /* payload devblock index (4 KiB units) */
    uint8_t data[BLK_DEVICE_SECTOR]; /* 3×1K data + 1K meta */
    blk_meta_t meta[BLK_PACK_RATIO]; /* decoded 3×341B metadata */
    uint8_t valid;
    uint8_t loaded;
    uint8_t dirty;
    uint8_t meta_dirty;
} cache_slot_t;

#define DISK_CACHE_SLOTS 8

/* ===== global state ===== */
static struct {
    VM *vm;
    struct blkio_dev *disk_dev;

    uint8_t *ram_base;
    size_t ram_size;
    uint8_t dirty_ram[BLK_RAM_BLOCKS];

    blk_volume_meta_t vol_meta;
    uint8_t vol_meta_dirty;

    uint32_t total_blkio_blocks_1k; /* LBAs in 1K units */

    uint8_t *bam; /* external BAM resident copy */
    size_t bam_bytes;
    uint8_t bam_dirty;

    uint32_t devblock_base_4k; /* payload base in 4K devblocks */

    cache_slot_t cache[DISK_CACHE_SLOTS];

    /* logical view */
    uint32_t ram_user; /* = BLK_RAM_BLOCKS - BLK_FORTH_SYS_RESERVED (== 992) */
    uint64_t total_user_lbn; /* logical blocks visible to users (0..N-1) */

    int initialized;
} g = {0};

/* ===== devblock mapping (4KiB) ===== */
static inline uint32_t fblock_to_devblock(uint32_t phys_block /* PBN */) {
    return (phys_block - BLK_DISK_START) / BLK_PACK_RATIO;
}

static inline uint32_t fblock_pack_offset(uint32_t phys_block /* PBN */) {
    return (phys_block - BLK_DISK_START) % BLK_PACK_RATIO;
}

static inline uint32_t devblock4k_to_lba1k(uint32_t dev4k) {
    return (g.devblock_base_4k + dev4k) * 4u; /* 4× 1KiB pages per 4KiB devblock */
}

/* ===== 4KiB I/O on a 1KiB backend ===== */
static int read_devblock_4k(uint32_t dev4k, uint8_t *buf4k) {
    uint32_t base = devblock4k_to_lba1k(dev4k);
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t lba = base + i;
        if (lba >= g.total_blkio_blocks_1k) {
            memset(buf4k + i * 1024u, 0, 1024u);
            log_message(LOG_WARN, "blk: read OOB (dev4k=%u, lba=%u) -> zeroed", dev4k, lba);
            continue;
        }
        int rc = blkio_read(g.disk_dev, lba, buf4k + i * 1024u);
        if (rc != BLKIO_OK) {
            memset(buf4k + i * 1024u, 0, 1024u);
            log_message(LOG_WARN, "blk: read err (dev4k=%u, lba=%u) -> zeroed", dev4k, lba);
        }
    }
    return BLK_OK;
}

static int write_devblock_4k(uint32_t dev4k, const uint8_t *buf4k) {
    uint32_t base = devblock4k_to_lba1k(dev4k);
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t lba = base + i;
        if (lba >= g.total_blkio_blocks_1k) {
            log_message(LOG_ERROR, "blk: write OOB (dev4k=%u, lba=%u)", dev4k, lba);
            return BLK_EIO;
        }
        int rc = blkio_write(g.disk_dev, lba, buf4k + i * 1024u);
        if (rc != BLKIO_OK) return BLK_EIO;
    }
    return BLK_OK;
}

/* ===== cache management ===== */
static void meta_from_slice(blk_meta_t *dst, const uint8_t *slice) {
    memset(dst, 0, sizeof(*dst));
    size_t n = minzu(sizeof(*dst), (size_t) META_PER_BLOCK);
    memcpy(dst, slice, n);
}

static void meta_to_slice(const blk_meta_t *src, uint8_t *slice) {
    memset(slice, 0, META_PER_BLOCK);
    size_t n = minzu(sizeof(*src), (size_t) META_PER_BLOCK);
    memcpy(slice, src, n);
}

static int cache_writeback(cache_slot_t *s) {
    if (!s || !g.disk_dev) return BLK_OK;
    if (!(s->dirty || s->meta_dirty)) return BLK_OK;

    if (s->meta_dirty) {
        uint8_t *mr = s->data + META_REGION_OFFSET;
        for (uint32_t j = 0; j < BLK_PACK_RATIO; j++)
            meta_to_slice(&s->meta[j], mr + j * META_PER_BLOCK);
    }
    int rc = write_devblock_4k(s->devblock, s->data);
    if (rc != BLK_OK) return rc;

    s->dirty = s->meta_dirty = 0;
    return BLK_OK;
}

static cache_slot_t *cache_get_slot(uint32_t dev4k) {
    for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
        if (g.cache[i].valid && g.cache[i].devblock == dev4k) return &g.cache[i];
    }
    for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
        if (!g.cache[i].valid) {
            cache_slot_t *s = &g.cache[i];
            memset(s, 0, sizeof(*s));
            s->valid = 1;
            s->devblock = dev4k;
            return s;
        }
    }
    cache_slot_t *ev = &g.cache[0];
    (void) cache_writeback(ev);
    for (int i = 0; i < DISK_CACHE_SLOTS - 1; i++) g.cache[i] = g.cache[i + 1];
    memset(&g.cache[DISK_CACHE_SLOTS - 1], 0, sizeof(cache_slot_t));
    g.cache[DISK_CACHE_SLOTS - 1].valid = 1;
    g.cache[DISK_CACHE_SLOTS - 1].devblock = dev4k;
    return &g.cache[DISK_CACHE_SLOTS - 1];
}

static cache_slot_t *cache_load_devblock(uint32_t dev4k) {
    cache_slot_t *s = cache_get_slot(dev4k);
    if (!s) return NULL;
    if (!s->loaded) {
        (void) read_devblock_4k(dev4k, s->data);
        uint8_t *mr = s->data + META_REGION_OFFSET;
        for (uint32_t i = 0; i < BLK_PACK_RATIO; i++) {
            meta_from_slice(&s->meta[i], mr + i * META_PER_BLOCK);
            if (s->meta[i].magic != 0x424C4B5F5354524BULL) {
                memset(&s->meta[i], 0, sizeof(s->meta[i]));
                s->meta[i].magic = 0x424C4B5F5354524BULL;
            }
        }
        s->loaded = 1;
    }
    return s;
}

/* ===== header+format helpers ===== */
static inline void volmeta_from_buf(blk_volume_meta_t *out, const uint8_t *buf) {
    memset(out, 0, sizeof(*out));
    size_t n = minzu(sizeof(*out), (size_t) BLK_DEVICE_SECTOR);
    memcpy(out, buf, n);
}

static inline void volmeta_to_buf(const blk_volume_meta_t *in, uint8_t *buf) {
    memset(buf, 0, BLK_DEVICE_SECTOR);
    size_t n = minzu(sizeof(*in), (size_t) BLK_DEVICE_SECTOR);
    memcpy(buf, in, n);
}

static int read_header_4k(uint8_t *buf4k) {
    for (uint32_t i = 0; i < 4; i++) {
        int rc = blkio_read(g.disk_dev, i, buf4k + i * 1024u);
        if (rc != BLKIO_OK) memset(buf4k + i * 1024u, 0, 1024u);
    }
    return BLK_OK;
}

static int write_header_4k(const uint8_t *buf4k) {
    for (uint32_t i = 0; i < 4; i++) {
        int rc = blkio_write(g.disk_dev, i, buf4k + i * 1024u);
        if (rc != BLKIO_OK) return BLK_EIO;
    }
    return BLK_OK;
}

static uint32_t choose_B(uint64_t total_devblocks_4k) {
    if (total_devblocks_4k <= 2) return 1; /* header + 1 bam */
    uint64_t num = 3ULL * (total_devblocks_4k - 1ULL);
    uint64_t den = 32768ULL; /* bits per 4K BAM page */
    uint64_t B = (num + den - 1) / den;
    if (B == 0) B = 1;
    if (B > 0xFFFFFFFFu) B = 0xFFFFFFFFu;
    return (uint32_t) B;
}

static void compute_totals_from_B(blk_volume_meta_t *m) {
    uint64_t B = m->bam_devblocks;
    m->tracked_blocks = 32768ULL * B;
    uint64_t payload4k = (m->total_devblocks > (1 + B)) ? (m->total_devblocks - 1 - B) : 0;
    uint64_t storable = 3ULL * payload4k;
    m->total_blocks = (m->tracked_blocks < storable) ? m->tracked_blocks : storable;

    uint64_t reserved = (uint64_t) BLK_DISK_SYS_RESERVED;
    if (reserved > m->total_blocks) reserved = m->total_blocks;

    m->first_free = BLK_DISK_START + reserved;
    m->last_allocated = BLK_DISK_START + reserved - 1;
}

/* ===== BAM I/O ===== */
static int bam_load_from_disk(void) {
    if (!g.disk_dev || g.vol_meta.bam_devblocks == 0) return BLK_EINVAL;
    size_t need = 4096ULL * g.vol_meta.bam_devblocks;
    g.bam = (uint8_t *) calloc(1, need);
    if (!g.bam) return BLK_ENODEV;
    g.bam_bytes = need;

    for (uint32_t i = 0; i < g.vol_meta.bam_devblocks; i++) {
        uint32_t base1k = (g.vol_meta.bam_start + i) * 4u;
        for (uint32_t k = 0; k < 4; k++) {
            int rc = blkio_read(g.disk_dev, base1k + k, g.bam + i * 4096u + k * 1024u);
            if (rc != BLKIO_OK) memset(g.bam + i * 4096u + k * 1024u, 0, 1024u);
        }
    }
    g.bam_dirty = 0;
    return BLK_OK;
}

static int bam_flush_to_disk(void) {
    if (!g.disk_dev || !g.bam || !g.bam_dirty) return BLK_OK;
    for (uint32_t i = 0; i < g.vol_meta.bam_devblocks; i++) {
        uint32_t base1k = (g.vol_meta.bam_start + i) * 4u;
        for (uint32_t k = 0; k < 4; k++) {
            int rc = blkio_write(g.disk_dev, base1k + k, g.bam + i * 4096u + k * 1024u);
            if (rc != BLKIO_OK) return BLK_EIO;
        }
    }
    blkio_flush(g.disk_dev);
    g.bam_dirty = 0;
    return BLK_OK;
}

/* ===== LBN→PBN mapping ===== */
static inline int lbn_in_range(uint32_t lbn) {
    return (uint64_t) lbn < g.total_user_lbn;
}

static inline uint32_t lbn_to_pbn(uint32_t lbn) {
    /* Caller must ensure lbn_in_range(lbn) */
    if (lbn < g.ram_user) {
        return lbn + BLK_FORTH_SYS_RESERVED; /* RAM window after reserved (0..991 -> 32..1023) */
    } else {
        uint32_t disk_index = lbn - g.ram_user;
        return BLK_DISK_START + BLK_DISK_SYS_RESERVED + disk_index; /* 992 -> 1056 */
    }
}

/* ===== timestamp helper ===== */
/**
 * @brief Get current timestamp with RTC fallback to monotonic clock
 *
 * Uses real-time clock (wall-clock) if available, otherwise falls back
 * to monotonic clock. This ensures timestamps work on both POSIX and L4Re
 * platforms, even when RTC capability is unavailable.
 *
 * @return Nanoseconds since epoch (realtime) or boot (monotonic fallback)
 */
static inline uint64_t blk_get_timestamp(void) {
    if (sf_has_rtc()) {
        return sf_realtime_ns();
    } else {
        return sf_monotonic_ns();
    }
}

/* ===== init / attach / shutdown ===== */
int blk_subsys_init(VM *vm, uint8_t *ram_base, size_t ram_size) {
    if (!vm || !ram_base || ram_size < (BLK_RAM_BLOCKS * BLK_FORTH_SIZE)) return BLK_EINVAL;
    memset(&g, 0, sizeof(g));
    g.vm = vm;
    g.ram_base = ram_base;
    g.ram_size = ram_size;
    g.initialized = 1;

    /* v2 defaults */
    g.vol_meta.magic = 0x53544652; /* "STFR" */
    g.vol_meta.version = 2;
    g.vol_meta.total_volumes = 1;
    g.vol_meta.reserved_ram_lo = BLK_FORTH_SYS_RESERVED;
    g.vol_meta.reserved_disk_lo = BLK_DISK_SYS_RESERVED;
    strncpy(g.vol_meta.label, "StarForth Volume", sizeof(g.vol_meta.label) - 1);

    g.ram_user = (BLK_RAM_BLOCKS > BLK_FORTH_SYS_RESERVED) ? (BLK_RAM_BLOCKS - BLK_FORTH_SYS_RESERVED) : 0; /* 992 */
    g.total_user_lbn = g.ram_user; /* disk portion added after attach */
    return BLK_OK;
}

static int blk_format_or_load_v2(void) {
    uint8_t hdr[BLK_DEVICE_SECTOR] = {0};
    (void) read_header_4k(hdr);
    volmeta_from_buf(&g.vol_meta, hdr);

    /* if valid v2 header, load it */
    if (g.vol_meta.magic == 0x53544652 && g.vol_meta.version == 2) {
        g.vol_meta.total_devblocks = (uint64_t) udiv_floor(g.total_blkio_blocks_1k, 4);
        if (g.vol_meta.bam_devblocks == 0) goto fresh_format;
        g.devblock_base_4k = g.vol_meta.devblock_base;
        int rc = bam_load_from_disk();
        if (rc != BLK_OK) return rc;

        /* compute logical total */
        uint64_t disk_user = (g.vol_meta.total_blocks > BLK_DISK_SYS_RESERVED)
                                 ? (g.vol_meta.total_blocks - BLK_DISK_SYS_RESERVED)
                                 : 0;
        g.total_user_lbn = (uint64_t) g.ram_user + disk_user;
        return BLK_OK;
    }

fresh_format:
    /* create v2 header + BAM from scratch */
    memset(&g.vol_meta, 0, sizeof(g.vol_meta));
    g.vol_meta.magic = 0x53544652;
    g.vol_meta.version = 2;
    g.vol_meta.total_volumes = 1;
    g.vol_meta.reserved_ram_lo = BLK_FORTH_SYS_RESERVED;
    g.vol_meta.reserved_disk_lo = BLK_DISK_SYS_RESERVED;
    strncpy(g.vol_meta.label, "StarForth Volume", sizeof(g.vol_meta.label) - 1);

    /* Set volume creation timestamp */
    g.vol_meta.created_time = blk_get_timestamp();

    g.vol_meta.total_devblocks = (uint64_t) udiv_floor(g.total_blkio_blocks_1k, 4);
    g.vol_meta.bam_start = 1;
    g.vol_meta.bam_devblocks = choose_B(g.vol_meta.total_devblocks);
    g.vol_meta.devblock_base = g.vol_meta.bam_start + g.vol_meta.bam_devblocks;
    compute_totals_from_B(&g.vol_meta);

    size_t need = 4096ULL * g.vol_meta.bam_devblocks;
    g.bam = (uint8_t *) calloc(1, need);
    if (!g.bam) return BLK_ENODEV;
    g.bam_bytes = need;

    /* reserve the first D disk blocks from logical view by marking them allocated */
    uint64_t reserve = (g.vol_meta.total_blocks < BLK_DISK_SYS_RESERVED)
                           ? g.vol_meta.total_blocks
                           : BLK_DISK_SYS_RESERVED;
    for (uint64_t i = 0; i < reserve; i++) bam_set(g.bam, i);
    g.vol_meta.free_blocks = g.vol_meta.total_blocks - reserve;

    volmeta_to_buf(&g.vol_meta, hdr);
    (void) write_header_4k(hdr);

    /* zero BAM pages on disk */
    uint8_t z[1024] = {0};
    for (uint32_t i = 0; i < g.vol_meta.bam_devblocks; i++) {
        uint32_t base1k = (g.vol_meta.bam_start + i) * 4u;
        for (uint32_t k = 0; k < 4; k++) (void) blkio_write(g.disk_dev, base1k + k, z);
    }
    g.bam_dirty = 1;
    (void) bam_flush_to_disk();
    blkio_flush(g.disk_dev);

    g.devblock_base_4k = g.vol_meta.devblock_base;

    /* compute logical total */
    uint64_t disk_user = (g.vol_meta.total_blocks > BLK_DISK_SYS_RESERVED)
                             ? (g.vol_meta.total_blocks - BLK_DISK_SYS_RESERVED)
                             : 0;
    g.total_user_lbn = (uint64_t) g.ram_user + disk_user;

    return BLK_OK;
}

int blk_subsys_attach_device(struct blkio_dev *dev) {
    if (!g.initialized) return BLK_ENODEV;
    if (!dev) return BLK_EINVAL;
    g.disk_dev = dev;

    blkio_info_t info = {0};
    if (blkio_info(dev, &info) == BLKIO_OK) {
        g.total_blkio_blocks_1k = info.total_blocks;
        g.vol_meta.total_volumes = 1;
    } else {
        return BLK_EIO;
    }

    int rc = blk_format_or_load_v2();
    if (rc != BLK_OK) return rc;

    /* Set volume mounted timestamp */
    g.vol_meta.mounted_time = blk_get_timestamp();
    g.vol_meta_dirty = 1;

    log_message(LOG_INFO,
                "blk: hdr '%s' v2: total_devblocks(4k)=%llu, bam_devblocks=%u, devblock_base=%u, tracked=%llu, total_blocks=%llu, free=%llu",
                g.vol_meta.label,
                (unsigned long long) g.vol_meta.total_devblocks,
                g.vol_meta.bam_devblocks, g.vol_meta.devblock_base,
                (unsigned long long) g.vol_meta.tracked_blocks,
                (unsigned long long) g.vol_meta.total_blocks,
                (unsigned long long) g.vol_meta.free_blocks);

    return BLK_OK;
}

int blk_subsys_shutdown(void) {
    if (!g.initialized) return BLK_OK;

    blk_flush(0);

    if (g.vol_meta_dirty && g.disk_dev) {
        uint8_t hdr[4096];
        volmeta_to_buf(&g.vol_meta, hdr);
        (void) write_header_4k(hdr);
        blkio_flush(g.disk_dev);
        g.vol_meta_dirty = 0;
    }

    if (g.bam) {
        free(g.bam);
        g.bam = NULL;
        g.bam_bytes = 0;
        g.bam_dirty = 0;
    }
    if (g.disk_dev) {
        blkio_flush(g.disk_dev);
        g.disk_dev = NULL;
    }
    memset(g.cache, 0, sizeof(g.cache));
    memset(g.dirty_ram, 0, sizeof(g.dirty_ram));
    memset(&g.vol_meta, 0, sizeof(g.vol_meta));
    g.ram_user = 0;
    g.total_user_lbn = 0;
    g.initialized = 0;
    return BLK_OK;
}

/* ===== public API, now using LBNs ===== */
uint8_t *blk_get_buffer(uint32_t block_num /* LBN */, int writable) {
    if (!g.initialized) return NULL;
    if (!lbn_in_range(block_num)) return NULL;

    /* map LBN→PBN */
    uint32_t pbn;
    if (block_num < g.ram_user) {
        /* user RAM window after reserved */
        pbn = block_num + BLK_FORTH_SYS_RESERVED;
        if (writable) g.dirty_ram[pbn] = 1;
        return g.ram_base + (size_t) pbn * BLK_FORTH_SIZE;
    }

    /* disk */
    if (!g.disk_dev) return NULL;
    pbn = BLK_DISK_START + BLK_DISK_SYS_RESERVED + (block_num - g.ram_user);

    uint32_t dev4k = fblock_to_devblock(pbn);
    uint32_t pack = fblock_pack_offset(pbn);
    cache_slot_t *s = cache_load_devblock(dev4k);
    if (!s) return NULL;

    if (writable) s->dirty = 1;
    return s->data + pack * BLK_FORTH_SIZE;
}

uint8_t *blk_get_empty_buffer(uint32_t block_num /* LBN */) {
    uint8_t *p = blk_get_buffer(block_num, 1);
    if (p) memset(p, 0, BLK_FORTH_SIZE);
    return p;
}

int blk_update(uint32_t block_num /* LBN */) {
    if (!g.initialized) return BLK_ENODEV;
    if (!lbn_in_range(block_num)) return BLK_ERANGE;

    /* RAM */
    if (block_num < g.ram_user) {
        uint32_t pbn = block_num + BLK_FORTH_SYS_RESERVED;
        g.dirty_ram[pbn] = 1; /* volatile; no persistence */
        return BLK_OK;
    }

    /* DISK */
    if (!g.disk_dev) return BLK_ENODEV;
    uint32_t pbn = BLK_DISK_START + BLK_DISK_SYS_RESERVED + (block_num - g.ram_user);

    uint32_t dev4k = fblock_to_devblock(pbn);
    uint32_t pack = fblock_pack_offset(pbn);
    cache_slot_t *s = cache_load_devblock(dev4k);
    if (!s) return BLK_EIO;

    uint8_t *blk = s->data + pack * BLK_FORTH_SIZE;
    uint64_t crc = compute_crc64(blk, BLK_FORTH_SIZE);
    s->meta[pack].checksum = crc;

    /* Set timestamps: created_time on first write, modified_time always */
    uint64_t now = blk_get_timestamp();
    if (s->meta[pack].magic == 0) {
        s->meta[pack].magic = 0x424C4B5F5354524BULL;
        s->meta[pack].created_time = now;
    }
    s->meta[pack].modified_time = now;

    s->meta_dirty = 1;
    s->dirty = 1;

    /* mark allocated in BAM: BAM is indexed by PBN-1024 (disk-only) */
    {
        uint64_t off = (uint64_t) pbn - BLK_DISK_START;
        if (off < g.vol_meta.total_blocks) {
            if (!bam_test(g.bam, off)) {
                bam_set(g.bam, off);
                g.bam_dirty = 1;
                if (g.vol_meta.free_blocks) g.vol_meta.free_blocks--;
                g.vol_meta_dirty = 1;
            }
        }
    }
    return BLK_OK;
}

int blk_flush(uint32_t block_num /* LBN or 0 for all */) {
    if (!g.initialized) return BLK_ENODEV;

    if (block_num > 0) {
        if (!lbn_in_range(block_num)) return BLK_ERANGE;

        /* RAM */
        if (block_num < g.ram_user) {
            uint32_t pbn = block_num + BLK_FORTH_SYS_RESERVED;
            g.dirty_ram[pbn] = 0; /* no persistent flush for RAM */
            return BLK_OK;
        }

        /* DISK */
        if (!g.disk_dev) return BLK_ENODEV;
        uint32_t pbn = BLK_DISK_START + BLK_DISK_SYS_RESERVED + (block_num - g.ram_user);
        uint32_t dev4k = fblock_to_devblock(pbn);

        for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
            cache_slot_t *c = &g.cache[i];
            if (c->valid && c->devblock == dev4k && (c->dirty || c->meta_dirty)) {
                int rc = cache_writeback(c);
                if (rc != BLK_OK) return rc;
                blkio_flush(g.disk_dev);
                (void) bam_flush_to_disk();
                return BLK_OK;
            }
        }
        return BLK_OK;
    }

    /* flush all */
    if (g.disk_dev) {
        for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
            cache_slot_t *c = &g.cache[i];
            if (c->valid && (c->dirty || c->meta_dirty))
                (void) cache_writeback(c);
        }
        blkio_flush(g.disk_dev);
    }
    (void) bam_flush_to_disk();
    memset(g.dirty_ram, 0, sizeof(g.dirty_ram));
    return BLK_OK;
}

/* ===== BAM wrappers (disk LBNs only) ===== */
int blk_is_allocated(uint32_t block_num /* LBN */) {
    if (!g.initialized) return BLK_ENODEV;
    if (!lbn_in_range(block_num)) return 0;

    if (block_num < g.ram_user) return BLK_EINVAL; /* RAM has no persistent BAM */

    uint32_t pbn = BLK_DISK_START + BLK_DISK_SYS_RESERVED + (block_num - g.ram_user);
    uint64_t off = (uint64_t) pbn - BLK_DISK_START;
    if (off >= g.vol_meta.total_blocks) return BLK_ERANGE;
    return bam_test(g.bam, off) ? 1 : 0;
}

int blk_mark_allocated(uint32_t block_num /* LBN */) {
    if (!g.initialized) return BLK_ENODEV;
    if (!lbn_in_range(block_num)) return BLK_ERANGE;
    if (block_num < g.ram_user) return BLK_EINVAL;

    uint32_t pbn = BLK_DISK_START + BLK_DISK_SYS_RESERVED + (block_num - g.ram_user);
    uint64_t off = (uint64_t) pbn - BLK_DISK_START;
    if (off >= g.vol_meta.total_blocks) return BLK_ERANGE;
    if (!bam_test(g.bam, off)) {
        bam_set(g.bam, off);
        g.bam_dirty = 1;
        if (g.vol_meta.free_blocks) g.vol_meta.free_blocks--;
        g.vol_meta_dirty = 1;
    }
    return BLK_OK;
}

int blk_mark_free(uint32_t block_num /* LBN */) {
    if (!g.initialized) return BLK_ENODEV;
    if (!lbn_in_range(block_num)) return BLK_ERANGE;
    if (block_num < g.ram_user) return BLK_EINVAL;

    uint32_t pbn = BLK_DISK_START + BLK_DISK_SYS_RESERVED + (block_num - g.ram_user);
    uint64_t off = (uint64_t) pbn - BLK_DISK_START;
    if (off >= g.vol_meta.total_blocks) return BLK_ERANGE;

    if (bam_test(g.bam, off)) {
        bam_clr(g.bam, off);
        g.bam_dirty = 1;
        g.vol_meta.free_blocks++;
        if ((uint64_t) pbn < g.vol_meta.first_free) g.vol_meta.first_free = pbn;
        g.vol_meta_dirty = 1;
    }
    return BLK_OK;
}

int blk_allocate(uint32_t *block_num /* returns LBN */) {
    if (!block_num) return BLK_EINVAL;
    if (!g.initialized) return BLK_ENODEV;
    if (g.vol_meta.free_blocks == 0) return BLK_ERANGE;

    uint64_t limit = g.vol_meta.total_blocks;
    uint64_t start_off = (g.vol_meta.first_free > BLK_DISK_START)
                             ? (g.vol_meta.first_free - BLK_DISK_START)
                             : 0;

    for (uint64_t i = 0; i < limit; i++) {
        uint64_t off = (start_off + i) % limit;
        if (!bam_test(g.bam, off)) {
            bam_set(g.bam, off);
            g.bam_dirty = 1;
            if (g.vol_meta.free_blocks) g.vol_meta.free_blocks--;
            uint32_t pbn = BLK_DISK_START + (uint32_t) off;
            g.vol_meta.last_allocated = pbn;
            g.vol_meta.first_free = pbn + 1;
            g.vol_meta_dirty = 1;

            /* convert PBN to LBN for the caller */
            if (pbn < BLK_DISK_START + BLK_DISK_SYS_RESERVED) {
                *block_num = 0; /* shouldn't happen */
                return BLK_EIO;
            }
            *block_num = g.ram_user + (pbn - (BLK_DISK_START + BLK_DISK_SYS_RESERVED));
            return BLK_OK;
        }
    }
    return BLK_EIO;
}

/* ===== info/meta ===== */
int blk_get_volume_meta(blk_volume_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    *meta = g.vol_meta;
    return BLK_OK;
}

int blk_set_volume_meta(const blk_volume_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    g.vol_meta = *meta;
    g.vol_meta_dirty = 1;
    return BLK_OK;
}

int blk_is_valid(uint32_t block_num /* LBN */) {
    if (!g.initialized) return 0;
    return lbn_in_range(block_num) ? 1 : 0;
}

/* Total logical blocks visible to user (LBN count). */
uint32_t blk_get_total_blocks(void) {
    uint64_t n = g.total_user_lbn;
    return (n > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t) n;
}

/* Disk-only persistent block metadata (by LBN) */
int blk_get_meta(uint32_t block_num /* LBN */, blk_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    if (!g.initialized) return BLK_ENODEV;
    if (!lbn_in_range(block_num)) return BLK_ERANGE;
    if (block_num < g.ram_user) {
        memset(meta, 0, sizeof(*meta));
        meta->magic = 0x424C4B5F5354524BULL;
        return BLK_OK;
    }
    if (!g.disk_dev) return BLK_ENODEV;
    uint32_t pbn = BLK_DISK_START + BLK_DISK_SYS_RESERVED + (block_num - g.ram_user);
    uint32_t dev4k = fblock_to_devblock(pbn);
    uint32_t pack = fblock_pack_offset(pbn);
    cache_slot_t *s = cache_load_devblock(dev4k);
    if (!s) return BLK_EIO;
    *meta = s->meta[pack];
    return BLK_OK;
}

int blk_set_meta(uint32_t block_num /* LBN */, const blk_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    if (!g.initialized) return BLK_ENODEV;
    if (!lbn_in_range(block_num)) return BLK_ERANGE;
    if (block_num < g.ram_user) {
        /* RAM has no persistent per-block meta; ignore */
        return BLK_OK;
    }
    if (!g.disk_dev) return BLK_ENODEV;
    uint32_t pbn = BLK_DISK_START + BLK_DISK_SYS_RESERVED + (block_num - g.ram_user);
    uint32_t dev4k = fblock_to_devblock(pbn);
    uint32_t pack = fblock_pack_offset(pbn);
    cache_slot_t *s = cache_load_devblock(dev4k);
    if (!s) return BLK_EIO;
    s->meta[pack] = *meta;
    s->meta_dirty = 1;
    return BLK_OK;
}

/* ===== weak hook (for main.c) ===== */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((weak))
#endif
void blk_layer_attach_device(struct blkio_dev *dev) {
    (void) dev;
    blk_subsys_attach_device(dev);
}