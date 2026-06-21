# src/starkernel/

LithosAnanke bare-metal UEFI kernel source — v1.5.3. Boots StarForth directly
on hardware (amd64, aarch64, riscv64) without an OS. Three ISAs share all files
except `arch/`.

## Top-level files

| File | Purpose |
|------|---------|
| `kernel_main.c` | Kernel entry point; drives milestones M0–M7 |
| `repl.c` | Kernel REPL (`ok>` / `zuse)ok>`) |
| `doe_log.c` | DoE tick log writer |

## Subdirectories

| Directory | Contents |
|-----------|----------|
| `arch/amd64/` | `arch.c` `apic.c` `timer.c` `interrupts.c` `boot.S` `isr.S` |
| `arch/aarch64/` | AArch64 arch support |
| `arch/riscv64/` | RISC-V 64 arch support |
| `boot/` | `uefi_loader.c` `elf_loader.c` `reloc_stub.c` `reloc.S` `cmdline.c` |
| `capsule/` | `capsule_birth.c` `capsule_run.c` `capsule_loader.c` `capsule_find.c` `capsule_validate.c` `capsule_vm_hooks.c` `mama_forth_words.c` |
| `hal/` | `hal.c` `console.c` `memory.c` `host_services.c` `framebuffer.c` `vt100.c` `font_8x16.c` |
| `memory/` | `kmalloc.c` `pmm.c` `vmm.c` |
| `math/` | `q48_16.c` — Q48.16 fixed-point (kernel build) |
| `hash/` | `xxhash64.c` — XXHash64 (content addressing) |
| `vm/` | Kernel VM subsystem (see below) |

## `vm/` subsystem

| File | Purpose |
|------|---------|
| `vm_core.c` | Interpreter loop (kernel build, `__STARKERNEL__`) |
| `vm_runtime.c` | Runtime support |
| `vm_bootstrap.c` | VM init for kernel |
| `parity.c` | Birth/execution parity logging |
| `arena.c` | Capsule arena allocator |
| `alloc_kernel.c` | Kernel memory allocator shim |
| `bootstrap/sk_vm_bootstrap.c` | Capsule pipeline bootstrap |
| `host/shim.c` | Hosted API shim (kernel side) |

## Boot sequence

```
UEFI Firmware → uefi_loader.c (BOOTX64.EFI)
    ExitBootServices()
    → kernel_main()
        M1: Console (UART 16550 + framebuffer)
        M2: PMM (physical memory bitmap)
        M3: VMM (4-level x86_64 paging)
        M4: IDT + APIC interrupts
        M5: TSC + HPET + APIC timer (100 Hz)
        M6: kmalloc heap
        M7: StarForth VM + capsule loading → zuse)ok>
```

## Acceptance test

The ONLY valid acceptance test is booting all three ISAs in QEMU:

```bash
make -f Makefile.starkernel ARCH=amd64   clean qemu
make -f Makefile.starkernel ARCH=aarch64 clean qemu
make -f Makefile.starkernel ARCH=riscv64 clean qemu
```

Each must reach `zuse)ok>` with POST PASSED.

## See also

- [`src/README.md`](../README.md) — parent directory
- [`linker/`](../../linker/README.md) — GNU ld linker scripts
- [`capsules/`](../../capsules/README.md) — capsule `.4th` files
- [`Makefile.starkernel`](../../Makefile.starkernel) — kernel build system
- [Project root](../../README.md)
