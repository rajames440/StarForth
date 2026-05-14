/**
 * reloc.S - Minimal .reloc section for UEFI PE32+ loader
 *
 * UEFI firmware requires a .reloc section for base relocations.
 * This provides a minimal placeholder that objcopy can recognize
 * when converting ELF to PE32+ format.
 */

#if defined(__x86_64__) || defined(ARCH_AMD64)

    .section .reloc, "a"
    .balign 4
dummy_reloc:
    .long 0

#elif defined(__aarch64__) || defined(ARCH_AARCH64)

    .section .reloc, "a"
    .balign 4
dummy_reloc:
    .word 0

#elif defined(__riscv) || defined(ARCH_RISCV64)

    .section .reloc, "a"
    .balign 4
dummy_reloc:
    .word 0

#endif
