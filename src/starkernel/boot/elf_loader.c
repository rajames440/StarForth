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

/* Simple memory operations (no libc) */
static void *elf_memset(void *s, int c, uint64_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

static void *elf_memcpy(void *dest, const void *src, uint64_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

/* Validate ELF header */
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

/* Load ELF segments into memory */
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

/* Apply relocations */
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
 * Load and relocate the StarKernel ELF binary
 *
 * @param elf_data Pointer to ELF file in memory
 * @param elf_size Size of ELF file in bytes
 * @param entry_out Output: kernel entry point address
 * @return 1 on success, 0 on failure
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
