# StarKernel Implementation Roadmap (UEFI Loader + ELF Kernel)

**Status:** Planning → Implementation  
**Last Updated:** 2025-12-24  
**Goal:** Boot StarKernel to `ok` prompt on **QEMU + real hardware** (amd64 + aarch64), with **real logging/strings/heartbeat** (no stubs).

## Non-Negotiables (Project Law)

1. **Boot path is: UEFI PE/COFF LOADER → loads ELF KERNEL**
    - Loader is `BOOTX64.EFI` / `BOOTAA64.EFI`.
    - Kernel is **ELF** and must be relocatable and loadable by our loader.
2. **ELF relocator is required** (REL/RELA as needed; handle common cases first).
3. **`__STARKERNEL__` build fence is mandatory** (clean separation from StarForth userspace build).
4. **amd64 + aarch64 are first-class**:
    - amd64: QEMU + at least one real x86_64 box.
    - aarch64: QEMU + Raspberry Pi 3/4 real hardware.
5. **Logging, strings, heartbeat must be FUNCTIONAL**:
    - No “pretend heartbeat,” no “stub console,” no “simulated logging.”
6. **QEMU + real hardware REQUIRED**:
    - If it “works in QEMU only,” it’s not done.
7. **Timer trust gating** is real:
    - QEMU timers are often garbage; determinism claims require proven time.
8. **LLM safety rule:**
    - LLMs may read/write files and run builds/tests.
    - **NO git commands** (`git status/add/commit/push/pull/checkout/rebase/reset/merge`) **without explicit approval**.

---

# Operating Procedure for LLM Work (applies to Claude, Codex, all)

## All LLMs (must follow)
- Do not run any `git` command. If you think you need one, STOP and ask Captain Bob.
- Make changes only in the working directory.
- Provide results as:
    - (a) a file list of what changed
    - (b) full file contents for any new/modified file
    - (c) exact build/test commands run + output summary
- Keep changes minimal and milestone-scoped.
- Never “refactor while you’re here.” No drive-by changes.

## Claude (preferred workflow)
- Work in small, verifiable increments.
- After each increment: build + run QEMU + capture serial output.
- Provide a short “What changed / Why / How verified” note.

## Codex (preferred workflow)
- Use for targeted implementation tasks:
    - ELF parsing/relocations
    - small C modules
    - test harnesses
    - build-system edits
- Codex outputs must be copy/paste-ready full files.

---

# Milestone 0 — Build Fencing + Artifact Split (Loader vs Kernel) (Week 1)

## Objective
Establish a hard separation:
- `starkernel_loader.efi` (PE/COFF UEFI app)
- `starkernel_kernel.elf` (ELF kernel image)

## Deliverables
- `make -f Makefile.starkernel ARCH=amd64 __STARKERNEL__=1 loader`
- `make -f Makefile.starkernel ARCH=amd64 __STARKERNEL__=1 kernel`
- `make -f Makefile.starkernel ARCH=amd64 __STARKERNEL__=1 iso`
- Same for `ARCH=aarch64` producing `BOOTAA64.EFI` + kernel ELF.
- Zero stubs for logging/strings.

## Key Requirements
- `__STARKERNEL__` fence:
    - Compiler defines `-D__STARKERNEL__`
    - Code guarded appropriately (no accidental StarForth host-mode behavior).
- Output layout:
    - `build/${ARCH}/kernel/loader/starkernel_loader.efi`
    - `build/${ARCH}/kernel/kernel/starkernel_kernel.elf`
    - ISO/image outputs in `build/${ARCH}/kernel/`

## Claude Instructions
- Confirm build products exist and are the correct formats:
    - `file starkernel_loader.efi` → PE32+/EFI
    - `file starkernel_kernel.elf` → ELF64 (or ELF for arch)
- Do NOT “convert ELF to PE.” Loader stays PE, kernel stays ELF.

## Codex Instructions
- Implement Makefile targets and output paths cleanly.
- Avoid cleverness; explicit is better than “DRY.”

## Exit Criteria
- Both artifacts build on amd64 and aarch64.
- Artifacts are correctly typed (PE/COFF loader + ELF kernel).

---

# Milestone 1 — Loader Boots + Real Console + Real Strings + Real Logging (Week 2)

## Objective
UEFI loader runs, initializes minimal runtime, and prints through a real console backend.
**Strings + logging must be real**.

## Requirements
- Implement a small string layer usable in both loader and kernel:
    - `strlen`, `memcpy`, `memset`, `vsnprintf` (or minimal snprintf subset)
- Implement a logging layer:
    - `klog(level, fmt, ...)` (or `log_*` macros)
    - Must work in loader first.
- Console output backend priority order:
    1. UEFI SimpleTextOutput (guaranteed early)
    2. Serial (UART) when available
- On amd64 QEMU: verify serial output on `-serial stdio`.
- On real hardware: verify at least UEFI text output; serial if available.

## Claude Instructions
- Ensure output is not stubbed:
    - demonstrate formatting works: hex values, pointers, line wrapping.
