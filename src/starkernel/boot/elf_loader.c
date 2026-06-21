/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

/**
 * elf_loader.c - ELF64 kernel loader for StarKernel
 *
 * Loads the StarKernel ELF binary from ESP, parses segments,
 * applies relocations, and jumps to entry point.
 */

#include "uefi.h"
#include "elf64.h"
#include "arch.h"

/**
 * @brief Fill a memory region with a byte value (freestanding memset).
 *
 * Writes @p n copies of the low byte of @p c to the memory starting at
 * @p s. Used by @c elf_load_segments() to zero-fill BSS regions
 * (@c p_memsz > @c p_filesz) without a libc dependency. The byte loop
 * is correct for all sizes; a future optimisation could use SIMD or
 * word-width stores for large BSS regions.
 *
 * @param s Pointer to the start of the destination region.
 * @param c Value to fill (only the low 8 bits are used).
 * @param n Number of bytes to write.
 * @return @p s (matches standard @c memset signature).
 */
static void *elf_memset(void *s, int c, uint64_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

/**
 * @brief Copy a non-overlapping memory region (freestanding memcpy).
 *
 * Copies @p n bytes from @p src to @p dest using a byte-at-a-time loop.
 * Behaviour is undefined if the source and destination ranges overlap.
 * Used by @c elf_load_segments() to copy each PT_LOAD segment's
 * @c p_filesz bytes from the ELF image in memory to its load address.
 *
 * @param dest Destination buffer; must not overlap with @p src.
 * @param src  Source buffer.
 * @param n    Number of bytes to copy.
 * @return @p dest (matches standard @c memcpy signature).
 */
static void *elf_memcpy(void *dest, const void *src, uint64_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

/**
 * @brief Validate an ELF64 header for the current target architecture.
 *
 * Checks the following fields in the ELF identification block and header:
 * - Magic bytes @c e_ident[0..3]: must be @c 0x7F 'E' 'L' 'F'.
 * - Class @c e_ident[EI_CLASS]: must be @c ELFCLASS64 (64-bit).
 * - Data encoding @c e_ident[EI_DATA]: must be @c ELFDATA2LSB
 *   (little-endian, the only byte order used by all three supported ISAs).
 * - Version @c e_ident[EI_VERSION]: must be @c EV_CURRENT (1).
 * - Type @c e_type: must be @c ET_EXEC (statically linked) or
 *   @c ET_DYN (position-independent executable).
 * - Machine @c e_machine: must match the build target —
 *   @c EM_X86_64 for @c ARCH_AMD64,
 *   @c EM_AARCH64 for @c ARCH_AARCH64,
 *   @c EM_RISCV for @c ARCH_RISCV64.
 *
 * @param ehdr Pointer to the ELF64 header at the start of the image.
 * @return 1 if the header is valid for this architecture, 0 otherwise.
 */
static int elf_validate_header(const Elf64_Ehdr *ehdr)
{
    /* Check magic number */
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3)
    {
        return 0; /* Not an ELF file */
    }

    /* Check class (64-bit) */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        return 0;
    }

    /* Check data encoding (little-endian) */
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        return 0;
    }

    /* Check version */
    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        return 0;
    }

    /* Check type (ET_EXEC or ET_DYN) */
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        return 0;
    }

    /* Check machine type matches architecture */
#if defined(ARCH_AMD64)
    if (ehdr->e_machine != EM_X86_64) {
        return 0;
    }
#elif defined(ARCH_AARCH64)
    if (ehdr->e_machine != EM_AARCH64) {
        return 0;
    }
#elif defined(ARCH_RISCV64)
    if (ehdr->e_machine != EM_RISCV) {
        return 0;
    }
#else
    #error "Unknown architecture"
#endif

    return 1;
}

/**
 * @brief Map all PT_LOAD segments from an ELF64 image into memory.
 *
 * Iterates the program header table (@c e_phnum entries starting at
 * offset @c e_phoff) and processes every entry with @c p_type == @c PT_LOAD:
 *
 * 1. Computes the destination address as @p load_base + @c p_vaddr.
 * 2. Copies @c p_filesz bytes from @p elf_data + @c p_offset using
 *    @c elf_memcpy().
 * 3. If @c p_memsz > @c p_filesz, zero-fills the remaining
 *    (@c p_memsz - @c p_filesz) bytes with @c elf_memset() to
 *    initialise the BSS region within the segment.
 *
 * The function does not check for address-range overlaps or that
 * destination memory is physically available; the UEFI loader is
 * responsible for allocating pages before calling @c elf_load_kernel().
 *
 * @param elf_data  Pointer to the start of the ELF64 file in memory.
 * @param load_base Base address at which to load the ELF image
 *                  (0 for @c ET_EXEC, 0x400000 for @c ET_DYN).
 * @return 1 always (errors are silently ignored at this milestone; a
 *         future revision should return 0 on a failed memory access).
 */
