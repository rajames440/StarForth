/**
 * apic.h - Local APIC minimal interface
 */

#ifndef STARKERNEL_APIC_H
#define STARKERNEL_APIC_H

#include "uefi.h"
#include <stdint.h>

int apic_init(BootInfo *boot_info);
void apic_eoi(void);

#endif /* STARKERNEL_APIC_H */
