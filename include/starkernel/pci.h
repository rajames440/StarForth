/*
 * pci.h — Generic PCI/PCIe subsystem for StarKernel
 *
 * Config space access:
 *   amd64  — Type 1 port I/O (CF8/CFC); no memory mapping required.
 *   others — ECAM via ACPI MCFG; UEFI identity-maps the ECAM window.
 *
 * BAR MMIO:
 *   Call pci_map_bar() before the first MMIO access.  On amd64 this
 *   adds a VMM page-table mapping (cache-disabled); on other arches
 *   the UEFI tables already cover the region and the call is a no-op.
 */

#ifndef STARKERNEL_PCI_H
#define STARKERNEL_PCI_H

#include <stdint.h>

/* PCI standard config-space offsets */
#define PCI_CFG_VENDOR_ID    0x00
#define PCI_CFG_DEVICE_ID    0x02
#define PCI_CFG_COMMAND      0x04
#define PCI_CFG_STATUS       0x06
#define PCI_CFG_CLASS_REV    0x08
#define PCI_CFG_HEADER_TYPE  0x0E
#define PCI_CFG_BAR0         0x10
#define PCI_CFG_BAR1         0x14
#define PCI_CFG_BAR2         0x18
#define PCI_CFG_BAR3         0x1C
#define PCI_CFG_BAR4         0x20
#define PCI_CFG_BAR5         0x24
#define PCI_CFG_CAP_PTR      0x34
#define PCI_CFG_INT_LINE     0x3C

/* PCI command register bits */
#define PCI_CMD_IO_SPACE     (1u << 0)
#define PCI_CMD_MEM_SPACE    (1u << 1)
#define PCI_CMD_BUS_MASTER   (1u << 2)

/* BAR type flags */
#define PCI_BAR_TYPE_MASK    0x1u
#define PCI_BAR_IO           0x1u
#define PCI_BAR_MEM_64       0x4u   /* bits [2:1] == 10 in memory BAR */

/* Identified PCI device */
typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
} PciDevice;

/*
 * pci_init — parse ACPI RSDP → XSDT → MCFG to find ECAM base.
 *            On amd64, also maps the ECAM window via the VMM.
 *            May be called with rsdp==NULL; port-I/O fallback still works
 *            on amd64.  On aarch64/riscv64 ECAM is required.
 * Returns 0 on success, -1 if no ECAM found (amd64 still functional via
 * port I/O).
 */
int pci_init(void *rsdp);

/*
 * pci_find_first — linear scan of bus 0 for the first device matching
 *                  (vendor_id, device_id).  Fill *out on match.
 * Returns 0 on found, -1 if not found.
 */
int pci_find_first(uint16_t vendor_id, uint16_t device_id, PciDevice *out);

/* Config space accessors */
uint32_t pci_read32 (const PciDevice *d, uint16_t offset);
uint16_t pci_read16 (const PciDevice *d, uint16_t offset);
uint8_t  pci_read8  (const PciDevice *d, uint16_t offset);
void     pci_write32(const PciDevice *d, uint16_t offset, uint32_t val);
void     pci_write16(const PciDevice *d, uint16_t offset, uint16_t val);
void     pci_write8 (const PciDevice *d, uint16_t offset, uint8_t  val);

/*
 * pci_enable — set I/O space + memory space + bus-master bits in the
 *              PCI command register.
 */
void pci_enable(const PciDevice *d);

/*
 * pci_bar — return the base address (physical) of BAR bar_idx.
 *           Handles 32-bit and 64-bit memory BARs; returns 0 for I/O BARs.
 *           Does NOT map the region — call pci_map_bar() separately.
 */
uint64_t pci_bar(const PciDevice *d, int bar_idx);

/*
 * pci_map_bar — make the BAR MMIO region accessible.
 *               amd64: calls vmm_map_range(phys, phys, size, WR|CD).
 *               others: no-op (UEFI identity map covers it).
 * Returns 0 on success, -1 on failure.
 */
int pci_map_bar(uint64_t phys_addr, uint64_t size);

#endif /* STARKERNEL_PCI_H */
