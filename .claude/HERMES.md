# HERMES.md — Hermes VM Architecture
# StarshipOS / StarForth — Captain Bob (Robert Allan James)
# This document constrains Claude Code behavior. Read it completely before touching Hermes.

---

## Language Constraint — Non-Negotiable

All Hermes implementation is in StarForth dialect ONLY.

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

## What Hermes Is

Hermes is the messenger. He moves messages between VMs and manages channels.
That is his complete contract.

Hermes does **not** store persistent data — that is Artemis.
Hermes does **not** make lifecycle decisions — that is Hera.
Hermes does **not** route by load or heat — dispatch is capability-based, always.

Capsules are baked into the OS blob by mkcapsule.c at build time.
Hermes has no capsule cache, no fetch logic, no repository concern.
That is a future chapter.

---

## The Event Loop

Hermes runs a compudynamic event loop. Every message is a compudynamic object:

- Born hot (full heat at creation)
- Cools each heartbeat tick
- ACK received → clean death, K redistributed to fleet
- NACK received → requeue with reduced heat, or reaped if heat floor reached
- TTL expires (heat → 0) → Hermes reaps the message, K rebalanced

There is no special timeout logic. The physics handles it.
A message that nobody answers simply cools to death.

---

## Message Structure

Every message carries:

```
type        — work type tag (determines recipient)
sender      — originating VM identity
recipient   — destination VM identity
payload     — message body
heat        — current thermal value (compudynamic)
sequence    — monotonic sequence number
```

Point-to-point only. Sender addresses recipient directly.
Hermes delivers and tracks thermal state. He does not inspect payload.

---

## ACK/NACK Protocol

- ACK → message dies cleanly, K redistributed
- NACK → message requeued with reduced heat
- No response → message cools naturally to death via TTL

The binary receipt of a response IS the ACK where applicable.
No separate acknowledgement frame needed in those cases.

---

## PubSub — Channels

Hermes implements a publish/subscribe model. Topics are called **channels**.

### The Common Channel

There is one permanent channel: `COMMON`.

- Always exists, always hot
- Any VM can publish to it
- Used for negotiation and channel establishment only
- Not for business transactions — those happen on ephemeral channels

### Ephemeral Channels

Ephemeral channels are compudynamic objects — born hot, reaped when cold or closed.

**Channel structure:**

```
channel_id    — owner VM identity + monotonic sequence number
owner         — VM that created the channel (always the responder)
members[]     — participant list
heat          — compudynamic, born hot at creation
state         — NEGOTIATING | OPEN | CLOSING
```

### The Negotiation Protocol

```
1. X publishes request on COMMON — born hot, TTL ticking
2. Y sees request, responds yes/no on COMMON
   — No response → message cools to death → implicit NACK to X
3. Y accepts → Y creates ephemeral channel
             → Y publishes channel_id to X over COMMON
4. X and Y conduct business on ephemeral channel
5. Business concluded → owner Y reaps channel → K rebalanced
```

Y owns channel creation and destruction. Always the responder, never the requester.
A channel dying of cold (abandoned transaction) is not an error — the physics handles it.

### Multi-Party Channels

Channels are bilateral by default. The owner may invite additional VMs:

```
1. Owner publishes invite to candidate VM on COMMON
2. Candidate accepts or declines on COMMON — same TTL/cool protocol
3. Accept → candidate added to members[]
4. Decline or timeout → candidate never joins, channel continues bilateral
```

Multi-party is designed for but not implemented yet.
Do not implement invite logic until explicitly instructed.
The members[] structure must exist from the start to avoid future redesign.

### Channel Identity

`channel_id` = owner VM identity + monotonic sequence number.
Unforgeable by non-owners. Hermes mints it. No VM constructs its own channel_id.

### Compudynamic Invariant for Channels

Channels participate in K≡1.0.
An open channel with active messages holds heat.
Channel reap must redistribute K correctly to the fleet.
A channel cannot be reaped while messages on it are still in flight —
drain first, then reap.

---

## What Is Not Hermes

- **Capsule cache** — future chapter, not now
- **Remote fetch** — future chapter, not now
- **UDP server / client** — future chapter, not now
- **Instance authentication** — future chapter, not now
- **Persistent block storage** — that is Artemis
- **VM lifecycle decisions** — that is Hera
- **Heat-based routing** — wrong model, never implement

---

## Compudynamic Invariant

Every object Hermes manages — messages and channels — participates in K≡1.0.
Fleet K includes the thermal contribution of in-flight messages and open channels.
Hermes reaping a message or channel must rebalance K correctly.

Do not write code that breaks K≡1.0 and add a comment explaining why it's okay.

---

## Current Scope

1. Compudynamic event loop — message emit, drain, TTL, ACK/NACK, reap
2. COMMON channel — permanent, always hot, negotiation only
3. Ephemeral channels — compudynamic lifecycle, owner=responder, bilateral default
4. Channel negotiation protocol — request on COMMON, Y creates, Y reaps
5. members[] structure — present from the start, multi-party invite not yet implemented

