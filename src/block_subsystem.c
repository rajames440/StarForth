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

   License: See LICENSE file. No warranty.
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

/** @brief Return the smaller of two @c size_t values. */
static inline size_t minzu(size_t a, size_t b) { return a < b ? a : b; }
/** @brief Return the integer quotient @c a / @c b (floor division for non-negative operands). */
static inline uint32_t udiv_floor(uint32_t a, uint32_t b) { return a / b; }

/* ===== CRC64-ISO (runtime table) ===== */
static uint64_t crc64_table[256];
static int crc64_inited = 0;

/**
 * @brief Initialise the CRC64-ISO lookup table (once).
 *
 * Builds the 256-entry CRC64 table using the ISO polynomial
 * 0x42F0E1EBA9EA3693. The function is idempotent — subsequent calls
 * return immediately via the @c crc64_inited guard. The table is
 * generated at runtime rather than baked as a constant array so that
 * the binary can be placed in read-only flash without a relocation entry.
 *
 * The reflected (LSB-first) algorithm is used: each byte is processed
 * from the least-significant bit, and the polynomial is bit-reversed.
 */
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

/**
 * @brief Compute a CRC64-ISO checksum over a byte buffer.
 *
 * Ensures the lookup table is initialised via @c crc64_init(), then
 * processes each byte of @p data using the standard table-driven
 * reflected CRC algorithm. The initial and final XOR mask is
 * 0xFFFFFFFFFFFFFFFF so that an all-zero buffer produces a non-trivial
 * result and single-bit errors are always detected.
 *
 * @param data  Pointer to the byte buffer to checksum.
 * @param len   Number of bytes to process.
 * @return      64-bit CRC checksum (ISO 3309 variant).
 */
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

/**
 * @brief Test whether bit @p idx is set in a BAM bitset.
 *
 * BAM (Block Allocation Map) bits are packed 8 per byte, LSB-first.
 * Returns 1 if the bit is set (block allocated), 0 otherwise.
 *
 * @param bm   Pointer to the raw bitset byte array.
 * @param idx  Zero-based bit index (physical block offset from BLK_DISK_START).
 * @return     Non-zero if the bit is set, 0 if clear.
 */
static inline int bam_test(const uint8_t *bm, uint64_t idx) { return (bm[idx >> 3] >> (idx & 7)) & 1; }

/**
 * @brief Set bit @p idx in a BAM bitset (mark block allocated).
 *
 * @param bm   Pointer to the raw bitset byte array.
 * @param idx  Zero-based bit index to set.
 */
static inline void bam_set(uint8_t *bm, uint64_t idx) { bm[idx >> 3] |= (uint8_t)(1u << (idx & 7)); }

/**
 * @brief Clear bit @p idx in a BAM bitset (mark block free).
 *
 * @param bm   Pointer to the raw bitset byte array.
 * @param idx  Zero-based bit index to clear.
 */
static inline void bam_clr(uint8_t *bm, uint64_t idx) { bm[idx >> 3] &= (uint8_t) ~(1u << (idx & 7)); }

/* ===== cache slot ===== */

/**
 * @brief 4 KiB disk cache slot (one cached payload devblock).
 *
 * Each slot holds one 4 KiB devblock of packed FORTH data:
 *  - 3 × 1 KiB FORTH data pages at bytes 0–3071.
 *  - 1 × 1 KiB metadata region at bytes 3072–4095; each 341-byte
 *    sub-slice stores the @c blk_meta_t for one of the three data pages.
 *
 * @c meta[] is the decoded in-memory form of the on-disk metadata region.
 * @c dirty flags data page writes; @c meta_dirty flags metadata updates.
 * @c loaded is set on first I/O; @c valid indicates the slot is occupied.
 */
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

/**
 * @brief Convert a physical block number (PBN) to its 4 KiB devblock index.
 *
 * Three consecutive 1 KiB FORTH blocks are packed into each 4 KiB
 * devblock. The mapping subtracts @c BLK_DISK_START (the first disk PBN)
 * and divides by @c BLK_PACK_RATIO (3) to yield the devblock index
 * relative to the start of the payload area.
 *
 * @param phys_block  Physical block number; must be ≥ BLK_DISK_START.
 * @return            Zero-based index into the payload devblock array.
 */
