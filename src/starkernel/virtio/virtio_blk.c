/*
 * virtio_blk.c — Virtio 1.0 block device driver for StarKernel
 *
 * Modern virtio 1.0 interface only (device ID 0x1042).
 * Falls back to checking 0x1001 (transitional).
 *
 * Split virtqueue, queue depth = 4 (minimum for one in-flight I/O chain).
 * All I/O is synchronous: submit → poll used ring → return status.
 *
 * Memory model: all allocations via kmalloc(); identity-mapped so
 * virtual address == physical address for virtqueue ring pointers.
 */

#ifndef __STARKERNEL__
#error "virtio_blk.c is kernel-only"
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "starkernel/pci.h"
#include "starkernel/virtio_blk.h"
#include "starkernel/kmalloc.h"
#include "console.h"
#include "blkio.h"

/* -------------------------------------------------------------------------
 * Virtio 1.0 PCI capability structures
 * ------------------------------------------------------------------------- */

#define VIRTIO_PCI_CAP_VENDOR_ID  0x09u

#define VIRTIO_PCI_CAP_COMMON_CFG 1u
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2u
#define VIRTIO_PCI_CAP_ISR_CFG    3u
#define VIRTIO_PCI_CAP_DEVICE_CFG 4u

/* Offsets within a virtio PCI capability (relative to cap ptr in config) */
#define VCAP_OFF_CAP_VNDR   0u
#define VCAP_OFF_CAP_NEXT   1u
#define VCAP_OFF_CAP_LEN    2u
#define VCAP_OFF_CFG_TYPE   3u
#define VCAP_OFF_BAR        4u
#define VCAP_OFF_OFFSET     8u
#define VCAP_OFF_LENGTH    12u
/* For notify cap only: 4-byte notify_off_multiplier after the 16-byte base */
#define VCAP_OFF_NOTIFY_MULT 16u

/* -------------------------------------------------------------------------
 * Virtio common config (MMIO layout, virtio 1.0 §4.1.4.3)
 * ------------------------------------------------------------------------- */

typedef struct {
    /* device feature selection */
    volatile uint32_t device_feature_select;
    volatile uint32_t device_feature;
    /* driver feature selection */
    volatile uint32_t driver_feature_select;
    volatile uint32_t driver_feature;
    /* MSIX — we write 0xFFFF to disable */
    volatile uint16_t config_msix_vector;
    volatile uint16_t num_queues;
    volatile uint8_t  device_status;
    volatile uint8_t  config_generation;
    /* per-queue */
    volatile uint16_t queue_select;
    volatile uint16_t queue_size;
    volatile uint16_t queue_msix_vector;
    volatile uint16_t queue_enable;
    volatile uint16_t queue_notify_off;
    volatile uint64_t queue_desc;
    volatile uint64_t queue_driver;
    volatile uint64_t queue_device;
    volatile uint16_t queue_notify_data;
    volatile uint16_t queue_reset;
} __attribute__((packed)) VirtioCommonCfg;

/* Device status bits */
#define VIRTIO_STATUS_ACKNOWLEDGE  0x01u
#define VIRTIO_STATUS_DRIVER       0x02u
#define VIRTIO_STATUS_DRIVER_OK    0x04u
#define VIRTIO_STATUS_FEATURES_OK  0x08u
#define VIRTIO_STATUS_FAILED       0x80u

/* Feature bits */
#define VIRTIO_F_VERSION_1         (1ULL << 32)

/* -------------------------------------------------------------------------
 * Split virtqueue structures (virtio 1.0 §2.6)
 * ------------------------------------------------------------------------- */

#define VQUEUE_SIZE  4u   /* power of 2; ≥ 3 for one in-flight chain */

#define VRING_DESC_F_NEXT   1u
#define VRING_DESC_F_WRITE  2u

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) VirtqDesc;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VQUEUE_SIZE];
    uint16_t used_event;
} __attribute__((packed)) VirtqAvail;

typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) VirtqUsedElem;

typedef struct {
    uint16_t    flags;
    uint16_t    idx;
    VirtqUsedElem ring[VQUEUE_SIZE];
    uint16_t    avail_event;
} __attribute__((packed)) VirtqUsed;

