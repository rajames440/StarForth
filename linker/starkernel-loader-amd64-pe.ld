/* Linker script for StarKernel UEFI Loader (amd64/x86_64) - PE32+ direct */
/* Used with ld -m i386pep for direct PE output with base relocations */

OUTPUT_FORMAT("pei-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

/* PE image base - UEFI will relocate this */
/* Note: For PE, ImageBase sets the preferred load address */
/* Sections are RVA offsets from this base */

SECTIONS
{
    /* First section starts at RVA 0x1000 (standard PE alignment) */
    . = 0x1000;

    __kernel_start = .;
    __loader_start = .;

    .text : {
        __text_start = .;
        *(.text.entry)
        *(.text .text.*)
        __text_end = .;
    }

    .rodata : ALIGN(0x1000) {
        __rodata_start = .;
        *(.rodata .rodata.*)
        *(.rdata .rdata.*)
        __rodata_end = .;
    }

    .data : ALIGN(0x1000) {
        __data_start = .;
        *(.data .data.*)
        *(.got .got.*)
        __data_end = .;
    }

    .pdata : ALIGN(0x1000) {
        *(.pdata)
    }

    .xdata : {
        *(.xdata)
    }

    .bss : ALIGN(0x1000) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Base relocation section - PE requires this for UEFI to relocate */
    .reloc : ALIGN(0x1000) {
        *(.reloc)
    }

    __loader_end = .;
    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.dynsym)
        *(.dynstr)
        *(.dynamic)
        *(.hash)
        *(.gnu.hash)
    }
}