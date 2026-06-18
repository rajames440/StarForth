# Word-Level ACL System

**Status:** Implemented through Phase 7 — Phase 8 (PKI / thumbdrive) remaining
**Target branch:** `master` + `lithosananke` both complete through Phase 7
**Implementation files:** `capsules/ACL.4th`, `capsules/zuse.4th`, `src/word_source/acl_words.c`, `src/test_runner/modules/acl_words_test.c`

---

## Summary

### Overview

A word-level access control system for StarForth VMs, implemented as a
combination of a thin C infrastructure layer and a pure-FORTH policy capsule
(`ACL.4th`). The C layer provides four `DictEntry` fields for the hot path
(TTL counter + allow bit + mode + pin). All policy, all adaptive logic, all
inheritance rules live as colon definitions in `ACL.4th`. A bootstrap
superuser (`zuse.4th`) provides the sole authenticated escalation path.

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
if (vm->emergency_console) goto execute;   /* 100% bypass — physical access  */
if (vm->zuse_session)      goto execute;   /* 100% bypass — Zuse authenticated */
if (entry->acl_ttl-- > 0) goto execute;    /* TTL good — single decrement    */
acl_recheck(vm, entry);                    /* TTL=0: call FORTH ACL-RECHECK  */
if (!entry->acl_allow) goto reject;
```

**Hot path cost:** two flag checks + one decrement and a branch — essentially free.
**Cold path:** calls `ACL-RECHECK` in `ACL.4th`, which recomputes the
adaptive TTL, updates `acl_allow`, and resets the counter. The C side
never reasons about policy; it only reads the result.

---

### Two-Console Architecture

Two permanent, independent console layers exist at all times:

#### 1. Emergency Console (always present)

- `emergency_console` (`uint8_t`) flag in `VM` struct — C-only write; no
  FORTH word sets it
- Checked first in the interpreter hot path — 100% ACL bypass
- `acl_recheck()` uses save/restore around it for re-entrancy protection
- `EMERGENCY_CONSOLE_ENABLED` Makefile build flag (default=1): when set to
  0, strips interactive fallthrough; `vm_fault_handler()` weak symbol is
  called instead (override for hardware reset, watchdog, debug probe)
- `BYE` returns to the emergency console
- Only `panic` kills the VM
- Prompt: `ok>`

#### 2. Zuse Console (omnipresent, awaiting authentication)

- `zuse_session` (`uint8_t`) flag in `VM` struct — C-only write; no FORTH
  word sets it
- Prompt: `zuse)ok>`
- Full ACL bypass when a Zuse session is active
- Completely independent of `emergency_console` — setting one does not
  affect the other

---

### Superuser: Zuse

Named for Konrad Zuse, pioneer of programmable computers. Zuse is the
bootstrap superuser: the sole entity that can own the zuse console and mint
user credentials.

- Defined in `capsules/zuse.4th`, loaded by `ACL.4th` at capsule boot
- Software-only for now (no thumbdrive); `zuse.4th` is a capsule citizen
  from day one
- Zuse's words are pinned by `ACL-ZUSE-BOOT` at load time
- `ACL-BOOT` is called first, then `S" zuse.4th" EXEC` — both from within
  `ACL.4th` itself (Block 2067)
- Future: replace with thumbdrive PKI (Ed25519 challenge-response);
  `zuse.4th` becomes the bootstrapper that validates the physical drive

---

### CA Root

- Embedded in `ACL.4th` (Block 2066) as constants `ACL-CA-KEY-LO` /
  `ACL-CA-KEY-HI`
- Capsule hash = root-of-trust fingerprint: any CA key change changes the
  capsule hash and the birth protocol detects tampering
- Placeholder (zeros) until `tools/mkcapsule` embeds the real Ed25519 key
  at build time

---

### Security Model / No-Security Condition

Security is opt-in via `init.4th`. The toggle is one commented-out line:

```forth
\ S" ACL.4th" EXEC
```

| `ACL.4th` | `zuse.4th` | Result |
|-----------|------------|--------|
| absent    | absent     | No security — open dev mode |
| present   | absent     | ACL enforced; emergency REPL locked until Zuse provisioned |
| present   | present    | Full lockdown; Zuse owns the zuse console |

Uncomment the line in `init.4th` to enable full lockdown. Leave it
commented out for development builds.

---

### Self-Activating Capsule

`ACL.4th` is self-activating — `init.4th` only needs one line:

```forth
S" ACL.4th" EXEC
```

Internal structure of `ACL.4th`:
- **Block 2066**: CA root placeholder constants (`ACL-CA-KEY-LO` /
  `ACL-CA-KEY-HI`)
- **Block 2067**: Calls `ACL-BOOT` then `S" zuse.4th" EXEC`

`ACL-BOOT` stamps default ACL entries on all existing dictionary words.
After it runs, every subsequent `:` definition gets an ACL entry via the
defining-word hook. No word can exist without an ACL entry.

---

### Emergency Console — 100% Bypass

`vm->emergency_console` is set by the C layer when the physical `ok>` REPL
is active. It is checked first, before any ACL table lookup. Physical
presence always wins. The emergency console is never subject to ACL
constraints regardless of what `ACL.4th` defines.

The `EMERGENCY_CONSOLE_ENABLED=0` build flag strips the interactive
fallthrough for production or embedded builds, routing faults to
`vm_fault_handler()` instead.

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
\ In init.4th (one line, opt-in toggle — uncomment to enable):
S" ACL.4th" EXEC       \ self-activating: calls ACL-BOOT then loads zuse.4th

\ ACL.4th Block 2067 does internally:
ACL-BOOT               \ stamp default ACLs on all existing words
S" zuse.4th" EXEC      \ load and pin Zuse's words

\ Subsequent capsule loads (e.g. doe.4th) get ACL entries via : hook
S" doe.4th" EXEC
```

