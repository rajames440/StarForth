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
 * elf64.h - ELF64 structures for kernel loading
 * Minimal ELF64 definitions for the UEFI loader
 */

#ifndef STARKERNEL_ELF64_H
#define STARKERNEL_ELF64_H

#include <stdint.h>

/* ELF identification */
#define EI_MAG0       0
#define EI_MAG1       1
#define EI_MAG2       2
#define EI_MAG3       3
#define EI_CLASS      4
#define EI_DATA       5
#define EI_VERSION    6
#define EI_OSABI      7
#define EI_ABIVERSION 8
#define EI_PAD        9
#define EI_NIDENT     16

/* ELF magic number */
#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

/* ELF class */
#define ELFCLASS32 1
#define ELFCLASS64 2

/* ELF data encoding */
#define ELFDATA2LSB 1  /* Little-endian */
#define ELFDATA2MSB 2  /* Big-endian */

/* ELF version */
#define EV_CURRENT 1

/* ELF OS/ABI */
#define ELFOSABI_NONE 0  /* UNIX System V ABI */

/* ELF types */
#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3
#define ET_CORE 4

/* ELF machine types */
#define EM_X86_64  62  /* AMD x86-64 */
#define EM_AARCH64 183 /* ARM AARCH64 */
#define EM_RISCV   243 /* RISC-V */

/* Program header types */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7

/* Program header flags */
#define PF_X 0x1 /* Execute */
#define PF_W 0x2 /* Write */
#define PF_R 0x4 /* Read */

/* Section header types */
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_HASH     5
#define SHT_DYNAMIC  6
#define SHT_NOTE     7
#define SHT_NOBITS   8
#define SHT_REL      9
#define SHT_SHLIB    10
#define SHT_DYNSYM   11

/* Relocation types (x86_64) */
#define R_X86_64_NONE     0
#define R_X86_64_64       1
#define R_X86_64_PC32     2
#define R_X86_64_RELATIVE 8
#define R_X86_64_32       10
#define R_X86_64_32S      11

/* Relocation types (aarch64) */
#define R_AARCH64_NONE     0
#define R_AARCH64_ABS64    257
#define R_AARCH64_RELATIVE 1027

/* Relocation types (riscv64) */
#define R_RISCV_NONE     0
#define R_RISCV_64       2
#define R_RISCV_RELATIVE 3

/* ELF64 types */
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

/* ELF64 header */
typedef struct {
    unsigned char e_ident[EI_NIDENT]; /* ELF identification */
    Elf64_Half    e_type;             /* Object file type */
    Elf64_Half    e_machine;          /* Machine type */
    Elf64_Word    e_version;          /* Object file version */
    Elf64_Addr    e_entry;            /* Entry point address */
    Elf64_Off     e_phoff;            /* Program header offset */
    Elf64_Off     e_shoff;            /* Section header offset */
    Elf64_Word    e_flags;            /* Processor-specific flags */
    Elf64_Half    e_ehsize;           /* ELF header size */
    Elf64_Half    e_phentsize;        /* Size of program header entry */
    Elf64_Half    e_phnum;            /* Number of program header entries */
    Elf64_Half    e_shentsize;        /* Size of section header entry */
    Elf64_Half    e_shnum;            /* Number of section header entries */
    Elf64_Half    e_shstrndx;         /* Section name string table index */
} Elf64_Ehdr;

/* ELF64 program header */
typedef struct {
    Elf64_Word  p_type;   /* Segment type */
    Elf64_Word  p_flags;  /* Segment flags */
    Elf64_Off   p_offset; /* Segment file offset */
    Elf64_Addr  p_vaddr;  /* Segment virtual address */
    Elf64_Addr  p_paddr;  /* Segment physical address */
    Elf64_Xword p_filesz; /* Segment size in file */
    Elf64_Xword p_memsz;  /* Segment size in memory */
    Elf64_Xword p_align;  /* Segment alignment */
} Elf64_Phdr;

/* ELF64 section header */
typedef struct {
    Elf64_Word  sh_name;      /* Section name (string table index) */
    Elf64_Word  sh_type;      /* Section type */
    Elf64_Xword sh_flags;     /* Section flags */
    Elf64_Addr  sh_addr;      /* Section virtual addr at execution */
    Elf64_Off   sh_offset;    /* Section file offset */
    Elf64_Xword sh_size;      /* Section size in bytes */
    Elf64_Word  sh_link;      /* Link to another section */
    Elf64_Word  sh_info;      /* Additional section information */
    Elf64_Xword sh_addralign; /* Section alignment */
    Elf64_Xword sh_entsize;   /* Entry size if section holds table */
} Elf64_Shdr;

/* ELF64 relocation with addend */
typedef struct {
    Elf64_Addr   r_offset; /* Address */
    Elf64_Xword  r_info;   /* Relocation type and symbol index */
    Elf64_Sxword r_addend; /* Addend */
} Elf64_Rela;

typedef struct {
    Elf64_Word  st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half  st_shndx;
    Elf64_Addr  st_value;
    Elf64_Xword st_size;
} Elf64_Sym;

/* ELF64 relocation without addend */
typedef struct {
    Elf64_Addr  r_offset; /* Address */
    Elf64_Xword r_info;   /* Relocation type and symbol index */
} Elf64_Rel;

/* Extract relocation symbol and type from r_info */
#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffffL)
#define ELF64_R_INFO(s,t) (((Elf64_Xword)(s) << 32) + ((Elf64_Xword)(t) & 0xffffffffL))

#endif /* STARKERNEL_ELF64_H */