Capsule management is a future chapter.
Remote repository is a future chapter.
Multi-party channel invite is a future chapter.

---

## v1 Known Scope Gaps

Named, bounded limitations. Logged here to prevent future confusion and make
the upgrade path explicit. None of these are defects in v1.

### Synchronous delivery — async event loop not implemented

`MSG-SEND` calls `MSG-DELIVER → VM-EXEC` immediately. The spec describes an async
event loop where messages sit in the arena, cool, and are processed or reaped.
v1 uses the arena as a TTL cleanup buffer for already-delivered messages, not as
a live dispatch queue. ACK/NACK and NACK-requeue are not implemented. When v2
adds true async dispatch, `MSG-SEND` must stop calling `MSG-DELIVER` directly and
let `HERMES-TICK` drive delivery.

### Payload size coupled to block size

`MSG-PADDR`/`MSG-PLEN` carry FORTH string pointers. Payloads from `S"` literals
in block source are bounded by block size (1024 bytes). No logical-block spanning
is implemented. No real Hermes script has hit this ceiling yet — revisit when
something forces the issue.

### `IDX>NAME` and `CH-ACCEPT` owner hardcoded for 3-VM Tripod

`IDX>NAME` maps 0→Hera, 1→Hermes, 2→Artemis only. `CH-ACCEPT` writes
`1 OVER CH-OWNER!` unconditionally. Correct for v1 Tripod; generalization
needed if Tripod grows beyond three VMs.

### Terminology: heat-gated reclamation, not mark-and-sweep

Channel and message reaping is **heat-gated reclamation**: objects are freed only
when compudynamic heat decays to zero. This serializes reaping against active
traffic on that object only — it is not stop-the-world for the VM as a whole.
Do not apply GC vocabulary (mark-and-sweep, generational, root-set tracing) to
this mechanism; those terms carry expectations the design never intended.

---

## v1 Block Map — LOCKED

Blocks 4110–4113 are Artemis. NEVER touch them.

```
4100  Constants: event codes, channel states, node sizes, arena sizes
4101  Arena CREATE: MSG-ARENA CH-ARENA MBR-ARENA; free-list vars; MSG-SEQ CH-ACTIVE
4102  MSG-INIT-FREE + CH-INIT-FREE
4103  MBR-INIT-FREE + MSG-ALLOC + MSG-FREE-NODE
4104  CH-ALLOC + CH-FREE-NODE + MBR-ALLOC + MBR-FREE-NODE
4105  Message field accessors: MSG-TYPE@/! MSG-FROM@/! MSG-TO@/! MSG-PADDR@/! MSG-PLEN@/! MSG-HEAT@/! MSG-SEQ@/!
4106  Channel+member accessors: CH-ID@/! CH-OWNER@/! CH-STATE@/! CH-HEAT@/! CH-MBRS@/! CH-NEXT@/! MBR-NEXT@ MBR-VM@
4107  IDX>NAME + MSG-DELIVER + MSG-SEND
4108  VARIABLE MSG-SCAN + MSG-COOL-ONE + MSG-COOL-ALL
4109  MSG-REAP
---- 4110–4113: ARTEMIS — DO NOT TOUCH ----
4114  VARIABLE CH-SCAN + CH-COOL-ALL
4115  CH-REAP-SAFE
4116  VARIABLE COMMON-CH + COMMON-INIT + HERMES-TICK
4117  EVENT-EMIT + EVENT-WAIT + EVENT-DRAIN (backward compat)
4118  HERA-NOTIFY-SPAWN + HERA-NOTIFY-KILL + HERA-DISPATCH-ONE + HERA-DISPATCH
4119  CH-REQUEST + CH-ACCEPT + CH-CLOSE
4120  WELCOME + CD-INIT
```

### Node layouts (cells)

**Message node — 7 cells:**
- 0: type (in-use) / next-free ptr (free)
- 1: sender VM index
- 2: recipient VM index
- 3: payload addr (FORTH string addr)
- 4: payload len
- 5: heat (Q48.16)
- 6: seq

**Channel node — 6 cells:**
- 0: channel-id (in-use) / next-free ptr (free)
- 1: owner VM index
- 2: state (0=NEGOTIATING 1=OPEN 2=CLOSING)
- 3: heat (Q48.16)
- 4: members-head (→ member list)
- 5: next-active (active channel list link)

**Member node — 2 cells:**
- 0: next-ptr
- 1: vm-id

### Physics

Cooling constant: Q-DECAY (65208) from compudynamics.4th — same as VM-DECAY-ONE.
Messages and VMs cool at identical rates. Fleet is thermodynamically consistent.

---

*This document is authoritative. If it conflicts with something in the codebase,
the codebase is wrong.*
