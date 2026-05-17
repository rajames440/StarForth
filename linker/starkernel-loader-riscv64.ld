/* Linker script for StarKernel UEFI Loader (riscv64) - ELF64 intermediate */
/* Linked as shared ELF, then converted to PE via objcopy -O pei-riscv64-little */
/*
 * Section layout:
 *   .text/.rodata/.data  — code and initialized data
 *   .bss                 — uninitialized data (right after .data, VMA-only, no file data)
 *   .reloc at 0x700000   — PE base relocation table injected post-link
 *                          (large fixed gap avoids overlap with ~6MB BSS)
 *   .rela.dyn / ELF dyn  — ELF dynamic sections, stripped before PE conversion
 */

OUTPUT_FORMAT("elf64-littleriscv")
OUTPUT_ARCH(riscv:rv64)
ENTRY(efi_main)

SECTIONS
{
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
        *(.got.plt)
        __data_end = .;
    }

    /* BSS immediately after .data — no gap, no section overlap */
    .bss : ALIGN(4K) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    __loader_end = .;

    /* PE base relocation table at fixed high address.
     * BSS is ~6.4MB starting at ~0x81000, so it ends ~0x6ED000.
     * 0x700000 gives a safe gap.  Scripts/gen_pe_reloc.py replaces
     * this stub with real IMAGE_REL_BASED_DIR64 entries after linking. */
    . = 0x700000;
    .reloc : ALIGN(4K) {
        KEEP(*(.reloc))
    }

    /* ELF dynamic sections — stripped by objcopy before PE conversion */
    .rela.dyn : ALIGN(4K) { *(.rela .rela.*) }
    .dynsym   : { *(.dynsym) }
    .dynamic  : { *(.dynamic) }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.gnu.hash)
        *(.hash)
        *(.dynstr)
        *(.riscv.attributes)
    }
}
