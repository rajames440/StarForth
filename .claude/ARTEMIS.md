# ARTEMIS.md — Artemis VM Architecture
# StarshipOS / StarForth — Captain Bob (Robert Allan James)
# This document constrains Claude Code behavior. Read it completely before touching Artemis.

---

## Language Constraint — Non-Negotiable

All Artemis implementation is in StarForth dialect ONLY.

This includes:
- Data structures
- Constants
- Variables
- Control flow
- Everything

C99 is forbidden unless explicitly blocked in StarForth AND explicit written permission
is given by Captain Bob for that specific construct. "I could not figure out how to do
this in StarForth" is not permission. Ask first. Wait for the answer.

If you are about to write a C struct, a #define, or a C variable — stop.
Implement it in StarForth or ask.

---

## When Stuck — Non-Negotiable

If you are stuck, STOP.

Do not confabulate.
Do not invent dictionary words that do not exist.
Do not fabricate primitives.
Do not guess at behavior.
Do not work around the problem silently.

STOP. Report exactly what is blocking you. Wait for instruction.

Silent confabulation has caused git resets on a project with a hard patent deadline.
It is not acceptable under any circumstances.

---

## What Artemis Is

Artemis is memory. She owns the external attached virtual disk image and everything
that lives on it. She manages block storage compudynamically.

Artemis does **not** move messages — that is Hermes.
Artemis does **not** make VM lifecycle decisions — that is Hera.
Artemis does **not** own the internal ramdisk — that is kernel territory.

---

## Immediate Goal

This is the complete scope for the current build. Nothing else. Everything past
this section is future material, preserved for when we circle back — not a
specification to build against today.

**The target:**

1. QEMU boots on all three architectures (amd64, aarch64, riscv64), each with
   one attached raw virtual disk image — plain, no filesystem, one blank partition.
2. It is the **same single image** across all three arches, not one per arch.
3. Size: **~30MB**. Small enough for plain git, no LFS needed.
4. The image is git-tracked as a development artifact surface.
5. Artemis boots and begins managing the disk compudynamically.

### Flat Pool — No Zones Yet

For this pass, Artemis treats the entire 30MB image as **one flat pool**.
No tiering, no zone distinction, no migration. That sophistication is real
and preserved below (see Future material) but is explicitly out of scope
until this flat-pool foundation works.

### Arch-Neutral On-Disk Format

The same image is read and written by three different architectures. The
on-disk block/BAM layout must be fixed and arch-neutral — explicit byte
order, fixed cell widths — regardless of which host arch last touched it.
A block written under aarch64 must read correctly under riscv64.

### Persistence Across Arch and Reboots

**Data persists.** This supersedes the earlier "nothing persists between
runs" position — that was correct for the prior design and is no longer
correct now that the disk image itself is the point.

**Heat does not persist.** See Ground-State Rejoin, below.

### Boot-Time Disk State

At boot, Artemis must determine what she's looking at before touching
anything:

1. **BLANK** — disk is genuinely empty (no recognized marker). Initialize
   fresh: free map spans the whole image, nothing allocated.
2. **LithosAnanke** — disk carries a recognized Artemis/LithosAnanke marker.
   Load and resume from what's there.
3. **Unrecognized** — disk has *something* on it that is neither blank nor
   a recognized marker. **Refuse to mount. Halt with a clear error.**
   Do not silently treat unrecognized data as blank. Do not overwrite it.
   This requires explicit human instruction to proceed, not an automatic
   fallback.

### Data Loss Warning — Must Be Documented for Users and Agents

There is no filesystem layer under Artemis. No journal, no fsck, no
recovery guarantee. A bug, a bad write, or a crash mid-write can destroy
data with nothing underneath to catch it. This is a real, standing risk,
not a hypothetical — it must be disclosed plainly to anyone (human or
agent) working with this system, not buried in a design doc.

### Ground-State Rejoin

K≡1.0 is a **live-runtime** concept. It is not persisted and it does not
need to be. A device — including this same disk image — starts from
ground state (cold) every time it joins the fleet, whether this is the
first boot ever or the hundredth. The disk remembers *content*. It does
not remember *heat*. Every join is a cold join.

### Acceptance Criteria

1. Artemis correctly distinguishes BLANK / LithosAnanke / Unrecognized at boot.
2. On BLANK: free map initializes fresh across the whole 30MB flat pool.
3. Artemis can fetch a block and persist a block against the real disk image.
4. K conserves correctly across those fetch/persist operations.
5. Data written in one boot is present and correctly readable in a
   subsequent boot — same image, same or different arch.
