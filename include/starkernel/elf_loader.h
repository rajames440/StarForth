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