# Word-Level ACL System

**Status:** Design complete — implementation not yet started
**Target branch:** `master` first; `lithosananke` parity after
**Implementation file:** `capsules/ACL.4th`

---

## Summary

### Overview

A word-level access control system for StarForth VMs, implemented entirely
in FORTH as `ACL.4th`. No new C primitives carry policy logic — only two
C-level fields per `DictEntry` exist for the hot path (TTL counter + allow
bit). All policy, all adaptive logic, all inheritance rules live as colon
definitions in `ACL.4th`.

---

### ACL Disposition — Three States

Each word in the dictionary has one of three enforcement dispositions:

| State | Behavior |
|-------|---------|
| `STRICT` | ACL re-checked on every execution |
| `TTL` | ACL cached; re-checked only when TTL counter reaches zero |
| `PINNED` | Current mode and decision frozen permanently; one-way ratchet |

---

### TTL — Statistically Adaptive

The TTL is not a fixed value. It is derived and continuously updated from
the existing SSM execution physics:

- **Heat** (`execution_heat`): hotter words earn longer TTLs; check cost
  amortized over more executions
- **Rolling window**: sudden shift in caller pattern or access rate causes
  TTL to shrink aggressively
- **Decay**: quiescent words pull their TTL back down so the next burst
  re-validates early
- **Inference engine** (L5/L6): detects oscillating TTL and stabilizes it —
  same CV threshold logic already used for window-width inference

`PINNED` is the asymptote: the adaptive process converges, you thumbtack
it, and the accumulator freezes permanently.

A FORTH security word selects enforcement mode per word:

```forth
' MY-WORD ACL-STRICT      \ check every execution
' MY-WORD ACL-TTL-MODE    \ use adaptive TTL
' MY-WORD ACL-PIN         \ freeze — mode and decision immutable forever
```

---

### The Thumbtack / Pin Flag

`ACL-PIN ( xt -- )` is a one-way ratchet:

- Transitions: `STRICT → PINNED` or `TTL → PINNED` — never back
- Once pinned, `acl_mode` and `acl_ttl` are immutable for that VM context
- Any attempt to change a pinned word's ACL is silently ignored (or errors,
  depending on policy)
- Kernel primitive words (`BIRTH`, `EXEC`, `BYE`, etc.) are pinned by Mama
  at boot before the first `BIRTH` fires

---

### Inheritance at Birth

When a child VM is born:

```
acl_mode     = parent->acl_mode    \ policy propagates (STRICT or TTL)
acl_pinned   = 0                    \ always clear — child owns its own lock
acl_ttl      = default              \ reset; child has no execution history
acl_decision = ALLOW                \ re-evaluated on first access
```

Key properties:

- **Pin is contextual, not viral.** A parent's pinned words do not force
  the child to be pinned. The child inherits the mode as a starting point
  and can tighten, relax, or re-pin freely.
- **Security lattice flows downward at birth only.** After birth, each VM's
  ACL state is independent. A compromised child cannot bootstrap its way
  back to Mama's pinned ACLs.

---

### Interpreter Hook — Two-Level Check

The C interpreter reads only two fields per `DictEntry`:

```c
if (vm->emergency_console) goto execute;   /* 100% bypass — physical access */
if (entry->acl_ttl-- > 0) goto execute;    /* TTL good — single decrement   */
acl_recheck(vm, entry);                    /* TTL=0: call FORTH ACL-RECHECK  */
if (!entry->acl_allow) goto reject;
```

**Hot path cost:** one decrement and a branch — essentially free.
**Cold path:** calls `ACL-RECHECK` in `ACL.4th`, which recomputes the
adaptive TTL, updates `acl_allow`, and resets the counter. The C side
never reasons about policy; it only reads the result.

---

### Emergency Console — 100% Bypass

`vm->emergency_console` is a flag set by the kernel when the physical
`ok>` REPL is active. It is checked first, before any ACL table lookup.
Physical presence always wins. The emergency console is never subject to
ACL constraints regardless of what `ACL.4th` defines.

---

### ACL.4th — Pure FORTH Implementation

