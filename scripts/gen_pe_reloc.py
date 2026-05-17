#!/usr/bin/env python3
"""
gen_pe_reloc.py — Prepare a RISC-V ELF shared library for PE/EFI conversion.

Usage: gen_pe_reloc.py <input.elf> <patched.elf> <reloc.bin>

Problem:
  When the GNU linker builds a shared ELF with -shared at base 0, it leaves
  R_RISCV_RELATIVE target locations ZEROED.  The ELF dynamic linker fills them
  at load time via:  *target = base + r_addend
  But a PE IMAGE_REL_BASED_DIR64 entry does:  *target += delta
  so if *target starts at 0 the result is just delta, not addend + delta.

Fix (done here):
  1. Parse .rela.dyn to get all R_RISCV_RELATIVE (offset, addend) pairs.
  2. Pre-fill each target location in the ELF with its addend value.
     Now *target = addend, so PE relocation produces addend + delta. ✓
  3. Write the patched ELF to <patched.elf>.
  4. Build a PE IMAGE_BASE_RELOCATION table from the offsets and write
     it to <reloc.bin>.  This is later injected as the .reloc section.
"""
import struct
import sys
from collections import defaultdict


def parse_segments(elf_data):
    """Return list of (p_type, p_offset, p_vaddr, p_filesz) from PT_LOAD."""
    e_phoff     = struct.unpack_from('<Q', elf_data, 32)[0]
    e_phentsize = struct.unpack_from('<H', elf_data, 54)[0]
    e_phnum     = struct.unpack_from('<H', elf_data, 56)[0]
    segs = []
    for i in range(e_phnum):
        base = e_phoff + i * e_phentsize
        p_type   = struct.unpack_from('<I', elf_data, base)[0]
        p_offset = struct.unpack_from('<Q', elf_data, base + 8)[0]
        p_vaddr  = struct.unpack_from('<Q', elf_data, base + 16)[0]
        p_filesz = struct.unpack_from('<Q', elf_data, base + 32)[0]
        if p_type == 1:  # PT_LOAD
            segs.append((p_offset, p_vaddr, p_filesz))
    return segs


def vma_to_fileoff(segs, vma):
    """Convert an ELF VMA to a file offset using PT_LOAD segments."""
    for p_offset, p_vaddr, p_filesz in segs:
        if p_vaddr <= vma < p_vaddr + p_filesz:
            return p_offset + (vma - p_vaddr)
    return None


def parse_rela_dyn(elf_data):
    """Return list of (r_offset, r_addend) for R_RISCV_RELATIVE entries."""
    e_shoff     = struct.unpack_from('<Q', elf_data, 40)[0]
    e_shentsize = struct.unpack_from('<H', elf_data, 58)[0]
    e_shnum     = struct.unpack_from('<H', elf_data, 60)[0]
    e_shstrndx  = struct.unpack_from('<H', elf_data, 62)[0]

    shdr = []
    for i in range(e_shnum):
        base = e_shoff + i * e_shentsize
        shdr.append((
            struct.unpack_from('<I', elf_data, base)[0],
            struct.unpack_from('<I', elf_data, base + 4)[0],
            struct.unpack_from('<Q', elf_data, base + 24)[0],
            struct.unpack_from('<Q', elf_data, base + 32)[0],
        ))

    strtab_off = shdr[e_shstrndx][2]

    def sec_name(n):
        end = elf_data.index(b'\0', strtab_off + n)
        return elf_data[strtab_off + n:end].decode('ascii')

    pairs = []
    for sh_name, sh_type, sh_off, sh_sz in shdr:
        if sh_type == 4 and sec_name(sh_name) == '.rela.dyn':
            for j in range(0, sh_sz, 24):
                r_offset = struct.unpack_from('<Q', elf_data, sh_off + j)[0]
                r_info   = struct.unpack_from('<Q', elf_data, sh_off + j + 8)[0]
                r_addend = struct.unpack_from('<q', elf_data, sh_off + j + 16)[0]
                if (r_info & 0xFFFFFFFF) == 3:  # R_RISCV_RELATIVE
                    pairs.append((r_offset, r_addend))
    return pairs


def build_pe_reloc(offsets):
    """Build PE IMAGE_BASE_RELOCATION table from a list of RVA offsets."""
    pages = defaultdict(list)
    for off in sorted(offsets):
        pages[off & ~0xFFF].append(off & 0xFFF)
    out = b''
    for page_rva in sorted(pages.keys()):
        words = [(0xA << 12) | o for o in pages[page_rva]]  # DIR64 = 0xA
        if len(words) % 2:
            words.append(0)  # padding
        entries = struct.pack(f'<{len(words)}H', *words)
        out += struct.pack('<II', page_rva, 8 + len(entries)) + entries
    return out


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input.elf> <patched.elf> <reloc.bin>",
              file=sys.stderr)
        sys.exit(1)

    elf_data = bytearray(open(sys.argv[1], 'rb').read())

    assert elf_data[:4] == b'\x7fELF', "Not an ELF file"
    assert elf_data[4] == 2 and elf_data[5] == 1, "Need 64-bit LE ELF"

    segs  = parse_segments(elf_data)
    pairs = parse_rela_dyn(elf_data)

    patched = 0
    skipped = 0
    for r_offset, r_addend in pairs:
        file_off = vma_to_fileoff(segs, r_offset)
        if file_off is None:
            skipped += 1
            continue
        # Pre-fill the 8-byte slot with the addend so that
        # PE DIR64 relocation (+=delta) produces addend+delta.
        struct.pack_into('<q', elf_data, file_off, r_addend)
        patched += 1

    print(f"gen_pe_reloc: {len(pairs)} R_RISCV_RELATIVE entries, "
          f"{patched} patched, {skipped} skipped (outside LOAD segments)",
          file=sys.stderr)

    reloc = build_pe_reloc([off for off, _ in pairs])
    print(f"gen_pe_reloc: PE .reloc = {len(reloc)} bytes", file=sys.stderr)

    open(sys.argv[2], 'wb').write(elf_data)
    open(sys.argv[3], 'wb').write(reloc)
