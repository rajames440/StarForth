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
