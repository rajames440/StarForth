/*
                                 ***   StarForth   ***
                      Block Subsystem - Layer 2: Mapping & Business Logic
                      -------------------------------------------------------
   Architecture:
     - Forth blocks 0-1023: RAM (fast, in-memory buffer cache)
     - Forth blocks 1024+:  Disk (persistent storage via blkio)
     - Block 0: RESERVED for volume metadata (invalid for Forth use)

   Physical Layout (4KB alignment for devices):
     - 3× 1KB Forth blocks packed into each 4KB device block
     - 1KB metadata (341 bytes × 3 blocks) per 4KB device block

   Responsibilities:
     - Attach blkio device from main.c
     - Translate Forth block# → RAM or Disk backend
     - Manage dirty tracking & flush
     - Provide buffer access to block_words.c
     - Support hot-attach (USB, cloud mounts, etc.)

   License: CC0-1.0 / Public Domain. No warranty.
*/
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct blkio_dev;

/* Configuration constants */
#define BLK_FORTH_SIZE        1024u     /* Forth block size in bytes */
#define BLK_RAM_BLOCKS        1024u     /* Blocks 0-1023 in RAM */
#define BLK_DISK_START        1024u     /* Blocks 1024+ on disk */
#define BLK_DEVICE_SECTOR     4096u     /* Physical device block size */
#define BLK_PACK_RATIO        3u        /* 3× Forth blocks per device sector */
#define BLK_META_PER_BLOCK    341u      /* Metadata bytes per Forth block */
#define BLK_META_TOTAL        1024u     /* 341×3 = 1023, rounded to 1024 */

/* Volume metadata (block 0 reserved) */
typedef struct {
    uint32_t magic; /* 0x53544652 "STFR" */
    uint32_t version; /* Format version */
    uint32_t total_volumes; /* Number of attached volumes */
    uint32_t flags; /* Volume flags */
    char label[64]; /* Volume label */
    uint8_t reserved[944]; /* Pad to 1024 bytes */
} blk_volume_meta_t;

/* Block metadata (341 bytes per block in 4KB pack) */
typedef struct {
    uint32_t checksum; /* CRC32 of block data */
    uint32_t timestamp; /* Last write timestamp */
    uint32_t flags; /* Block flags */
    uint8_t reserved[329]; /* Pad to 341 bytes */
} blk_meta_t;

/* Error codes */
enum {
    BLK_OK = 0,
    BLK_EINVAL = -1, /* Invalid block number or parameter */
    BLK_ERANGE = -2, /* Block out of range */
    BLK_EIO = -3, /* I/O error */
    BLK_ENODEV = -4, /* No device attached */
    BLK_ERESERVED = -5, /* Block 0 reserved for metadata */
    BLK_EDIRTY = -6 /* Unflushed dirty blocks */
};

/**
 * @brief Initialize block subsystem (called once at startup)
 * @param vm Pointer to VM instance
 * @param ram_base Base pointer to RAM block storage (blocks 0-1023)
 * @param ram_size Size of RAM storage in bytes (should be 1024 * 1024)
 * @return BLK_OK on success, negative error code on failure
 */
int blk_subsys_init(VM *vm, uint8_t *ram_base, size_t ram_size);

/**
 * @brief Attach a block I/O device (called from main.c after blkio_factory_open)
 * @param dev Pointer to opened blkio device
 * @return BLK_OK on success, negative error code on failure
 */
int blk_subsys_attach_device(struct blkio_dev *dev);

/**
 * @brief Detach all block devices and cleanup
 * @return BLK_OK on success
 */
int blk_subsys_shutdown(void);

/**
 * @brief Get buffer pointer for a Forth block (read-only or read-write)
 * @param block_num Forth block number (1-1023 RAM, 1024+ disk)
 * @param writable If non-zero, mark block as dirty
 * @return Pointer to 1024-byte block buffer, or NULL on error
 *
 * Notes:
 *  - Block 0 returns NULL (reserved for metadata)
 *  - Blocks 1-1023: served from RAM
 *  - Blocks 1024+: loaded from disk via blkio, cached
 */
uint8_t *blk_get_buffer(uint32_t block_num, int writable);

/**
 * @brief Get empty (zeroed) buffer for a Forth block
 * @param block_num Forth block number
 * @return Pointer to zeroed 1024-byte buffer, or NULL on error
 */
uint8_t *blk_get_empty_buffer(uint32_t block_num);

/**
 * @brief Flush dirty blocks to disk
 * @param block_num If 0, flush all dirty blocks; else flush specific block
 * @return BLK_OK on success, negative error code on failure
 */
int blk_flush(uint32_t block_num);

/**
 * @brief Update block (mark as dirty, will flush on next blk_flush)
 * @param block_num Forth block number
 * @return BLK_OK on success
 */
int blk_update(uint32_t block_num);

/**
 * @brief Get volume metadata (block 0)
 * @param meta Output buffer for metadata
 * @return BLK_OK on success
 */
int blk_get_volume_meta(blk_volume_meta_t *meta);

/**
 * @brief Set volume metadata (block 0)
 * @param meta Metadata to write
 * @return BLK_OK on success
 */
int blk_set_volume_meta(const blk_volume_meta_t *meta);

/**
 * @brief Check if block is in valid range
 * @param block_num Block number to check
 * @return 1 if valid (>0), 0 if invalid
 */
int blk_is_valid(uint32_t block_num);

/**
 * @brief Get total number of available blocks
 * @return Total blocks (RAM + disk)
 */
uint32_t blk_get_total_blocks(void);

#ifdef __cplusplus
} /* extern "C" */
#endif