static inline uint32_t fblock_to_devblock(uint32_t phys_block /* PBN */) {
    return (phys_block - BLK_DISK_START) / BLK_PACK_RATIO;
}

/**
 * @brief Return the pack slot (0–2) of a PBN within its 4 KiB devblock.
 *
 * Three 1 KiB FORTH blocks share one 4 KiB devblock. This function
 * returns the intra-devblock slot (0, 1, or 2) for @p phys_block,
 * which directly indexes the @c data[] byte region and the @c meta[]
 * array inside a @c cache_slot_t.
 *
 * @param phys_block  Physical block number; must be ≥ BLK_DISK_START.
 * @return            Pack slot in range [0, BLK_PACK_RATIO).
 */
static inline uint32_t fblock_pack_offset(uint32_t phys_block /* PBN */) {
    return (phys_block - BLK_DISK_START) % BLK_PACK_RATIO;
}

/**
 * @brief Convert a payload devblock index to its first 1 KiB LBA.
 *
 * The on-disk layout places payload devblocks after the header and BAM
 * (starting at @c g.devblock_base_4k). Each devblock spans four
 * consecutive 1 KiB LBAs on the underlying @c blkio_dev. This function
 * returns the LBA of the first 1 KiB page in the specified devblock.
 *
 * @param dev4k  Zero-based payload devblock index.
 * @return       1 KiB LBA of the first page in that devblock.
 */
static inline uint32_t devblock4k_to_lba1k(uint32_t dev4k) {
    return (g.devblock_base_4k + dev4k) * 4u; /* 4× 1KiB pages per 4KiB devblock */
}

/* ===== 4KiB I/O on a 1KiB backend ===== */

/**
 * @brief Read one 4 KiB devblock from the underlying 1 KiB backend.
 *
 * Issues four consecutive @c blkio_read() calls to fill @p buf4k with
 * the 4 × 1 KiB pages of @p dev4k. Out-of-bounds or failed reads zero
 * the affected 1 KiB slice and log a warning. The function always
 * returns @c BLK_OK so callers receive zeroed data rather than an error
 * on a partially readable device.
 *
 * @param dev4k  Zero-based payload devblock index to read.
 * @param buf4k  Output buffer; must be at least @c BLK_DEVICE_SECTOR (4096) bytes.
 * @return       @c BLK_OK always (per-LBA errors are logged and zeroed).
 */
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

/**
 * @brief Write one 4 KiB devblock to the underlying 1 KiB backend.
 *
 * Issues four consecutive @c blkio_write() calls to persist @p buf4k
 * as 4 × 1 KiB pages. Returns @c BLK_EIO immediately on the first
 * out-of-bounds or failed write; no partial-write recovery is attempted.
 *
 * @param dev4k  Zero-based payload devblock index to write.
 * @param buf4k  Input buffer; must be at least @c BLK_DEVICE_SECTOR (4096) bytes.
 * @return       @c BLK_OK on success, @c BLK_EIO if any LBA write fails.
 */
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

/**
 * @brief Deserialise per-block metadata from a raw on-disk metadata slice.
 *
 * Zeroes @p dst, then copies up to @c META_PER_BLOCK (341) bytes from
 * the packed metadata region into it. The copy length is the minimum of
 * @c sizeof(blk_meta_t) and @c META_PER_BLOCK so the function remains
 * safe as @c blk_meta_t grows over time.
 *
 * @param dst    Destination @c blk_meta_t to populate.
 * @param slice  Pointer to the 341-byte on-disk metadata sub-slice.
 */
static void meta_from_slice(blk_meta_t *dst, const uint8_t *slice) {
    memset(dst, 0, sizeof(*dst));
    size_t n = minzu(sizeof(*dst), (size_t) META_PER_BLOCK);
    memcpy(dst, slice, n);
}

/**
 * @brief Serialise per-block metadata to a raw on-disk metadata slice.
 *
 * Zeroes @p slice to @c META_PER_BLOCK (341) bytes, then copies up to
 * @c META_PER_BLOCK bytes from @p src. Unused trailing bytes are left
 * zero-padded so older kernels reading newer @c blk_meta_t layouts see
 * clean data in unknown fields.
 *
 * @param src    Source @c blk_meta_t to serialise.
 * @param slice  Destination 341-byte on-disk metadata sub-slice.
 */
