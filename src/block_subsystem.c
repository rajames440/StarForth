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

 */

/*
                                 ***   StarForth   ***
             Block Subsystem v2 — Unified LBN Device Chain
             -----------------------------------------------
   Architecture (unified block address space):
     - LBN 0..2047:  FAST RAM  (volatile, g.ram_base)
     - LBN 2048..x:  RAMDRIVE  (raw RAM buffer, volatile; first attached device)
     - LBN x..y:     DISK IMG  (virtio-blk, persistent; second attached device)
     - LBN y+:       USB / future devices (chained, including hot-attach/detach)

   Devices are registered via blk_subsys_add_raw_device() (volatile RAM buffer)
   or blk_subsys_attach_device() (formatted disk).  Each allocates a
   blk_dev_slot_t node on the heap and appends it to g.head linked list.

   BAM design:
     - Each device slot owns a heap-allocated blk_bam_entry_t[] array, one
       entry per user block.  No global shared bitset.
     - blk_dev_slot_t is the kernel/Artemis decoupling boundary:
       the kernel hands a blkio_dev* to blk_subsys_attach_device(); Artemis
       owns everything below that call.

   blkio NOTE:
     - blkio backends operate on 1 KiB units.
     - One 4 KiB "devblock" == 4 consecutive 1 KiB blkio blocks.

   License: See LICENSE file. No warranty.
*/

#include "../include/blkio.h"
#include "../include/block_subsystem.h"
#include "../include/log.h"
#include "../include/platform_time.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ===== compile-time config ===== */
#ifndef BLK_FORTH_SYS_RESERVED
# define BLK_FORTH_SYS_RESERVED  32u
#endif
#ifndef BLK_DISK_SYS_RESERVED
# define BLK_DISK_SYS_RESERVED   32u
#endif

#define META_REGION_OFFSET   (BLK_PACK_RATIO * BLK_FORTH_SIZE)
#define META_REGION_SIZE     (BLK_DEVICE_SECTOR - META_REGION_OFFSET)
#define META_PER_BLOCK       (META_REGION_SIZE / BLK_PACK_RATIO)
#define DISK_CACHE_SLOTS     8

static inline size_t   minzu(size_t a, size_t b)          { return a < b ? a : b; }
static inline uint32_t udiv_floor(uint32_t a, uint32_t b) { return a / b; }

/* ===== CRC64-ISO ===== */
static uint64_t crc64_table[256];
static int      crc64_inited = 0;