- Capture boot log transcripts for QEMU runs.

## Codex Instructions
- Write minimal `printf`-style formatting sufficient for:
    - `%s %c %d %u %x %p`
- Keep it freestanding; no libc.

## Exit Criteria
- Loader prints:
    - version banner
    - detected arch
    - “loading ELF kernel…” (even before it actually loads it)

---

# Milestone 2 — ELF Loader Core + Segment Mapping (Week 3)

## Objective
Loader reads `starkernel_kernel.elf` from the ESP / FAT and loads it into memory.

## Requirements
- Parse ELF header and program headers.
- Support at minimum:
    - `PT_LOAD` segments
    - alignment handling
    - zero-fill `.bss` via `memsz > filesz`
- Choose a load strategy:
    - either identity-map into physical addresses and jump,
    - or load to allocated pages and jump to virtual entry after page tables.
    - (Start simple; refine in Milestone 3.)

## Claude Instructions
- Verify ELF parsing is correct using diagnostics:
    - print entry point
    - print each LOAD segment VA/PA/offset/filesz/memsz/flags
- Do not attempt relocations yet unless required to boot.

## Codex Instructions
- Implement robust bounds checking:
    - reject malformed ELFs
    - reject unsupported classes/endianness/arch

## Exit Criteria
- Loader successfully loads the kernel segments and reaches a “ready to jump” state (even if jump still disabled).

---

# Milestone 3 — ELF Relocator + Jump Contract (Week 4)

## Objective
Implement **ELF relocation** sufficient for your kernel build mode and jump into `kernel_entry()` reliably.

## Requirements
- Decide kernel model:
    - **PIE-ish** kernel is ideal: relocations applied by loader.
- Implement relocation handling:
    - Start with amd64:
        - `R_X86_64_RELATIVE` first (often enough for PIE kernel)
        - then add others as needed (e.g., `GLOB_DAT`, `JUMP_SLOT`) if your link model requires.
    - aarch64 equivalents:
        - `R_AARCH64_RELATIVE` first.
- BootInfo contract:
    - Loader constructs a `BootInfo` struct and passes pointer in a defined register/ABI.
    - Document ABI clearly per arch.

## Claude Instructions
- Add a loader flag `--dry-run` equivalent (compile-time) that loads + relocates but does NOT jump, printing “relocs applied OK”.
- Only enable jump when dry-run is clean.

## Codex Instructions
- Implement relocation as a separate module:
    - `elf_relocate.c` with clean function boundaries.

## Exit Criteria
- Loader jumps into kernel and kernel prints its own banner using the kernel console backend.

---

# Milestone 4 — Kernel Early Init + Real UART Console (Week 5)

## Objective
Kernel owns the machine after ExitBootServices (where applicable) and prints to serial via real UART driver.

## Requirements
- On amd64:
    - UART 16550 at 0x3F8 for QEMU standard.
    - Confirm output on QEMU + at least one real machine (if UART exists; if not, fallback path must still log somewhere useful).
- On aarch64:
    - Pi3/Pi4: choose UART backend (PL011 vs miniUART) consistent with your boot flow.
    - Confirm serial on real Pi hardware with correct wiring and baud.

## Claude Instructions
- No stubs. If serial isn’t working yet, fail loudly and explain why.
- Provide the exact QEMU invocation used and transcripts.

## Codex Instructions
- Keep UART driver minimal:
    - init
    - putc
    - optional getc later

## Exit Criteria
- Kernel prints reliably on QEMU and real hardware (amd64 + aarch64).

---

# Milestone 5 — Heartbeat Is REAL (Timer + ISR + Output) (Week 6)

## Objective
Implement a heartbeat driven by a real timer interrupt, not a fake loop.

## Requirements
- Heartbeat must be interrupt-driven.
- On amd64:
    - Start with APIC timer or PIT depending on stability.
    - Expect QEMU timer pain; treat QEMU as “RELATIVE” unless validated.
- On aarch64:
    - Use ARM generic timer for tick source.
- Heartbeat output:
    - Print a concise marker every N ticks without flooding output.
    - Also maintain an in-memory counter.

## Trust Modes (Mandatory)
- ABSOLUTE: proven stable frequency + monotonic behavior within bounds
- RELATIVE: monotonic-ish but not trustworthy for determinism
- Determinism tests must be gated behind ABSOLUTE.

## Claude Instructions
- Implement trust gating and ensure it’s enforced.
- Log: source, calibration numbers, PASS/FAIL.

## Codex Instructions
- Write calibration as small modules with explicit error paths.
- Hard fail when validation fails.

## Exit Criteria
- Heartbeat runs on QEMU and real hardware.
- Trust mode printed clearly at boot.

---

# Milestone 6 — Physical + Virtual Memory Baseline (Week 7)

## Objective
Bring up PMM + minimal VMM sufficient for kernel heap and VM memory.

