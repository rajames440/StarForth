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

## Storage Topology

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
- Any block device discovered at the boot scan (external SSD, USB drive, USB mass storage, etc.)
- Ephemeral — present only when discovered at boot; hot-plug is a future chapter
- Accessed through the block subsystem (same path as Zone 1)
- Physical BAM sized at runtime from discovered block count
- Cold/persistent tier. Largest. Slowest.
- If no device found at boot: Artemis enters NO-DISK mode (Zone 2 absent)

### USB Thumbdrive — Identity First, Then Storage
- Dual-role device: identity credential carrier AND storage
- Protocol: identity verified first; storage claimed after identity passes
- Hardware PKI anchor for ALL users and agent services (not zuse alone)
- Carries user certs and agent service identities (Ed25519, ACL Phase 8)
- After identity verification: storage portion claimed as a Zone 2 System Block
- Artemis must recognize and sequence this — identity gate before storage mount

### Cloud Blocks — Deferred Future Chapter
- Do not implement or design around.

### Nothing Persists Between Runs Yet
Persistence across runs is a future chapter. Do not implement it yet.
Do not design around it. The current model is: VM starts, Artemis initializes
her free map, VM stops, everything is gone. That is correct for now.

---

## What Artemis Owns

- Physical BAM for each zone (Zone 0, Zone 1, Zone 2)
- Logical BAM spanning all zones (block identity → physical LBN + heat + zone tag)
- Block fetch and persist operations across all storage zones
- Block thermal state — heat tracking and compudynamic lifecycle for all managed blocks
- Identity verification gate for the USB thumbdrive (before storage is claimed)
- ACL lists — structured as Artemis-managed blocks (see ACL section)
- Cold capsule store — evicted from Hermes cache (future chapter)
- Block metadata

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

### Structure
ACL lists must be structured as Artemis-managed blocks from day one.
Even before Artemis is fully built, ACL data structures must assume
they will live in Artemis-managed storage. No shortcuts that require
redesign later.

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

---

## What Is Not Artemis

- **Internal ramdisk** — kernel territory, not Artemis scope
- **Message routing** — that is Hermes
- **VM lifecycle** — that is Hera
- **Persistence across runs** — future chapter, not now
- **Cold capsule eviction from Hermes** — future chapter, not now

---

## Compudynamic Invariant

Every block Artemis manages participates in K≡1.0.
Block fetch, persist, and reap operations must rebalance K correctly.
Artemis's thermal contribution to the fleet is the sum of her active block heat.

Do not write code that breaks K≡1.0 and add a comment explaining why it's okay.

---

## Storage Design — Scratchpad (design notes, not yet authoritative)

### Driving Principle

Artemis is managed compudynamically. The same heat/cool/reap model that drives
Hermes message lifecycle and word execution in Hera drives block lifecycle in
Artemis. Compudynamics is not bolted on — it IS the allocator.

### Two BAMs

Artemis uses two Block Allocation Maps:

**Physical BAM**
- Bitmap over Artemis's owned LBN range on the external disk image
- One bit per physical block: 0 = free, 1 = allocated
- No semantics. No heat. No identity. Just free/not-free.
- The raw substrate. Knows nothing about what lives in a block.

**Logical BAM**
- Maps logical block identities to physical LBNs
- Each logical entry carries: physical LBN + heat (Q48.16) + metadata
- Heat is the compudynamic lifecycle driver
- A logical block at heat=0 is a reap candidate: its physical LBN returns
  to the Physical BAM free pool
- Content-addressing, ACL records, and cold capsule storage live here

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

### Device Discovery — Boot Scan

External storage is **dynamic and ephemeral**. Artemis must not assume a device
is present. At boot, Artemis performs a block device scan:

1. **Scan** — probe for attached USB/block devices (USB mass storage, external
   SSD, USB drive, other USB block devices)
2. **Claim** — if a device is found, Artemis claims it as her external disk;
   Physical BAM is sized from the discovered block count (this resolves the
   BAM sizing open question — it is runtime, not compile-time)
3. **No device** — Artemis enters NO-DISK mode: Physical BAM empty, Logical
   BAM empty, K contribution = 0; rest of Tripod continues normally

