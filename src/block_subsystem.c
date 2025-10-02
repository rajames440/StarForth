/*
                                 ***   StarForth   ***
                      Block Subsystem - Layer 2: Implementation
                      -------------------------------------------
   License: CC0-1.0 / Public Domain. No warranty.
*/

#include "../include/block_subsystem.h"
#include "../include/blkio.h"
#include "../include/log.h"
#include <string.h>

/* ========== Internal State ========== */

static struct {
    VM *vm; /* VM instance */
    struct blkio_dev *disk_dev; /* Disk device (blocks 1024+) */
    uint8_t *ram_base; /* RAM storage (blocks 0-1023) */
    size_t ram_size; /* RAM storage size */
    uint8_t dirty_ram[BLK_RAM_BLOCKS]; /* Dirty flags for RAM blocks */
    blk_volume_meta_t vol_meta; /* Block 0 metadata */
    int initialized;
    uint32_t total_disk_blocks; /* Cached from disk device */
} g_blk;

/* ========== 4KB Packing Helpers ========== */

/**
 * @brief Calculate device block number for a Forth block
 * @param fblock Forth block number (1024+)
 * @return 4KB device block number
 *
 * Packing: 3× 1KB Forth blocks per 4KB device block
 * Device block N contains Forth blocks [N*3, N*3+1, N*3+2]
 */
static inline uint32_t fblock_to_devblock(uint32_t fblock) {
    /* Adjust for disk start: fblock 1024 → device block 0 */
    uint32_t disk_offset = fblock - BLK_DISK_START;
    return disk_offset / BLK_PACK_RATIO;
}

/**
 * @brief Calculate offset within 4KB device block for a Forth block
 * @param fblock Forth block number
 * @return Offset (0, 1, or 2) within packed device block
 */
static inline uint32_t fblock_pack_offset(uint32_t fblock) {
    uint32_t disk_offset = fblock - BLK_DISK_START;
    return disk_offset % BLK_PACK_RATIO;
}

/* ========== Internal Cache (for disk blocks) ========== */

#define DISK_CACHE_SLOTS 8

typedef struct {
    uint32_t devblock; /* Device block number */
    uint8_t data[BLK_DEVICE_SECTOR]; /* 4KB buffer */
    uint8_t valid;
    uint8_t dirty;
} disk_cache_slot_t;

static disk_cache_slot_t g_cache[DISK_CACHE_SLOTS] = {0};

/**
 * @brief Find or allocate cache slot for device block
 * @param devblock Device block number
 * @return Pointer to cache slot, or NULL on error
 */
static disk_cache_slot_t *cache_get_slot(uint32_t devblock) {
    /* Check if already cached */
    for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
        if (g_cache[i].valid && g_cache[i].devblock == devblock) {
            return &g_cache[i];
        }
    }

    /* Find empty slot */
    for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
        if (!g_cache[i].valid) {
            g_cache[i].devblock = devblock;
            g_cache[i].valid = 1;
            g_cache[i].dirty = 0;
            return &g_cache[i];
        }
    }

    /* Evict LRU (simple: evict slot 0, shift others) */
    disk_cache_slot_t *evict = &g_cache[0];

    if (evict->dirty && g_blk.disk_dev) {
        /* Flush before eviction - unpack 4KB → 3× 1KB writes */
        for (uint32_t i = 0; i < BLK_PACK_RATIO; i++) {
            uint32_t dev_blk = evict->devblock * BLK_PACK_RATIO + i;
            uint8_t *src = evict->data + (i * BLK_FORTH_SIZE);

            if (blkio_write(g_blk.disk_dev, dev_blk, src) != BLKIO_OK) {
                log_message(LOG_ERROR, "block_subsystem: Failed to flush devblock %u on eviction",
                            evict->devblock);
            }
        }
    }

    /* Shift cache slots */
    for (int i = 0; i < DISK_CACHE_SLOTS - 1; i++) {
        g_cache[i] = g_cache[i + 1];
    }

    /* Use last slot */
    evict = &g_cache[DISK_CACHE_SLOTS - 1];
    evict->devblock = devblock;
    evict->valid = 1;
    evict->dirty = 0;
    memset(evict->data, 0, BLK_DEVICE_SECTOR);

    return evict;
}

/**
 * @brief Load device block from disk into cache
 * @param devblock Device block number
 * @return Pointer to cache slot, or NULL on error
 */