The ACL table is a `CREATE`d FORTH array indexed by execution token (XT).
`'` (tick) gives the XT of any word; XT is just a cell value usable as a
table key.

Core words:

| Word | Stack | Description |
|------|-------|-------------|
| `ACL-ENTRY` | `( xt -- addr )` | O(1) table lookup by XT |
| `ACL-MODE@` | `( xt -- mode )` | read enforcement mode |
| `ACL-MODE!` | `( mode xt -- )` | set mode (no-op if pinned) |
| `ACL-PINNED?` | `( xt -- flag )` | test pin bit |
| `ACL-PIN` | `( xt -- )` | set pin — one-way, irreversible |
| `ACL-STRICT` | `( xt -- )` | set STRICT mode (no-op if pinned) |
| `ACL-TTL-MODE` | `( xt -- )` | set TTL mode (no-op if pinned) |
| `ACL-TTL@` | `( xt -- n )` | read current TTL counter |
| `ACL-TTL!` | `( n xt -- )` | write TTL counter |
| `ACL-ALLOW@` | `( xt -- flag )` | read cached allow/deny decision |
| `ACL-ALLOW!` | `( flag xt -- )` | write allow/deny decision |
| `ACL-INHERIT` | `( src dst -- )` | birth inheritance: copy mode, clear pin, reset ttl+decision |
| `ACL-RECHECK` | `( xt -- )` | adaptive TTL recomputation; updates allow + new TTL |
| `ACL-INIT-PRIMITIVES` | `( -- )` | bulk-initialize ACL entries for all existing dictionary words |

Example pin at boot in `init.4th`:

```forth
' BIRTH  ACL-STRICT  ' BIRTH  ACL-PIN
' EXEC   ACL-STRICT  ' EXEC   ACL-PIN
' BYE    ACL-STRICT  ' BYE    ACL-PIN
```

---

### Bootstrap Sequence

```forth
S" ACL.4th" EXEC       \ load policy words and ACL-INIT-PRIMITIVES
ACL-INIT-PRIMITIVES    \ stamp default ACLs on every existing word
S" doe.4th" EXEC       \ subsequent loads get ACL entries via : hook
```

Every `:` definition after `ACL-INIT-PRIMITIVES` runs creates its own
ACL entry at definition time via a hook in the defining word. No word is
ever born without an ACL entry.

---

### Capsule Namespace

| File | Role |
|------|------|
| `init.4th` | Mama VM personality (default) |
| `init-*.4th` | Alternate personalities |
| `doe.4th` | DoE experiment harness |
| `ACL.4th` | Word-level ACL subsystem |
| `std-blob.4th` | Standard library layer (future) |

---

## Implementation Punch List

### Phase 1 — C Infrastructure (`master`)

- [ ] Add `acl_ttl` (`uint32_t`) to `DictEntry` in `include/vm.h`
- [ ] Add `acl_allow` (`uint8_t`) to `DictEntry`
- [ ] Add `acl_mode` (`uint8_t`: STRICT=0 / TTL=1) to `DictEntry`
- [ ] Add `acl_pinned` (`uint8_t`) to `DictEntry`
- [ ] Default-initialize all four fields in `word_register()`:
  `ttl=ACL_OPEN, allow=1, mode=TTL, pinned=0`
- [ ] Add `emergency_console` (`uint8_t`) flag to `VM` struct
- [ ] Insert two-level ACL check into interpreter loop in `vm.c`
- [ ] Implement `acl_recheck()` C stub — calls FORTH `ACL-RECHECK` when TTL=0
- [ ] Add `:` definition hook to create ACL entry for each newly defined word

### Phase 2 — `ACL.4th` (`master`)

- [ ] `CREATE ACL-TABLE` sized to max dictionary entries
- [ ] `ACL-ENTRY ( xt -- addr )` — XT-indexed O(1) lookup
- [ ] Field accessors: `ACL-MODE@`, `ACL-MODE!`, `ACL-PINNED?`,
  `ACL-TTL@`, `ACL-TTL!`, `ACL-ALLOW@`, `ACL-ALLOW!`