static void crc64_init(void) {
    if (crc64_inited) return;
    const uint64_t poly = 0x42F0E1EBA9EA3693ULL;
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

/* ===== physical BAM bitset helpers (temporary buffers only) ===== */
static inline int  pbam_test(const uint8_t *bm, uint32_t i) { return (bm[i>>3] >> (i&7)) & 1; }
static inline void pbam_set (uint8_t *bm, uint32_t i)       { bm[i>>3] |=  (uint8_t)(1u << (i&7)); }
static inline void pbam_clr (uint8_t *bm, uint32_t i)       { bm[i>>3] &= (uint8_t)~(1u << (i&7)); }

/* ===== cache slot ===== */
typedef struct {
    uint32_t   devblock;
    uint8_t    data[BLK_DEVICE_SECTOR];
    blk_meta_t meta[BLK_PACK_RATIO];
    uint8_t    valid;
    uint8_t    loaded;
    uint8_t    dirty;
    uint8_t    meta_dirty;
} cache_slot_t;

/* ===== device slot (linked list node — kernel/Artemis decoupling boundary) ===== */
typedef struct blk_dev_slot blk_dev_slot_t;
struct blk_dev_slot {
    uint32_t   start_lbn;    /* first LBN served by this slot */
    uint32_t   user_blocks;  /* count of LBNs served */

    /* raw (ramdrive) path — raw_base != NULL, dev == NULL */
    uint8_t   *raw_base;

    /* disk path — dev != NULL, raw_base == NULL */
    struct blkio_dev  *dev;
    uint32_t           total_blkio_blocks_1k;
    uint32_t           devblock_base_4k;
    blk_volume_meta_t  vol_meta;
    uint8_t            vol_meta_dirty;
    uint8_t            bam_dirty;     /* any bam entry dirty → needs BAM flush to disk */
    cache_slot_t       cache[DISK_CACHE_SLOTS];

    /* BAM: one entry per user block, heap-allocated at attach time */
    blk_bam_entry_t   *bam;

    blk_dev_slot_t    *next;
};

/* ===== global state ===== */
static struct {
    VM       *vm;
    uint8_t  *ram_base;
    size_t    ram_size;
    uint8_t   dirty_ram[BLK_RAM_BLOCKS];

    uint32_t       ram_user;       /* user-visible RAM LBNs = BLK_RAM_BLOCKS - BLK_FORTH_SYS_RESERVED */
    uint64_t       total_user_lbn; /* total user-visible LBNs across RAM + all device slots */
    blk_dev_slot_t *head;          /* linked list of device slots */

    int initialized;
} g = {0};

/* ===== slot routing ===== */
static blk_dev_slot_t *lbn_to_slot(uint32_t lbn) {
    blk_dev_slot_t *s = g.head;
    while (s) {
        if (lbn >= s->start_lbn && lbn < s->start_lbn + s->user_blocks)
            return s;
        s = s->next;
    }
    return NULL;
}

/* append a new slot to the tail of the device chain */
static void chain_append(blk_dev_slot_t *slot) {
    if (!g.head) { g.head = slot; return; }
    blk_dev_slot_t *t = g.head;
    while (t->next) t = t->next;
    t->next = slot;
}

/* ===== per-slot disk helpers ===== */

static inline uint32_t devblock4k_to_lba1k(blk_dev_slot_t *slot, uint32_t dev4k) {
    return (slot->devblock_base_4k + dev4k) * 4u;
}

static inline uint32_t lbn_to_slot_pbn(blk_dev_slot_t *slot, uint32_t lbn) {
    return BLK_DISK_SYS_RESERVED + (lbn - slot->start_lbn);
}

static inline uint32_t slot_pbn_to_devblock(uint32_t rel_pbn) { return rel_pbn / BLK_PACK_RATIO; }
static inline uint32_t slot_pbn_pack_offset(uint32_t rel_pbn) { return rel_pbn % BLK_PACK_RATIO; }

static int read_devblock_4k(blk_dev_slot_t *slot, uint32_t dev4k, uint8_t *buf4k) {
    uint32_t base = devblock4k_to_lba1k(slot, dev4k);
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t lba = base + i;
        if (lba >= slot->total_blkio_blocks_1k) { memset(buf4k + i*1024u, 0, 1024u); continue; }
        int rc = blkio_read(slot->dev, lba, buf4k + i*1024u);
        if (rc != BLKIO_OK) memset(buf4k + i*1024u, 0, 1024u);
    }
    return BLK_OK;
}

static int write_devblock_4k(blk_dev_slot_t *slot, uint32_t dev4k, const uint8_t *buf4k) {
    uint32_t base = devblock4k_to_lba1k(slot, dev4k);
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t lba = base + i;
        if (lba >= slot->total_blkio_blocks_1k) return BLK_EIO;
        if (blkio_write(slot->dev, lba, buf4k + i*1024u) != BLKIO_OK) return BLK_EIO;
    }
    return BLK_OK;
}

/* ===== cache management (per slot) ===== */

static void meta_from_slice(blk_meta_t *dst, const uint8_t *slice) {
    memset(dst, 0, sizeof(*dst));
    memcpy(dst, slice, minzu(sizeof(*dst), (size_t) META_PER_BLOCK));
}

static void meta_to_slice(const blk_meta_t *src, uint8_t *slice) {
    memset(slice, 0, META_PER_BLOCK);
    memcpy(slice, src, minzu(sizeof(*src), (size_t) META_PER_BLOCK));
}

static int cache_writeback(blk_dev_slot_t *slot, cache_slot_t *s) {
    if (!s || !slot->dev || !(s->dirty || s->meta_dirty)) return BLK_OK;
    if (s->meta_dirty) {
        uint8_t *mr = s->data + META_REGION_OFFSET;
        for (uint32_t j = 0; j < BLK_PACK_RATIO; j++)
            meta_to_slice(&s->meta[j], mr + j * META_PER_BLOCK);
    }
    int rc = write_devblock_4k(slot, s->devblock, s->data);
    if (rc != BLK_OK) return rc;
    s->dirty = s->meta_dirty = 0;
    return BLK_OK;
}

