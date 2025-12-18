# QEMU Bring-Up (Milestone 0)

Use these targets after building `starkernel.efi` with `Makefile.starkernel`.

## amd64 (OVMF, serial on stdio)
```bash
make -f Makefile.starkernel ARCH=amd64 TARGET=kernel qemu
# Canonical invocation baked into the target:
# qemu-system-x86_64 \
#   -nodefaults -display none \
#   -machine q35,accel=tcg \
#   -m 1024 -smp 1 -cpu qemu64 \
#   -monitor none \
#   -chardev stdio,id=ser0,signal=off \
#   -device isa-serial,chardev=ser0,iobase=0x3f8 \
#   -chardev file,id=dbg,path=build/amd64/kernel/ovmf.log \
#   -device isa-debugcon,chardev=dbg,iobase=0x402 \
#   -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
#   -drive if=pflash,format=raw,file=build/amd64/kernel/OVMF_VARS.fd \
#   -drive format=raw,file=fat:rw:build/amd64/kernel/esp \
#   -no-reboot
# Guarantees:
# - Deterministic memory map under TCG
# - ExitBootServices() succeeds
# - Explicit COM1 for raw/early logging
# - No Secure Boot, stable CODE/VARS pairing
# Output shows serial console; expect StarKernel banner + memory map summary and
# “UEFI BootServices: EXITED”.
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