## Requirements
- Use UEFI memory map to build PMM.
- Establish kernel virtual map policy (identity vs higher-half) and keep it consistent.
- Provide `kmalloc` or bump allocator early.

## Claude Instructions
- No “it should work.” Add self-tests:
    - allocate/free pages
    - map/unmap translations

## Codex Instructions
- Keep APIs small and deterministic.
- Avoid premature abstractions.

## Exit Criteria
- Memory subsystem passes boot self-tests on QEMU + hardware.

---

# Milestone 7 — VM Integration (Primordial VM) (Week 8) — **EXPLICIT INSTRUCTIONS**

## Objective
Bring StarForth VM up **inside the kernel** using the HAL, and execute a tiny deterministic script.

## HARD RULES FOR THIS MILESTONE
- This is NOT the REPL milestone.
- This is NOT multi-VM.
- This is “VM runs in kernel context with kernel-backed HAL services.”

## Step-by-Step Implementation Plan

### 7.0 — Create a StarKernel/StarForth Boundary Layer
1. Create a dedicated integration directory:
    - `src/starkernel/vm/`
2. Create a single entrypoint:
    - `starkernel_vm_bootstrap.c`
3. Add a compile fence:
    - all VM-in-kernel compilation requires `__STARKERNEL__`.

**Goal:** No accidental host build contamination.

### 7.1 — Define the Minimal HAL Contract Needed by the VM
Implement (real) HAL hooks the VM needs:
- Memory:
    - `hal_mem_alloc(size)`
    - `hal_mem_free(ptr)`
- Console:
    - `hal_console_putc`
    - `hal_console_puts`
- Time:
    - `hal_time_now_ns()` (must be real; uses Milestone 5 timer)
- Heartbeat:
    - `hal_heartbeat_count()` (reads the real interrupt-driven counter)

**Rule:** No stubs. If something isn’t ready, the build should fail or the kernel should halt with a clear message.

### 7.2 — VM Boot Sequence (Primordial VM)
In `kernel_main()` after HAL init:
1. `vm = kmalloc(sizeof(*vm))`
2. `vm_init(vm, &hal)` (whatever your VM expects; adapt cleanly)
3. `vm_init_dictionary(vm)`
4. `vm_init_physics(vm)` (heat/window/cache)
5. Run a minimal deterministic script:
    - `1 2 + .`
6. Verify output includes `3` exactly.

### 7.3 — Add Kernel Words (Minimal Set Only)
Implement:
- `TICKS` → pushes `hal_time_now_ns()`
- `HB` → pushes heartbeat counter
- `YIELD` → no-op for now (returns immediately)

Test script:
- `TICKS . HB .`

### 7.4 — Determinism Safety
- If timer trust != ABSOLUTE:
    - VM may run, but determinism assertions are OFF.
    - Logs must say: “Timer trust RELATIVE — determinism checks disabled.”

### 7.5 — Verification Checklist (must be demonstrated)
- VM boots without accessing forbidden host services.
- Output is correct and formatted.
- Heartbeat counter increments independently of VM execution.
- Script execution result is exact.

## Claude Instructions (Milestone 7)
- DO NOT TOUCH git.
- Work in these micro-steps, each with build + QEMU run:
    1) add boundary layer scaffolding
    2) wire HAL calls
    3) boot VM and run `1 2 + .`
    4) add `TICKS/HB/YIELD`
- Provide:
    - files changed
    - build commands run
    - QEMU invocation
    - serial transcript showing `3`

## Codex Instructions (Milestone 7)
- Focus only on:
    - boundary layer file(s)
    - minimal glue code
    - keep changes surgical

## Exit Criteria
- On amd64 QEMU and at least one real x86_64 machine:
    - kernel boots
    - VM executes `1 2 + .` and prints `3`
- On aarch64 QEMU and real Pi:
    - same outcome (even if slower), with real heartbeat.

---

# Milestone 8 — REPL on Serial Console (Week 9)

(unchanged conceptually, but must use real UART + real line editing; no fake input)

Claude/Codex/All LLM instructions as above.

Exit Criteria:
- Boot to `ok`, accept input on serial, execute words.

---

# Milestones 9–18

(keep the structure from the original roadmap, but enforce the same rules:
- loader+ELF kernel stays
- trust gating stays
- real hardware validation stays
- no git commands by LLMs)

For each milestone:
- add explicit “Claude / Codex / All LLMs” blocks
- add “Proof of completion” requirements:
    - exact command lines
    - serial transcript
    - test output

---

# QEMU + Hardware Test Matrix (MANDATORY)

## amd64
- QEMU OVMF: required
- Real hardware: required

## aarch64
- QEMU: required
- Raspberry Pi 3/4: required

## riscv64
- QEMU only acceptable (for now)

---

# Repo Safety Policy (repeat, because LLMs forget)
- LLMs can have read/write filesystem access.
- **No git commands without approval.**
- If a git operation is needed, Captain Bob does it manually.

---

*This roadmap is living. Update status only after the proof artifacts exist (logs, transcripts, build outputs).*