/* -------------------------------------------------------------------------
 * Virtio-blk request header
 * ------------------------------------------------------------------------- */

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed)) VirtBlkReq;

/* -------------------------------------------------------------------------
 * Driver state
 * ------------------------------------------------------------------------- */

typedef struct {
    VirtioCommonCfg  *common;
    volatile uint16_t *notify;         /* doorbell register address */
    uint32_t          notify_off_mult; /* notify_off_multiplier */
    uint16_t          queue_notify_off;/* per-queue notify offset */

    VirtqDesc    *desc;
    VirtqAvail   *avail;
    VirtqUsed    *used;

    uint16_t avail_idx;
    uint16_t last_used_idx;

    uint64_t capacity_sectors; /* from device config */
    uint32_t total_forth_blocks;

    /* I/O bounce buffers (DMA-accessible, allocated via kmalloc) */
    VirtBlkReq *req_buf;
    uint8_t    *data_buf;   /* BLKIO_FORTH_BLOCK_SIZE bytes */
    uint8_t    *status_buf; /* 1 byte status */
} VirtBlkState;

/* Singleton — one Artemis disk */
static VirtBlkState g_vblk;
static int          g_vblk_ready = 0;

/* -------------------------------------------------------------------------
 * Capability walker
 * Returns MMIO pointer to the virtio capability region, or NULL.
 * cap_type: VIRTIO_PCI_CAP_COMMON_CFG etc.
 * extra_out: if non-NULL, receives the 4-byte value 16 bytes into the cap
 *            (used for notify_off_multiplier).
 * ------------------------------------------------------------------------- */

static void *walk_virtio_caps(const PciDevice *d, uint8_t cap_type,
                               uint32_t *extra_out) {
    uint8_t cap_ptr = pci_read8(d, (uint16_t)PCI_CFG_CAP_PTR) & 0xFCu;
    if (!cap_ptr) return NULL;

    int limit = 48; /* guard against malformed cap list */
    while (cap_ptr && limit--) {
        uint8_t  vndr = pci_read8(d, cap_ptr + (uint16_t)VCAP_OFF_CAP_VNDR);
        uint8_t  next = pci_read8(d, cap_ptr + (uint16_t)VCAP_OFF_CAP_NEXT);
        uint8_t  ctype = pci_read8(d, cap_ptr + (uint16_t)VCAP_OFF_CFG_TYPE);

        if (vndr == (uint8_t)VIRTIO_PCI_CAP_VENDOR_ID && ctype == cap_type) {
            uint8_t  bar    = pci_read8 (d, cap_ptr + (uint16_t)VCAP_OFF_BAR);
            uint32_t offset = pci_read32(d, cap_ptr + (uint16_t)VCAP_OFF_OFFSET);
            uint32_t length = pci_read32(d, cap_ptr + (uint16_t)VCAP_OFF_LENGTH);

            if (bar > 5u) { cap_ptr = next & 0xFCu; continue; }

            uint64_t bar_base = pci_bar(d, (int)bar);
            if (!bar_base) { cap_ptr = next & 0xFCu; continue; }

            if (pci_map_bar(bar_base, (uint64_t)length + offset) != 0) {
                cap_ptr = next & 0xFCu; continue;
            }

            if (extra_out && cap_type == VIRTIO_PCI_CAP_NOTIFY_CFG) {
                *extra_out = pci_read32(d, cap_ptr + (uint16_t)VCAP_OFF_NOTIFY_MULT);
            }

            return (void *)(uintptr_t)(bar_base + offset);
        }

        cap_ptr = next & 0xFCu;
    }
    return NULL;
}

/* -------------------------------------------------------------------------
 * Memory barrier helpers
 * ------------------------------------------------------------------------- */
static inline void wmb(void) {
    __asm__ volatile("" : : : "memory");
}
static inline void rmb(void) {
    __asm__ volatile("" : : : "memory");
}

/* -------------------------------------------------------------------------
 * Virtqueue I/O
 * desc[0] = request header (read by device)
 * desc[1] = data buffer   (read or write by device)
 * desc[2] = status byte   (written by device)
 * ------------------------------------------------------------------------- */
