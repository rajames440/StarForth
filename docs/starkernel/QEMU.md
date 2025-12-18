# QEMU Bring-Up (Milestone 0)

Use these targets after building `starkernel.efi` with `Makefile.starkernel`.

## amd64 (OVMF, serial on stdio)
```bash
make -f Makefile.starkernel ARCH=amd64 TARGET=kernel qemu
# Uses OVMF firmware, creates GPT disk with EFI partition and BOOTX64.EFI
# Output shows serial console; expect StarKernel banner + memory map summary.
```
GDB variant:
```bash
make -f Makefile.starkernel ARCH=amd64 TARGET=kernel qemu-gdb
# In another shell: gdb build/amd64/kernel/starkernel.elf -ex "target remote :1234"
```

## aarch64 (Raspberry Pi 3 emulation)
```bash
make -f Makefile.starkernel ARCH=aarch64 TARGET=kernel qemu
# Runs kernel as -kernel on raspi3b machine; serial on stdio.
```

## riscv64 (virt machine)
```bash
make -f Makefile.starkernel ARCH=riscv64 TARGET=kernel qemu
# Boots on virt with serial console; firmware defaults from host QEMU.
```

## Troubleshooting
- Missing OVMF: install `ovmf` (Debian/Ubuntu: `sudo apt install ovmf mtools`).
- Missing QEMU system binaries: install `qemu-system-x86 qemu-system-arm qemu-system-misc`.
- No serial output: ensure `-nographic` hosts stdout, and check toolchain produced `starkernel.efi`.
- If `mtools`/`sfdisk` are absent, install them; the amd64 QEMU target builds a GPT + FAT partitioned disk image on the fly.
