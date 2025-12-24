/**
 * elf_loader.h - ELF64 kernel loader interface
 */

#ifndef STARKERNEL_ELF_LOADER_H
#define STARKERNEL_ELF_LOADER_H

#include "elf64.h"

/**
 * Load and relocate the StarKernel ELF binary
 *
 * @param elf_data Pointer to ELF file in memory
 * @param elf_size Size of ELF file in bytes (unused but for future validation)
 * @param entry_out Output: kernel entry point address
 * @return 1 on success, 0 on failure
 */
int elf_load_kernel(const uint8_t *elf_data, uint64_t elf_size,
                    Elf64_Addr *entry_out);

#endif /* STARKERNEL_ELF_LOADER_H */