static int vblk_io(int write, uint64_t sector, void *buf, uint32_t nbytes) {
    VirtBlkState *s = &g_vblk;
    VirtqDesc    *d = s->desc;

    /* Fill request header */
    s->req_buf->type     = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    s->req_buf->reserved = 0;
    s->req_buf->sector   = sector;
    *s->status_buf = 0xFF; /* sentinel: device overwrites with result */

    /* Descriptor 0: request header (device-readable) */
    d[0].addr  = (uint64_t)(uintptr_t)s->req_buf;
    d[0].len   = (uint32_t)sizeof(VirtBlkReq);
    d[0].flags = (uint16_t)VRING_DESC_F_NEXT;
    d[0].next  = 1;

    /* Descriptor 1: data buffer (device-readable for OUT, device-writable for IN) */
    d[1].addr  = (uint64_t)(uintptr_t)buf;
    d[1].len   = nbytes;
    d[1].flags = (uint16_t)(write ? VRING_DESC_F_NEXT
                                  : (VRING_DESC_F_NEXT | VRING_DESC_F_WRITE));
    d[1].next  = 2;

    /* Descriptor 2: status (device-writable) */
    d[2].addr  = (uint64_t)(uintptr_t)s->status_buf;
    d[2].len   = 1;
    d[2].flags = (uint16_t)VRING_DESC_F_WRITE;
    d[2].next  = 0;

    /* Add desc chain 0 to available ring */
    uint16_t avail_idx = s->avail_idx & (uint16_t)(VQUEUE_SIZE - 1u);
    s->avail->ring[avail_idx] = 0; /* first descriptor in chain */
    wmb();
    s->avail->idx = (uint16_t)(s->avail->idx + 1u);
    s->avail_idx  = s->avail->idx;
    wmb();

    /* Notify device: queue 0 */
    uint16_t notify_idx = (uint16_t)(s->queue_notify_off *
                                      (s->notify_off_mult & 0xFFFFu));
    volatile uint16_t *doorbell = s->notify + notify_idx;
    *doorbell = 0; /* queue index 0 */
    wmb();

    /* Poll until device posts a used entry */
    uint32_t spin = 0x2000000u; /* ~seconds at ~1 GHz; QEMU is fast */
    while (s->used->idx == s->last_used_idx) {
        rmb();
        if (!--spin) return BLKIO_EIO;
    }
    s->last_used_idx = s->used->idx;

    uint8_t status = *s->status_buf;
    if (status != (uint8_t)VIRTIO_BLK_S_OK) return BLKIO_EIO;

    return BLKIO_OK;
}

/* -------------------------------------------------------------------------
 * blkio_dev vtable callbacks
 * ------------------------------------------------------------------------- */

static int vblk_open(blkio_dev_t *dev, const blkio_params_t *p) {
    (void)p;
    if (!dev) return BLKIO_EINVAL;
    dev->state            = &g_vblk;
    dev->forth_block_size = BLKIO_FORTH_BLOCK_SIZE;
    dev->total_blocks     = g_vblk.total_forth_blocks;
    return BLKIO_OK;
}

static int vblk_close(blkio_dev_t *dev) {
    (void)dev;
    return BLKIO_OK;
}

static int vblk_read(blkio_dev_t *dev, uint32_t fblock, void *dst) {
    if (!dev || !dst) return BLKIO_EINVAL;
    VirtBlkState *s = (VirtBlkState *)dev->state;
    if (fblock >= s->total_forth_blocks) return BLKIO_EINVAL;

    uint64_t sector = (uint64_t)fblock * VBLK_SECTORS_PER_BLOK;
    int rc = vblk_io(0, sector, s->data_buf, BLKIO_FORTH_BLOCK_SIZE);
    if (rc != BLKIO_OK) return rc;
    memcpy(dst, s->data_buf, BLKIO_FORTH_BLOCK_SIZE);
    return BLKIO_OK;
}

static int vblk_write(blkio_dev_t *dev, uint32_t fblock, const void *src) {
    if (!dev || !src) return BLKIO_EINVAL;
    VirtBlkState *s = (VirtBlkState *)dev->state;
    if (fblock >= s->total_forth_blocks) return BLKIO_EINVAL;

    memcpy(s->data_buf, src, BLKIO_FORTH_BLOCK_SIZE);
    uint64_t sector = (uint64_t)fblock * VBLK_SECTORS_PER_BLOK;
    return vblk_io(1, sector, s->data_buf, BLKIO_FORTH_BLOCK_SIZE);
}

