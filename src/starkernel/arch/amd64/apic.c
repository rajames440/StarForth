/**
 * apic.c - Minimal Local APIC enablement (amd64)
 */

#include <stdint.h>
#include "apic.h"
#include "console.h"
#include "vmm.h"

#define LAPIC_DEFAULT_PHYS 0xFEE00000u
#define LAPIC_VIRT_BASE    0xFEE00000u  /* identity-mapped */

static volatile uint32_t *lapic_base = (volatile uint32_t *)(uintptr_t)LAPIC_VIRT_BASE;
static uint64_t lapic_phys_base = LAPIC_DEFAULT_PHYS;

static void lapic_write(uint32_t reg, uint32_t val) {
    lapic_base[reg / 4] = val;
}

int apic_init(BootInfo *boot_info) {
    (void)boot_info;
    lapic_phys_base = LAPIC_DEFAULT_PHYS;
    lapic_base = (volatile uint32_t *)(uintptr_t)LAPIC_VIRT_BASE;

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
