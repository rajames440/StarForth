/*
 * boot.S (aarch64) - Placeholder entry for future EL transitions.
 * The UEFI loader currently calls efi_main() directly.
 */
    .section .text.entry
    .align 2
    .global arch_bootstrap
arch_bootstrap:
    ret
