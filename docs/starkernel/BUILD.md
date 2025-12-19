# StarKernel Build Guide (Milestone 0)

Status: Initial bring-up for amd64 reference build; cross builds compile but remain stubbed.

## Prerequisites
- Toolchains: `x86_64-elf-gcc` (or native `gcc`), `aarch64-linux-gnu-gcc` (or `aarch64-none-elf-gcc`), `riscv64-unknown-elf-gcc` (or `riscv64-linux-gnu-gcc`).
- Firmware/VM tools: `ovmf` (OVMF_CODE/OVMF_VARS), `qemu-system-*`.
- Image utilities: `mtools`, `sfdisk`, `xorriso` (for ISO), `parted`/`mkfs.vfat` (for RasPi image scripts).

## Build Targets
- amd64 EFI binary (default host autodetect):
  ```bash
  make -f Makefile.starkernel ARCH=amd64 TARGET=kernel
  # Output: build/amd64/kernel/starkernel.efi
  ```
- aarch64 EFI binary (RasPi 4B):
  ```bash
  make -f Makefile.starkernel ARCH=aarch64 TARGET=kernel
  # Output: build/aarch64/kernel/starkernel.efi
  ```
- riscv64 EFI binary (QEMU virt):
  ```bash
  make -f Makefile.starkernel ARCH=riscv64 TARGET=kernel
  # Output: build/riscv64/kernel/starkernel.efi
  ```
- Convenience: `make -f Makefile.starkernel kernel-all` builds all three.

## Bootable Artifacts
- amd64 ISO:
  ```bash
  make -f Makefile.starkernel ARCH=amd64 TARGET=kernel iso
  # Output: build/amd64/kernel/starkernel.iso
  ```
- RasPi 4B SD image (requires firmware download in the helper script):
  ```bash
  make -f Makefile.starkernel ARCH=aarch64 TARGET=kernel rpi4-image
  # Output: build/aarch64/kernel/starkernel-rpi4.img
  ```

## Clean
```bash
make -f Makefile.starkernel clean-kernel
```

## Notes
- Build flags are freestanding (`-ffreestanding -nostdlib -fPIC`) with arch-specific tuning; see `Makefile.starkernel`.
- Early arch abstractions live in `include/starkernel/arch.h` with per-arch stubs under `src/starkernel/arch/<arch>/`. These will evolve as milestones progress.