static int elf_load_segments(const uint8_t *elf_data, Elf64_Addr load_base)
{
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_data;
    const Elf64_Phdr *phdr = (const Elf64_Phdr *)(elf_data + ehdr->e_phoff);

    /* Iterate through program headers */
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            /* Calculate load address */
            uint8_t *dest = (uint8_t *)(load_base + phdr[i].p_vaddr);
            const uint8_t *src = elf_data + phdr[i].p_offset;

            /* Copy file contents to memory */
            elf_memcpy(dest, src, phdr[i].p_filesz);

            /* Zero out BSS (p_memsz > p_filesz) */
            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                elf_memset(dest + phdr[i].p_filesz, 0,
                          phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    return 1;
}

/**
 * @brief Apply all RELA relocation sections from an ELF64 image.
 *
 * Scans the section header table for sections of type @c SHT_RELA. For
 * each such section, resolves the associated symbol table (via
 * @c sh_link) and applies every relocation entry to the loaded image.
 *
 * Supported relocation types are ISA-specific and compile-time selected:
 *
 * - **amd64 (@c ARCH_AMD64)**:
 *   - @c R_X86_64_NONE — no-op.
 *   - @c R_X86_64_RELATIVE — absolute address = @c load_base + @c r_addend.
 *   - @c R_X86_64_64 — @c load_base + symbol value + @c r_addend (64-bit).
 *   - @c R_X86_64_32 / @c R_X86_64_32S — same, truncated to 32 bits.
 *
 * - **aarch64 (@c ARCH_AARCH64)**:
 *   - @c R_AARCH64_NONE — no-op.
 *   - @c R_AARCH64_RELATIVE — @c load_base + @c r_addend.
 *   - @c R_AARCH64_ABS64 — @c load_base + symbol + @c r_addend.
 *
 * - **riscv64 (@c ARCH_RISCV64)**:
 *   - @c R_RISCV_NONE — no-op.
 *   - @c R_RISCV_RELATIVE — @c load_base + @c r_addend.
 *   - @c R_RISCV_64 — @c load_base + symbol + @c r_addend.
 *
 * Returns 0 (failure) if a symbol index in a relocation entry is out
 * of bounds, if the symbol table entry size is zero, or if an unknown
 * relocation type is encountered. Returns 1 on success.
 *
 * @param elf_data  Pointer to the ELF64 file image.
 * @param load_base Load base address applied during @c elf_load_segments().
 * @return 1 on success, 0 on any relocation error.
 */
static int elf_apply_relocations(const uint8_t *elf_data, Elf64_Addr load_base)
{
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_data;
    const Elf64_Shdr *shdr = (const Elf64_Shdr *)(elf_data + ehdr->e_shoff);

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        const Elf64_Shdr *rela_sh = &shdr[i];
        if (rela_sh->sh_type != SHT_RELA) {
            continue;
        }
        if (rela_sh->sh_link >= ehdr->e_shnum) {
            return 0;
        }

        const Elf64_Shdr *symtab_sh = &shdr[rela_sh->sh_link];
        if (symtab_sh->sh_entsize == 0) {
            return 0;
        }

        const Elf64_Rela *rela = (const Elf64_Rela *)(elf_data + rela_sh->sh_offset);
        uint64_t num_rela = rela_sh->sh_size / sizeof(Elf64_Rela);
        const Elf64_Sym *symtab = (const Elf64_Sym *)(elf_data + symtab_sh->sh_offset);
        uint64_t sym_count = symtab_sh->sh_size / symtab_sh->sh_entsize;

        for (uint64_t j = 0; j < num_rela; j++) {
            uint32_t type = ELF64_R_TYPE(rela[j].r_info);
            uint32_t sym_index = ELF64_R_SYM(rela[j].r_info);
            uint64_t reloc_addr = load_base + rela[j].r_offset;
            uint64_t sym_value = 0;
            if (sym_index < sym_count) {
                sym_value = symtab[sym_index].st_value;
            } else if (sym_index != 0) {
                return 0;
            }

#if defined(ARCH_AMD64)
            switch (type) {
            case R_X86_64_NONE:
                break;
            case R_X86_64_RELATIVE: {
                uint64_t *target = (uint64_t *)reloc_addr;
                *target = load_base + rela[j].r_addend;
                break;
            }
            case R_X86_64_64: {
                uint64_t *target = (uint64_t *)reloc_addr;
                *target = load_base + sym_value + rela[j].r_addend;
                break;
            }
            case R_X86_64_32:
            case R_X86_64_32S: {
                uint32_t *target32 = (uint32_t *)reloc_addr;
                uint64_t value = load_base + sym_value + rela[j].r_addend;
                *target32 = (uint32_t)value;
                break;
            }
            default:
                return 0;
            }
#elif defined(ARCH_AARCH64)
            switch (type) {
            case R_AARCH64_NONE:
                break;
            case R_AARCH64_RELATIVE: {
                uint64_t *target = (uint64_t *)reloc_addr;
                *target = load_base + rela[j].r_addend;
                break;
            }
            case R_AARCH64_ABS64: {
                uint64_t *target = (uint64_t *)reloc_addr;
                *target = load_base + sym_value + rela[j].r_addend;
                break;
            }
            default:
                return 0;
            }
#elif defined(ARCH_RISCV64)
            switch (type) {
            case R_RISCV_NONE:
                break;
            case R_RISCV_RELATIVE: {
                uint64_t *target = (uint64_t *)reloc_addr;
                *target = load_base + rela[j].r_addend;
                break;
            }
            case R_RISCV_64: {
                uint64_t *target = (uint64_t *)reloc_addr;
                *target = load_base + sym_value + rela[j].r_addend;
                break;
            }
            default:
                return 0;
            }
#endif
        }
    }

    return 1;
}

