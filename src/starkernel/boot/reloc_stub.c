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
 * reloc_stub.c - Provide a minimal .reloc section for PE/COFF EFI loader
 *
 * Some firmware refuses to load images without a Base Relocation Directory.
 * This stub supplies an empty relocation block to satisfy the loader.
 */

#include <stdint.h>

struct reloc_block {
    uint32_t page_rva;
    uint32_t block_size;
    uint16_t entry; /* TYPE=0, OFFSET=0 */
} __attribute__((packed, aligned(4)));

__attribute__((section(".reloc"), used))
static const struct reloc_block reloc_stub = {
    .page_rva = 0,
    .block_size = sizeof(struct reloc_block),
    .entry = 0
};
