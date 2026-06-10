# Next Session Plan

Generated at end of session `cdcb1844` (2026-06-10).
Branch at close: `claude/starforth-fb-terrm-ukdFL`

---

## What Was Accomplished This Session

- Fixed SysV ABI stack alignment bug in `kernel_entry.S` (`sub rax, 8`)
- Removed redundant kmalloc stack switch from `kernel_main.c`
- Fixed GOT/PE crash in `apply_sgr` — `FB_ANSI_PALETTE` got
  `__attribute__((visibility("hidden")))` in both `framebuffer.h` and
  `framebuffer.c`; EXEC-DOE now runs to full completion (160,673 rows)
- Added framebuffer host render test (`tools/fbtest.c`) and QEMU screendump
  (`scripts/qemu_screenshot.sh`) to the amd64 `qemu` target
- CLAUDE.md: one-process-at-a-time build rule
- Design doc: `docs/lithosananke/kernel-args/DESIGN.md` — UEFI
  command-line argument system (KernelArgs)

---

## Task 1 — Carry-over acceptance tests (do first, one at a time)

amd64 is green.  These two are still pending:

```bash
make -f Makefile.starkernel ARCH=aarch64 clean qemu
make -f Makefile.starkernel ARCH=riscv64  clean qemu
```

Commit logs after each run.  Do not proceed to any implementation task
until all three architectures have a clean `ok>` log committed.

---

## Task 2 — Memory size audit

Before tuning anything, measure the actual headroom:

- BSS stack (`g_kernel_stack`): currently 2 MB — log the deepest RSP
  observed during EXEC-DOE and compare against the stack base
- VM data stack / return stack depths — check `vm.h` for current limits
- Rolling window buffer (`ROLLING_WINDOW_SIZE=4096`) — verify it isn't
  being silently truncated under DoE load
- `KERNEL_HEAP_SIZE` (2 GB allocated, 1 GB physical in QEMU) — confirm
  kmalloc never hits the limit during a full DoE run

There are no hard rules on these sizes; increase freely if headroom is
tight.  Document findings in `docs/lithosananke/memory-audit.md`.

---

## Task 3 — Implement KernelArgs

Design doc: `docs/lithosananke/kernel-args/DESIGN.md`

Implementation order (one commit per step):

1. `include/starkernel/kernel_args.h` — struct + default macros
2. `include/starkernel/boot_info_offsets.h` — assembly-visible offsets
   with `_Static_assert` checks in C
3. `include/starkernel/uefi.h` — add `kernel_stack_base`,
   `kernel_stack_size`, `KernelArgs args` to `BootInfo`
4. `include/starkernel/cmdline.h` + `src/starkernel/boot/cmdline.c` —
   UCS-2→ASCII, tokeniser, parser
5. `src/starkernel/boot/uefi_loader.c` — call `cmdline_parse`, allocate
   dynamic stack via `AllocatePages`
6. `src/starkernel/arch/amd64/kernel_entry.S` — dynamic stack switch,
   fall back to BSS if `kernel_stack_base == 0`
7. `src/starkernel/kernel_main.c` — use `args.heap_size`, honour
   `args.run_doe` and `args.log_level`
8. `Makefile.starkernel` — pass `--doe` via OVMF boot entry; remove socat
   EXEC-DOE injection

Run all three arch acceptance tests after step 8.

---

## Task 4 — ISO real-hardware boot audit

The ISO built by xorriso works in QEMU but has a known gap for real
hardware (xorriso warns: "no directory /EFI/BOOT").  Real commodity UEFI
firmware expects `BOOTX64.EFI` present in the ISO *filesystem* at
`/EFI/BOOT/BOOTX64.EFI`, not just as an El Torito boot image.

Targets:
- **amd64** — commodity x86-64 machine: add `/EFI/BOOT/BOOTX64.EFI` to
  the ISO filesystem tree in the Makefile xorriso invocation
- **aarch64 / RPi4** — needs `BOOTAA64.EFI`; RPi4 requires the
  tianocore RPi4 UEFI firmware layer on the SD card/USB first; the ISO
  sits on top
- **aarch64 / BeagleBone AI-64** (aspirational) — same EFI path as RPi4

Document the verified boot procedure for each target in
`docs/lithosananke/hardware-boot.md`.

---

## Task 5 — `REBOOT` word

A FORTH word that optionally accepts a next-boot argument string and
performs a cold reset via UEFI Runtime Services.

Key design decisions (agreed this session):
- **Not interpreter-only at the C level** — no `STATE` check, no special
  flag
- **ACL-controlled** — the word-level ACL system (Task 6) governs access
- **On ACL denial**: emit warning, return to REPL; system keeps running
- **No abort** — consistent with how all sensitive words will work under ACL

Implementation sketch:
```forth
S" --doe --stack=8M" REBOOT   \ reboot with next-boot args
REBOOT                         \ plain cold reboot
```

Under the hood:
1. If a string is on the stack: write it to a UEFI variable
   (`StarForthNextArgs` under a custom GUID) via `RT->SetVariable`
2. Call `RT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL)`
3. Loader reads the variable on next boot, merges with `LoadOptions`;
   clears the variable after reading (one-shot)

`REBOOT` with no args is a plain cold reboot.

---

## Task 6 — `USE` word and resilient capsule loading

A general FORTH word (not capsule-specific) for loading external blobs.

```forth
S" hires-video.4th" USE
S" sound-card-1"    USE   \ fails → line gets commented out
S" sound-card-2"    USE
```

On failure, `USE`:
1. Locates the calling line in the block buffer
2. Prepends `\ ` to comment it out (writes to RAM block)
3. Continues — one failure does not stop subsequent `USE` calls
4. Up to 16 lines per block may be commented out

**Self-healing property:** over multiple boots the system converges to a
stable configuration.  Hardware that isn't present or drivers that don't
work are silently excluded.  Hardware that works is locked in.

`REBOOT` integrates naturally:
```forth
S" hires-video.4th" USE
REBOOT                      \ apply hardware init needing a cold reset
hires-video-init? NOT IF
    \ didn't take — USE mechanism will comment it out on next boot
THEN
```

---

## Task 7 — Forward word declarations

A FORTH analog of C prototypes: declare a word's name and stack effect
before its implementing capsule is loaded.  Enables circular dependencies
between capsules without load-order brittleness.

```forth
FORWARD: init-network  ( -- )    \ stub; filled when net capsule loads
FORWARD: video-mode?   ( -- f )  \ stub; filled when video capsule loads
```

If a forward-declared word is called before its definition arrives, it
emits a warning and returns without crashing.  When the implementing
capsule loads and defines the word, the stub is replaced.

This is a general VM feature — not kernel or capsule specific.

---

## Task 8 — Word-level ACL system (existing design, next to implement)

Design doc: `docs/03-architecture/word-acl/DESIGN.md`

This is already designed.  It is the policy engine that makes Tasks 5–7
safe.  Implement before or alongside Task 5 (`REBOOT`) since `REBOOT`
depends on ACL for its safety guarantees.

---

## Pointer for Next Session

Start with Task 1 (aarch64 + riscv64 acceptance).  Read this file and the
design docs listed above before touching any code.  Present the task order
to Captain Bob for approval before each implementation step.

All development stays on the working branch; never merge to master.