static int vblk_flush(blkio_dev_t *dev) {
    (void)dev;
    return BLKIO_OK; /* virtio-blk flush command not implemented; write-through */
}

static int vblk_info(blkio_dev_t *dev, blkio_info_t *out) {
    if (!dev || !out) return BLKIO_EINVAL;
    VirtBlkState *s = (VirtBlkState *)dev->state;
    out->forth_block_size = BLKIO_FORTH_BLOCK_SIZE;
    out->total_blocks     = s->total_forth_blocks;
    out->phys_sector_size = VBLK_SECTOR_SIZE;
    out->phys_size_bytes  = s->capacity_sectors * VBLK_SECTOR_SIZE;
    out->read_only        = 0;
    return BLKIO_OK;
}

static const blkio_vtable_t g_vblk_vtable = {
    .open  = vblk_open,
    .close = vblk_close,
    .read  = vblk_read,
    .write = vblk_write,
    .flush = vblk_flush,
    .info  = vblk_info
};

/* -------------------------------------------------------------------------
 * Device initialisation
 * ------------------------------------------------------------------------- */

static int vblk_init_device(const PciDevice *pci) {
    VirtBlkState *s = &g_vblk;

    /* Enable bus mastering + MMIO */
    pci_enable(pci);

    /* Walk capabilities: find common config and notify regions */
    uint32_t notify_mult = 0;
    VirtioCommonCfg  *common = (VirtioCommonCfg *)
                               walk_virtio_caps(pci, VIRTIO_PCI_CAP_COMMON_CFG, NULL);
    volatile uint16_t *notify = (volatile uint16_t *)
                               walk_virtio_caps(pci, VIRTIO_PCI_CAP_NOTIFY_CFG, &notify_mult);

    if (!common || !notify) {
        console_println("virtio-blk: cap walk failed");
        return -2;
    }

    s->common           = common;
    s->notify           = notify;
    s->notify_off_mult  = notify_mult;

    /* Step 1: reset device */
    common->device_status = 0;
    wmb();

    /* Step 2: ACKNOWLEDGE */
    common->device_status = (uint8_t)VIRTIO_STATUS_ACKNOWLEDGE;
    wmb();

    /* Step 3: DRIVER */
    common->device_status = (uint8_t)(VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    wmb();

    /* Step 4: feature negotiation — request VERSION_1 only */
    common->driver_feature_select = 1; /* select high 32 bits */
    wmb();
    common->driver_feature = (uint32_t)(VIRTIO_F_VERSION_1 >> 32);
    common->driver_feature_select = 0;
    wmb();
    common->driver_feature = 0; /* no low-32 features required */
    wmb();

    /* Step 5: FEATURES_OK */
    common->device_status = (uint8_t)(VIRTIO_STATUS_ACKNOWLEDGE |
                                      VIRTIO_STATUS_DRIVER |
                                      VIRTIO_STATUS_FEATURES_OK);
    wmb();
    rmb();
    if (!(common->device_status & (uint8_t)VIRTIO_STATUS_FEATURES_OK)) {
        console_println("virtio-blk: FEATURES_OK rejected");
        common->device_status = (uint8_t)VIRTIO_STATUS_FAILED;
        return -2;
    }

    /* Step 6: configure virtqueue 0 */
    common->queue_select = 0;
    wmb();
    uint16_t max_size = common->queue_size;
    if (max_size == 0 || max_size > 0x8000u) {
        console_println("virtio-blk: bad queue size");
        return -2;
    }
    uint16_t qsize = (max_size < (uint16_t)VQUEUE_SIZE)
                     ? max_size : (uint16_t)VQUEUE_SIZE;
    common->queue_size = qsize;
    s->queue_notify_off = common->queue_notify_off;
    wmb();

    /* Disable MSIX (not supported) */
    common->config_msix_vector = 0xFFFFu;
    common->queue_msix_vector  = 0xFFFFu;
    wmb();

    /* Allocate virtqueue rings — 64-byte aligned */
    size_t desc_bytes  = (size_t)qsize * sizeof(VirtqDesc);
    size_t avail_bytes = sizeof(uint16_t) * 2u +
                         (size_t)qsize * sizeof(uint16_t) +
                         sizeof(uint16_t);
    size_t used_bytes  = sizeof(uint16_t) * 2u +
                         (size_t)qsize * sizeof(VirtqUsedElem) +
                         sizeof(uint16_t);

    s->desc  = (VirtqDesc *)kmalloc_aligned(desc_bytes, 64);
    s->avail = (VirtqAvail *)kmalloc_aligned(avail_bytes, 2);
    s->used  = (VirtqUsed  *)kmalloc_aligned(used_bytes, 4);
    if (!s->desc || !s->avail || !s->used) {
        console_println("virtio-blk: queue alloc failed");
        return -2;
    }
    memset(s->desc,  0, desc_bytes);
    memset(s->avail, 0, avail_bytes);
    memset(s->used,  0, used_bytes);

    /* Allocate I/O bounce buffers */
    s->req_buf    = (VirtBlkReq *)kmalloc_aligned(sizeof(VirtBlkReq), 16);
    s->data_buf   = (uint8_t *)   kmalloc_aligned(BLKIO_FORTH_BLOCK_SIZE, 512);
    s->status_buf = (uint8_t *)   kmalloc(1);
    if (!s->req_buf || !s->data_buf || !s->status_buf) {
        console_println("virtio-blk: buf alloc failed");
        return -2;
    }

    s->avail_idx     = 0;
    s->last_used_idx = 0;

    /* Wire queue physical addresses into device */
    common->queue_desc   = (uint64_t)(uintptr_t)s->desc;
    common->queue_driver = (uint64_t)(uintptr_t)s->avail;
    common->queue_device = (uint64_t)(uintptr_t)s->used;
    wmb();

    common->queue_enable = 1;
    wmb();

    /* Read device capacity from device config (at BAR offset after common) */
    {
        /* Capacity is the first 8 bytes of the device-specific config region */
        void *dev_cfg_raw = walk_virtio_caps(pci, VIRTIO_PCI_CAP_DEVICE_CFG, NULL);
        if (dev_cfg_raw) {
            volatile uint32_t *cap32 = (volatile uint32_t *)dev_cfg_raw;
            uint64_t lo = cap32[0];
            uint64_t hi = cap32[1];
            s->capacity_sectors = lo | (hi << 32);
        } else {
            /* Fallback: assume Artemis size */
            s->capacity_sectors = VBLK_ARTEMIS_SECTORS;
        }
    }
    s->total_forth_blocks = (uint32_t)(s->capacity_sectors / VBLK_SECTORS_PER_BLOK);

    /* Step 7: DRIVER_OK */
    common->device_status = (uint8_t)(VIRTIO_STATUS_ACKNOWLEDGE |
                                      VIRTIO_STATUS_DRIVER |
                                      VIRTIO_STATUS_FEATURES_OK |
                                      VIRTIO_STATUS_DRIVER_OK);
    wmb();

    return 0;
}

/* -------------------------------------------------------------------------
 * Public entry point
 * ------------------------------------------------------------------------- */

int virtio_blk_find_artemis(blkio_dev_t *dev_out) {
    if (!dev_out) return -1;

    PciDevice pci;
    /* Try modern device first, then legacy/transitional */
    int found = pci_find_first(VIRTIO_PCI_VENDOR_ID, VIRTIO_BLK_DEVICE_MODERN, &pci);
    if (found != 0)
        found = pci_find_first(VIRTIO_PCI_VENDOR_ID, VIRTIO_BLK_DEVICE_LEGACY, &pci);
    if (found != 0) {
        console_println("virtio-blk: no device on PCI bus 0");
        return -1;
    }

    console_println("virtio-blk: found device");

    int rc = vblk_init_device(&pci);
    if (rc != 0) return rc;

    g_vblk_ready = 1;

    blkio_params_t params;
    params.forth_block_size = BLKIO_FORTH_BLOCK_SIZE;
    params.total_blocks     = g_vblk.total_forth_blocks;
    params.opaque           = NULL;

    return blkio_open(dev_out, &g_vblk_vtable, &params);
}