static void meta_to_slice(const blk_meta_t *src, uint8_t *slice) {
    memset(slice, 0, META_PER_BLOCK);
    size_t n = minzu(sizeof(*src), (size_t) META_PER_BLOCK);
    memcpy(slice, src, n);
}

/**
 * @brief Writeback a dirty cache slot to the underlying block device.
 *
 * If @c meta_dirty is set, serialises all three per-block metadata
 * structs back into the metadata region of @c s->data before issuing
 * the write. After a successful write, both @c dirty and @c meta_dirty
 * are cleared.
 *
 * Returns @c BLK_OK immediately if the slot is already clean, if @p s
 * is NULL, or if no disk device is attached.
 *
 * @param s  Cache slot to write back.
 * @return   @c BLK_OK on success or if already clean; @c BLK_EIO on write failure.
 */
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

/**
 * @brief Find or allocate a cache slot for a 4 KiB devblock.
 *
 * Search order:
 *  1. Hit  — return the matching valid slot.
 *  2. Empty — initialise and return a free slot.
 *  3. Evict — writeback slot[0] (FIFO), shift all remaining slots down
 *             by one, and return the newly freed tail slot.
 *
 * Writeback errors during eviction are silently discarded to avoid
 * blocking the caller on a failing device.
 *
 * @param dev4k  Zero-based payload devblock index to find or allocate.
 * @return       Pointer to the (possibly newly allocated) cache slot.
 */
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

/**
 * @brief Load a 4 KiB devblock into the cache (or return the existing entry).
 *
 * Delegates to @c cache_get_slot() for slot allocation, then on first
 * access issues @c read_devblock_4k() and deserialises metadata for all
 * three packed FORTH blocks. If a metadata magic sentinel is absent the
 * in-memory metadata is zero-initialised and the magic written in so
 * subsequent reads see a consistent empty block rather than garbage.
 *
 * The @c loaded flag prevents re-reading an already-resident devblock.
 *
 * @param dev4k  Zero-based payload devblock index to load.
 * @return       Pointer to the populated cache slot, or NULL on slot failure.
 */
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

/**
 * @brief Deserialise the volume header from a 4 KiB raw buffer.
 *
 * Zeroes @p out, then copies up to @c BLK_DEVICE_SECTOR (4096) bytes
 * from @p buf. The smaller of the struct size and the sector size is
 * used so the function stays safe as @c blk_volume_meta_t grows over
 * time.
 *
 * @param out  Destination @c blk_volume_meta_t to populate.
 * @param buf  4096-byte raw on-disk buffer holding the volume header.
 */
static inline void volmeta_from_buf(blk_volume_meta_t *out, const uint8_t *buf) {
    memset(out, 0, sizeof(*out));
    size_t n = minzu(sizeof(*out), (size_t) BLK_DEVICE_SECTOR);
    memcpy(out, buf, n);
}

/**
 * @brief Serialise the volume header into a 4 KiB raw buffer.
 *
 * Zeroes @p buf to @c BLK_DEVICE_SECTOR (4096) bytes, then copies the
 * volume metadata struct. Remaining bytes are zero-padded so older
 * readers see clean data in fields introduced by newer versions.
 *
 * @param in   Source @c blk_volume_meta_t to serialise.
 * @param buf  4096-byte output buffer to populate.
 */
static inline void volmeta_to_buf(const blk_volume_meta_t *in, uint8_t *buf) {
    memset(buf, 0, BLK_DEVICE_SECTOR);
    size_t n = minzu(sizeof(*in), (size_t) BLK_DEVICE_SECTOR);
    memcpy(buf, in, n);
}

/**
 * @brief Read the 4 KiB volume header from disk (devblock 0, LBAs 0–3).
 *
 * Issues four @c blkio_read() calls for LBAs 0–3. Individual LBA
 * failures zero the affected 1 KiB slice; the function always returns
 * @c BLK_OK so the caller can inspect whatever header data was readable.
 *
 * @param buf4k  Output buffer; must be at least 4096 bytes.
 * @return       @c BLK_OK always.
 */
