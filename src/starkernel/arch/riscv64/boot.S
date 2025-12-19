/*
 * boot.S (riscv64) - Placeholder entry for future trap setup.
 * Current UEFI path enters at efi_main().
 */
    .section .text.entry
    .align 2
    .global arch_bootstrap
arch_bootstrap:
    ret