static cache_slot_t *cache_get_slot(blk_dev_slot_t *slot, uint32_t dev4k) {
    for (int i = 0; i < DISK_CACHE_SLOTS; i++)
        if (slot->cache[i].valid && slot->cache[i].devblock == dev4k)
            return &slot->cache[i];
    for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
        if (!slot->cache[i].valid) {
            cache_slot_t *s = &slot->cache[i];
            memset(s, 0, sizeof(*s));
            s->valid = 1; s->devblock = dev4k;
            return s;
        }
    }
    /* evict slot[0] FIFO */
    (void) cache_writeback(slot, &slot->cache[0]);
    for (int i = 0; i < DISK_CACHE_SLOTS - 1; i++) slot->cache[i] = slot->cache[i+1];
    memset(&slot->cache[DISK_CACHE_SLOTS-1], 0, sizeof(cache_slot_t));
    slot->cache[DISK_CACHE_SLOTS-1].valid   = 1;
    slot->cache[DISK_CACHE_SLOTS-1].devblock = dev4k;
    return &slot->cache[DISK_CACHE_SLOTS-1];
}

static cache_slot_t *cache_load_devblock(blk_dev_slot_t *slot, uint32_t dev4k) {
    cache_slot_t *s = cache_get_slot(slot, dev4k);
    if (!s) return NULL;
    if (!s->loaded) {
        (void) read_devblock_4k(slot, dev4k, s->data);
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

/* ===== header I/O ===== */

static inline void volmeta_from_buf(blk_volume_meta_t *out, const uint8_t *buf) {
    memset(out, 0, sizeof(*out));
    memcpy(out, buf, minzu(sizeof(*out), (size_t) BLK_DEVICE_SECTOR));
}

static inline void volmeta_to_buf(const blk_volume_meta_t *in, uint8_t *buf) {
    memset(buf, 0, BLK_DEVICE_SECTOR);
    memcpy(buf, in, minzu(sizeof(*in), (size_t) BLK_DEVICE_SECTOR));
}

static int read_header_4k(blk_dev_slot_t *slot, uint8_t *buf4k) {
    for (uint32_t i = 0; i < 4; i++) {
        if (blkio_read(slot->dev, i, buf4k + i*1024u) != BLKIO_OK)
            memset(buf4k + i*1024u, 0, 1024u);
    }
    return BLK_OK;
}

static int write_header_4k(blk_dev_slot_t *slot, const uint8_t *buf4k) {
    for (uint32_t i = 0; i < 4; i++)
        if (blkio_write(slot->dev, i, buf4k + i*1024u) != BLKIO_OK) return BLK_EIO;
    return BLK_OK;
}

/* ===== volume sizing ===== */

static uint32_t choose_B(uint64_t total_devblocks_4k) {
    if (total_devblocks_4k <= 2) return 1;
    uint64_t B = (3ULL*(total_devblocks_4k - 1ULL) + 32767ULL) / 32768ULL;
    if (B == 0) B = 1;
    if (B > 0xFFFFFFFFu) B = 0xFFFFFFFFu;
    return (uint32_t) B;
}

static void compute_totals_from_B(blk_volume_meta_t *m) {
    uint64_t B        = m->bam_devblocks;
    m->tracked_blocks = 32768ULL * B;
    uint64_t payload4k = (m->total_devblocks > (1+B)) ? (m->total_devblocks - 1 - B) : 0;
    uint64_t storable  = 3ULL * payload4k;
    m->total_blocks    = (m->tracked_blocks < storable) ? m->tracked_blocks : storable;
    uint64_t reserved  = (uint64_t) BLK_DISK_SYS_RESERVED;
    if (reserved > m->total_blocks) reserved = m->total_blocks;
    m->first_free     = BLK_DISK_SYS_RESERVED + reserved;
    m->last_allocated = BLK_DISK_SYS_RESERVED + reserved - 1;
}

/* ===== physical BAM I/O (sync to/from slot->bam[]) ===== */

/*
 * Read physical BAM from disk; populate slot->bam[i].allocated for user blocks.
 * Physical bit [BLK_DISK_SYS_RESERVED + i] → slot->bam[i].allocated.
 */
static int bam_sync_from_disk(blk_dev_slot_t *slot) {
    blk_volume_meta_t *m = &slot->vol_meta;
    if (!slot->dev || m->bam_devblocks == 0) return BLK_EINVAL;

    size_t psize = (size_t)4096u * m->bam_devblocks;
    uint8_t *pbam = (uint8_t *) calloc(1, psize);
    if (!pbam) return BLK_ENOMEM;

    for (uint32_t i = 0; i < m->bam_devblocks; i++) {
        uint32_t base1k = (m->bam_start + i) * 4u;
        for (uint32_t k = 0; k < 4; k++) {
            if (blkio_read(slot->dev, base1k + k, pbam + i*4096u + k*1024u) != BLKIO_OK)
                memset(pbam + i*4096u + k*1024u, 0, 1024u);
        }
    }

    for (uint32_t i = 0; i < slot->user_blocks; i++)
        slot->bam[i].allocated = pbam_test(pbam, BLK_DISK_SYS_RESERVED + i) ? 1 : 0;

    free(pbam);
    slot->bam_dirty = 0;
    return BLK_OK;
}

/*
 * Write slot->bam[i].allocated back to the physical BAM on disk (read-modify-write).
 * Preserves reserved bits.  Only runs when slot->bam_dirty is set.
 */
static int bam_flush_to_disk(blk_dev_slot_t *slot) {
    if (!slot->dev || !slot->bam_dirty) return BLK_OK;
    blk_volume_meta_t *m = &slot->vol_meta;
    if (m->bam_devblocks == 0) return BLK_OK;

    size_t psize = (size_t)4096u * m->bam_devblocks;
    uint8_t *pbam = (uint8_t *) calloc(1, psize);
    if (!pbam) return BLK_ENOMEM;

    /* read current on-disk BAM */
    for (uint32_t i = 0; i < m->bam_devblocks; i++) {
        uint32_t base1k = (m->bam_start + i) * 4u;
        for (uint32_t k = 0; k < 4; k++) {
            if (blkio_read(slot->dev, base1k + k, pbam + i*4096u + k*1024u) != BLKIO_OK)
                memset(pbam + i*4096u + k*1024u, 0, 1024u);
        }
    }

    /* reserved physical bits always set */
    for (uint32_t i = 0; i < BLK_DISK_SYS_RESERVED; i++) pbam_set(pbam, i);

    /* copy user allocation state */
    for (uint32_t i = 0; i < slot->user_blocks; i++) {
        if (slot->bam[i].allocated) pbam_set(pbam, BLK_DISK_SYS_RESERVED + i);
        else                        pbam_clr(pbam, BLK_DISK_SYS_RESERVED + i);
    }

    /* write back */
    for (uint32_t i = 0; i < m->bam_devblocks; i++) {
        uint32_t base1k = (m->bam_start + i) * 4u;
        for (uint32_t k = 0; k < 4; k++) {
            if (blkio_write(slot->dev, base1k + k, pbam + i*4096u + k*1024u) != BLKIO_OK) {
                free(pbam); return BLK_EIO;
            }
        }
    }
    blkio_flush(slot->dev);
    free(pbam);
    slot->bam_dirty = 0;
    return BLK_OK;
}

/* ===== volume format / load (per disk slot) ===== */

static int blk_format_or_load_disk(blk_dev_slot_t *slot) {
    uint8_t hdr[BLK_DEVICE_SECTOR] = {0};
    (void) read_header_4k(slot, hdr);
    volmeta_from_buf(&slot->vol_meta, hdr);

    if (slot->vol_meta.magic == 0x53544652u && slot->vol_meta.version == 2) {
        slot->vol_meta.total_devblocks = (uint64_t) udiv_floor(slot->total_blkio_blocks_1k, 4);
        if (slot->vol_meta.bam_devblocks == 0) goto fresh_format;

        uint64_t disk_user = (slot->vol_meta.total_blocks > BLK_DISK_SYS_RESERVED)
                             ? (slot->vol_meta.total_blocks - BLK_DISK_SYS_RESERVED) : 0;
        slot->user_blocks  = (disk_user > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t) disk_user;

        slot->bam = (blk_bam_entry_t *) calloc(slot->user_blocks, sizeof(blk_bam_entry_t));
        if (!slot->bam) return BLK_ENOMEM;

        slot->devblock_base_4k = slot->vol_meta.devblock_base;
        return bam_sync_from_disk(slot);
    }

    if (slot->vol_meta.magic != 0 || slot->vol_meta.version != 0) {
        log_message(LOG_WARN, "blk: unrecognised disk header (magic=0x%08x ver=%u) — fresh format",
                    slot->vol_meta.magic, slot->vol_meta.version);
    }

fresh_format:
    memset(&slot->vol_meta, 0, sizeof(slot->vol_meta));
    slot->vol_meta.magic            = 0x53544652u;
    slot->vol_meta.version          = 2;
    slot->vol_meta.total_volumes    = 1;
    slot->vol_meta.reserved_ram_lo  = BLK_FORTH_SYS_RESERVED;
    slot->vol_meta.reserved_disk_lo = BLK_DISK_SYS_RESERVED;
    strncpy(slot->vol_meta.label, "StarForth Volume", sizeof(slot->vol_meta.label) - 1);
    slot->vol_meta.total_devblocks  = (uint64_t) udiv_floor(slot->total_blkio_blocks_1k, 4);
    slot->vol_meta.bam_start        = 1;
    slot->vol_meta.bam_devblocks    = choose_B(slot->vol_meta.total_devblocks);
    slot->vol_meta.devblock_base    = slot->vol_meta.bam_start + slot->vol_meta.bam_devblocks;
    compute_totals_from_B(&slot->vol_meta);

    if (sf_has_rtc()) slot->vol_meta.created_time = sf_realtime_ns();
    else              slot->vol_meta.created_time = sf_monotonic_ns();

    uint64_t disk_user = (slot->vol_meta.total_blocks > BLK_DISK_SYS_RESERVED)
                         ? (slot->vol_meta.total_blocks - BLK_DISK_SYS_RESERVED) : 0;
    slot->user_blocks  = (disk_user > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t) disk_user;
    slot->devblock_base_4k = slot->vol_meta.devblock_base;

    slot->bam = (blk_bam_entry_t *) calloc(slot->user_blocks, sizeof(blk_bam_entry_t));
    if (!slot->bam) return BLK_ENOMEM;

    slot->vol_meta.free_blocks = slot->vol_meta.total_blocks > BLK_DISK_SYS_RESERVED
                                 ? slot->vol_meta.total_blocks - BLK_DISK_SYS_RESERVED : 0;

    /* zero BAM pages on disk, then write header */
    uint8_t z[1024] = {0};
    for (uint32_t i = 0; i < slot->vol_meta.bam_devblocks; i++) {
        uint32_t base1k = (slot->vol_meta.bam_start + i) * 4u;
        for (uint32_t k = 0; k < 4; k++) (void) blkio_write(slot->dev, base1k + k, z);
    }
    volmeta_to_buf(&slot->vol_meta, hdr);
    (void) write_header_4k(slot, hdr);
    /* flush with reserved bits marked */
    slot->bam_dirty = 1;
    (void) bam_flush_to_disk(slot);
    blkio_flush(slot->dev);
    return BLK_OK;
}

/* ===== timestamp helper ===== */
static inline uint64_t blk_get_timestamp(void) {
    if (sf_has_rtc()) return sf_realtime_ns();
    return sf_monotonic_ns();
}

/* ===== public API ===== */

int blk_subsys_init(VM *vm, uint8_t *ram_base, size_t ram_size) {
    if (!vm || !ram_base || ram_size < ((size_t) BLK_RAM_BLOCKS * BLK_FORTH_SIZE))
        return BLK_EINVAL;
    memset(&g, 0, sizeof(g));
    g.vm          = vm;
    g.ram_base    = ram_base;
    g.ram_size    = ram_size;
    g.ram_user    = (BLK_RAM_BLOCKS > BLK_FORTH_SYS_RESERVED)
                    ? (BLK_RAM_BLOCKS - BLK_FORTH_SYS_RESERVED) : 0u;
    g.total_user_lbn = g.ram_user;
    g.head        = NULL;
    g.initialized = 1;
    return BLK_OK;
}

int blk_subsys_add_raw_device(uint8_t *buf, uint32_t nblocks) {
    if (!g.initialized)       return BLK_ENODEV;
    if (!buf || nblocks == 0) return BLK_EINVAL;

    blk_dev_slot_t *slot = (blk_dev_slot_t *) calloc(1, sizeof(*slot));
    if (!slot) return BLK_ENOMEM;

    slot->bam = (blk_bam_entry_t *) calloc(nblocks, sizeof(blk_bam_entry_t));
    if (!slot->bam) { free(slot); return BLK_ENOMEM; }

    slot->start_lbn   = (uint32_t) g.total_user_lbn;
    slot->user_blocks = nblocks;
    slot->raw_base    = buf;

    chain_append(slot);
    g.total_user_lbn += nblocks;

    log_message(LOG_INFO, "blk: raw device LBN %u..%u (%u blocks)",
                slot->start_lbn, slot->start_lbn + nblocks - 1, nblocks);
    return BLK_OK;
}

int blk_subsys_attach_device(struct blkio_dev *dev) {
    if (!g.initialized) return BLK_ENODEV;
    if (!dev)           return BLK_EINVAL;

    blk_dev_slot_t *slot = (blk_dev_slot_t *) calloc(1, sizeof(*slot));
    if (!slot) return BLK_ENOMEM;

    slot->dev = dev;
    blkio_info_t info = {0};
    if (blkio_info(dev, &info) != BLKIO_OK) { free(slot); return BLK_EIO; }
    slot->total_blkio_blocks_1k = info.total_blocks;
    slot->start_lbn = (uint32_t) g.total_user_lbn;

    int rc = blk_format_or_load_disk(slot);
    if (rc != BLK_OK) { if (slot->bam) free(slot->bam); free(slot); return rc; }

    slot->vol_meta.mounted_time = blk_get_timestamp();
    slot->vol_meta_dirty        = 1;

    chain_append(slot);
    g.total_user_lbn += slot->user_blocks;

    log_message(LOG_INFO,
                "blk: disk '%s' v2 LBN %u..%u (%u user blocks); "
                "devblocks=%llu bam=%u base=%u total=%llu free=%llu",
                slot->vol_meta.label,
                slot->start_lbn, slot->start_lbn + slot->user_blocks - 1,
                slot->user_blocks,
                (unsigned long long) slot->vol_meta.total_devblocks,
                slot->vol_meta.bam_devblocks, slot->vol_meta.devblock_base,
                (unsigned long long) slot->vol_meta.total_blocks,
                (unsigned long long) slot->vol_meta.free_blocks);
    return BLK_OK;
}

int blk_subsys_shutdown(void) {
    if (!g.initialized) return BLK_OK;

    blk_flush(0);

    blk_dev_slot_t *s = g.head;
    while (s) {
        if (s->dev) {
            if (s->vol_meta_dirty) {
                uint8_t hdr[BLK_DEVICE_SECTOR];
                volmeta_to_buf(&s->vol_meta, hdr);
                (void) write_header_4k(s, hdr);
                blkio_flush(s->dev);
            }
            blkio_flush(s->dev);
        }
        blk_dev_slot_t *next = s->next;
        if (s->bam) free(s->bam);
        free(s);
        s = next;
    }

    memset(&g, 0, sizeof(g));
    return BLK_OK;
}

/* ===== block buffer access ===== */

uint8_t *blk_get_buffer(uint32_t block_num, int writable) {
    if (!g.initialized) return NULL;

    /* RAM */
    if (block_num < g.ram_user) {
        uint32_t pbn = block_num + BLK_FORTH_SYS_RESERVED;
        if (pbn >= BLK_RAM_BLOCKS) return NULL;
        if (writable) g.dirty_ram[pbn] = 1;
        return g.ram_base + (size_t) pbn * BLK_FORTH_SIZE;
    }

    blk_dev_slot_t *slot = lbn_to_slot(block_num);
    if (!slot) return NULL;

    uint32_t offset = block_num - slot->start_lbn;

    /* raw (ramdrive) */
    if (slot->raw_base) {
        if (writable) {
            slot->bam[offset].allocated = 1;
            slot->bam[offset].dirty     = 1;
        }
        return slot->raw_base + (size_t) offset * BLK_FORTH_SIZE;
    }

    /* disk */
    uint32_t rel_pbn = lbn_to_slot_pbn(slot, block_num);
    uint32_t dev4k   = slot_pbn_to_devblock(rel_pbn);
    uint32_t pack    = slot_pbn_pack_offset(rel_pbn);
    cache_slot_t *c  = cache_load_devblock(slot, dev4k);
    if (!c) return NULL;
    if (writable) c->dirty = 1;
    return c->data + pack * BLK_FORTH_SIZE;
}

uint8_t *blk_get_empty_buffer(uint32_t block_num) {
    uint8_t *p = blk_get_buffer(block_num, 1);
    if (p) memset(p, 0, BLK_FORTH_SIZE);
    return p;
}

int blk_update(uint32_t block_num) {
    if (!g.initialized) return BLK_ENODEV;

    /* RAM */
    if (block_num < g.ram_user) {
        uint32_t pbn = block_num + BLK_FORTH_SYS_RESERVED;
        if (pbn >= BLK_RAM_BLOCKS) return BLK_ERANGE;
        g.dirty_ram[pbn] = 1;
        return BLK_OK;
    }

    blk_dev_slot_t *slot = lbn_to_slot(block_num);
    if (!slot) return BLK_ERANGE;

    uint32_t offset = block_num - slot->start_lbn;
    slot->bam[offset].allocated = 1;
    slot->bam[offset].dirty     = 1;
    slot->bam_dirty             = 1;

    /* raw: done */
    if (slot->raw_base) return BLK_OK;

    /* disk: update CRC + timestamps in cache */
    uint32_t rel_pbn = lbn_to_slot_pbn(slot, block_num);
    uint32_t dev4k   = slot_pbn_to_devblock(rel_pbn);
    uint32_t pack    = slot_pbn_pack_offset(rel_pbn);
    cache_slot_t *c  = cache_load_devblock(slot, dev4k);
    if (!c) return BLK_EIO;

    uint8_t *blkdata = c->data + pack * BLK_FORTH_SIZE;
    c->meta[pack].checksum = compute_crc64(blkdata, BLK_FORTH_SIZE);
    uint64_t now = blk_get_timestamp();
    if (c->meta[pack].magic == 0) {
        c->meta[pack].magic        = 0x424C4B5F5354524BULL;
        c->meta[pack].created_time = now;
    }
    c->meta[pack].modified_time = now;
    c->meta_dirty = c->dirty = 1;

    if (slot->vol_meta.free_blocks > 0 &&
        !slot->bam[offset].allocated) { /* was free before this call */
        slot->vol_meta.free_blocks--;
        slot->vol_meta_dirty = 1;
    }
    return BLK_OK;
}

int blk_flush(uint32_t block_num) {
    if (!g.initialized) return BLK_ENODEV;

    if (block_num > 0) {
        if (block_num < g.ram_user) {
            uint32_t pbn = block_num + BLK_FORTH_SYS_RESERVED;
            if (pbn < BLK_RAM_BLOCKS) g.dirty_ram[pbn] = 0;
            return BLK_OK;
        }
        blk_dev_slot_t *slot = lbn_to_slot(block_num);
        if (!slot) return BLK_ERANGE;

        uint32_t offset = block_num - slot->start_lbn;
        slot->bam[offset].dirty = 0;

        if (slot->raw_base) return BLK_OK;

        uint32_t rel_pbn = lbn_to_slot_pbn(slot, block_num);
        uint32_t dev4k   = slot_pbn_to_devblock(rel_pbn);
        for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
            cache_slot_t *c = &slot->cache[i];
            if (c->valid && c->devblock == dev4k && (c->dirty || c->meta_dirty)) {
                int rc = cache_writeback(slot, c);
                if (rc != BLK_OK) return rc;
                blkio_flush(slot->dev);
                (void) bam_flush_to_disk(slot);
                return BLK_OK;
            }
        }
        return BLK_OK;
    }

    /* flush all */
    blk_dev_slot_t *s = g.head;
    while (s) {
        if (s->dev) {
            for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
                cache_slot_t *c = &s->cache[i];
                if (c->valid && (c->dirty || c->meta_dirty))
                    (void) cache_writeback(s, c);
            }
            blkio_flush(s->dev);
            (void) bam_flush_to_disk(s);
        }
        /* clear all dirty flags for this slot */
        for (uint32_t i = 0; i < s->user_blocks; i++) s->bam[i].dirty = 0;
        s = s->next;
    }
    memset(g.dirty_ram, 0, sizeof(g.dirty_ram));
    return BLK_OK;
}

