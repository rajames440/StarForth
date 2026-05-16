/*
 * boot.S (aarch64) - Placeholder entry + minimal PE relocation stub.
 *
 * UEFI requires the PE image to have a .reloc section.  With fully PIC code
 * (-fPIC -fvisibility=hidden) there are no load-time fixups, so an empty
 * base-relocation block (size=10, no entries) satisfies the requirement.
 * This is the same trick used by GNU-EFI's crt0-efi-aarch64.S.
 */
    .section .text.entry
    .global arch_bootstrap
arch_bootstrap:
    ret

/*
 * Minimal PE base-relocation block: one block covering page 0 with no
 * individual relocations.  The block header (8 bytes) + size word (2 bytes)
 * = 10 bytes total — the minimum valid block.
 */
    .section .reloc, "a"
    .long  0        /* VirtualAddress of the page */
    .long  10       /* SizeOfBlock (header only = no relocations) */
    .short 0        /* relocation entry terminator */
