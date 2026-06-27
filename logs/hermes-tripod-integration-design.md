# Hermes Tripod Integration Test — Executive Summary

**Date:** 2026-06-27  
**Author:** Claude Code / Captain Bob (Robert Allan James)  
**Branch:** `lithosananke`  
**Status:** Design — not yet implemented

---

## What This Addresses

The Hermes smoke test confirms the messaging substrate in isolation. What it does
not exercise is the inter-VM dispatch path: `MSG-DELIVER → VM-EXEC → recipient handler`.
In the hosted build `VM-EXEC` is a no-op stub (`2DROP 2DROP`). This design replaces
that stub for the duration of the test with a working dispatcher, then drives all six
directional message paths through the Tripod and confirms each one with a stdout line.

No logs are kept. No persistent state changes. The test script patches `init.4th`,
runs, and restores — same pattern as `tools/hermes_smoke.sh`.

---

## Architectural Basis

From `TRIPOD.md` — dispatch is **capability-based**, never heat-based:

| Work type       | Correct recipient |
|-----------------|-------------------|
| VM lifecycle    | Hera              |
| Event emit/drain| Hermes            |
| Block I/O       | Artemis           |

`VM-EXEC` signature (from `MSG-DELIVER`, block 4107):

```
( paddr plen name-addr name-len -- )
```

`IDX>NAME` maps VM index → string:
- `0` → `"Hera"`   (len 4)
- `1` → `"Hermes"` (len 6)
- `2` → `"Artemis"` (len 7)

All three names have unique lengths. The hosted test dispatcher uses name length as
its discriminator — no `COMPARE` word is needed.

---

## The Six Message Paths

| # | Sender  | Recipient | Type          | Rationale (capability-based)           |
|---|---------|-----------|---------------|----------------------------------------|
| 1 | Hera    | Hermes    | SPAWN-EVENT   | Hera notifies Hermes: child was spawned |
| 2 | Hera    | Artemis   | 10 (arbitrary)| Hera requests persistence of boot record|
| 3 | Hermes  | Hera      | SPAWN-EVENT   | Hermes confirms delivery back to Hera   |
| 4 | Hermes  | Artemis   | 11 (arbitrary)| Hermes stores message log to Artemis    |
| 5 | Artemis | Hera      | 12 (arbitrary)| Artemis ACKs Hera boot record stored    |
| 6 | Artemis | Hermes    | 13 (arbitrary)| Artemis asks Hermes to route a result   |

After all six sends, `HERMES-TICK` runs once. All six messages cool one step
(heat: Q.1 → Q-DECAY). Then `0 FILL` heat on each and a second `HERMES-TICK`
reaps them. Final check: `MSG-ARENA MSG-TYPE@` must be 0 (arena clean).

---

## Exemplar: VM-EXEC Dispatcher Shim

This block replaces the no-op `VM-EXEC` for the duration of the test.
Name-length dispatch: 4=Hera, 6=Hermes, 7=Artemis.

```forth
( test VM-EXEC dispatcher — hosted only )
: VM-EXEC ( paddr plen name-addr name-len -- )
  >R >R 2DROP R> R>        ( drop payload, keep name )
  DUP 4 = IF DROP ." [Hera recv]"    CR EXIT THEN
  DUP 6 = IF DROP ." [Hermes recv]"  CR EXIT THEN
           DROP ." [Artemis recv]"   CR ;
```

Stack trace for the `4 =` branch:
- Entry: `( name-addr name-len )`
- `DUP 4 =` → `( name-addr name-len flag )`
- `IF DROP` → `( name-addr )` — then `." ..."` and `EXIT`, name-addr is dead on stack

Wait — name-addr leaks. The handler must also drop it:

```forth
: VM-EXEC ( paddr plen name-addr name-len -- )
  >R >R 2DROP R> R>
  DUP 4 = IF 2DROP ." [Hera recv]"    CR EXIT THEN
  DUP 6 = IF 2DROP ." [Hermes recv]"  CR EXIT THEN
             2DROP ." [Artemis recv]"  CR ;
```

`2DROP` consumes `( name-addr name-len )` after `DUP` copies len for the test.
Actually with `DUP 4 = IF`:

```
( name-addr name-len )
DUP →  ( name-addr name-len name-len )
4 =  → ( name-addr name-len flag )
IF   → ( name-addr name-len )   ← both still on stack inside IF
2DROP  ( -- )
." [Hera recv]" CR EXIT
```

That is correct. Remainder branch falls through with `( name-addr name-len )` still
on stack after the failed IF, so `2DROP` at the end of each branch is correct.

---

## Exemplar: Six-Path Test Sequence

```forth
( Hermes Tripod Integration Test )
CD-INIT
." >>> Tripod integration test" CR

( Path 1: Hera → Hermes )
." >>> [1] Hera->Hermes SPAWN-EVENT" CR
SPAWN-EVENT 0 1 S" boot" MSG-SEND

( Path 2: Hera → Artemis )
." >>> [2] Hera->Artemis store request" CR
10 0 2 S" boot-rec" MSG-SEND

( Path 3: Hermes → Hera )
." >>> [3] Hermes->Hera delivery confirm" CR
SPAWN-EVENT 1 0 S" ok" MSG-SEND

( Path 4: Hermes → Artemis )
." >>> [4] Hermes->Artemis log store" CR
11 1 2 S" msglog" MSG-SEND

( Path 5: Artemis → Hera )
." >>> [5] Artemis->Hera ACK" CR
12 2 0 S" stored" MSG-SEND

( Path 6: Artemis → Hermes )
." >>> [6] Artemis->Hermes route result" CR
13 2 1 S" result" MSG-SEND

." >>> HERMES-TICK (cool all 6)" CR
HERMES-TICK
." >>> HERMES-TICK ok" CR

( Zero heat on all 6 and reap )
MSG-ARENA MSG-SCAN !
6 0 DO
  0 MSG-SCAN @ MSG-HEAT!
  MSG-SCAN @ MSG-CELLS CELLS + MSG-SCAN !
LOOP
MSG-REAP
." >>> MSG-ARENA clean (expect 0): " MSG-ARENA MSG-TYPE@ . CR
." >>> === Tripod integration complete ===" CR
BYE
```

`MSG-SEND` calls `MSG-DELIVER` which calls `VM-EXEC`. With the test shim in place,
each send prints `[Hera recv]`, `[Hermes recv]`, or `[Artemis recv]` to stdout
immediately at send time — no async; hosted FORTH is synchronous.

---

## Expected Output

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

The test script greps for `>>>` lines to confirm the sequence completed.
The `[VM recv]` lines confirm dispatch fired on the correct path.

---

## Pass / Fail Criteria

| Check | Pass condition |
|-------|---------------|
| 6 `[VM recv]` lines appear in the right order | Hermes, Artemis, Hera, Artemis, Hera, Hermes |
| `HERMES-TICK` completes without error | Line `>>> HERMES-TICK ok` present |
| Arena clean after reap | `>>> MSG-ARENA clean (expect 0): 0` |
| Script exit code 0 | grep matched at least one `>>>` line |

---

## Block Size Note

The test shim + 6-path sequence shown above totals approximately 580 bytes combined.
Split into two smoke blocks as with the existing test (Block 9996 = shim + CD-INIT,
Block 9997 = 6 sends + tick + reap + BYE). Each well under 1024 bytes.

---

## What This Does NOT Test

- True async inter-process delivery (hosted FORTH is single-threaded; `VM-EXEC`
  fires synchronously inside `MSG-DELIVER`)
- K≡1.0 fleet accounting across VM boundaries (that requires the kernel Tripod
  with real separate VM instances and a fleet K observer)
- Channel negotiation (CH-REQUEST → CH-ACCEPT → ephemeral channel lifecycle)
- Hera lifecycle decisions driven by fleet K telemetry

Those belong to the kernel integration test (three-arch QEMU boot). This test
validates the Hermes dispatch substrate — that messages reach the named VM and the
thermal machinery cleans up correctly.

---

## Implementation Vehicle

`tools/hermes_tripod_smoke.sh` — same structure as `tools/hermes_smoke.sh`:

1. Back up `capsules/core/init.4th`
2. Concatenate: core stubs + VM-EXEC shim block + Hermes capsule + 6-path test blocks
3. Run `./build/amd64/standard/starforth --log-none 2>&1 | grep '^>>>'`
4. Restore `capsules/core/init.4th` on EXIT/INT/TERM

Say the word and this gets built.