/* ===== BAM wrappers ===== */

int blk_is_allocated(uint32_t block_num) {
    if (!g.initialized)     return BLK_ENODEV;
    if (block_num < g.ram_user) return BLK_EINVAL;
    blk_dev_slot_t *slot = lbn_to_slot(block_num);
    if (!slot) return 0;
    return slot->bam[block_num - slot->start_lbn].allocated ? 1 : 0;
}

int blk_mark_allocated(uint32_t block_num) {
    if (!g.initialized)     return BLK_ENODEV;
    if (block_num < g.ram_user) return BLK_EINVAL;
    blk_dev_slot_t *slot = lbn_to_slot(block_num);
    if (!slot) return BLK_ERANGE;
    uint32_t offset = block_num - slot->start_lbn;
    if (!slot->bam[offset].allocated) {
        slot->bam[offset].allocated = 1;
        slot->bam_dirty = 1;
        if (!slot->raw_base && slot->vol_meta.free_blocks) {
            slot->vol_meta.free_blocks--;
            slot->vol_meta_dirty = 1;
        }
    }
    return BLK_OK;
}

int blk_mark_free(uint32_t block_num) {
    if (!g.initialized)     return BLK_ENODEV;
    if (block_num < g.ram_user) return BLK_EINVAL;
    blk_dev_slot_t *slot = lbn_to_slot(block_num);
    if (!slot) return BLK_ERANGE;
    uint32_t offset = block_num - slot->start_lbn;
    if (slot->bam[offset].allocated) {
        slot->bam[offset].allocated = 0;
        slot->bam_dirty = 1;
        if (!slot->raw_base) {
            slot->vol_meta.free_blocks++;
            if ((uint64_t) block_num < slot->vol_meta.first_free)
                slot->vol_meta.first_free = block_num;
            slot->vol_meta_dirty = 1;
        }
    }
    return BLK_OK;
}

