# Birthing — Kernel QEMU Acceptance Test Summary

**Date:** 2026-05-27
**Branch:** `birthing`
**Feature:** Lifecycle words (BIRTH KILL START STOP USE) + `."` compile-mode fix

## Scope

These logs cover the **kernel UEFI build** (`make -f Makefile.starkernel ARCH=<arch> clean qemu`).
All three architectures booted to `ok>`, ran POST, and executed `capsules/init.4th` successfully.

## Kernel QEMU Results

| Arch | Tests | Passed | Failed | Errors | BIRTH Hermes | BIRTH Artemis | `."` |
|------|-------|--------|--------|--------|-------------|---------------|------|
| amd64   | 800 | 734 | 0 | 0 | `[Hermes] Hermes Up` / live | `[Artemis] Artimis Up` / live | PASS |
| aarch64 | 800 | 734 | 0 | 0 | `[Hermes] Hermes Up` / live | `[Artemis] Artimis Up` / live | PASS |
| riscv64 | 800 | 734 | 0 | 0 | `[Hermes] Hermes Up` / live | `[Artemis] Artimis Up` / live | PASS |

## Key Evidence (all three architectures identical)

```
[Hera] POST: PASSED
[Hera] Init: loading init.4th...
[Hermes] Hermes UpBIRTH: Hermes live
[Artemis] Artimis UpBIRTH: Artemis live
[Hera] Init: init.4th OK
```

`."` in compile mode now uses the `(do-string)` inline runtime — string bytes live
inside the threaded code body, not at `body_addr`. The previous LIT-addr-LIT-len-TYPE
approach overwrote the start of the word body with string bytes, causing a triple fault
when `execute_colon_word` read them as DictEntry pointers.

## What Changed

- `src/word_source/io_words.c` — `."` compile mode replaced with `(do-string)` inline
  approach; `(do-string)` runtime word reads inline string via return-stack IP, prints,
  advances IP past padded block
- `src/starkernel/capsule/mama_forth_words.c` — BIRTH KILL START STOP USE kernel
  implementations; fixed caddr+1 offset bug to align with FORTH-79 S"
- `src/word_source/string_words.c` — S" fixed to FORTH-79 standard ( -- c-addr u )
- `src/word_source/lifecycle_words_hosted.c` — hosted Linux stubs (guarded #ifndef __STARKERNEL__)
- `src/main.c` — register_lifecycle_words(&vm) wired in for hosted build
- `capsules/init.4th` — S" Hermes" BIRTH + S" Artemis" BIRTH
- `capsules/hermes/init.4th` — stub: `: UP ." Hermes Up" ; UP`
- `capsules/artemis/init.4th` — stub: `: UP ." Artimis Up" ; UP`

## Serial Logs

| Arch | Log file |
|------|----------|
| amd64   | `logs/qemu-amd64-20260526-234957.log` |
| aarch64 | `logs/qemu-aarch64-20260526-235056.log` |
| riscv64 | `logs/qemu-riscv64-20260527-000127.log` |

(`logs/` is gitignored — run locally to reproduce)

## Remaining Interactive Tests (require Captain Bob)

- `S" Hermes" USE` — switches system console to Hermes
- `S" Hera" USE` — switches back
- KILL + re-BIRTH a VM in the same session