static int read_header_4k(uint8_t *buf4k) {
    for (uint32_t i = 0; i < 4; i++) {
        int rc = blkio_read(g.disk_dev, i, buf4k + i * 1024u);
        if (rc != BLKIO_OK) memset(buf4k + i * 1024u, 0, 1024u);
    }
    return BLK_OK;
}

/**
 * @brief Write the 4 KiB volume header to disk (devblock 0, LBAs 0–3).
 *
 * Issues four @c blkio_write() calls for LBAs 0–3. Returns @c BLK_EIO
 * immediately on the first failed write; no partial-write recovery is
 * attempted.
 *
 * @param buf4k  Input buffer; must be at least 4096 bytes.
 * @return       @c BLK_OK on success, @c BLK_EIO on any write failure.
 */
static int write_header_4k(const uint8_t *buf4k) {
    for (uint32_t i = 0; i < 4; i++) {
        int rc = blkio_write(g.disk_dev, i, buf4k + i * 1024u);
        if (rc != BLKIO_OK) return BLK_EIO;
    }
    return BLK_OK;
}

/**
 * @brief Compute the minimum number of BAM devblocks needed for a volume.
 *
 * Each 4 KiB BAM devblock holds 32768 bits, each tracking one physical
 * FORTH block. Three FORTH blocks are packed per payload devblock, so the
 * total trackable slots are 3 × (total_devblocks_4k − 1) (excluding the
 * header devblock). The result is rounded up and clamped to [1, UINT32_MAX].
 *
 * @param total_devblocks_4k  Total 4 KiB devblocks available on the device.
 * @return                    Number of BAM devblocks required to track all
 *                            payload blocks, minimum 1.
 */
static uint32_t choose_B(uint64_t total_devblocks_4k) {
    if (total_devblocks_4k <= 2) return 1; /* header + 1 bam */
    uint64_t num = 3ULL * (total_devblocks_4k - 1ULL);
    uint64_t den = 32768ULL; /* bits per 4K BAM page */
    uint64_t B = (num + den - 1) / den;
    if (B == 0) B = 1;
    if (B > 0xFFFFFFFFu) B = 0xFFFFFFFFu;
    return (uint32_t) B;
}

/**
 * @brief Derive all size-dependent volume fields from the BAM block count.
 *
 * Given @c m->bam_devblocks (B), fills:
 *  - @c m->tracked_blocks  — 32768 × B (BAM capacity in FORTH blocks)
 *  - @c m->total_blocks    — min(tracked_blocks, 3 × payload_devblocks)
 *  - @c m->first_free      — first block PBN after the system-reserved range
 *  - @c m->last_allocated  — last PBN in the system-reserved range
 *
 * System-reserved disk blocks (@c BLK_DISK_SYS_RESERVED) are accounted
 * against @c total_blocks but are never returned to user code.
 *
 * @param m  Volume metadata struct to update in-place.
 */
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

/**
 * @brief Load the resident BAM copy from disk into heap memory.
 *
 * Allocates 4096 × @c vol_meta.bam_devblocks bytes via @c calloc() and
 * reads each BAM devblock as four consecutive 1 KiB pages from the device.
 * Individual page read failures zero the affected 1 KiB slice rather than
 * aborting, so a partially readable device still yields a usable BAM.
 *
 * After this call @c g.bam points to the resident copy, @c g.bam_bytes
 * holds its size, and @c g.bam_dirty is cleared.
 *
 * @return @c BLK_OK on success; @c BLK_EINVAL if no disk or zero BAM count;
 *         @c BLK_ENODEV if @c calloc() fails.
 */
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

/**
 * @brief Write the resident BAM copy back to disk if dirty.
 *
 * Iterates all BAM devblocks, writing each as four consecutive 1 KiB
 * pages. On success issues @c blkio_flush() and clears @c g.bam_dirty.
 * Returns @c BLK_OK immediately if the BAM is clean, if no disk device
 * is attached, or if the resident BAM pointer is NULL.
 *
 * @return @c BLK_OK on success or if clean; @c BLK_EIO on any write failure.
 */
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

/**
 * @brief Check whether an LBN is within the user-accessible range.
 *
 * User LBNs span [0, g.total_user_lbn). LBNs at or beyond that limit
 * are either unformatted, beyond device capacity, or system-reserved.
 *
 * @param lbn  Logical block number to validate.
 * @return     Non-zero if in range, 0 if out of range.
 */
