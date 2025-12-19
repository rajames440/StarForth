# Timer Policy (VM vs Bare Metal)

The kernel will always run with the best time source available, but will only make determinism claims when the hardware contract is provably satisfied.

## Overview

- VM path: no HPET/PIT MMIO; prefer invariant TSC for absolute time. Derive TSC Hz from CPUID 0x15/0x16 when available; otherwise fall back to ACPI PM Timer (port I/O) for a timebase. If invariant TSC is missing, default to RELATIVE mode unless strict is enabled.
- Bare metal path: calibrate TSC using HPET (primary) with optional PIT cross-check. Fail hard on non-convergence.
- Failure mode: panic/halt when invariants are not met in the chosen policy.

## Build Knobs

- `TIMER_VM_STRICT_INVARIANT_TSC=1`: In VM mode, require invariant TSC and fail hard if absent. Default is 0 (will fall back to PM Timer + RELATIVE trust if invariant TSC is missing).
- `TIMER_REQUIRE_PIT=1`: In bare-metal mode, require PIT for cross-check; otherwise PIT is optional.

## VM Mode Details

- HPET/PIT are skipped (avoid VM-exit MMIO). 
- Timebase derivation order:
  1) CPUID 0x15/0x16 for TSC Hz (absolute)
  2) ACPI PM Timer (port 0x408, 3.579545 MHz) to derive a timebase
- Trust levels:
  - `ABSOLUTE`: invariant TSC present + frequency derived. ABSOLUTE refers to *stable, invariant, frequency-known monotonic time suitable for internal determinism and drift bounds; it does not imply wall-clock correctness or UTC alignment.*
  - `RELATIVE`: invariant TSC missing OR TSC frequency not derivable; PM Timer used for monotonic ns (no determinism claims). RELATIVE mode guarantees monotonic progression but may have scale error and cross-CPU skew; values are unsuitable for frequency-sensitive or comparative measurements.
  - `NONE`: no usable timebase (should not persist after init)

## Bare Metal Details

- HPET mapped at 0xFED00000, used as primary reference.
- PIT is optional by default; required if `TIMER_REQUIRE_PIT=1`.
- Convergence uses windowed HPET (and PIT when available), CV gating, fail-hard on non-convergence.
- Drift check (bare metal only) compares TSC to HPET over a small window; panic on excessive drift.

## APIs

- `timer_init(BootInfo *boot_info)`: initializes according to VM vs bare-metal policy; sets trust level and calibration record.
- `timer_tsc_hz()`: returns locked TSC Hz (0 if RELATIVE mode without TSC freq).
- `timer_now_ns()`: returns ns based on TSC (ABSOLUTE) or PM Timer (RELATIVE).
- `timer_check_drift_now()`: bare-metal drift check (no-op in VM mode).
- `timer_calibration_record()`: exposes `timer_calibration_record_t` (trust, vm_mode, means, CVs, etc.).

## QEMU Notes

- For ABSOLUTE timing in VM mode, use a CPU model exposing invariant TSC and CPUID 0x15/0x16 (e.g., `-cpu host` under KVM, or `-cpu max` in TCG if modeled).
- With the default `-nodefaults` TCG setup used here, invariant TSC/CPUID freq may be absent; the code will enter RELATIVE mode using PM Timer.
- Enable `TIMER_VM_STRICT_INVARIANT_TSC=1` to fail hard when invariant TSC is missing.

## DoE/Determinism Stance

- ABSOLUTE trust is required for determinism claims; RELATIVE trust is sufficient for bring-up/logging but not for DoE assertions.
- VM RELATIVE mode is explicitly logged and should not be used to claim deterministic timing.
