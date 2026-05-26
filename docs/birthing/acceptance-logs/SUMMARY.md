# Birthing — Hosted Build Acceptance Test Summary

**Date:** 2026-05-26
**Branch:** `birthing`
**Feature:** Lifecycle words (BIRTH KILL PAUSE RESUME USE) + S" FORTH-79 standard

## Scope

These logs cover the **hosted Linux build** only (`lfs/<arch>/starforth`).
The canonical acceptance test for LithosAnanke is QEMU UEFI boot via
`make -f Makefile.starkernel ARCH=<arch> clean qemu` — that requires OVMF
firmware and QEMU, which are not available in this build environment.
Kernel QEMU acceptance must be run by Captain Bob before merge to lithosananke.

## Hosted Build Results

| Arch | Tests | Passed | Failed | INIT | BIRTH Hermes | BIRTH Artemis |
|------|-------|--------|--------|------|-------------|---------------|
| amd64 | 800 | 729 | 0 | CLEAN | LOGGED | LOGGED |
| arm64 | 800 | 729 | 0 | CLEAN | LOGGED | LOGGED |
| riscv64 | 800 | 729 | 0 | CLEAN | LOGGED | LOGGED |

## Key Evidence (hosted build)

All three architectures show:

```
INIT: Starting system initialization from init.4th
INIT: Loaded 1 blocks from init.4th
INIT: Executing initialization blocks...
BIRTH Hermes (hosted)
BIRTH Artemis (hosted)
INIT: System initialization complete
```

## What Changed

- `src/starkernel/capsule/mama_forth_words.c` — BIRTH KILL START STOP USE kernel
  implementations; fixed caddr+1 offset bug (old counted-string convention) to
  align with FORTH-79 S" which pushes chars directly at caddr
- `src/word_source/string_words.c` — S" fixed to FORTH-79 standard ( -- c-addr u ),
  stores string at HERE automatically, no count-byte prefix
- `src/word_source/lifecycle_words_hosted.c` — hosted Linux stubs; BIRTH KILL PAUSE
  RESUME USE all ( c-addr u -- ), stack-clean, errors logged not pushed
- `src/main.c` — register_lifecycle_words(&vm) called after init_vm_and_subsystem
- `capsules/init.4th` — already has S" Hermes" BIRTH and S" Artemis" BIRTH

## Binaries Tested

| Arch | Binary | Notes |
|------|--------|-------|
| amd64 | `lfs/amd64/starforth` | Native x86-64, static, hosted Linux |
| arm64 | `lfs/arm64/starforth` | aarch64, static, hosted Linux |
| riscv64 | `lfs/riscv64/starforth` | riscv64, static, hosted Linux |

## Kernel QEMU Acceptance (pending — requires Captain Bob's environment)

```bash
make -f Makefile.starkernel ARCH=amd64   clean qemu
make -f Makefile.starkernel ARCH=aarch64 clean qemu
make -f Makefile.starkernel ARCH=riscv64 clean qemu
```

Expected serial output after kernel BIRTH fix (caddr+1 → caddr):
```
[Hera] BIRTH: Hermes live
[Hera] BIRTH: Artemis live
```