static disk_cache_slot_t *cache_load_devblock(uint32_t devblock) {
    if (!g_blk.disk_dev) {
        log_message(LOG_ERROR, "block_subsystem: No disk device attached");
        return NULL;
    }

    disk_cache_slot_t *slot = cache_get_slot(devblock);
    if (!slot) {
        log_message(LOG_ERROR, "block_subsystem: Failed to get cache slot for devblock %u", devblock);
        return NULL;
    }

    log_message(LOG_DEBUG, "block_subsystem: Loading devblock %u (total_disk_blocks=%u)",
                devblock, g_blk.total_disk_blocks);

    /* Load 3× 1KB Forth blocks into 4KB device buffer */
    for (uint32_t i = 0; i < BLK_PACK_RATIO; i++) {
        uint32_t fblock = BLK_DISK_START + (devblock * BLK_PACK_RATIO) + i;
        uint32_t dev_blk = devblock * BLK_PACK_RATIO + i; /* Device block number (0-based) */
        uint8_t *dst = slot->data + (i * BLK_FORTH_SIZE);

        log_message(LOG_DEBUG, "block_subsystem: Loading fblock=%u -> dev_blk=%u (i=%u)",
                    fblock, dev_blk, i);

        if (dev_blk >= g_blk.total_disk_blocks) {
            /* Beyond disk capacity, zero-fill */
            log_message(LOG_DEBUG, "block_subsystem: dev_blk %u >= total %u, zero-filling",
                        dev_blk, g_blk.total_disk_blocks);
            memset(dst, 0, BLK_FORTH_SIZE);
        } else {
            int rc = blkio_read(g_blk.disk_dev, dev_blk, dst);
            if (rc != BLKIO_OK) {
                log_message(LOG_WARN, "block_subsystem: Read error fblock=%u dev_blk=%u (rc=%d), zero-filling",
                            fblock, dev_blk, rc);
                memset(dst, 0, BLK_FORTH_SIZE);
            } else {
                log_message(LOG_DEBUG, "block_subsystem: Successfully read dev_blk=%u", dev_blk);
            }
        }
    }

    slot->dirty = 0;
    return slot;
}

/* ========== Public API ========== */

int blk_subsys_init(VM *vm, uint8_t *ram_base, size_t ram_size) {
    if (!vm || !ram_base || ram_size < (BLK_RAM_BLOCKS * BLK_FORTH_SIZE)) {
        return BLK_EINVAL;
    }

    memset(&g_blk, 0, sizeof(g_blk));
    g_blk.vm = vm;
    g_blk.ram_base = ram_base;
    g_blk.ram_size = ram_size;
    g_blk.initialized = 1;

    /* Initialize volume metadata (block 0) */
    g_blk.vol_meta.magic = 0x53544652; /* "STFR" */
    g_blk.vol_meta.version = 1;
    g_blk.vol_meta.total_volumes = 0;
    strncpy(g_blk.vol_meta.label, "StarForth Volume", sizeof(g_blk.vol_meta.label) - 1);

    log_message(LOG_INFO, "block_subsystem: Initialized (RAM: 0-1023, %zu bytes)", ram_size);
    return BLK_OK;
}

int blk_subsys_attach_device(struct blkio_dev *dev) {
    if (!g_blk.initialized) return BLK_ENODEV;
    if (!dev) return BLK_EINVAL;

    g_blk.disk_dev = dev;

    /* Query disk size */
    blkio_info_t info = {0};
    if (blkio_info(dev, &info) == BLKIO_OK) {
        g_blk.total_disk_blocks = info.total_blocks;
        g_blk.vol_meta.total_volumes = 1; /* At least one volume attached */

        log_message(LOG_INFO, "block_subsystem: Attached disk device (blocks: %u, size: %llu bytes)",
                    info.total_blocks, (unsigned long long) info.phys_size_bytes);
    } else {
        g_blk.total_disk_blocks = 0;
        log_message(LOG_WARN, "block_subsystem: Disk device info unavailable");
    }

    return BLK_OK;
}

int blk_subsys_shutdown(void) {
    if (!g_blk.initialized) return BLK_OK;

    /* Flush all dirty blocks */
    blk_flush(0);

    /* Close disk device (if opened) */
    if (g_blk.disk_dev) {
        blkio_flush(g_blk.disk_dev);
        /* Note: main.c owns blkio_close, we just detach */
        g_blk.disk_dev = NULL;
    }

    memset(&g_blk, 0, sizeof(g_blk));
    memset(g_cache, 0, sizeof(g_cache));

    log_message(LOG_INFO, "block_subsystem: Shutdown complete");
    return BLK_OK;
}

uint8_t *blk_get_buffer(uint32_t block_num, int writable) {
    if (!g_blk.initialized) return NULL;
    if (block_num == 0) return NULL; /* Reserved */

    /* RAM blocks (1-1023) */
    if (block_num < BLK_DISK_START) {
        if (block_num >= BLK_RAM_BLOCKS) return NULL;

        if (writable) {
            g_blk.dirty_ram[block_num] = 1;
        }

        return g_blk.ram_base + (block_num * BLK_FORTH_SIZE);
    }

    /* Disk blocks (1024+) */
    if (!g_blk.disk_dev) {
        log_message(LOG_WARN, "block_subsystem: No disk device for block %u", block_num);
        return NULL;
    }

    uint32_t devblock = fblock_to_devblock(block_num);
    uint32_t pack_idx = fblock_pack_offset(block_num);

    disk_cache_slot_t *slot = cache_load_devblock(devblock);
    if (!slot) return NULL;

    if (writable) {
        slot->dirty = 1;
    }

    return slot->data + (pack_idx * BLK_FORTH_SIZE);
}