6. Unrecognized disk content halts the mount with a clear error — no
   silent overwrite.

**Do not begin implementation without explicit instruction from Captain Bob.**

---

## What Artemis Owns

- The free map over the flat pool (Immediate Goal) — zone-based Physical/Logical
  BAMs are future material, see below
- Block fetch and persist operations against the disk image
- Block thermal state — heat tracking and compudynamic lifecycle
- Block metadata
- ACL lists — structured as Artemis-managed blocks, build deferred (see ACL section)
- Cold capsule store — evicted from Hermes cache (future chapter)

---

## Compudynamic Block Lifecycle

Artemis manages blocks compudynamically. Same model as everything else in StarshipOS:

- Block enters active use — born hot
- Cools over time with each heartbeat tick
- Frequently accessed blocks stay warm (heat refreshed on access)
- Cold blocks are candidates for eviction or reuse
- Block reap must rebalance K correctly

Artemis is the hardest VM to design because she has three interleaved
compudynamic concerns:
1. The free map — which LBNs are available
2. Block thermal state — how hot is each active block
3. K≡1.0 — fleet conservation across all block operations

These must be designed together. Do not implement one without the others.
Do not begin implementation without explicit instruction from Captain Bob.

---

## ACL — Access Control Lists

### Current State
ACL is a toggle. It is currently OFF.