static inline int lbn_in_range(uint32_t lbn) {
    return (uint64_t) lbn < g.total_user_lbn;
}

/**
 * @brief Translate a logical block number (LBN) to a physical block number (PBN).
 *
 * Two-region mapping:
 *  - LBN [0, ram_user)   → PBN [BLK_FORTH_SYS_RESERVED, BLK_RAM_BLOCKS)
 *                          (volatile RAM-backed blocks, 992 entries)
 *  - LBN [ram_user, …)   → PBN [BLK_DISK_START + BLK_DISK_SYS_RESERVED, …)
 *                          (persistent disk-backed blocks)
 *
 * The caller is responsible for ensuring @c lbn_in_range(lbn) before calling.
 *
 * @param lbn  Logical block number; must satisfy @c lbn_in_range(lbn).
 * @return     Corresponding physical block number.
 */
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

/**
 * @brief Initialise the block subsystem in RAM-only mode.
 *
 * Zeroes global state, stores the VM pointer and RAM buffer, and
 * populates the v2 volume metadata defaults. No disk device is
 * attached at this stage — call @c blk_subsys_attach_device() to add
 * a persistent backend.
 *
 * @c ram_size must be at least @c BLK_RAM_BLOCKS × @c BLK_FORTH_SIZE
 * bytes (1024 × 1024 = 1 MiB).
 *
 * @param vm        VM instance that owns this block subsystem.
 * @param ram_base  Base pointer of the pre-allocated RAM block store.
 * @param ram_size  Size of the RAM block store in bytes.
 * @return          @c BLK_OK on success; @c BLK_EINVAL if any argument
 *                  is invalid or @p ram_size is too small.
 */
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

/**
 * @brief Format a new v2 volume or load an existing one from the disk device.
 *
 * Reads LBAs 0–3 (the 4 KiB header devblock) and inspects the magic and
 * version fields. Two paths:
 *
 *  - **Valid v2 header**: loads the BAM from disk, recomputes the logical
 *    block total from @c total_blocks, and returns @c BLK_OK.
 *  - **Missing or corrupt header** (@c fresh_format label): writes a new
 *    v2 header, allocates an in-memory BAM, zeroes BAM pages on disk,
 *    marks the system-reserved range allocated in the BAM, and flushes.
 *
 * Must be called from @c blk_subsys_attach_device() only, after
 * @c g.total_blkio_blocks_1k has been set by @c blkio_info().
 *
 * @return @c BLK_OK on success; @c BLK_ENODEV on allocation failure;
 *         @c BLK_EIO on I/O failure.
 */
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

/**
 * @brief Attach a block device to the subsystem and prepare the volume.
 *
 * Queries @c blkio_info() for device capacity, then calls
 * @c blk_format_or_load_v2() to mount or create the v2 volume.
 * On success, stamps the volume's @c mounted_time and logs the layout.
 *
 * @c blk_subsys_init() must be called before attaching a device.
 *
 * @param dev  Block I/O device handle; must not be NULL.
 * @return     @c BLK_OK on success; @c BLK_ENODEV if not initialised;
 *             @c BLK_EINVAL if @p dev is NULL; @c BLK_EIO on I/O or
 *             format failure.
 */
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

/**
 * @brief Flush all pending writes and shut down the block subsystem.
 *
 * Sequence:
 *  1. @c blk_flush(0) — write back all dirty cache slots and clear RAM
 *     dirty flags.
 *  2. Flush the volume header if modified (@c vol_meta_dirty).
 *  3. Free the resident BAM heap buffer.
 *  4. @c blkio_flush() the disk device.
 *  5. Zero all cache, dirty, and volume state; mark subsystem uninitialised.
 *
 * Safe to call on an uninitialised subsystem (returns @c BLK_OK immediately).
 *
 * @return @c BLK_OK always.
 */
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

/**
 * @brief Return a pointer to the raw 1 KiB buffer for LBN @p block_num.
 *
 * Implements the FORTH-79 @c BLOCK word buffer-acquisition semantics.
 * For RAM LBNs, the pointer addresses the RAM region directly. For disk
 * LBNs, the owning devblock is loaded into the cache on demand and a
 * pointer into the cached slot is returned.
 *
 * Setting @p writable non-zero marks the block dirty so @c blk_flush()
 * will persist the change.
 *
 * @param block_num  LBN to access.
 * @param writable   Non-zero if the caller intends to modify the buffer.
 * @return           Pointer to 1 KiB block data, or NULL on error.
 */
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

