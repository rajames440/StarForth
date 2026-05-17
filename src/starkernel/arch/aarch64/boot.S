/*
 * boot.S (aarch64) - Entry stub and linker-script symbol substitutes.
 *
 * With lld-link (PE/COFF), GNU ld linker-script symbols are unavailable.
 * Define __text_start, __text_end, __kernel_start, __kernel_end here so
 * hal.c and pmm.c resolve.  They all point to the image base, which is
 * sufficient for the monolithic loader build (UEFI manages page ownership).
 *
 * The .reloc section (PE base-relocation table) is generated automatically
 * by lld-link from the COFF relocations in the object files.
 */
    .text
    .global __text_start
    .global __kernel_start
__text_start:
__kernel_start:

    .global arch_bootstrap
arch_bootstrap:
    ret

    .global __text_end
    .global __kernel_end
__text_end:
__kernel_end:
