# linker/

GNU ld linker scripts for the LithosAnanke UEFI kernel. Three ISAs × two
build variants (monolithic loader / split ELF kernel).

## Files

| Script | ISA | Use |
|--------|-----|-----|
| `starkernel-amd64.ld` | amd64 | Monolithic build (loader + kernel in one PE32+) |
| `starkernel-loader-amd64.ld` | amd64 | UEFI loader (split build) |
| `starkernel-loader-amd64-pe.ld` | amd64 | PE32+ format (split build) |
| `starkernel-kernel-amd64.ld` | amd64 | ELF kernel (split build) |
| `starkernel-loader-aarch64.ld` | aarch64 | UEFI loader (split build) |
| `starkernel-loader-aarch64-pe.ld` | aarch64 | PE32+ format (split build) |
| `starkernel-kernel-aarch64.ld` | aarch64 | ELF kernel (split build) |
| `starkernel-loader-riscv64.ld` | riscv64 | UEFI loader |
| `starkernel-kernel-riscv64.ld` | riscv64 | ELF kernel |

## See also

- [`Makefile.starkernel`](../Makefile.starkernel) — selects the correct script per `ARCH`
- [Project root](../README.md)