A `\` comment disables ACL enforcement. When commented out, zuse is the
implicit and only user. No enforcement occurs.

```forth
\ ACL-ENFORCE   ( disabled — zuse only )
```

### When ACL Turns On
ACL enforcement becomes mandatory when VMs outside the Tripod arrive.
Until then the toggle stays off.

When enforcement is enabled:
- Every VM has an identity
- Every operation is checked against the ACL
- Artemis is the authority — she holds the lists

### Security Model
Security grows with the threat surface. Not before it.
The architecture is always structurally correct — enforcement is simply
not active yet. No retrofit needed when the time comes.

Toggling ACL on must require zero structural changes to the codebase.
Only the `\` comment is removed. If toggling ACL requires anything else,
the implementation is wrong.

### Build Status — Deferred

All ACL activity — including implementing the variable-length record
scheme for ACL certs — is deferred until all three Tripod legs (Hera,
Hermes, Artemis) are working cleanly without ACL enforcement. The record
design (see Future material, Storage Design) is confirmed and ready when
that day comes; it is not a green light to build it now. Building core
Artemis storage does not require or depend on any ACL work.

---

## What Is Not Artemis

- **Internal ramdisk** — kernel territory, not Artemis scope
- **Message routing** — that is Hermes
- **VM lifecycle** — that is Hera
- **Cold capsule eviction from Hermes** — future chapter, not now

---

## Compudynamic Invariant

Every block Artemis manages participates in K≡1.0.
Block fetch, persist, and reap operations must rebalance K correctly.
Artemis's thermal contribution to the fleet is the sum of her active block heat.

Do not write code that breaks K≡1.0 and add a comment explaining why it's okay.

---
---

# FUTURE MATERIAL — Not In Scope

Everything below this line is real design work, preserved intentionally,
and is **not** part of the Immediate Goal above. Zones, multi-device pools,
hot-plug, thermal migration, and the ACL record geometry all belong here.
Do not implement any of this without Captain Bob explicitly reopening it.

### A Note on Zones

Earlier drafts of this document treated Zone 0 / Zone 1 / Zone 2 as fixed,
pre-assigned LBN ranges decided at boot. That was a mental-model
illustration, not a literal design commitment. The corrected framing:
**a device finds its own zone after it settles in** — zone membership is
an emergent property of compudynamic placement (heat, access pattern),
not a range assigned in advance. The detailed zone material below still
reflects the older fixed-range framing and needs to be reconciled with
this correction when this chapter is reopened. Do not build the fixed-range
version as written below without first resolving that.

---

## Storage Topology (Future)

Artemis manages four active storage types. Cloud blocks are a deferred future chapter.

### RAM Dedicated Blocks — Zone 0
- Direct-access block RAM; no block subsystem indirection
- LBN 0–991 (1MB, per kernel memory layout)
- Artemis's hot working tier — fastest possible access
- Always present. K participation never zero.
- Physical BAM is compile-time constant

### Ramdisk Blocks — Zone 1
- Kernel ramdrive; LBN 2048–3071 (1MB)
- RAM-backed but accessed through the block subsystem — same I/O path as USB
- Always present. Warm tier.
- Physical BAM is compile-time constant (1024 blocks)

### System Blocks — Zone 2
- Block devices, additive/subtractive against one shared pool — devices can
  arrive or depart at **any time**, not just at boot (corrected — see below)
- Artemis is owner of all block devices — no disambiguation, no pick rule
- Zone 2 is a pool: Physical BAM spans the total block count of all claimed devices
- Accessed through the block subsystem (same path as Zone 1)
- Cold/persistent tier. Largest. Slowest.
- If no device found at boot: Artemis enters NO-DISK mode (Zone 2 pool empty)

### USB Thumbdrive — Identity First, Then Storage
- Dual-role device: identity credential carrier AND storage
- Protocol: identity verified first; storage claimed after identity passes
- Hardware PKI anchor for ALL users and agent services (not zuse alone)
- Carries user certs and agent service identities (Ed25519, ACL Phase 8)
- After identity verification: storage portion claimed as a Zone 2 System Block
- Artemis must recognize and sequence this — identity gate before storage mount

### Cloud Blocks — Deferred Future Chapter
- Do not implement or design around.

---

## Device Discovery — Event-Driven, Not Boot-Only

**Correction:** external block devices are not boot-scan-only. USB devices
can arrive or depart at any time during a run. Detection is **event-driven**
— device add/remove triggers immediately, not on a tick poll.

Device presence is a runtime condition, not a boot invariant:
- **Device arrives** — admitted into the shared Zone 2 pool (additive).
  Physical BAM extends to cover it. K rebalanced.
- **Device disappears** — must not crash; graceful K rebalancing; all
  logical entries backed by that device reap immediately (subtractive).
- Multiple devices may be present simultaneously in one shared pool.
- The USB thumbdrive's identity-first gate is specific to that
  credential-carrying device; plain storage devices do not carry an
  identity-verification step.

The exact mechanism for growing the Zone 2 Physical BAM without a live
resize (e.g., per-device LBN segments rather than one array that grows)
is not yet settled — needs design work when this chapter reopens.

---

## Storage Design (Future)

Record design confirmed 2026-07-01: dynamic-directory scheme (offset +
length + identity + flags per entry) for variable-length records —
supersedes an earlier same-day consideration of fixed-size slots. This is
design-confirmed, not build-authorized. See ACL Build Status, above.

### Driving Principle

Artemis is managed compudynamically. The same heat/cool/reap model that drives
Hermes message lifecycle and word execution in Hera drives block lifecycle in
Artemis. Compudynamics is not bolted on — it IS the allocator.

### Two BAMs

Artemis uses two Block Allocation Maps with near-identical cell-based structure.
No zone tag in either — the physical LBN is the block's location. Zone membership
is derived from the LBN range at query time. A block that knows its physical LBN
knows everything it needs.

**Physical BAM — 2 cells per entry**
```
cell 0 — physical LBN
cell 1 — free flag  ( 0=free  1=allocated )
```
- One entry per physical data block slot
- Per-zone array (each zone has its own PBAM)
- The raw substrate. Knows free/not-free. No heat, no identity.

**Logical BAM — 4 cells per entry**
```
cell 0 — physical LBN   ( zone derived from LBN range )
cell 1 — heat           ( Q48.16 — compudynamic lifecycle driver )
cell 2 — identity       ( XXHash64 content hash — 1 cell on 64-bit )
cell 3 — flags
```
- Spans all zones in one array
- Heat=0 → reap: physical LBN returned to PBAM free pool
- Content-addressing, ACL records, and cold capsule storage live here
- Identity is derived from block content — same content, same identity
- Mutation is detectable: hash changes if content changes
- Dedup is natural: duplicate ACL certs, capsules, or data recognized without scan
- Consistent with capsule identity (capsule ID = XXHash64 content hash)
- Uses existing `xxhash64.c` — no new dependency

### Compudynamic Block Lifecycle (sketch)

1. **Alloc** — claim a physical LBN from Physical BAM, create a logical entry,
   born hot (Q.1)
2. **Access** — heat refreshed on every read or write
3. **Cool** — each heartbeat tick applies Q-DECAY to all logical block heat values
4. **Reap** — logical entries at heat=0 release their physical LBN back to
   the Physical BAM free pool

### K Participation

Every logical block's heat contributes to Artemis's K total.
Reap must credit K back. Alloc must charge K correctly.
The Physical BAM carries no K — only live logical blocks do.

### Thermal Zones

Artemis manages **three thermal zones**, ordered fastest to slowest:

**Zone 0 — Block RAM (LBN 0–991)**
- Raw RAM, direct access, no block subsystem indirection
- Artemis's hot working tier — fastest possible access
- Always present. Artemis's floor. K participation never zero.
- Physical BAM is compile-time constant: 992 bits = 124 bytes
- No 4KB→3+1 geometry — Zone 0 is 1KB native, no sector alignment constraint
- 1:1 layout: one data block per LBN; no implicit metadata sibling blocks
- Block metadata lives in the LBAM entry, not on-block
- LBN 992–2047: VM dictionary territory (kernel/kmalloc); Artemis does not touch

**Zone 1 — Ramdrive (LBN 2048–3071)**
- RAM-backed but accessed through the block subsystem —
  same interface as an external device
- This is why it is Zone 1 and not Zone 0: it goes through the
  same block I/O path as a USB device, just faster
- Always present. Warm tier.
- Physical BAM is compile-time constant (1024 blocks = 1MB)

**Zone 2 — External USB Device**
- Dynamic, event-driven presence (see Device Discovery, above)
- Accessed through block subsystem (same path as Zone 1)
- Physical BAM sized from discovered/claimed block count, grows and
  shrinks as devices join/leave
- Cold/persistent tier. Largest. Slowest.

Block lifecycle across zones (sketch):
- Born hot in Zone 0 (direct block RAM)
- Cools → migrates to Zone 1 (ramdrive, same block subsystem as Zone 2)
- Cools further → migrates to Zone 2 (System Blocks, if present)
- Cools to zero in Zone 2 → reaped; physical slot returned to Zone 2 Physical BAM
- If Zone 2 absent: Zone 1 is the cold terminus; reap happens there

When Zone 2 disappears mid-run: all Zone 2 logical entries reap immediately,
K rebalanced, Zones 0 and 1 continue unaffected.

Each zone has its own Physical BAM. The Logical BAM spans all zones and
carries a zone tag per entry.

### Migration Policy

No zone is special. Blocks are blocks. The same compudynamic rules apply
uniformly across all zones:

- Every block cools each heartbeat tick via Q-DECAY regardless of zone
- When a block's heat crosses the migration threshold it moves to the next zone
- Zone membership is a property of current location, not a different regime
- No write-buffer, no forced flush, no per-zone special cases
- Pressure (Physical BAM full) resolves naturally: the coldest block migrates
  first because it is coldest — not because of any zone policy
- Migration copies the block to the next zone and frees the source physical slot
- The Logical BAM entry zone tag is updated; heat carries over unchanged
- Reap happens at heat=0 in whichever zone the block occupies at that moment

There are no migration threshold values. Compudynamics provides the policy
directly: when a zone has pressure (Physical BAM full), the coldest block
migrates — heat ordering determines the candidate, zone pressure is the
trigger. No magic constants. Reap fires at heat=0, a natural zero-crossing.
Everything else is self-organizing.

### Physical BAM Block Geometry

The block subsystem operates in 1KB StarForth blocks. Physical devices
operate in 4KB sectors. The mapping:

```
1 physical 4KB device sector → 4 × 1KB StarForth blocks:
  [ data ][ data ][ data ][ meta ]
    LBN+0   LBN+1   LBN+2   LBN+3
