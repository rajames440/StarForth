# APIC Timer ISR Fix — amd64

**Date**: 2026-06-03  
**Branch**: `claude/starforth-architecture-review-oubN0`  
**Commits**: `fb0c256`, `901f827`

---

## Problem

The LithosAnanke amd64 kernel was triple-faulting on the first APIC timer interrupt,
causing QEMU to exit immediately after printing `ok>`. The adaptive heartbeat never
fired and `Heartbeat: N ticks` never appeared in serial logs.

## Root Cause

The kernel is compiled with `-fPIC -fvisibility=hidden` (required for UEFI PE
relocation). Assembly symbols `isr_stub_table` and `isr_stub0` in `isr.S` were
declared `.global` with **default visibility**. The C compiler therefore accessed
them through GOT indirection. However, the linker — building a final non-shared
binary — collapsed the GOT references to point directly at the symbols themselves.

The net effect: "load from GOT[`isr_stub_table`]" loaded the 8-byte instruction
bytes of `isr_stub0` (`0x006a90eb006a006a`) rather than its address. Both
`link_time_addr` and `runtime_addr` in `arch_interrupts_init()` became the same
wrong value (instruction bytes), so `reloc_offset = 0` and all 256 IDT entries
received garbage handler addresses. IDT[32] was set to `0x6a006afffffe91e9`
(non-canonical), which triggered #GP → double fault → triple fault on the first
timer delivery.

The bug was confirmed by disassembling `arch_interrupts_init()` in the loader PE:
the compiler emitted a `mov` (load from memory) for `&isr_stub0` instead of a
`lea` (compute address), reading the function's instruction bytes.

## Fix

Two changes, one line each:

**`src/starkernel/arch/amd64/isr.S`** — Added `.hidden` directives for all symbols
referenced from C:

```asm
.hidden isr_stub_table
.hidden isr_stub0
.hidden isr_stub32
.hidden isr_stub_default
.hidden isr_common_entry
```

**`src/starkernel/arch/amd64/interrupts.c`** — Added visibility attributes to the
`extern` declarations:

```c
extern void *isr_stub_table[IDT_ENTRIES] __attribute__((visibility("hidden")));
extern void isr_stub0(void) __attribute__((visibility("hidden")));
```

With hidden visibility, the compiler uses direct RIP-relative `lea` instead of GOT
indirection, yielding correct runtime addresses for all 256 IDT entries.

## Result

| Criterion | Before | After |
|---|---|---|
| IDT[32] handler | `0x6a006afffffe91e9` (corrupt) | `0x3d90251c` (valid canonical address) |
| First timer interrupt | #GP → triple fault → QEMU exit | ISR fires cleanly, returns via `iretq` |
| `PARITY:OK` | ✅ (unaffected) | ✅ |
| `Init: init.4th OK` | ✅ (unaffected) | ✅ |
| `ok>` REPL | ✅ reached, then immediate crash | ✅ runs indefinitely |
| `Heartbeat: N ticks` | Never | Every second at 100 Hz, TIME-TRUST ≈ 0.999 |

## Secondary Fix

`repl_print_dec32`, `repl_lapic_ccr`, `repl_print_hex32`, and `idle_iters` in
`repl.c` are x86-64 specific (LAPIC MMIO, `pushfq`). They were unguarded, causing
`-Werror=unused-function` / `unused-variable` failures on aarch64 (clang) and
riscv64 (gcc). All moved inside `#ifdef ARCH_AMD64`.

## Acceptance Testing

All three architectures must boot to `ok>` with `PARITY:OK` and `Init: init.4th OK`.
amd64 additionally shows `Heartbeat: N ticks TIME-TRUST≈0.999` repeating at 1 Hz.

```bash
make -f Makefile.starkernel ARCH=amd64   clean qemu
make -f Makefile.starkernel ARCH=aarch64 clean qemu
make -f Makefile.starkernel ARCH=riscv64 clean qemu
```

Verify each log:

```bash
grep "PARITY:OK"         logs/qemu-<arch>-*.log
grep "Init: init.4th OK" logs/qemu-<arch>-*.log
grep "ok>"               logs/qemu-<arch>-*.log
grep "Heartbeat:"        logs/qemu-amd64-*.log | head -5
```
