# Hermes Tripod Integration Test — Results

**Date:** 2026-06-27  
**Branch:** `lithosananke`  
**Commit:** `fe4b2442`  
**Script:** `tools/hermes_tripod_smoke.sh`  
**Status:** PASS — 16/16

---

## Summary

All six inter-VM message paths through the Tripod dispatched to the correct
recipient. HERMES-TICK ran clean. Arena was empty after reap.

---

## Output

```
>>> Tripod integration test
>>> [1] Hera->Hermes SPAWN-EVENT
[Hermes recv]
>>> [2] Hera->Artemis store request
[Artemis recv]
>>> [3] Hermes->Hera delivery confirm
[Hera recv]
>>> [4] Hermes->Artemis log store
[Artemis recv]
>>> [5] Artemis->Hera ACK
[Hera recv]
>>> [6] Artemis->Hermes route result
[Hermes recv]
>>> HERMES-TICK (cool all 6)
>>> HERMES-TICK ok
>>> MSG-ARENA clean (expect 0): 0
>>> === Tripod integration complete ===
```

---

## Validation Results

| # | Check | Result |
|---|-------|--------|
| 1 | test started | PASS |
| 2 | path1 header — `[1] Hera->Hermes SPAWN-EVENT` | PASS |
| 3 | path1 recv — `[Hermes recv]` | PASS |
| 4 | path2 header — `[2] Hera->Artemis store request` | PASS |
| 5 | path2 recv — `[Artemis recv]` | PASS |
| 6 | path3 header — `[3] Hermes->Hera delivery confirm` | PASS |
| 7 | path3 recv — `[Hera recv]` | PASS |
| 8 | path4 header — `[4] Hermes->Artemis log store` | PASS |
| 9 | path4 recv — `[Artemis recv]` | PASS |
| 10 | path5 header — `[5] Artemis->Hera ACK` | PASS |
| 11 | path5 recv — `[Hera recv]` | PASS |
| 12 | path6 header — `[6] Artemis->Hermes route result` | PASS |
| 13 | path6 recv — `[Hermes recv]` | PASS |
| 14 | HERMES-TICK ok | PASS |
| 15 | arena clean (expect 0): 0 | PASS |
| 16 | integration complete | PASS |

**Result: 16 passed, 0 failed**

---

## Architecture Notes

**Dispatch mechanism:** Name-length discriminator in `VM-EXEC-TRIPOD` —
Hera (len=4), Hermes (len=6), Artemis (len=7). No `COMPARE` word needed.

**Late-binding shim install:** `DEFER VM-EXEC` declared in `capsules/core/init.4th`.
The test wires `' VM-EXEC-TRIPOD IS VM-EXEC` from Block 9996 before Hermes loads.
This is the first live use of the new `DEFER`/`IS`/`DEFER@` C primitives (Module 27).

**DO loop in block content:** `DO...LOOP` is compile-only in StarForth.
`TRIP-ZERO-6` is compiled as a colon word at the top of Block 9997 so the
loop body compiles correctly before it is called.

**Routing order confirmed:**

| Path | Sender | Recipient | Type | `[VM recv]` |
|------|--------|-----------|------|------------|
| 1 | Hera | Hermes | SPAWN-EVENT (1) | `[Hermes recv]` |
| 2 | Hera | Artemis | 10 | `[Artemis recv]` |
| 3 | Hermes | Hera | SPAWN-EVENT (1) | `[Hera recv]` |
| 4 | Hermes | Artemis | 11 | `[Artemis recv]` |
| 5 | Artemis | Hera | 12 | `[Hera recv]` |
| 6 | Artemis | Hermes | 13 | `[Hermes recv]` |

---

## Bugs Addressed (prerequisite work)

| Bug | Description | Fix |
|-----|-------------|-----|
| H1 | `MSG-ALLOC` returned dirty nodes (free-list link in type field) | Zero node after allocation with `FILL` |
| H2 | `CH-COOL-ALL` only walked `CH-ACTIVE` list; raw `CH-ALLOC` nodes missed | Smoke test inserts CH1 into CH-ACTIVE before cooling |
| H3 | `MSG-SEND` had `DUP >R` leaving message address on data stack; field writes scrambled | Changed to `>R` |

---

## What This Does Not Test

- True async inter-process delivery (hosted FORTH is single-threaded; `VM-EXEC`
  fires synchronously inside `MSG-DELIVER`)
- K≡1.0 fleet accounting across real VM boundaries
- Channel negotiation (`CH-REQUEST → CH-ACCEPT → ephemeral channel lifecycle`)
- Hera lifecycle decisions driven by fleet K telemetry

Those belong to the kernel integration test (three-arch QEMU boot with
real separate VM instances). This test validates the Hermes dispatch substrate.
