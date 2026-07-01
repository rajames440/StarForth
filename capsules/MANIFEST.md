# capsules/MANIFEST.md — Block Ownership Registry
# StarshipOS / StarForth — Captain Bob (Robert Allan James)
# This document is authoritative for capsule block assignments.
# Any new capsule MUST claim a range here before first commit.
# Collisions are marked ⚠️ — resolve before production bake.

---

## How to Read This Document

Each entry lists:
- **File** — path relative to `capsules/`
- **Blocks** — LBN range(s) claimed
- **Immutable** — block is locked for life; reason given
- **Justification** — why each block or range lives where it does

The block namespace is shared across all loaded capsules. `mkcapsule.c` bakes
every `.4th` file into the kernel image. If two capsules claim the same block
and are loaded together, the later-loaded definition wins in block RAM.
Definitions already compiled into the dictionary are unaffected, but LOADing
that block again would give wrong results. Flag collisions; resolve before
production bake.

---

## Namespace Map (at a glance)

```
0    – 2047   RESERVED  kernel / dictionary / VM internal
2048 – 2099   init.4th  (Mama VM default personality)
2100 – 2199   doe.4th   (DoE Mama personality — never with init.4th)
2130 – 2132   init-4.4th (Mama variant)
2150          init-7.4th (Mama variant)
2160          init-8.4th (Mama variant)
2200 – 2201   init-0.4th (Mama variant)
2048 – 2063   init-6.4th / init-l8-omni.4th  ⚠️ (see below)
3000 – 3999   WORKLOAD  (exactly one workload capsule per DoE run)
  3001 – 3004   init-l8-volatile.4th
  3001 – 3003   init-l8-diverse.4th / init-l8-transition.4th
  3001 – 3002   init-l8-temporal.4th
  3001          init-l8-stable.4th
  3001 – 3060   init-1..3, init-9 (non-L8 workloads, 3001/3010/3020…)
4000 – 4007   ACL.4th (core)
4008 – 4009   UNASSIGNED
4010 – 4015   ACL.4th (RWT)
4016 – 4018   zuse.4th
4019 – 4049   UNASSIGNED
4050          lib.4th
4051 – 4054   UNASSIGNED
4055          common/msg.4th
4056 – 4059   UNASSIGNED
4060 – 4065   doe-campaign.4th
4066 – 4099   UNASSIGNED
4100 – 4109   hermes/init.4th
4110 – 4113   artemis/init.4th  ★ HARD LOCKED — never touch from Hermes
4114 – 4121   hermes/init.4th (continued)
4122 – 4127   artemis/init.4th (extended: allocator, magic, boot-detect, hdr-write, boot-seq, status)
4128          UNASSIGNED
4129          artemis/init.4th (entry block)
4130 – 4199   UNASSIGNED
4200 – 4204   compudynamics.4th
4205 – 4299   UNASSIGNED
4300 – 4301   process.4th
4302 – 4399   UNASSIGNED
4400 – 4405   fleet-k.4th
4406 +        OPEN (future capsules)
```

---

## Mama VM Personality Capsules

Mama VM is the root VM (Hera). Exactly one Mama personality is active at a time.
All entries in this section are mutually exclusive — they are never loaded together.

### `init.4th` — Default Mama personality

Blocks: **2049–2052, 2057**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 2049  | No        | Hera VM init: loads compudynamics, VM-INIT, lib, common:msg, fleet-k, process; BIRTHs Artemis + Hermes. VM-TREE + VM-CHILDREN implemented (`b52281b9`), unrolled for v1 Tripod |
| 2050  | No        | BOOT-BANNER call — separated so the banner block can be swapped without touching init logic |
| 2051  | No        | TRIPOD-TEST — 6 acceptance gates per TRIPOD.md. Dead EVENT-WAIT step replaced with HERMES-TICK liveness check (`3436d564`) |
| 2052  | No        | TRIPOD-TEST invocation block |
| 2057  | No        | BOOT-BANNER word definition (reusable across Mama variants) |

