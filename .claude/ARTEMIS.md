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
