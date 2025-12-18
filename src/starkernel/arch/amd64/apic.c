/**
 * apic.c - Minimal Local APIC enablement (amd64)
 */

#include <stdint.h>
#include "apic.h"
#include "console.h"

typedef struct {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) rsdp_t;

typedef struct {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

static volatile uint32_t *lapic_base = (volatile uint32_t *)(uintptr_t)0xFEE00000u;

static uint8_t acpi_checksum(const void *tbl, uint32_t len) {
    const uint8_t *p = (const uint8_t *)tbl;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; ++i) {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum;
}

static acpi_sdt_header_t *find_sdt(BootInfo *boot_info, const char sig[4]) {
    if (!boot_info || !boot_info->acpi_table) {
        return NULL;
    }

    rsdp_t *rsdp = (rsdp_t *)boot_info->acpi_table;
    if (__builtin_memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        return NULL;
    }

    /* Prefer XSDT when available */
    acpi_sdt_header_t *xsdt = NULL;
    acpi_sdt_header_t *rsdt = NULL;

    if (rsdp->length >= sizeof(rsdp_t) && rsdp->xsdt_address) {
        xsdt = (acpi_sdt_header_t *)(uintptr_t)rsdp->xsdt_address;
        if (acpi_checksum(xsdt, xsdt->length) != 0) {
            xsdt = NULL;
        }
    }

    if (rsdp->rsdt_address) {
        rsdt = (acpi_sdt_header_t *)(uintptr_t)rsdp->rsdt_address;
        if (acpi_checksum(rsdt, rsdt->length) != 0) {
            rsdt = NULL;
        }
    }

    if (xsdt) {
        uint32_t count = (xsdt->length - sizeof(acpi_sdt_header_t)) / 8;
        uint64_t *entries = (uint64_t *)((uint8_t *)xsdt + sizeof(acpi_sdt_header_t));
        for (uint32_t i = 0; i < count; ++i) {
            acpi_sdt_header_t *h = (acpi_sdt_header_t *)(uintptr_t)entries[i];
            if (__builtin_memcmp(h->signature, sig, 4) == 0) {
                return h;
            }
        }
    }

    if (rsdt) {
        uint32_t count = (rsdt->length - sizeof(acpi_sdt_header_t)) / 4;
        uint32_t *entries = (uint32_t *)((uint8_t *)rsdt + sizeof(acpi_sdt_header_t));
        for (uint32_t i = 0; i < count; ++i) {
            acpi_sdt_header_t *h = (acpi_sdt_header_t *)(uintptr_t)entries[i];
            if (__builtin_memcmp(h->signature, sig, 4) == 0) {
                return h;
            }
        }
    }

    return NULL;
}

static void lapic_write(uint32_t reg, uint32_t val) {
    lapic_base[reg / 4] = val;
}

int apic_init(BootInfo *boot_info) {
    /* Try ACPI MADT for LAPIC address */
    acpi_sdt_header_t *madt = find_sdt(boot_info, "APIC");
    if (madt && madt->length >= sizeof(acpi_sdt_header_t) + 8) {
        uint32_t lapic_phys = *(uint32_t *)((uint8_t *)madt + sizeof(acpi_sdt_header_t));
        if (lapic_phys != 0) {
            lapic_base = (volatile uint32_t *)(uintptr_t)lapic_phys;
        }
    } else {
        console_println("APIC: MADT not found, using default 0xFEE00000");
    }

    /* Enable local APIC via Spurious Interrupt Vector Register (0xF0) */
    const uint32_t APIC_SIVR = 0xF0;
    const uint8_t  SPURIOUS_VECTOR = 0xFF;
    const uint32_t APIC_ENABLE = (1u << 8);

    lapic_write(APIC_SIVR, APIC_ENABLE | SPURIOUS_VECTOR);
    console_println("APIC enabled (SIVR=0xFF).");
    return 0;
}

void apic_eoi(void) {
    /* End of Interrupt register at 0xB0 */
    const uint32_t APIC_EOI = 0xB0;
    lapic_write(APIC_EOI, 0);
}