Note: 2048 is the kernel ramdrive entry point. `init.4th` starts at 2049 intentionally
(2048 = PERSONALITY block, loaded separately by the capsule birth protocol).

### `doe.4th` — DoE Mama personality

Blocks: **2049–2056**

⚠️ Overlaps `init.4th` blocks 2049–2052. Intentional: these are alternate Mama
personalities. `doe.4th` is active only during DoE campaigns; `init.4th` is
active for production Tripod boot. Never loaded together.

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 2049  | No        | DoE Mama init — different VM lifecycle for campaign harness |
| 2050  | No        | DoE configuration block |
| 2051  | No        | DoE workload dispatch |
| 2052  | No        | DoE loop control |
| 2053  | No        | DoE metrics collection hook |
| 2054  | No        | DoE CSV output words |
| 2055  | No        | DoE campaign state machine |
| 2056  | No        | DoE teardown / flush |

### `init-4.4th` — Mama variant (numbered)

Blocks: **2130–2132**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 2130  | No        | Mama variant init logic |
| 2131  | No        | Variant personality block 2 |
| 2132  | No        | Variant personality block 3 |

### `init-7.4th` — Mama variant (numbered)

Blocks: **2150**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 2150  | No        | Single-block Mama variant |

### `init-8.4th` — Mama variant (numbered)

Blocks: **2160**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 2160  | No        | Single-block Mama variant |

### `init-0.4th` — Mama variant (numbered)

Blocks: **2200–2201**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 2200  | No        | Mama variant init |
| 2201  | No        | Mama variant personality continuation |

### `init-6.4th` — Full-range Mama variant

Blocks: **2048–2063**

⚠️ Overlaps `init-l8-omni.4th` (same block range). These are alternate Mama
personalities for the L8 Jacquard omni mode vs. the plain full-range variant.
Never loaded together. See `init-l8-omni.4th` below.

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 2048–2063 | No   | Full Mama personality suite: 16 blocks covering init, loop control, L8 hooks, workload dispatch, banner, teardown |

### `init-l8-omni.4th` — L8 Jacquard omni Mama variant

Blocks: **2048–2063**

⚠️ Same range as `init-6.4th`. Intentional: `init-l8-omni.4th` is the L8-enabled
Mama variant that activates all Jacquard SSM modes. Mutually exclusive with
`init-6.4th`. Never loaded together.

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 2048–2063 | No   | L8 Jacquard omni Mama personality; mirrors init-6 range by design for structural alignment |

### `init-5.4th` — Raw code capsule (no block format)

Blocks: **none** (no `Block N` headers)

`init-5.4th` defines words inline without FORTH block formatting. It is a raw
code capsule compiled directly into the dictionary at load time. It does not
claim any LBN slot and cannot be LOADed by number. This is intentional for
volatile/chaos workloads where block-structured layout adds no value.

---

## Workload Capsules (3000–3999)

Exactly one workload capsule is active per DoE run. All capsules in this
section claim blocks in the 3001–3060 range. The block numbers are shared by
design: these are mutually exclusive workload personalities, never co-loaded.
The sparse layout (3001, 3010, 3020, …) leaves room for future per-workload
extension blocks without renumbering.

### `init-1.4th` — Non-L8 workload, 4 phases

Blocks: **3001, 3010, 3020, 3030**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 3001  | No        | Workload entry point (shared slot — workload capsules are mutually exclusive) |
| 3010  | No        | Phase 2 workload words |
| 3020  | No        | Phase 3 workload words |
| 3030  | No        | Phase 4 workload words |

### `init-2.4th` — Non-L8 workload, 5 phases

Blocks: **3001, 3010, 3020, 3030, 3040**

Same slot pattern as `init-1.4th` plus one additional phase block at 3040.

### `init-3.4th` — Non-L8 workload, 7 phases

Blocks: **3001, 3010, 3020, 3030, 3040, 3050, 3060**

Largest non-L8 workload. Fills the 3001–3060 span. No remaining headroom in
the current layout; next phase would require a new block (3070).

