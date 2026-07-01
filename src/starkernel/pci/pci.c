/*
 * pci.c — Generic PCI/PCIe subsystem for StarKernel
 *
 * Config space strategy:
 *   ARCH_AMD64  : Type 1 port I/O (0xCF8 / 0xCFC).  No ECAM mapping needed
 *                 for config access; ECAM is still parsed for completeness
 *                 but port I/O is always the primary path on amd64.
 *   ARCH_AARCH64/ARCH_RISCV64 : ECAM via ACPI MCFG.  UEFI identity-maps
 *                 the window so no additional VMM call is required.
 *
 * BAR MMIO strategy:
 *   ARCH_AMD64  : vmm_map_range(phys, phys, size, WR|CD) before first access.
 *   others      : no-op; the UEFI identity map covers PCI MMIO windows.
 *
 * Only bus 0 is scanned (sufficient for all QEMU virtio devices).
 */

#ifndef __STARKERNEL__
#error "pci.c is kernel-only"
#endif

#include <stddef.h>
#include <stdint.h>
#include "starkernel/pci.h"
#include "console.h"

#ifdef ARCH_AMD64
#include "vmm.h"
#endif

/* -------------------------------------------------------------------------
 * ACPI structures for MCFG / ECAM discovery
 * ------------------------------------------------------------------------- */

typedef struct {
    uint8_t  signature[8];
    uint8_t  checksum;
    uint8_t  oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) Rsdp2;

typedef struct {
    uint8_t  signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    uint8_t  oem_id[6];
    uint8_t  oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) AcpiHeader;

typedef struct {
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t  start_bus;
    uint8_t  end_bus;
    uint32_t reserved;
} __attribute__((packed)) McfgEntry;

typedef struct {
    AcpiHeader hdr;
    uint64_t   reserved;
    /* McfgEntry[] follows */
} __attribute__((packed)) McfgTable;

/* -------------------------------------------------------------------------
 * Module state
 * ------------------------------------------------------------------------- */

static uint64_t g_ecam_base = 0;

/* -------------------------------------------------------------------------
 * Port I/O helpers (amd64 only)
 * ------------------------------------------------------------------------- */

#ifdef ARCH_AMD64
static inline void sk_outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint32_t sk_inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile("inl %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}
static inline void sk_outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint16_t sk_inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}
static inline void sk_outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint8_t sk_inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

#define PCI_CF8  ((uint16_t)0x0CF8)
#define PCI_CFC  ((uint16_t)0x0CFC)

static uint32_t cfg_addr(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
    return 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
           ((uint32_t)fn << 8) | (off & 0xFCu);
}

static uint32_t portio_read32(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
    sk_outl(PCI_CF8, cfg_addr(bus, slot, fn, off));
    return sk_inl(PCI_CFC);
}
static uint16_t portio_read16(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
    sk_outl(PCI_CF8, cfg_addr(bus, slot, fn, off));
    return sk_inw((uint16_t)(PCI_CFC + (off & 2u)));
}
static uint8_t portio_read8(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
    sk_outl(PCI_CF8, cfg_addr(bus, slot, fn, off));
    return sk_inb((uint16_t)(PCI_CFC + (off & 3u)));
}
static void portio_write32(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off, uint32_t v) {
    sk_outl(PCI_CF8, cfg_addr(bus, slot, fn, off));
    sk_outl(PCI_CFC, v);
}
static void portio_write16(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off, uint16_t v) {
    sk_outl(PCI_CF8, cfg_addr(bus, slot, fn, off));
    sk_outw((uint16_t)(PCI_CFC + (off & 2u)), v);
}
static void portio_write8(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off, uint8_t v) {
    sk_outl(PCI_CF8, cfg_addr(bus, slot, fn, off));
    sk_outb((uint16_t)(PCI_CFC + (off & 3u)), v);
}
#endif /* ARCH_AMD64 */

/* -------------------------------------------------------------------------
 * ECAM helpers (aarch64 / riscv64 only — amd64 uses port I/O)
 * ------------------------------------------------------------------------- */

#ifndef ARCH_AMD64
static volatile uint32_t *ecam_cfg_ptr(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
    uint64_t addr = g_ecam_base +
                    ((uint64_t)bus  << 20) +
                    ((uint64_t)slot << 15) +
                    ((uint64_t)fn   << 12) +
                    (off & 0xFFCu);
    return (volatile uint32_t *)(uintptr_t)addr;
}