/**
 * @brief Return a pointer to a zeroed 1 KiB buffer for LBN @p block_num.
 *
 * Implements the FORTH-79 @c BUFFER word: acquires the writable block
 * buffer via @c blk_get_buffer() and clears it with @c memset(). The
 * block is marked dirty immediately so it will be persisted on the next
 * flush.
 *
 * @param block_num  LBN to acquire and zero.
 * @return           Pointer to the zeroed 1 KiB buffer, or NULL on error.
 */
uint8_t *blk_get_empty_buffer(uint32_t block_num /* LBN */) {
    uint8_t *p = blk_get_buffer(block_num, 1);
    if (p) memset(p, 0, BLK_FORTH_SIZE);
    return p;
}

/**
 * @brief Mark a block dirty and update its CRC64 checksum and timestamps.
 *
 * Implements the FORTH-79 @c UPDATE word semantics. For RAM blocks, sets
 * the in-memory dirty flag (RAM is volatile; no persistence). For disk
 * blocks:
 *  - Computes and stores the CRC64-ISO checksum of the 1 KiB data page.
 *  - Sets @c created_time on the first write (magic transition from 0).
 *  - Always updates @c modified_time.
 *  - Sets the BAM bit for the PBN and decrements @c free_blocks if the
 *    block was previously unallocated.
 *
 * @param block_num  LBN to mark dirty.
 * @return           @c BLK_OK on success; @c BLK_ENODEV / @c BLK_ERANGE
 *                   on validation failure; @c BLK_EIO on cache failure.
 */
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

/**
 * @brief Flush pending writes for one block or all blocks to persistent storage.
 *
 * Implements the FORTH-79 @c FLUSH word.
 *
 *  - @p block_num > 0: write back only the cache slot containing that
 *    LBN's devblock, then flush the disk and BAM.
 *  - @p block_num == 0: write back all dirty cache slots, flush the
 *    disk, flush the BAM, and clear all RAM dirty flags.
 *
 * RAM blocks have no persistent backing; flushing them only clears the
 * in-memory dirty flag.
 *
 * @param block_num  LBN to flush, or 0 to flush all pending writes.
 * @return           @c BLK_OK on success; @c BLK_ENODEV / @c BLK_ERANGE /
 *                   @c BLK_EIO on error.
 */
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

/**
 * @brief Query the BAM allocation status of a disk LBN.
 *
 * Returns 1 if the physical block corresponding to @p block_num is
 * marked allocated in the resident BAM, 0 if free. RAM LBNs have no
 * persistent BAM and return @c BLK_EINVAL.
 *
 * @param block_num  Disk LBN to query.
 * @return           1 if allocated; 0 if free or not tracked;
 *                   @c BLK_EINVAL for RAM LBNs;
 *                   @c BLK_ENODEV / @c BLK_ERANGE on other errors.
 */
int blk_is_allocated(uint32_t block_num /* LBN */) {
    if (!g.initialized) return BLK_ENODEV;
    if (!lbn_in_range(block_num)) return 0;

    if (block_num < g.ram_user) return BLK_EINVAL; /* RAM has no persistent BAM */

    uint32_t pbn = BLK_DISK_START + BLK_DISK_SYS_RESERVED + (block_num - g.ram_user);
    uint64_t off = (uint64_t) pbn - BLK_DISK_START;
    if (off >= g.vol_meta.total_blocks) return BLK_ERANGE;
    return bam_test(g.bam, off) ? 1 : 0;
}

/**
 * @brief Mark a disk LBN as allocated in the resident BAM.
 *
 * Sets the BAM bit for the physical block corresponding to @p block_num,
 * decrements @c free_blocks if the bit was previously clear, and marks
 * both the BAM and volume metadata dirty for deferred flush.
 *
 * @param block_num  Disk LBN to allocate.
 * @return           @c BLK_OK on success; error codes on validation failure.
 */
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

