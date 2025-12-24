/* Linker script for StarKernel Kernel ELF (amd64/x86_64) */
/* Freestanding ELF64 binary */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(kernel_main)

SECTIONS
{
    /* Kernel loaded at 4MB by ELF loader */
    . = 0x400000;

    __kernel_start = .;

    .text : ALIGN(4K) {
        *(.text.entry)
        *(.text .text.*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4K) {
        *(.data .data.*)
    }

    .rela.dyn : ALIGN(4K) {
        *(.rela .rela.*)
    }

    .bss : ALIGN(4K) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.dynamic)
        *(.dynsym)
    }
}