```

- **3 data blocks** — usable storage, tracked in the Physical BAM
- **1 metadata block** — describes the preceding 3 data blocks
  (heat, identity, flags per sibling; plus variable-length record index)
- Metadata position is implicit: always at (group × 4) + 3
- Physical BAM tracks data block slots only — 3 bits per 4KB sector

**Metadata block layout (LBN+3):**
```
[0–71]    3 × sibling descriptor: [heat | identity | flags]   72 bytes
                                   3 cells × 3 siblings
[72–1023] variable-length record index                        952 bytes
          entry: [offset | length | identity | flags]
          4 cells = 32 bytes per entry → ~29 entries per group
```

Variable-length records (ACL certs, ~200 bytes) are packed into the 3KB
data space of LBN+0/1/2. The metadata block's remaining 952 bytes index
them: offset into the data area, byte length, XXHash64 identity, flags.
Artemis finds a specific record by identity — no content scan needed.
The 3+1 geometry provides variable-length support at no extra cost.

Worked example for Zone 1 (ramdrive, 1024 × 1KB = 1MB):
- 1MB ÷ 4KB = 256 groups
- 256 × 3 = **768 data blocks** tracked in Zone 1 Physical BAM
- 256 × 1 = 256 metadata blocks (implicit, not in BAM)
- Zone 1 Physical BAM = 768 bits = **96 bytes**

Zone 2 (USB): same geometry, group count determined at runtime from
claimed device block count, adjusted as devices join/leave.

---

*This document is authoritative. If it conflicts with something in the codebase,
the codebase is wrong.*