static uint32_t ecam_read32(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
    return *ecam_cfg_ptr(bus, slot, fn, off);
}
static uint16_t ecam_read16(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
    uint32_t dw = *ecam_cfg_ptr(bus, slot, fn, off & ~2u);
    return (uint16_t)(dw >> ((off & 2u) * 8u));
}
static uint8_t ecam_read8(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
    uint32_t dw = *ecam_cfg_ptr(bus, slot, fn, off & ~3u);
    return (uint8_t)(dw >> ((off & 3u) * 8u));
}
static void ecam_write32(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off, uint32_t v) {
    *ecam_cfg_ptr(bus, slot, fn, off) = v;
}
static void ecam_write16(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off, uint16_t v) {
    volatile uint32_t *p = ecam_cfg_ptr(bus, slot, fn, off & ~2u);
    uint32_t dw = *p;
    uint32_t shift = (off & 2u) * 8u;
    dw = (dw & ~(0xFFFFu << shift)) | ((uint32_t)v << shift);
    *p = dw;
}
static void ecam_write8(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off, uint8_t v) {
    volatile uint32_t *p = ecam_cfg_ptr(bus, slot, fn, off & ~3u);
    uint32_t dw = *p;
    uint32_t shift = (off & 3u) * 8u;
    dw = (dw & ~(0xFFu << shift)) | ((uint32_t)v << shift);
    *p = dw;
}
#endif /* !ARCH_AMD64 */

/* -------------------------------------------------------------------------
 * Config space dispatch (portio on amd64, ecam on others)
 * ------------------------------------------------------------------------- */

static uint32_t cfg_read32(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
#ifdef ARCH_AMD64
    return portio_read32(bus, slot, fn, off);
#else
    return ecam_read32(bus, slot, fn, off);
#endif
}
static uint16_t cfg_read16(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
#ifdef ARCH_AMD64
    return portio_read16(bus, slot, fn, off);
#else
    return ecam_read16(bus, slot, fn, off);
#endif
}
static uint8_t cfg_read8(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off) {
#ifdef ARCH_AMD64
    return portio_read8(bus, slot, fn, off);
#else
    return ecam_read8(bus, slot, fn, off);
#endif
}
static void cfg_write32(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off, uint32_t v) {
#ifdef ARCH_AMD64
    portio_write32(bus, slot, fn, off, v);
#else
    ecam_write32(bus, slot, fn, off, v);
#endif
}
static void cfg_write16(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off, uint16_t v) {
#ifdef ARCH_AMD64
    portio_write16(bus, slot, fn, off, v);
#else
    ecam_write16(bus, slot, fn, off, v);
#endif
}
static void cfg_write8(uint8_t bus, uint8_t slot, uint8_t fn, uint16_t off, uint8_t v) {
#ifdef ARCH_AMD64
    portio_write8(bus, slot, fn, off, v);
#else
    ecam_write8(bus, slot, fn, off, v);
#endif
}

/* -------------------------------------------------------------------------
 * ACPI MCFG parsing
 * ------------------------------------------------------------------------- */

static int sig4_eq(const uint8_t *p, const char *s) {
    return p[0] == (uint8_t)s[0] && p[1] == (uint8_t)s[1] &&
           p[2] == (uint8_t)s[2] && p[3] == (uint8_t)s[3];
}
static int sig8_eq(const uint8_t *p, const char *s) {
    int i;
    for (i = 0; i < 8; i++)
        if (p[i] != (uint8_t)s[i]) return 0;
    return 1;
}