Every `:` definition after `ACL-BOOT` runs creates its own ACL entry at
definition time via a hook in the defining word. No word is ever born
without an ACL entry.

---

### Capsule Namespace

| File | Role |
|------|------|
| `init.4th` | Mama VM personality (default); contains opt-in ACL toggle |
| `init-*.4th` | Alternate personalities |
| `doe.4th` | DoE experiment harness |
| `ACL.4th` | Word-level ACL subsystem — self-activating; loads `zuse.4th` |
| `zuse.4th` | Bootstrap superuser personality; words pinned by `ACL-ZUSE-BOOT` |
| `std-blob.4th` | Standard library layer (future) |

---

## Implementation Punch List

### Phase 1 — C Infrastructure (`master`) ✅ COMPLETE

- [x] Add `acl_ttl` (`uint32_t`) to `DictEntry` in `include/vm.h`
- [x] Add `acl_allow` (`uint8_t`) to `DictEntry`
- [x] Add `acl_mode` (`uint8_t`: STRICT=0 / TTL=1) to `DictEntry`
- [x] Add `acl_pinned` (`uint8_t`) to `DictEntry`
- [x] Default-initialize all four fields in `word_register()`:
  `ttl=ACL_OPEN, allow=1, mode=TTL, pinned=0`
- [x] Add `emergency_console` (`uint8_t`) flag to `VM` struct
- [x] Add `zuse_session` (`uint8_t`) flag to `VM` struct
- [x] Insert two-level ACL check into interpreter loop in `vm.c`
- [x] Implement `acl_recheck()` in `vm.c` — calls FORTH `ACL-RECHECK` when
  TTL=0; save/restore `emergency_console` for re-entrancy protection
- [x] Add `:` definition hook to create ACL entry for each newly defined word

### Phase 2 — C Primitive Words + `ACL.4th` Capsule (`master`) ✅ COMPLETE

- [x] 12 C primitive words in `src/word_source/acl_words.c`
- [x] `CREATE ACL-TABLE` sized to max dictionary entries
- [x] `ACL-ENTRY ( xt -- addr )` — XT-indexed O(1) lookup
- [x] Field accessors: `ACL-MODE@`, `ACL-MODE!`, `ACL-PINNED?`,
  `ACL-TTL@`, `ACL-TTL!`, `ACL-ALLOW@`, `ACL-ALLOW!`
- [x] `ACL-PIN ( xt -- )` — one-way ratchet; no-op if already pinned
- [x] `ACL-STRICT ( xt -- )` — set STRICT mode; no-op if pinned
- [x] `ACL-TTL-MODE ( xt -- )` — set TTL mode; no-op if pinned
- [x] `ACL-INHERIT ( src-xt dst-xt -- )` — copy mode, clear pin,
  reset ttl+decision
- [x] `ACL-RECHECK ( xt -- )` — recompute adaptive TTL from heat and rolling
  window; update `acl_allow` and reset counter
- [x] `ACL-INIT-PRIMITIVES` — walk dictionary, create default entry per word
- [x] Pin privileged words: `BIRTH`, `EXEC`, `BYE`, and other Mama-only words

### Phase 3 — `capsules/ACL.4th` Self-Activating Capsule (`master`) ✅ COMPLETE

