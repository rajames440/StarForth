#!/usr/bin/env python3
"""
pe_reloc_gen.py — Patch PE .reloc from ELF .rela.dyn

Usage: pe_reloc_gen.py <input.elf> <input.efi> <output.efi>

On aarch64, ld -shared generates R_AARCH64_RELATIVE entries in .rela.dyn.
GNU ld cannot produce PE output natively for aarch64, so we link to ELF
then objcopy to PE.  objcopy does not convert .rela.dyn to PE base
relocations, so the resulting .efi has an empty .reloc section and UEFI
cannot apply load-time fixups for function-pointer tables.

This script reads the .rela.dyn RELATIVE entries from the ELF, groups them
by 4 KB page, and writes IMAGE_REL_BASED_DIR64 (type 10) blocks into the
.reloc section of the PE.  The .reloc section must be pre-allocated large
enough to hold the generated data (see linker script).
"""

import struct, sys

SHT_RELA              = 4
R_AARCH64_RELATIVE    = 0x403  # 1027
IMAGE_REL_BASED_DIR64 = 10

def read_elf_relative(elf_data):
    """Return sorted list of VAs that need RELATIVE fixup."""
    if elf_data[0:4] != b'\x7fELF':
        raise ValueError("Not an ELF file")
    endian = '<' if elf_data[5] == 1 else '>'
    if elf_data[4] != 2:
        raise ValueError("Only ELF64 supported")

    (e_shoff,)     = struct.unpack_from(endian + 'Q', elf_data, 40)
    (e_shentsize,) = struct.unpack_from(endian + 'H', elf_data, 58)
    (e_shnum,)     = struct.unpack_from(endian + 'H', elf_data, 60)
    (e_shstrndx,)  = struct.unpack_from(endian + 'H', elf_data, 62)

    shstr_sh = e_shoff + e_shstrndx * e_shentsize
    (shstr_off,) = struct.unpack_from(endian + 'Q', elf_data, shstr_sh + 24)

    def sh_name(idx):
        (n,) = struct.unpack_from(endian + 'I', elf_data, e_shoff + idx * e_shentsize)
        return elf_data[shstr_off + n:].split(b'\x00')[0].decode()

    offsets = []
    for i in range(e_shnum):
        sh = e_shoff + i * e_shentsize
        (stype,)   = struct.unpack_from(endian + 'I', elf_data, sh + 4)
        (soffset,) = struct.unpack_from(endian + 'Q', elf_data, sh + 24)
        (ssize,)   = struct.unpack_from(endian + 'Q', elf_data, sh + 32)
        (sentsize,)= struct.unpack_from(endian + 'Q', elf_data, sh + 56)
        if stype == SHT_RELA and sentsize == 24:
            name = sh_name(i)
            if '.rela' not in name:
                continue
            for j in range(ssize // 24):
                entry = soffset + j * 24
                (r_off,)  = struct.unpack_from(endian + 'Q', elf_data, entry)
                (r_info,) = struct.unpack_from(endian + 'Q', elf_data, entry + 8)
                r_type = r_info & 0xFFFFFFFF
                if r_type == R_AARCH64_RELATIVE:
                    offsets.append(r_off)
    return sorted(set(offsets))


def build_pe_reloc(offsets):
    """Generate PE .reloc section bytes from list of RVAs."""
    if not offsets:
        return bytes(10)   # minimal empty block

    out = b''
    i = 0
    while i < len(offsets):
        page = offsets[i] & ~0xFFF
        entries = []
        while i < len(offsets) and (offsets[i] & ~0xFFF) == page:
            entries.append((IMAGE_REL_BASED_DIR64 << 12) | (offsets[i] & 0xFFF))
            i += 1
        if len(entries) % 2:
            entries.append(0)   # padding entry
        size = 8 + 2 * len(entries)
        out += struct.pack('<II', page, size)
        for e in entries:
            out += struct.pack('<H', e)
    return out


def patch_pe(pe_bytes, reloc_data):
    """Replace .reloc content and update the Data Directory entry."""
    data = bytearray(pe_bytes)

    (pe_off,) = struct.unpack_from('<I', data, 0x3C)
    assert data[pe_off:pe_off+4] == b'PE\x00\x00'

    (num_sections,)   = struct.unpack_from('<H', data, pe_off + 6)
    (opt_header_size,)= struct.unpack_from('<H', data, pe_off + 20)
    opt_off = pe_off + 24
    (magic,) = struct.unpack_from('<H', data, opt_off)
    assert magic == 0x20B, "PE32+ required"

    # DataDirectory[5] = Base Relocation Table
    dd_off = opt_off + 112
    reloc_va_prev, _ = struct.unpack_from('<II', data, dd_off)

    # Walk section headers
    sec_off = opt_off + opt_header_size
    for i in range(num_sections):
        s = sec_off + i * 40
        name = data[s:s+8].rstrip(b'\x00').decode('ascii', errors='replace')
        (vsize,)  = struct.unpack_from('<I', data, s + 8)
        (vaddr,)  = struct.unpack_from('<I', data, s + 12)
        (rawsize,)= struct.unpack_from('<I', data, s + 16)
        (rawoff,) = struct.unpack_from('<I', data, s + 20)
        if name == '.reloc':
            new_len = len(reloc_data)
            if new_len > rawsize:
                print(f"ERROR: .reloc section too small ({rawsize} bytes); need {new_len}",
                      file=sys.stderr)
                sys.exit(1)
            data[rawoff:rawoff + rawsize] = b'\x00' * rawsize
            data[rawoff:rawoff + new_len] = reloc_data
            struct.pack_into('<I', data, s + 8, new_len)          # VirtualSize
            struct.pack_into('<II', data, dd_off, vaddr, new_len) # DataDirectory
            return bytes(data)

    print("ERROR: .reloc section not found in PE", file=sys.stderr)
    sys.exit(1)


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input.elf> <input.efi> <output.efi>",
              file=sys.stderr)
        sys.exit(1)

    elf_path, efi_in, efi_out = sys.argv[1], sys.argv[2], sys.argv[3]

    with open(elf_path, 'rb') as f:
        elf_data = f.read()
    with open(efi_in, 'rb') as f:
        pe_data = f.read()

    offsets = read_elf_relative(elf_data)
    print(f"  {len(offsets)} RELATIVE relocations", flush=True)

    reloc_bytes = build_pe_reloc(offsets)
    print(f"  .reloc section: {len(reloc_bytes)} bytes", flush=True)

    patched = patch_pe(pe_data, reloc_bytes)

    with open(efi_out, 'wb') as f:
        f.write(patched)
    print(f"  Written: {efi_out}", flush=True)