Device presence is a runtime condition, not a boot invariant:
- **Hot-plug** (device arrives after boot) — future chapter; not designed now
- **Device disappears during run** — must not crash; graceful K rebalancing;
  Logical BAM entries reap immediately since backing store is gone

### Thermal Zones

Artemis manages **three thermal zones**, ordered fastest to slowest:

**Zone 0 — Block RAM (LBN 0–~2047)**
- Raw RAM, direct access, no block subsystem indirection
- Artemis's hot working tier — fastest possible access
- Always present. Artemis's floor. K participation never zero.
- Physical BAM is compile-time constant (block RAM size is fixed)

**Zone 1 — Ramdrive (LBN 2048–3071)**
- RAM-backed but accessed through the block subsystem —
  same interface as an external device
- This is why it is Zone 1 and not Zone 0: it goes through the
  same block I/O path as a USB device, just faster
- Always present. Warm tier.
- Physical BAM is compile-time constant (1024 blocks = 1MB)

**Zone 2 — External USB Device**
- Ephemeral. Present only when discovered at boot scan.
- Accessed through block subsystem (same path as Zone 1)
- Physical BAM sized at runtime from discovered block count
- Cold/persistent tier. Largest. Slowest.

Block lifecycle across zones (sketch):
- Born hot in Zone 0 (direct block RAM)
- Cools → migrates to Zone 1 (ramdrive, same block subsystem as Zone 2)
- Cools further → migrates to Zone 2 (USB, if present)
- Cools to zero in Zone 2 → reaped; physical slot returned to Zone 2 Physical BAM
- If Zone 2 absent: Zone 1 is the cold terminus; reap happens there

When Zone 2 disappears mid-run: all Zone 2 logical entries reap immediately,
K rebalanced, Zones 0 and 1 continue unaffected.

Each zone has its own Physical BAM. The Logical BAM spans all zones and
carries a zone tag per entry.

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
  (heat values, logical identity, zone tag, flags)
- Metadata position is implicit: always at (group × 4) + 3
- Physical BAM tracks data block slots only — 3 bits per 4KB sector

Worked example for Zone 1 (ramdrive, 1024 × 1KB = 1MB):
- 1MB ÷ 4KB = 256 groups
- 256 × 3 = **768 data blocks** tracked in Zone 1 Physical BAM
- 256 × 1 = 256 metadata blocks (implicit, not in BAM)
- Zone 1 Physical BAM = 768 bits = **96 bytes**

Zone 2 (USB): same geometry, group count determined at boot from
discovered device block count.

### Ephemeral Contract

The USB device (Zone 1) is ephemeral by design. Zone 0 (ramdisk) is
session-scoped — it exists for the lifetime of the VM. Neither zone
persists state across reboots in the current design.

Persistence across runs is a **future chapter**. The architecture must be
structurally ready for it (stable on-disk format) but the runtime makes
no attempt to restore state today.

### Open Questions (to settle before any code)

- Logical BAM entry format — how many cells per entry? (must include zone tag)
- Whether logical block identity is content-addressed (XXHash64) or
  sequence-numbered
- Variable-length record support — needed for ACL certs; blocks are 1K,
  certs are ~200 bytes — how do we pack or stride?
- How does the boot scan identify which device to claim if more than one
  USB block device is attached? (first-found? largest? manifest label?)
- Zone 0 LBN range — what slice of the internal ramdisk belongs to Artemis?
- Migration policy — does heat threshold trigger Zone 0→Zone 1 migration,
  or is Zone 0 strictly a write buffer that always flushes to Zone 1?

---

## Current Scope

Artemis is not yet started. Before any implementation begins:

1. Captain Bob must give explicit instruction to begin
2. The three compudynamic concerns (free map, block thermal state, K≡1.0)
   must be designed together before any code is written
3. ACL data structures must be Artemis-block-shaped from first line of code
4. Storage boundaries (LBN ranges on external disk image) must be confirmed

**Do not begin implementation without explicit instruction.**

Future chapters (do not implement):
- Persistence across runs
- Cold capsule store from Hermes
- Full ACL enforcement
- LBN range expansion

---

*This document is authoritative. If it conflicts with something in the codebase,
the codebase is wrong.*
