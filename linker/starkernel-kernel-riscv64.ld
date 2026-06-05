/* Linker script for StarKernel Kernel ELF (riscv64) */

OUTPUT_FORMAT("elf64-littleriscv")
OUTPUT_ARCH(riscv:rv64)
ENTRY(kernel_main)

SECTIONS
{
    . = 0x400000;

    __kernel_start = .;

    .text : ALIGN(4K) {
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
        *(.got.plt)
        __data_end = .;
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