- [ ] `ACL-PIN ( xt -- )` — one-way ratchet; no-op if already pinned
- [ ] `ACL-STRICT ( xt -- )` — set STRICT mode; no-op if pinned
- [ ] `ACL-TTL-MODE ( xt -- )` — set TTL mode; no-op if pinned
- [ ] `ACL-INHERIT ( src-xt dst-xt -- )` — copy mode, clear pin,
  reset ttl+decision
- [ ] `ACL-RECHECK ( xt -- )` — recompute adaptive TTL from heat and rolling
  window; update `acl_allow` and reset counter
- [ ] `ACL-INIT-PRIMITIVES` — walk dictionary, create default entry per word
- [ ] Pin privileged words in `init.4th`: `BIRTH`, `EXEC`, `BYE`, and any
  other Mama-only words

### Phase 3 — Adaptive TTL Accumulator (`master`)

- [ ] `ACL-TTL-COMPUTE ( xt -- ttl )` — derive TTL from `execution_heat`
  and rolling window entropy
- [ ] `ACL-TTL-DECAY ( xt -- )` — decay TTL for quiescent words
- [ ] Hook `ACL-TTL-DECAY` into heartbeat tick path
- [ ] `ACL-TTL-STABILIZE ( xt -- )` — inference engine integration: detect
  oscillation, stabilize TTL (same CV threshold as L5/L6)
- [ ] Integration test: TTL grows on hot words; shrinks on anomalous access

### Phase 4 — Inheritance and Birth Integration (`master`)

- [ ] Call `ACL-INHERIT` from `BIRTH` for each dictionary entry copied to child
- [ ] Verify child can modify inherited (unpinned) ACL entries
- [ ] Verify child cannot modify pinned entries (inherited mode only)
- [ ] Test: compromised child cannot escalate to Mama's pinned ACLs

### Phase 5 — POST Tests and Formal Verification (`master`)

POST tests — new module `src/test_runner/modules/acl_words_test.c`,
registered in `src/test_runner/test_runner.c`, exercised at boot in POST
order alongside all other word categories:

- [ ] POST test: `ACL-PIN` is one-way — mode cannot change after pin set
- [ ] POST test: inheritance — child entry has mode copied, pin cleared,
  ttl and decision reset
- [ ] POST test: emergency console bypass — `emergency_console=1` skips
  ACL check entirely
- [ ] POST test: STRICT mode — ACL re-evaluated on every execution
- [ ] POST test: TTL hot path — TTL > 0 bypasses re-evaluation
- [ ] POST test: adaptive accumulator — heat increase produces TTL increase
- [ ] POST test: privileged words pinned at boot remain pinned after
  `ACL-INIT-PRIMITIVES`

Isabelle HOL formal verification — `.thy` files alongside existing VM
proofs:

- [ ] `ACL_Pin_Monotone.thy` — pin bit is set-only; no operation clears it
  once set
- [ ] `ACL_Inherit_Clears_Pin.thy` — `ACL-INHERIT` always produces an entry
  with `acl_pinned = 0` regardless of source entry state
- [ ] `ACL_TTL_Bounded.thy` — TTL counter is bounded above by
  `ACL-TTL-COMPUTE` output; cannot grow unboundedly
- [ ] `ACL_Emergency_Bypass.thy` — when `emergency_console = 1` the
  allow/deny decision is never consulted
- [ ] `ACL_No_Escalation.thy` — a child VM cannot produce a pinned entry
  with higher privilege than its inherited mode

### Phase 6 — LithosAnanke Parity

- [ ] Merge / port ACL subsystem to `lithosananke` branch
- [ ] Verify `ACL.4th` loads cleanly in kernel context (freestanding)
- [ ] `ACL-INIT-PRIMITIVES` runs at boot before first `BIRTH`
- [ ] `vm->emergency_console` wired to kernel REPL active flag
- [ ] Three-arch acceptance: amd64, aarch64, riscv64 boot to `ok>` with ACL
  active and no regressions
- [ ] Commit acceptance logs
