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

/**
 * @brief PE/COFF Base Relocation Block header.
 *
 * Mirrors the @c IMAGE_BASE_RELOCATION layout from the PE specification:
 * - @c page_rva   — RVA of the 4 KB page that the following entries apply to.
 * - @c block_size — total byte size of this block including the header and
 *                   all entries (minimum 8 bytes for the header alone).
 * - @c entry      — one or more 16-bit relocation entries packed as
 *                   TYPE (4 bits) | OFFSET (12 bits); TYPE=0 / OFFSET=0
 *                   is a no-op padding entry used to 32-bit-align the block.
 *
 * Marked @c packed to suppress any padding the compiler might insert between
 * fields; @c aligned(4) ensures the block sits on a 4-byte boundary as
 * required by the PE loader.
 */
struct reloc_block {
    uint32_t page_rva;
    uint32_t block_size;
    uint16_t entry; /* TYPE=0, OFFSET=0 */
} __attribute__((packed, aligned(4)));

/**
 * @brief Minimal @c .reloc section stub to satisfy UEFI firmware loaders.
 *
 * Some UEFI implementations (notably certain hardware vendor firmware and
 * strict OVMF builds) refuse to load a PE32+ image that lacks a Base
 * Relocation Directory entry in the optional-header Data Directory. Even
 * for monolithic position-independent images that require no run-time fix-ups,
 * the presence of the @c .reloc section with a well-formed (but empty)
 * relocation block is required to pass the firmware's image validation.
 *
 * This single statically-allocated @c reloc_block satisfies the requirement:
 * - @c page_rva = 0 — applies to the first 4 KB page (convention for stubs).
 * - @c block_size = sizeof(reloc_block) — minimal well-formed block (8 bytes
 *   + 2 bytes for the single no-op entry = 10 bytes).
 * - @c entry = 0 — TYPE=0 (absolute, no-op) / OFFSET=0; ignored by the loader.
 *
 * The @c used attribute prevents the compiler/linker from discarding the
 * symbol as unreferenced; the @c section(".reloc") attribute places it
 * in the named PE section so the linker emits the correct Data Directory.
 */
__attribute__((section(".reloc"), used))
static const struct reloc_block reloc_stub = {
    .page_rva = 0,
    .block_size = sizeof(struct reloc_block),
    .entry = 0
};