- [x] `ACL.4th` is self-activating: Block 2067 calls `ACL-BOOT` then
  `S" zuse.4th" EXEC`
- [x] Block 2066: CA root placeholder constants (`ACL-CA-KEY-LO` /
  `ACL-CA-KEY-HI`)
- [x] `ACL-BOOT` stamps default ACL entries on all existing dictionary words
- [x] `capsules/zuse.4th` loaded at end of `ACL.4th` capsule
- [x] Zuse's words pinned by `ACL-ZUSE-BOOT` at load time

### Phase 4 — `init.4th` Opt-In Toggle (`master`) ✅ COMPLETE

- [x] `init.4th` has one commented-out line as the opt-in toggle:
  `\ S" ACL.4th" EXEC`
- [x] Uncomment to enable full lockdown; leave commented for dev/open mode
- [x] No other changes to `init.4th` required — `ACL.4th` is self-contained

### Phase 5 — POST Tests (`master`) ✅ COMPLETE

POST tests implemented in `src/test_runner/modules/acl_words_test.c`,
registered in `src/test_runner/test_runner.c`. 800/800 passing.

- [x] POST test: `ACL-PIN` is one-way — mode cannot change after pin set
- [x] POST test: inheritance — child entry has mode copied, pin cleared,
  ttl and decision reset
- [x] POST test: emergency console bypass — `emergency_console=1` skips
  ACL check entirely
- [x] POST test: STRICT mode — ACL re-evaluated on every execution
- [x] POST test: TTL hot path — TTL > 0 bypasses re-evaluation
- [x] POST test: adaptive accumulator — heat increase produces TTL increase
- [x] POST test: privileged words pinned at boot remain pinned after
  `ACL-INIT-PRIMITIVES`

### Phase 6 — Isabelle/HOL Formal Verification (`master`) ✅ COMPLETE

Five `.thy` files in `proof/` alongside existing VM proofs:

- [x] `ACL_Pin_Monotone.thy` — pin bit is set-only; no operation clears it
  once set
- [x] `ACL_Inherit_Clears_Pin.thy` — `ACL-INHERIT` always produces an entry
  with `acl_pinned = 0` regardless of source entry state
- [x] `ACL_TTL_Bounded.thy` — TTL counter is bounded above by
  `ACL-TTL-COMPUTE` output; cannot grow unboundedly
- [x] `ACL_Emergency_Bypass.thy` — when `emergency_console = 1` the
  allow/deny decision is never consulted
- [x] `ACL_No_Escalation.thy` — a child VM cannot produce a pinned entry
  with higher privilege than its inherited mode

### Phase 7 — LithosAnanke Parity (`lithosananke`) ✅ COMPLETE (amd64)

- [x] Merge / port ACL subsystem to `lithosananke` branch
- [x] Verify `ACL.4th` loads cleanly in kernel context (freestanding); kernel
  build clean with `STARFORTH_ENABLE_VM=1`; EFI 730 KB, ELF 1 MB
- [x] `ACL-BOOT` runs at kernel boot before first `BIRTH` — `init.4th` has
  `S" ACL.4th" EXEC` active; ACL-BOOT stamps all words before capsule load
- [x] `vm->emergency_console` wired to kernel REPL active flag — `starkernel/repl.c`
  line 166: `active->emergency_console = active->zuse_session ? 0 : 1;`
- [x] `vm->zuse_session` wired to kernel Zuse console authentication path —
  ACL hot path in `vm.c` extended with `!vm->zuse_session` bypass at both
  call sites (execute_colon_word + outer interpreter loop)
- [x] `zuse_session` ACL bypass POST test (test 9) added and passing — 18/18
  ACL assertions, 981/981 hosted tests pass
- [x] Commit acceptance logs — `acceptance-logs/phase7-amd64.log`
- [ ] Three-arch parity: aarch64 + riscv64 cross-compilers not available in
  build environment — deferred to next hardware provisioning cycle

### Phase 8 — PKI / Thumbdrive Authentication (FUTURE)

- [ ] Ed25519 challenge-response authentication for Zuse thumbdrive
- [ ] `tools/mkcapsule` embeds real Ed25519 CA key into `ACL.4th` Block 2066
  at build time (replacing zero placeholder)
- [ ] User minting: admin creates thumbdrive with CA-signed certificate + home
  block image
- [ ] Lose the drive → admin mints a new one; no software recovery path by
  design
- [ ] `zuse.4th` becomes the bootstrapper that validates the physical drive
  before granting Zuse session access
