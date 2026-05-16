/*
 * boot.S (aarch64) - Placeholder entry stub.
 *
 * The .reloc section (PE base-relocation table) is generated automatically
 * by lld-link from the COFF relocations in the object files.  No manual
 * stub is needed here, unlike the former GNU objcopy-based approach.
 */
    .text
    .global arch_bootstrap
arch_bootstrap:
    ret