/**
 * @brief Load, relocate, and return the entry point of a StarKernel ELF64 binary.
 *
 * Top-level ELF loader called by @c efi_main() (in the split-build path)
 * after @c ExitBootServices(). Performs three operations in sequence:
 *
 * 1. **Header validation** via @c elf_validate_header() — checks magic,
 *    class, data encoding, version, type, and machine-specific architecture.
 *    Returns 0 immediately on failure.
 *
 * 2. **Segment loading** via @c elf_load_segments() — maps all @c PT_LOAD
 *    segments to their target addresses, initialises BSS to zero.
 *    The @p load_base is:
 *    - 0 for @c ET_EXEC (absolute-address kernel; typical for StarKernel).
 *    - 0x400000 (4 MB) for @c ET_DYN (position-independent kernel).
 *
 * 3. **Relocation processing** via @c elf_apply_relocations() — applies
 *    @c SHT_RELA sections to patch in the correct runtime addresses.
 *    Returns 0 on any relocation error.
 *
 * On success, stores @c load_base + @c e_entry in @c *entry_out — the
 * virtual address of the kernel's C entry function @c kernel_main() — and
 * returns 1. The caller performs an indirect call to this address.
 *
 * @c elf_size is currently unused (reserved for future bounds-checking of
 * program-header and section-header offsets against the file boundary).
 *
 * @param elf_data   Pointer to the ELF64 file image in UEFI loader memory;
 *                   must remain valid until after the call returns.
 * @param elf_size   Size of the ELF image in bytes (currently unchecked).
 * @param entry_out  Output: receives the kernel entry point address on success.
 * @return 1 on success, 0 if validation, segment loading, or relocation fails.
 */
int elf_load_kernel(const uint8_t *elf_data, uint64_t elf_size,
                    Elf64_Addr *entry_out)
{
    (void)elf_size; /* Reserved for future bounds checking */
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_data;

    /* Validate ELF header */
    if (!elf_validate_header(ehdr)) {
        return 0;
    }

    /* Determine load base address */
    Elf64_Addr load_base = 0;
    if (ehdr->e_type == ET_DYN) {
        /* Position-independent: use a fixed base for now */
        load_base = 0x400000; /* 4MB */
    } else if (ehdr->e_type == ET_EXEC) {
        /* Absolute addresses: load at 0 (segments have absolute vaddr) */
        load_base = 0;
    }

    /* Load segments */
    if (!elf_load_segments(elf_data, load_base)) {
        return 0;
    }

    /* Apply relocations */
    if (!elf_apply_relocations(elf_data, load_base)) {
        return 0;
    }

    /* Calculate entry point */
    *entry_out = load_base + ehdr->e_entry;

    return 1;
}
