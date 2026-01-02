/*
 * boot.S (amd64) - Placeholder entry for future low-level bring-up.
 * The UEFI loader currently calls efi_main() directly; this stub exists
 * to reserve the symbol for later early-boot work.
 */
    .section .text.entry
    .code64
    .global arch_bootstrap
arch_bootstrap:
    ret
