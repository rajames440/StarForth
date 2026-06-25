# TRIPOD.md — Architectural Constraints for the Tripod VMs
# StarshipOS / StarForth — Captain Bob (Robert Allan James)
# This document constrains Claude Code behavior. Read it completely before touching any Tripod code.

---

## The Tripod: What It Is

Three StarForth VMs constitute the Tripod. They are not generic workers. Each holds a
specific architectural contract expressed by its mythological name. Violating the
mythological contract is an architectural error, not a style preference.

| VM         | ID          | Contract                                      |
|------------|-------------|-----------------------------------------------|
| Hera       | VM-HERA=0   | Governor. Spawns, manages, and reaps VMs.     |
| Hermes     | VM-HERMES=1 | Messenger. Moves events between VMs.          |
| Artemis    | VM-ARTEMIS=2| Memory. Owns block storage and persistence.   |

**Hera is queen. She does not do work. She governs who exists and who doesn't.**
**Hermes moves messages. He does not store anything.**
**Artemis holds things. She does not route anything.**

If you find yourself putting event dispatch logic in Artemis, or storage logic in Hermes,
or work execution logic in Hera — stop. You have the wrong VM.

---

## The Immediate Goal

Hera must spawn Hermes and Artemis at boot, and all three must cooperate
compudynamically under K≡1.0 conservation.

That is the complete scope. Nothing else.

Acceptance criteria:
1. Hera boots and initializes her own compudynamic state.
2. Hera spawns Hermes. Hermes initializes his own compudynamic state.
3. Hera spawns Artemis. Artemis initializes her own compudynamic state.
4. All three VMs are running. Fleet K = sum of individual VM K values. K≡1.0 holds.
5. Hera can reap (cleanly terminate) a child VM.
6. The system is stable across a long-running test workload.

---

## Compudynamics: Dual Role — Read This Carefully

Compudynamics serves TWO roles in this system. Do not conflate them.

### Role 1: Intra-VM Physics Engine
Inside each VM, Compudynamics drives the adaptive heartbeat:
- 7-loop rolling window
- K conservation arithmetic
- Phase operators on the heartbeat signal

Every VM — Hera, Hermes, Artemis — runs its own compudynamic engine internally.
This is autonomous. No VM manages another VM's internal heartbeat.

### Role 2: Hera's VM Lifecycle Driver
Hera observes the fleet K picture and uses it to make **lifecycle decisions**:
- Is a child VM healthy?
- Is a child VM starved?
- Does a child VM need to be spawned, suspended, or reaped?

Hera does NOT use fleet K to route work. She uses it to govern existence.

**The fleet K view is lifecycle telemetry. It is not a dispatch mechanism.**

---

## Dispatch: Capability-Based, Always

Work routing is determined by work type. It is not determined by heat, K value,
VM load, or any compudynamic signal.

| Work type       | Destination |
|-----------------|-------------|
| Block I/O       | Artemis     |
| Event emit/drain| Hermes      |
| VM lifecycle    | Hera        |

`VM-EXEC` takes a VM name. The caller names the correct VM for the work type.
There is no routing algorithm. There is no dispatch table keyed on heat.
There is no "send to the hottest VM." That model is wrong and must not be implemented.

---

## CD-TICK: What It Is and Is Not

CD-TICK advances the compudynamic heartbeat. That is all it does.

CD-TICK is **not**:
- A scheduler
- A timeslicer
- A run-queue manager
- A work dispatcher
- A router

If you are calling CD-TICK to decide where work goes, you are using it wrong.

---

## The DoE (Design of Experiments) Test Fixture

The DoE campaign (PHASE6-TEST, K-conservation CSV logging, 38,400-run Latin Square)
is a **stress and measurement fixture**. It exists to:
- Validate K≡1.0 conservation under long-running diverse workloads
- Generate telemetry for the SSRN paper
- Provide a long-runtime acceptance test for the compudynamic physics

The DoE is **not**:
- A description of what the system does in production
- A feature
- A model for how work gets dispatched

`DOE-WORK` must never appear in the production dispatch path.
If you see `DOE-WORK` being dispatched by anything other than the test fixture, remove it.

The DoE is a good test precisely because it is long-running and diverse.
It exercises the physics. It does not define the system.

---

## What Not To Do — Explicit Prohibitions

1. **Do not implement heat-based VM routing.** It causes starvation. It is wrong.
2. **Do not pin Hera's heat floor.** That is a band-aid on a wrong model.
3. **Do not put DOE-WORK in the production dispatch path.**
4. **Do not call CD-TICK a scheduler** in comments, docs, or variable names.
5. **Do not call the fleet K view a router** in comments, docs, or variable names.
6. **Do not add primitives to StarForth without explicit instruction.**
   NIP is not registered. TUCK is not registered. Assume nothing is available
   that is not in the verified primitive table. Use SWAP DROP instead of NIP.
7. **Do not call BLOCK on LBNs outside the ramdrive range (0–3071).**
   LBN 4501–4599 are Artemis metadata space. They are not in the kernel ramdrive.
   Use ART-STATUS to touch the free map. Do not call ART-SELF-TEST in acceptance tests.
8. **Do not exceed 64 bytes per line or 16 lines per block in capsule files.**
   mkcapsule.c enforces this at build time. Violations will fail the build.

---

## The Invariant

K≡1.0 is conserved across the fleet at all times.
Fleet K = sum of all VM K values.
This is a physics constraint, not a software convention.
Do not write code that breaks it and then add a comment explaining why it's okay.

---

*This document is authoritative. If it conflicts with something in the codebase,
the codebase is wrong.*