static int parse_mcfg(void *rsdp) {
    if (!rsdp) return -1;

    Rsdp2 *r = (Rsdp2 *)rsdp;
    if (!sig8_eq(r->signature, "RSD PTR ")) return -1;
    if (r->revision < 2 || r->xsdt_address == 0) return -1;

    AcpiHeader *xsdt = (AcpiHeader *)(uintptr_t)r->xsdt_address;
    if (!sig4_eq(xsdt->signature, "XSDT")) return -1;

    uint32_t hdr_len = xsdt->length;
    if (hdr_len <= 36u) return -1;
    uint32_t n_entries = (hdr_len - 36u) / 8u;
    uint64_t *entries = (uint64_t *)((uint8_t *)xsdt + 36u);

    uint32_t i;
    for (i = 0; i < n_entries; i++) {
        AcpiHeader *sdt = (AcpiHeader *)(uintptr_t)entries[i];
        if (!sig4_eq(sdt->signature, "MCFG")) continue;

        McfgTable *mcfg = (McfgTable *)sdt;
        uint32_t mcfg_len = mcfg->hdr.length;
        if (mcfg_len <= 44u) return -1;
        uint32_t n_alloc = (mcfg_len - 44u) / 16u;
        McfgEntry *ents = (McfgEntry *)((uint8_t *)mcfg + 44u);

        uint32_t j;
        for (j = 0; j < n_alloc; j++) {
            if (ents[j].segment_group == 0) {
                g_ecam_base = ents[j].base_address;
                return 0;
            }
        }
        return -1;
    }
    return -1;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

int pci_init(void *rsdp) {
    int rc = parse_mcfg(rsdp);

#if defined(ARCH_AARCH64) || defined(ARCH_RISCV64)
    if (rc != 0) {
#ifdef ARCH_RISCV64
        g_ecam_base = 0x30000000ULL;
        console_println("PCI: MCFG miss; using riscv64 fallback ECAM 0x30000000");
        rc = 0;
#else
        console_println("PCI: MCFG parse failed on aarch64 — no ECAM");
#endif
    }
#endif

#ifdef ARCH_AMD64
    if (rc != 0) {
        console_println("PCI: MCFG parse failed; using port I/O (amd64 default)");
        rc = 0;
    } else {
        /* Map the ECAM window (256 buses × 1 MB = 256 MB) on amd64 so the
         * VMM page tables cover it. */
        if (g_ecam_base) {
            vmm_map_range(g_ecam_base, g_ecam_base, 0x10000000ULL,
                          VMM_FLAG_WRITABLE | VMM_FLAG_CACHE_DISABLE);
        }
        console_println("PCI: ECAM mapped (amd64)");
    }
#endif

    return rc;
}

int pci_find_first(uint16_t vendor_id, uint16_t device_id, PciDevice *out) {
    uint8_t slot;
    for (slot = 0; slot < 32; slot++) {
        uint32_t vid_did = cfg_read32(0, slot, 0, PCI_CFG_VENDOR_ID);
        if ((vid_did & 0xFFFFu) == 0xFFFFu) continue;
        if ((uint16_t)(vid_did & 0xFFFFu) == vendor_id &&
            (uint16_t)(vid_did >> 16)     == device_id) {
            if (out) {
                out->bus       = 0;
                out->device    = slot;
                out->function  = 0;
                out->vendor_id = vendor_id;
                out->device_id = device_id;
            }
            return 0;
        }
    }
    return -1;
}

uint32_t pci_read32(const PciDevice *d, uint16_t off) {
    return cfg_read32(d->bus, d->device, d->function, off);
}
uint16_t pci_read16(const PciDevice *d, uint16_t off) {
    return cfg_read16(d->bus, d->device, d->function, off);
}
uint8_t pci_read8(const PciDevice *d, uint16_t off) {
    return cfg_read8(d->bus, d->device, d->function, off);
}
void pci_write32(const PciDevice *d, uint16_t off, uint32_t v) {
    cfg_write32(d->bus, d->device, d->function, off, v);
}
void pci_write16(const PciDevice *d, uint16_t off, uint16_t v) {
    cfg_write16(d->bus, d->device, d->function, off, v);
}
void pci_write8(const PciDevice *d, uint16_t off, uint8_t v) {
    cfg_write8(d->bus, d->device, d->function, off, v);
}

void pci_enable(const PciDevice *d) {
    uint16_t cmd = pci_read16(d, PCI_CFG_COMMAND);
    cmd |= (uint16_t)(PCI_CMD_IO_SPACE | PCI_CMD_MEM_SPACE | PCI_CMD_BUS_MASTER);
    pci_write16(d, PCI_CFG_COMMAND, cmd);
}

uint64_t pci_bar(const PciDevice *d, int bar_idx) {
    uint16_t off = (uint16_t)(PCI_CFG_BAR0 + bar_idx * 4);
    uint32_t lo  = pci_read32(d, off);

    if (lo & PCI_BAR_TYPE_MASK) return 0; /* I/O BAR — unsupported */

    if ((lo & 0x6u) == PCI_BAR_MEM_64) {
        uint32_t hi = pci_read32(d, (uint16_t)(off + 4));
        return ((uint64_t)hi << 32) | (lo & ~0xFu);
    }
    return lo & ~0xFu;
}

int pci_map_bar(uint64_t phys_addr, uint64_t size) {
#ifdef ARCH_AMD64
    if (!phys_addr || !size) return -1;
    /* Round size up to page boundary */
    uint64_t aligned_size = (size + 0xFFFu) & ~0xFFFu;
    return vmm_map_range(phys_addr, phys_addr, aligned_size,
                         VMM_FLAG_WRITABLE | VMM_FLAG_CACHE_DISABLE);
#else
    (void)phys_addr; (void)size;
    return 0;
#endif
}