int blk_allocate(uint32_t *block_num) {
    if (!block_num)      return BLK_EINVAL;
    if (!g.initialized)  return BLK_ENODEV;

    blk_dev_slot_t *s = g.head;
    while (s) {
        if (s->raw_base || !s->dev || s->vol_meta.free_blocks == 0) { s = s->next; continue; }

        uint32_t limit = s->user_blocks;
        uint32_t hint  = (s->vol_meta.first_free > s->start_lbn)
                         ? (s->vol_meta.first_free - s->start_lbn) : 0;
        if (hint >= limit) hint = 0;

        for (uint32_t i = 0; i < limit; i++) {
            uint32_t k = (hint + i) % limit;
            if (!s->bam[k].allocated) {
                s->bam[k].allocated = 1;
                s->bam_dirty        = 1;
                if (s->vol_meta.free_blocks) s->vol_meta.free_blocks--;
                uint32_t lbn = s->start_lbn + k;
                s->vol_meta.last_allocated = lbn;
                s->vol_meta.first_free     = lbn + 1;
                s->vol_meta_dirty          = 1;
                *block_num = lbn;
                return BLK_OK;
            }
        }
        s = s->next;
    }
    return BLK_ERANGE;
}

/* ===== info / meta ===== */

static blk_dev_slot_t *first_disk_slot(void) {
    blk_dev_slot_t *s = g.head;
    while (s) { if (s->dev) return s; s = s->next; }
    return NULL;
}