### `init-9.4th` — Non-L8 workload, 5 phases

Blocks: **3001, 3010, 3020, 3030, 3040**

Structurally identical slot pattern to `init-2.4th`; different workload content.

### `init-l8-stable.4th` — L8 stable mode workload

Blocks: **3001**

Single-block workload. Stable mode = low CV, minimal variability. One block suffices.

### `init-l8-volatile.4th` — L8 volatile mode workload

Blocks: **3001–3004**

Four blocks. Volatile mode generates high-CV execution patterns requiring more
definition space.

### `init-l8-diverse.4th` — L8 diverse mode workload

Blocks: **3001–3003**

Three blocks. Diverse mode cycles across word categories.

### `init-l8-temporal.4th` — L8 temporal mode workload

Blocks: **3001–3002**

Two blocks. Temporal mode generates time-varying access patterns.

### `init-l8-transition.4th` — L8 transition mode workload

Blocks: **3001–3003**

Three blocks. Transition mode exercises mode-switching boundary behavior.

---

## Infrastructure Capsules (4000+)

Infrastructure capsules are permanent system libraries. Multiple may be loaded
simultaneously. Block assignments in this range are non-overlapping except
where explicitly flagged.

### `ACL.4th` — Word-level ACL system

Blocks: **4000–4007, 4010–4015**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 4000  | Yes       | ACL constants: ACL-BASE-TTL, ACL-MAX-TTL, mode constants, allow constants. Immutable: changing these alters security semantics of all downstream capsules |
| 4001  | Yes       | ACL C-primitive declarations (ACL-ALLOW@ ACL-ALLOW! ACL-MODE@ etc.). Immutable: these are the C ABI boundary; renaming breaks every policy word |
| 4002  | Yes       | ACL-INIT-PRIMITIVES: initializes all dictionary entries to default ACL policy at boot. Immutable: this is the security baseline; patching it defeats boot-time policy |
| 4003  | Yes       | ACL-PIN / ACL-INHERIT / ACL-STRICT-MODE / ACL-PERMISSIVE-MODE. Immutable: pin is one-way; these are load-bearing policy operations |
| 4004  | Yes       | ACL-RECHECK / ACL-TTL-TICK / ACL-TTL-COMPUTE. Immutable: core enforcement path; changes here affect every word execution under ACL |
| 4005  | Yes       | ACL-BOOT-STRICT / ACL-BOOT-PERMISSIVE / self-activation sequence. Immutable: self-activation runs once at boot; must be stable |
| 4006  | Yes       | Emergency console bypass guard. Immutable: removing this bypass creates a reboot-blocking deadlock on fault |
| 4007  | Yes       | ACL.4th loads zuse.4th (S" zuse.4th" EXEC). Immutable: this is the root-of-trust chain; reordering breaks superuser bootstrap |
| 4010  | Yes       | ACL Rolling Window of Truth (RWT) — ring buffer constants and C-prim declarations. Immutable: RWT drives TTL computation; changes here invalidate ACL-RWT DoE data |
| 4011  | Yes       | ACL-RWT-SLOPE-COMPUTE. Immutable: slope computation is formally verified (ACL_TTL_Bounded.thy); any change requires new proof |
| 4012  | Yes       | ACL-TTL-COMPUTE-RW. Immutable: same formal verification constraint as 4011 |
| 4013  | Yes       | ACL-RECHECK-RW: pin-guarded rolling-window recheck. Immutable: enforcement path |
| 4014  | Yes       | ACL-RW-MODE: mode switching for RWT operation. Immutable: mode gate |
| 4015  | Yes       | ACL-BOOT-RW: boot-time RWT initialization. Immutable: runs once at boot |

### `zuse.4th` — Bootstrap superuser

Blocks: **4016–4018**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 4016  | Yes       | ZUSE-CERT-LO / ZUSE-CERT-HI constants. Immutable: capsule hash = root of superuser trust; changing breaks PKI chain |
| 4017  | Yes       | ACL-ZUSE-BOOT: authenticates session + pins zuse words. Immutable: this is the sole path to zuse_session=1; must not be alterable post-boot |
| 4018  | Yes       | Self-activation (ACL-ZUSE-BOOT). Immutable: runs once at boot |

