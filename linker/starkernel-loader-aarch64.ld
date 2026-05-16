/* Linker script for StarKernel UEFI Loader (aarch64) - ELF64 intermediate */
/* UEFI PE64 format — used as ELF intermediate before objcopy to PE */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(efi_main)

SECTIONS
{
    /* Base address for UEFI applications */
    . = 0x1000;

    __loader_start = .;

    .text : {
        __text_start = .;
        *(.text.entry)
        *(.text .text.*)
        __text_end = .;
    }

    .rodata : ALIGN(4K) {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }

    .data : ALIGN(4K) {
        __data_start = .;
        *(.data .data.*)
        *(.got .got.*)
        __data_end = .;
    }

    .rela.dyn : ALIGN(4K) {
        *(.rela .rela.*)
    }

    .dynsym : {
        *(.dynsym)
    }

    .dynamic : {
        *(.dynamic)
    }

    /* PE32+ relocation section - required for UEFI to relocate the image.
     * Pre-allocate 128 KB; the tools/pe_reloc_gen.py script patches this
     * section after objcopy with proper DIR64 entries from .rela.dyn. */
    .reloc : ALIGN(4K) {
        KEEP(*(.reloc))
        . = ALIGN(4K);
        . += 0x20000;   /* 128 KB reserved for reloc entries */
    }

    .bss : ALIGN(4K) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    __loader_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
    }
}