uint8_t *blk_get_empty_buffer(uint32_t block_num) {
    uint8_t *buf = blk_get_buffer(block_num, 1);
    if (buf) {
        memset(buf, 0, BLK_FORTH_SIZE);
    }
    return buf;
}

int blk_flush(uint32_t block_num) {
    if (!g_blk.initialized) return BLK_ENODEV;

    /* Flush specific block */
    if (block_num > 0) {
        /* RAM block */
        if (block_num < BLK_DISK_START) {
            g_blk.dirty_ram[block_num] = 0;
            return BLK_OK; /* RAM blocks don't need flushing */
        }

        /* Disk block - flush containing device block */
        if (g_blk.disk_dev) {
            uint32_t devblock = fblock_to_devblock(block_num);

            for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
                if (g_cache[i].valid && g_cache[i].devblock == devblock && g_cache[i].dirty) {
                    /* Write all 3 packed blocks */
                    for (uint32_t j = 0; j < BLK_PACK_RATIO; j++) {
                        uint32_t dev_blk = devblock * BLK_PACK_RATIO + j;
                        uint8_t *src = g_cache[i].data + (j * BLK_FORTH_SIZE);

                        int rc = blkio_write(g_blk.disk_dev, dev_blk, src);
                        if (rc != BLKIO_OK) {
                            log_message(LOG_ERROR, "block_subsystem: Flush failed for dev_blk %u", dev_blk);
                            return BLK_EIO;
                        }
                    }

                    g_cache[i].dirty = 0;
                    blkio_flush(g_blk.disk_dev);
                    return BLK_OK;
                }
            }
        }

        return BLK_OK;
    }

    /* Flush all dirty blocks */

    /* Flush disk cache */
    if (g_blk.disk_dev) {
        for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
            if (g_cache[i].valid && g_cache[i].dirty) {
                /* Write all 3 packed blocks */
                for (uint32_t j = 0; j < BLK_PACK_RATIO; j++) {
                    uint32_t dev_blk = g_cache[i].devblock * BLK_PACK_RATIO + j;
                    uint8_t *src = g_cache[i].data + (j * BLK_FORTH_SIZE);

                    int rc = blkio_write(g_blk.disk_dev, dev_blk, src);
                    if (rc != BLKIO_OK) {
                        log_message(LOG_ERROR, "block_subsystem: Flush failed for dev_blk %u", dev_blk);
                    }
                }

                g_cache[i].dirty = 0;
            }
        }

        blkio_flush(g_blk.disk_dev);
    }

    /* Clear RAM dirty flags */
    memset(g_blk.dirty_ram, 0, sizeof(g_blk.dirty_ram));

    return BLK_OK;
}

int blk_update(uint32_t block_num) {
    if (!g_blk.initialized) return BLK_ENODEV;
    if (block_num == 0) return BLK_ERESERVED;

    /* RAM blocks */
    if (block_num < BLK_DISK_START) {
        if (block_num >= BLK_RAM_BLOCKS) return BLK_ERANGE;
        g_blk.dirty_ram[block_num] = 1;
        return BLK_OK;
    }

    /* Disk blocks - mark cache slot dirty */
    uint32_t devblock = fblock_to_devblock(block_num);

    for (int i = 0; i < DISK_CACHE_SLOTS; i++) {
        if (g_cache[i].valid && g_cache[i].devblock == devblock) {
            g_cache[i].dirty = 1;
            return BLK_OK;
        }
    }

    return BLK_OK;
}

int blk_get_volume_meta(blk_volume_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    *meta = g_blk.vol_meta;
    return BLK_OK;
}

int blk_set_volume_meta(const blk_volume_meta_t *meta) {
    if (!meta) return BLK_EINVAL;
    g_blk.vol_meta = *meta;
    return BLK_OK;
}

int blk_is_valid(uint32_t block_num) {
    if (block_num == 0) return 0; /* Reserved */
    if (block_num < BLK_DISK_START) return (block_num < BLK_RAM_BLOCKS);

    /* Disk blocks: valid if within disk capacity or cache can hold it */
    return 1; /* Allow expansion beyond current disk size (sparse) */
}

uint32_t blk_get_total_blocks(void) {
    uint32_t total = BLK_RAM_BLOCKS;
    if (g_blk.disk_dev && g_blk.total_disk_blocks > 0) {
        total += g_blk.total_disk_blocks;
    }
    return total;
}

/* ========== Weak symbol implementation for main.c hook ========== */

#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((weak))
#endif
void blk_layer_attach_device(struct blkio_dev *dev) {
    blk_subsys_attach_device(dev);
}