### `lib.4th` — Shared serial output + FORTH aliases

Blocks: **4050**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 4050  | No        | N. COMMA CRLF CSV-COL CSV-LAST Q.SHOW USE RUN. Not immutable: these are utilities; adding words is safe. Removing or renaming existing words requires audit of all callers |

### `common/msg.4th` — Hermes participant interface

Blocks: **4055**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 4055  | Yes       | HERMES-ACK / HERMES-NACK. Immutable: this is the cross-VM ACK/NACK ABI. Every messaging VM (Hera, Hermes, Artemis) loads this at birth. Changing the block or the word names breaks the Hermes delivery protocol |

### `doe-campaign.4th` — DoE campaign harness

Blocks: **4060–4065**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 4060  | No        | Campaign entry point + seed initialization |
| 4061  | No        | Observation loop control |
| 4062  | No        | Per-tick metrics capture |
| 4063  | No        | CSV output formatting |
| 4064  | No        | Statistical reduction words |
| 4065  | No        | Campaign teardown + report trigger |

### `hermes/init.4th` — Hermes VM (messenger)

Blocks: **4100–4109, 4114–4121**

Gap 4110–4113 = Artemis. **HARD LOCKED. Never touch from Hermes side.**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 4100  | Yes       | Hermes constants: event codes, channel states, node sizes, arena sizes, MSG-DELIVERED. Immutable: changing node size constants corrupts all arena math |
| 4101  | Yes       | Arena CREATE: MSG-ARENA CH-ARENA MBR-ARENA; free-list roots; MSG-SEQ CH-ACTIVE. Immutable: CREATE allocates memory at compile time; re-running changes VM memory layout |
| 4102  | Yes       | MSG-INIT-FREE + CH-INIT-FREE. Immutable: free-list init is called once from CD-INIT; structure change breaks allocator |
| 4103  | Yes       | MBR-INIT-FREE + MSG-ALLOC + MSG-FREE-NODE. Immutable: allocator ABI; MSG-ALLOC is called by every MSG-SEND |
| 4104  | Yes       | CH-ALLOC + CH-FREE-NODE + MBR-ALLOC + MBR-FREE-NODE. Immutable: same allocator ABI constraint |
| 4105  | Yes       | Message field accessors (MSG-TYPE@/! through MSG-CH@/!). Immutable: field offsets are load-bearing; changing breaks every accessor caller |
| 4106  | Yes       | Channel + member accessors (CH-ID@/! through MBR-VM@). Immutable: same field offset constraint |
| 4107  | No        | VARIABLE MSG-LAST-MSG + IDX>NAME + MSG-DELIVER + MSG-SEND. Not fully immutable: IDX>NAME is hardcoded for 3-VM Tripod (known scope gap); MSG-DELIVER/SEND may evolve for async model |
| 4108  | No        | VARIABLE MSG-SCAN + MSG-COOL-ONE + MSG-COOL-ALL + MSG-TOTAL-HEAT + MSG-DELIVER-ALL. Not immutable: delivery and cooling logic may evolve |
| 4109  | No        | MSG-REAP. Ordering bug fixed (`8ca0cda2`) — type cleared before free-node prepend |
| —     | —         | 4110–4113: ARTEMIS. NEVER TOUCH. |
| 4114  | No        | VARIABLE CH-SCAN + CH-COOL-ALL + CH-TOTAL-HEAT. Not immutable: channel cooling may evolve |
| 4115  | No        | CH-REAP-SAFE. Not immutable: channel reaping logic may evolve |
| 4116  | No        | VARIABLE COMMON-CH + COMMON-INIT + HERMES-TICK + HERMES-K. Not immutable: HERMES-TICK drives the event loop; may grow |
| 4117  | No        | EVENT-EMIT + EVENT-WAIT + EVENT-DRAIN (backward compat). Not immutable: EVENT-WAIT is a diagnostic peek only per HERMES.md |
| 4118  | No        | HERA-NOTIFY-SPAWN + HERA-NOTIFY-KILL. Not immutable: notification path may expand |
| 4119  | No        | CH-MINT-ID + CH-REQUEST + CH-ACCEPT + CH-CONFIRM + CH-CLOSE. Not immutable: channel negotiation protocol may expand (multi-party invite deferred) |
| 4120  | No        | WELCOME + CD-INIT. Not immutable: CD-INIT init sequence may grow |
| 4121  | No        | MSG-ACK-LAST + MSG-NACK-LAST. Not immutable: NACK-requeue (reduced heat, retry) deferred; block may need extension |

