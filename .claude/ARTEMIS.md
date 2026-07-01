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

### Internal Ramdisk — Not Artemis Scope
- Baked into each VM at build time by mkcapsule.c
- LBN 0–3071
- Kernel territory
- Every VM carries its own ramdisk
- Artemis does not touch it, manage it, or know about it

### External Attached Virtual Disk Image — Artemis Territory
- Mounted virtual disk image, attached to the QEMU instance
- Artemis owns this exclusively
- LBN boundaries to be determined as Artemis is built
- Nothing persists between runs yet — that is a future chapter

### Nothing Persists Between Runs Yet
Persistence across runs is a future chapter. Do not implement it yet.
Do not design around it. The current model is: VM starts, Artemis initializes
her free map, VM stops, everything is gone. That is correct for now.

---

## What Artemis Owns

- The free map over her LBN range on the external disk image
- Block fetch and persist operations on the external disk image
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

### Ephemeral Contract

The USB device is ephemeral by design. Artemis makes no assumption that data
written during one run survives to the next. The Physical BAM is rebuilt from
scratch on every boot scan. Nothing in Artemis's current design depends on
finding the same blocks in the same state across reboots.

Persistence across runs is a **future chapter**. The architecture must be
structurally ready for it (i.e., the on-disk format must be stable and
identifiable) but the runtime makes no attempt to restore state today.

### Open Questions (to settle before any code)

- Logical BAM entry format — how many cells per entry?
- Whether logical block identity is content-addressed (XXHash64) or
  sequence-numbered
- Variable-length record support — needed for ACL certs; blocks are 1K,
  certs are ~200 bytes — how do we pack or stride?
- How does the boot scan identify which device to claim if more than one
  USB block device is attached? (first-found? largest? manifest label?)
- NO-DISK mode: does Artemis still register K=0 with the fleet, or does
  she simply not participate until a device appears?

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