int blk_get_volume_meta(blk_volume_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    blk_dev_slot_t *slot = first_disk_slot();
    if (!slot) return BLK_ENODEV;
    *meta = slot->vol_meta;
    return BLK_OK;
}

int blk_set_volume_meta(const blk_volume_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    blk_dev_slot_t *slot = first_disk_slot();
    if (!slot) return BLK_ENODEV;
    slot->vol_meta       = *meta;
    slot->vol_meta_dirty = 1;
    return BLK_OK;
}

int blk_is_valid(uint32_t block_num) {
    if (!g.initialized) return 0;
    if (block_num < g.ram_user) return 1;
    return lbn_to_slot(block_num) ? 1 : 0;
}

uint32_t blk_get_total_blocks(void) {
    uint64_t n = g.total_user_lbn;
    return (n > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t) n;
}

int blk_get_meta(uint32_t block_num, blk_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    if (!g.initialized) return BLK_ENODEV;
    if (block_num < g.ram_user) {
        memset(meta, 0, sizeof(*meta));
        meta->magic = 0x424C4B5F5354524BULL;
        return BLK_OK;
    }
    blk_dev_slot_t *slot = lbn_to_slot(block_num);
    if (!slot) return BLK_ERANGE;
    if (slot->raw_base) {
        memset(meta, 0, sizeof(*meta));
        meta->magic = 0x424C4B5F5354524BULL;
        return BLK_OK;
    }
    uint32_t rel_pbn = lbn_to_slot_pbn(slot, block_num);
    cache_slot_t *c  = cache_load_devblock(slot, slot_pbn_to_devblock(rel_pbn));
    if (!c) return BLK_EIO;
    *meta = c->meta[slot_pbn_pack_offset(rel_pbn)];
    return BLK_OK;
}

int blk_set_meta(uint32_t block_num, const blk_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    if (!g.initialized) return BLK_ENODEV;
    if (block_num < g.ram_user) return BLK_OK;
    blk_dev_slot_t *slot = lbn_to_slot(block_num);
    if (!slot) return BLK_ERANGE;
    if (slot->raw_base) return BLK_OK;
    uint32_t rel_pbn = lbn_to_slot_pbn(slot, block_num);
    cache_slot_t *c  = cache_load_devblock(slot, slot_pbn_to_devblock(rel_pbn));
    if (!c) return BLK_EIO;
    c->meta[slot_pbn_pack_offset(rel_pbn)] = *meta;
    c->meta_dirty = 1;
    return BLK_OK;
}

/* ===== weak hook (for main.c) ===== */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
void blk_layer_attach_device(struct blkio_dev *dev) {
    (void) dev;
    blk_subsys_attach_device(dev);
}
