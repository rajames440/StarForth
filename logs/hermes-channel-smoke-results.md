# Hermes Channel Negotiation Smoke Test — Results

**Date:** 2026-06-27  
**Branch:** `lithosananke`  
**Script:** `tools/hermes_channel_smoke.sh`  
**Status:** PASS — 16/16

---

## Summary

Full channel negotiation lifecycle validated:
CH-REQUEST → CH-ACCEPT → business messaging → CH-CLOSE → HERMES-TICK (hot, no reap) → zero heat → HERMES-TICK (reap) → CH-ACTIVE = COMMON-CH only.

---

## Output

```
>>> [1] CH-REQUEST Hera->Hermes negotiate
>>> [2] CH-ACCEPT (Hermes creates ephemeral ch)
>>> [3] CH-STATE (expect 1=OPEN): 1
>>> [4] CH-ID (expect non-0): 2
>>> [5] CH-OWNER (expect 1=Hermes): 1
>>> [6] Y sends chanid to X on COMMON
>>> [7] COMMON-CH OPEN (expect 1): 1
>>> [8] business msg on ephemeral ch
>>> [9] CH-CLOSE
>>> [10] CH-STATE (expect 2=CLOSING): 2
>>> [11] HERMES-TICK (hot channel, no reap yet)
>>> [12] CH-STATE after tick (expect 2=CLOSING): 2
>>> [13] Zero heat + HERMES-TICK (reap)
>>> [14] COMMON-CH OPEN (expect 1): 1
>>> [15] CH-ACTIVE is COMMON-CH (expect 1): -1
>>> === Channel negotiation smoke complete ===
```

---

## Validation Results

| # | Check | Result |
|---|-------|--------|
| 1 | test started | PASS |
| 2 | CH-ACCEPT ok | PASS |
| 3 | ch state OPEN | PASS |
| 4 | ch id non-zero | PASS |
| 5 | ch owner Hermes | PASS |
| 6 | chanid sent | PASS |
| 7 | COMMON still OPEN | PASS |
| 8 | business msg | PASS |
| 9 | CH-CLOSE called | PASS |
| 10 | state CLOSING | PASS |
| 11 | tick no reap | PASS |
| 12 | still CLOSING after tick | PASS |
| 13 | zero+tick reap | PASS |
| 14 | COMMON still OPEN after | PASS |
| 15 | CH-ACTIVE is COMMON-CH | PASS |
| 16 | smoke complete | PASS |

**Result: 16 passed, 0 failed**

---

## Architecture Notes

**Protocol path confirmed:**

| Step | Actor | Action |
|------|-------|--------|
| 1 | Hera (0) | CH-REQUEST on COMMON → MSG-SEND with SPAWN-EVENT type |
| 2 | Hermes (1) | CH-ACCEPT → ephemeral channel allocated, ID=2, OWNER=1, STATE=OPEN |
| 3–5 | — | Verify ch fields: STATE=1, ID=2 (non-zero), OWNER=1 |
| 6 | Hermes | Sends ch ID to Hera on COMMON via MSG-SEND |
| 7 | — | COMMON-CH still OPEN after in-use |
| 8 | — | Business message on ephemeral channel |
| 9–10 | Hermes | CH-CLOSE → STATE becomes CH-CLOSING (2) |
| 11–12 | — | HERMES-TICK with heat > 0: channel not reaped, STATE still 2 |
| 13 | — | Zero heat manually; HERMES-TICK reaps ephemeral channel |
| 14–15 | — | COMMON-CH still OPEN; CH-ACTIVE = COMMON-CH only |

**CH-REAP-SAFE rebuild-the-list pattern:**  
Resets CH-ACTIVE to 0 at the start of the walk, then re-inserts non-reaped channels. Freed channels are simply skipped. This avoids the prior double-free bug where freed nodes remained linked in CH-ACTIVE.

**CH-ACCEPT stack fix:**  
`CH-ACTIVE !` → `DUP CH-ACTIVE !` — the original consumed the only copy of ch, returning an empty stack instead of `( ch )`.

**Block size constraint:**  
Test sequence split across blocks 9998 (helpers + VARIABLE MYCHAN + colon words) and 9999/9000 (two-part execution), each well under the 1024-byte limit. Block 9999 alone was 1159 bytes, causing silent truncation before steps [15] and "smoke complete".

**Compile-only constraint:**  
`IF THEN ELSE DO LOOP` are compile-only in StarForth — cannot appear in interpreted block context. All control flow in helpers is wrapped in colon word definitions (block 9998). The abort check `ASSERT-CH` wraps the `IF...BYE...THEN` idiom. `TRIP-ZERO-CH` wraps the zero-heat + HERMES-TICK sequence. Both are defined before `CD-INIT` so they are available when called from block 9999/9000.

---

## Bugs Fixed (prerequisite work)

| Bug | Description | Fix |
|-----|-------------|-----|
| H4 | `CH-REAP-SAFE` freed channel stayed in CH-ACTIVE → double-free on next tick | Rebuild-the-list: reset CH-ACTIVE=0, re-insert non-reaped channels only |
| H5 | `CH-ACCEPT` `CH-ACTIVE !` consumed the only copy of ch → empty stack return | `DUP CH-ACTIVE !` keeps ch on data stack |

---

## What This Does Not Test

- Multi-VM channel negotiation across separate process boundaries
- MBR (member) records within channels
- CH-MAX exhaustion (free list empty path)
- Concurrent channel lifecycle (multiple ephemeral channels)