/**
 * @brief Mark a disk LBN as free in the resident BAM.
 *
 * Clears the BAM bit for the physical block corresponding to @p block_num,
 * increments @c free_blocks, and updates @c first_free if the freed PBN
 * is earlier than the current first-free hint. Marks BAM and volume
 * metadata dirty.
 *
 * @param block_num  Disk LBN to free.
 * @return           @c BLK_OK on success; error codes on validation failure.
 */
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

/**
 * @brief Allocate the next free disk LBN via a linear BAM scan.
 *
 * Starts from the @c first_free hint and scans the resident BAM with
 * wrap-around, allocating the first unset bit. Updates @c last_allocated,
 * @c first_free, and @c free_blocks in the volume metadata, and marks
 * the BAM dirty. Converts the PBN back to an LBN before returning it
 * to the caller.
 *
 * @param block_num  Output: LBN of the newly allocated block.
 * @return           @c BLK_OK on success; @c BLK_ERANGE if the volume is
 *                   full; @c BLK_EINVAL if @p block_num is NULL; @c BLK_EIO
 *                   on an unexpected internal mapping error.
 */
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

/**
 * @brief Copy the current volume metadata to the caller.
 *
 * @param meta  Output @c blk_volume_meta_t to populate.
 * @return      @c BLK_OK on success; @c BLK_EINVAL if @p meta is NULL.
 */
int blk_get_volume_meta(blk_volume_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    *meta = g.vol_meta;
    return BLK_OK;
}

/**
 * @brief Replace the in-memory volume metadata and mark it dirty.
 *
 * The new metadata is not persisted immediately — it is written on the
 * next @c blk_flush() or @c blk_subsys_shutdown() call.
 *
 * @param meta  Source @c blk_volume_meta_t to copy in.
 * @return      @c BLK_OK on success; @c BLK_EINVAL if @p meta is NULL.
 */
int blk_set_volume_meta(const blk_volume_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    g.vol_meta = *meta;
    g.vol_meta_dirty = 1;
    return BLK_OK;
}

/**
 * @brief Check whether an LBN is within the user-accessible range.
 *
 * @param block_num  LBN to validate.
 * @return           1 if valid; 0 if out of range or subsystem not initialised.
 */
int blk_is_valid(uint32_t block_num /* LBN */) {
    if (!g.initialized) return 0;
    return lbn_in_range(block_num) ? 1 : 0;
}

/**
 * @brief Return the total number of user-visible logical blocks.
 *
 * Includes both volatile RAM LBNs (0 … ram_user−1) and persistent disk
 * LBNs (ram_user … total_user_lbn−1). The result is clamped to
 * UINT32_MAX for callers that use a 32-bit block count.
 *
 * @return Total LBN count, clamped to UINT32_MAX.
 */
uint32_t blk_get_total_blocks(void) {
    uint64_t n = g.total_user_lbn;
    return (n > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t) n;
}

/**
 * @brief Read the per-block metadata for a disk LBN.
 *
 * For RAM LBNs, returns a synthetic @c blk_meta_t with only the magic
 * sentinel set and all other fields zero (RAM has no persistent per-block
 * metadata). For disk LBNs, loads the owning devblock into the cache and
 * copies the decoded metadata for the corresponding pack slot.
 *
 * @param block_num  LBN to query.
 * @param meta       Output @c blk_meta_t to populate.
 * @return           @c BLK_OK on success; error codes on validation or I/O failure.
 */
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

/**
 * @brief Write per-block metadata for a disk LBN.
 *
 * RAM LBNs silently succeed (no persistent per-block metadata). For disk
 * LBNs, loads the owning devblock into the cache and overwrites the
 * in-memory metadata for the pack slot. The slot is marked @c meta_dirty
 * so the change is serialised on the next writeback.
 *
 * @param block_num  LBN to update.
 * @param meta       Source @c blk_meta_t to copy in.
 * @return           @c BLK_OK on success; error codes on validation or I/O failure.
 */
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

/**
 * @brief Weak hook allowing @c main.c to attach a block device.
 *
 * Defined @c __attribute__((weak)) so it can be overridden by platform
 * glue without modifying this file. The default implementation forwards
 * directly to @c blk_subsys_attach_device().
 *
 * @param dev  Block I/O device handle to attach.
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((weak))
#endif
void blk_layer_attach_device(struct blkio_dev *dev) {
    (void) dev;
    blk_subsys_attach_device(dev);
}