### `artemis/init.4th` — Artemis VM (flat pool disk manager v2)

Blocks: **4110–4113** (core, hard locked) + **4122–4127** (extended) + **4129** (entry)

★ **HARD LOCKED (4110–4113).** Core Artemis block storage primitives.
They live in the middle of the Hermes range by deliberate layout choice
(Hermes owns 4100–4121; Artemis owns 4110–4113 as a protected island).
No other capsule may ever claim these blocks. No tool, script, or automated
process may modify these blocks without Captain Bob's explicit written
permission.

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 4110  | Yes       | Constants (ART-HDR-LBN, ART-FM-LBN, ART-DATA-LBN, ART-DATA-BLKS) + ART-K + ART-K+!/ART-K-!. Immutable: constants define the on-disk geometry; changing them corrupts existing disk images |
| 4111  | Yes       | LE32!/LE32@/LE64!/LE64@ — arch-neutral little-endian byte I/O. Immutable: on-disk format depends on these; changing byte order corrupts cross-arch images |
| 4112  | Yes       | SHL1N/SHR1N/BIT-TEST/BIT-SET/BIT-CLR — bit manipulation. Immutable: free map correctness depends on these primitives |
| 4113  | Yes       | FM-ADDR-BIT/FM-TEST/FM-SET/FM-CLR — free map core. Immutable: free map ABI; changing breaks allocator |
| 4122  | No        | FM-FIND-FREE/BLK-ALLOC/BLK-FREE. Not immutable: scan strategy may improve (e.g. hint-based) |
| 4123  | No        | ART-MAGIC!/ART-MAGIC?/ART-BLANK? — disk detection primitives. Not immutable: magic or blank check may extend |
| 4124  | No        | ART-BOOT-DETECT/BLK-FETCH/BLK-PERSIST/ART-FLUSH. Not immutable: boot mode may gain additional cases |
| 4125  | No        | ART-HDR-WRITE — on-disk header format. Not immutable: header layout may extend |
| 4126  | No        | ART-FORMAT/ART-RESUME/ART-HALT-UNRECOG/ART-INIT — boot sequence. Not immutable: resume path will grow |
| 4127  | No        | ART-STATUS/ART-SELF-TEST/WELCOME. Not immutable: status/test may expand |
| 4129  | No        | Entry block — ART-INIT ART-SELF-TEST WELCOME. Not immutable: may grow |

### `compudynamics.4th` — Compudynamic physics primitives

Blocks: **4200–4204**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 4200  | Yes       | Q.1 constant (65536 = Q48.16 representation of 1.0). Immutable: Q.1 is the birth-heat value for every compudynamic object; changing it invalidates K≡1.0 invariant |
| 4201  | Yes       | Q.* (Q48.16 multiply) + VM-DECAY-ONE. Immutable: Q.* is the heat cooling primitive; changing it alters thermodynamic behavior globally |
| 4202  | Yes       | CD-TICK + VM-INIT. Immutable: CD-TICK drives the per-VM thermal cycle; signature is part of VM birth protocol |
| 4203  | Yes       | K≡1.0 accounting words. Immutable: fleet K invariant enforcement |
| 4204  | Yes       | Q-DECAY constant (65208). Immutable: this constant is the cooling rate shared by all compudynamic objects (messages, channels, VMs); it is measured and formally consistent |

