/* Linker script for StarKernel UEFI Loader (amd64/x86_64) */
/* UEFI PE32+ format - DUMB LOADER ONLY */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS
{
    /* Base address for UEFI applications - low address like GRUB */
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

    /* PE32+ relocation section - required for UEFI to relocate the image */
    .reloc : ALIGN(4K) {
        KEEP(*(.reloc))
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
