/*
 * virtio_blk.h — Virtio 1.0 block device driver for StarKernel
 *
 * Presents a blkio_dev_t interface for attachment to the StarForth
 * block subsystem via blk_subsys_attach_device().
 *
 * Only one virtio-blk device is supported (the Artemis disk).
 * I/O is synchronous (polling) — no interrupts, no DMA descriptors
 * beyond the split virtqueue shared memory.
 *
 * Artemis disk identification heuristic:
 *   The Artemis disk image is 30 MB (61440 sectors).  The first virtio-blk
 *   device matching PCI vendor=0x1AF4 / device=0x1042 or 0x1001 whose
 *   sector count equals 61440 is selected.  If no exact match, the first
 *   device found is used.
 */

#ifndef STARKERNEL_VIRTIO_BLK_H
#define STARKERNEL_VIRTIO_BLK_H

#include <stdint.h>
#include "blkio.h"

/* StarForth block size and sector ratio */
#define VBLK_SECTOR_SIZE      512u
#define VBLK_SECTORS_PER_BLOK 2u    /* 1 KiB Forth block = 2 × 512-byte sectors */
#define VBLK_ARTEMIS_SECTORS  61440u /* 30 MB */

/* virtio PCI vendor */
#define VIRTIO_PCI_VENDOR_ID  0x1AF4u
/* device IDs */
#define VIRTIO_BLK_DEVICE_MODERN  0x1042u
#define VIRTIO_BLK_DEVICE_LEGACY  0x1001u

/* virtio-blk request types */
#define VIRTIO_BLK_T_IN   0u
#define VIRTIO_BLK_T_OUT  1u

/* virtio-blk status codes (device → driver) */
#define VIRTIO_BLK_S_OK     0u
#define VIRTIO_BLK_S_IOERR  1u
#define VIRTIO_BLK_S_UNSUPP 2u

/*
 * virtio_blk_find_artemis — locate the Artemis virtio-blk disk, initialise
 *                           the driver, and fill in *dev_out so the caller
 *                           can pass it to blk_subsys_attach_device().
 *
 * dev_out must point to a zero-initialised blkio_dev_t.
 *
 * Returns  0 on success.
 * Returns -1 if no virtio-blk device was found on the PCI bus.
 * Returns -2 on driver initialisation failure (bad BAR, queue setup, etc.).
 */
int virtio_blk_find_artemis(blkio_dev_t *dev_out);

#endif /* STARKERNEL_VIRTIO_BLK_H */