### `process.4th` — VM process management

Blocks: **4300–4301**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 4300  | No        | Event code constants + SPAWN + PAUSE lifecycle operators. Not immutable: process model may evolve |
| 4301  | No        | RESUME + KILL-VM + CD-PHASE@. Double K-KILL-HOOK bug fixed (`182a8a23`). Not immutable: process model may evolve |

### `fleet-k.4th` — Fleet K≡1.0 accounting

Blocks: **4400–4405**

| Block | Immutable | Justification |
|-------|-----------|---------------|
| 4400  | Yes       | K-FLEET variable + K-INIT. Immutable: K-FLEET is the global conservation ledger; renaming breaks all K accounting |
| 4401  | Yes       | K-SPAWN-HOOK + K-KILL-HOOK. Immutable: these are the VM lifecycle hooks called by Hermes on BIRTH/KILL; signature must be stable |
| 4402  | Yes       | K-STATUS + K-CONSERVED?. Immutable: K-CONSERVED? is the TRIPOD-TEST acceptance gate |
| 4403  | No        | K drift detection. Not immutable: tolerance values may be tuned |
| 4404  | No        | K-FLEET integration (wiring to HERMES-K deferred per HERMES.md G8). Not immutable: pending async inter-VM return value support |
| 4405  | No        | K diagnostic words. Not immutable |

---

## Unassigned Ranges

| Range       | Status      | Notes |
|-------------|-------------|-------|
| 0–2047      | KERNEL      | VM internal; never use in capsules |
| 2064–2099   | UNASSIGNED  | Available for init.4th extension or new Mama variants |
| 2100–2129   | doe.4th     | Owned by doe.4th (blocks 2100–2199 per CLAUDE.md; only 2049–2056 currently used) |
| 2133–2149   | UNASSIGNED  | Between init-4 and init-7 variants |
| 2151–2159   | UNASSIGNED  | Between init-7 and init-8 variants |
| 2161–2199   | UNASSIGNED  | Between init-8 and init-0 variants |
| 2202–2999   | UNASSIGNED  | Open for future Mama variants |
| 3061–3999   | UNASSIGNED  | Open for future workload capsule phases |
| 4008–4009   | UNASSIGNED  | ACL extension space |
| 4019–4049   | UNASSIGNED  | ACL / zuse extension space |
| 4051–4054   | UNASSIGNED  | lib.4th extension space |
| 4056–4059   | UNASSIGNED  | common:msg extension space |
| 4066–4099   | UNASSIGNED  | doe-campaign extension space |
| 4122–4199   | UNASSIGNED  | Hermes extension space (post-4121) |
| 4205–4299   | UNASSIGNED  | compudynamics extension space |
| 4302–4399   | UNASSIGNED  | process extension space |
| 4406+       | OPEN        | Future capsules — claim here first |

---

## Conflict Register

| ID  | Blocks    | Capsule A       | Capsule B           | Risk     | Resolution |
|-----|-----------|-----------------|---------------------|----------|------------|
| C1  | 2048–2063 | init-6.4th      | init-l8-omni.4th    | Low      | By design — alternate Mama personalities, never co-loaded |
| C2  | 2049–2056 | init.4th        | doe.4th             | Low      | By design — alternate Mama personalities, never co-loaded |
| C3  | 3001–3060 | init-1..3,9     | init-l8-*.4th       | Low      | By design — workload capsules, exactly one active per run |
| C4  | ~~4010–4012~~ | ACL.4th     | zuse.4th            | Resolved   | zuse.4th moved to 4016–4018; no overlap |

---

*This document is authoritative for capsule block assignments.*
*Last updated: 2026-06-30 — Hermes v1 + C4 resolved; T1 double K-KILL-HOOK fixed (`182a8a23`); T2 VM-TREE/VM-CHILDREN implemented (`b52281b9`); T4 dead EVENT-WAIT replaced with HERMES-TICK (`3436d564`); MSG-REAP ordering bug fixed (`8ca0cda2`).*
