# tools/

Build-time utilities for the StarForth / LithosAnanke toolchain.

## Contents

| File | Purpose |
|------|---------|
| `mkcapsule.c` | Assembles `.4th` capsule files into `capsule_generated.c` baked into the kernel image. Invoked automatically by `Makefile.starkernel`. |
| `mkcapsule` | Compiled host binary (rebuilt on demand). |
| `fbtest.c` | Host framebuffer render test — VT100/framebuffer unit tests, no QEMU needed. |
| `pe_reloc_gen.py` | Generates PE32+ relocation tables for the UEFI loader. |

## mkcapsule

```bash
# Invoked automatically by the kernel build:
make -f Makefile.starkernel ARCH=amd64 clean qemu

# Manual invocation (for inspection):
./tools/mkcapsule capsules/ build/amd64/kernel/capsule_generated.c
```

Scans `capsules/` for `.4th` files, computes XXHash64 per capsule, assigns
type `(m)` to `init.4th` and `(p)` to everything else.

## See also

- [`capsules/README.md`](../capsules/README.md) — capsule file format and block namespace
- [`Makefile.starkernel`](../Makefile.starkernel) — kernel build system
- [Project root](../README.md)
