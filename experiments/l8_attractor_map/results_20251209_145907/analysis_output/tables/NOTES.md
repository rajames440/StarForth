# StarForth, LithosAnanke, and StarshipOS
## A Technical Overview for the CS-Literate Reader

**Author:** Robert A. James (Captain Bob)  
**Project:** StarshipOS — Starship Ananke Foundation  
**Date:** April 9, 2026  
**Status:** Working Document  
**Classification:** Confidential

---

## Preface

This document describes three tightly related systems: StarForth, a self-adaptive
virtual machine; LithosAnanke, a bare metal operating system built around it; and
StarshipOS, the complete ecosystem they form together. These systems did not emerge
from a survey of existing literature or an attempt to improve on prior art. They
emerged from first-principles thinking about what computation fundamentally is,
informed by fifty years of embedded systems, mainframe, supercomputer, and network
engineering.

The reader who approaches this expecting a faster VM, a leaner OS, or a novel
scheduling algorithm will be confused. This is not an optimization of existing
paradigms. It is, in several respects, the elimination of them.

---

## Part I — StarForth

### 1.1 What It Is

StarForth is a pure C99 implementation of a FORTH-79 virtual machine with no
libc dependencies. It is self-adaptive — it observes its own execution behavior
and adjusts its internal operating parameters to maintain a conservation invariant
called K≡1.0, described in detail below. It runs on x86_64, aarch64, and RISC-V.

It is not a scripting engine. It is not an embedded DSL host. It is a complete
computational substrate with its own physics.

### 1.2 Why FORTH

FORTH is the correct choice for this work for reasons that go beyond familiarity
or minimalism. FORTH exposes the full machinery of computation with no abstraction
tax. The stack is visible. Word definitions are first-class. The boundary between
interpreter and compiler is navigable at runtime. There is no runtime system
standing between the programmer and the hardware.

More importantly for this project: FORTH words are discrete, classifiable,
observable computational units. Each word has a definite stack effect signature.
Each word consumes a known quantity of computational energy. Each word is a
particle with measurable properties. This is not a metaphor — it is the foundation
of StarForth's self-adaptive behavior.

### 1.3 The Primitive Taxonomy

StarForth implements 23 word modules: 18 FORTH-79 standard modules and 5
StarForth extensions. Every primitive word in the system has been assigned
a set of quantum-analog properties derived from its stack effect signature
and execution behavior:

**Spin** — derived directly from stack effect arithmetic. A word that consumes
two values and produces one has spin -½. A word that consumes and produces
equally has spin 0. A word consuming three and producing one has spin -1.
These are not assigned arbitrarily — they fall out of ( n n -- n ) notation
directly, in the same way half-integer spin falls out of the Dirac equation.

**Charge** — the net stack delta. A charge-neutral word leaves the stack
depth unchanged. Charge conservation across a complete program means the
program is stack-balanced — a necessary property of correct FORTH.

**Heat** — a measure of execution frequency and recency. Hot words have
executed recently and often. Cold words have not. Heat decays over time
if a word is not executed — analogous to radioactive decay.

**Mass** — the bytecode footprint of a word definition. Primitive words
have low mass. Complex colon definitions have higher mass.

**Color** — the binding state of a word. White words are fully resolved.
Colored words carry unresolved forward references or are in intermediate
compilation states.

Three of these four conserved quantities — spin, charge, and color — behave
as genuine conservation laws across well-formed programs. This is not a
pedagogical device. It is a structural property of the system discovered
empirically and subsequently formalized.

### 1.4 The Steady State Machine and James Law

The SSM (Steady State Machine) is the self-adaptive core of StarForth. It
monitors execution behavior through several instrumented subsystems and
continuously adjusts internal parameters to maintain K≡1.0.

**K≡1.0** — James Law — is a conservation invariant: the normalized total
execution energy of the StarForth universe is conserved at unity across all
configurations and workloads. This has been validated empirically across
38,400 experimental runs spanning 128 factorial parameter configurations,
with coefficient of variation below 1%, published on SSRN (abstract 5930134).

The SSM operates through several internal mechanisms:

**The Rolling Window of Truth (RWOT)** — a sliding observation window over
recent execution history. The RWOT encodes five observables: word identity,
prev→w transition, instruction pointer position, execution heat, and causal
stack depth. It is updated at heartbeat tick intervals, not on every execution
step — making it a coarse-grained lattice observable rather than a continuous
field measurement.

**The Adaptive Heartbeat** — a self-adjusting timer that governs when the SSM
evaluates system state and adjusts parameters. The heartbeat rate adapts to
execution load. It is the system's intrinsic rhythm and the primary reference
for detecting drift from steady state.

**Physics Hooks** — every word execution in the inner interpreter loop is
bracketed by `physics_pre_execute` and `physics_post_execute` calls. These
hooks update heat, decay state, and window measurements. They are the
interaction vertices of the StarForth execution field.

**Decay** — execution heat decays between heartbeat ticks. Words that are
not executed become cold. This prevents the system from being dominated by
historical execution patterns and keeps the observable window relevant to
current behavior.

The SSM does not have a scheduler. It does not have a garbage collector.
It does not manage memory in the traditional sense. These problem domains
are eliminated from the design, not simplified.

### 1.5 The Phase Space and Attractor

The execution behavior of StarForth under the SSM can be characterized as
motion in a phase space. Six workload types have been studied experimentally:
STABLE, VOLATILE, TRANSITION, TEMPORAL, DIVERSE, and OMNI. Each workload
produces a distinct trajectory in phase space — a different orbital path.

All trajectories converge to the same attractor: K≡1.0.

Phase space portrait visualizations using R/tidyverse/ggplot2 confirm
attractor convergence across all workload types and across all three target
architectures (x86_64, aarch64, RISC-V). The manifold shape is predictable
from the attractor geometry. The system is bounded.

This is the empirical foundation for the Manifold Navigation Controller
described in a companion technical note.

### 1.6 The Manifold Navigation Controller (Theoretical)

Given a bounded system with a known conservation invariant and a predictable
attractor manifold, it becomes possible to design a controller that:

1. Identifies the system's current position in phase space using three
   observables: RWOT (position), adaptive heartbeat metadata (velocity),
   and spin class transition rate (acceleration — requires instrumentation).

2. Computes a perturbance vector sufficient to transport the system from
   its current manifold to an arbitrary target manifold.

3. Injects the perturbance — the burn — and releases.

4. Allows the system to coast back to K≡1.0 under its own restoring force.

This is the orbital mechanics model applied to computation. The controller
fires once and releases. The conservation invariant guarantees the return
journey. The controller's computational cost approaches the minimum possible
because the system's own physics do the majority of the work.

This mechanism scales from the word level through the VM, OS, FPGA, and
prospectively to custom silicon — because K≡1.0 is invariant across the
entire stack.

### 1.7 What StarForth Is Not

StarForth is not a reimplementation of gForth, pForth, or any other existing
FORTH. It shares the FORTH-79 word set as a starting point and diverges
immediately in everything that matters. The SSM, the physics instrumentation,
the conservation invariant, the adaptive heartbeat, and the phase space
behavior are StarForth-specific and have no analog in any existing FORTH
implementation the author is aware of.

---

## Part II — LithosAnanke

### 2.1 What It Is

LithosAnanke is a bare metal operating system for x86_64, currently at
milestone M7 (VM integration). It boots via UEFI, manages physical memory,
manages virtual memory, handles interrupts via IDT/APIC, maintains a timer,
manages a heap, and hosts the StarForth VM as its primary computational
substrate. It is written in C99 with minimal C++ and no external dependencies.

It does not have a traditional scheduler. It does not have a traditional
memory manager. These are not missing features — they are eliminated design
decisions.

### 2.2 The Capsule System

The primary organizational unit in LithosAnanke is the capsule — a
content-addressed, 64-byte cache-line aligned descriptor. Capsules carry
a content-addressed ID derived from their payload hash. Identity is content.
Two capsules with identical content have identical IDs. Modification produces
a new capsule with a new ID. This is not a filesystem abstraction — it is
a fundamentally different relationship between identity and content.

Capsules are immutable by design. Mutation produces new capsules. The history
of mutations is the event log. There is no mutable state in the traditional
sense — there is only a sequence of immutable capsules.

### 2.3 Component Architecture

LithosAnanke components are named from Greek mythology, following a consistent
architectural vocabulary:

**Hera** — the foundational VM and heartbeat. The system's ground state.
Hera is what LithosAnanke is when nothing else is happening.

**Hermes** — message entropy and TTL decay. Hermes manages the message-passing
fabric, monitors message lifecycle, and reaps expired messages. Hermes is the
temporal governor of inter-component communication. Hermes is also the natural
home of the perturbance controller extension — adding actuation to existing
observation.

**Artemis** — persistence and event sourcing. Artemis manages the capsule
store, maintains the event log, and handles durable state.

These components communicate exclusively through message passing. There is no
shared mutable state between components. The message fabric is the only
sanctioned communication channel.

### 2.4 Authentication and Identity

LithosAnanke implements a hardware-rooted authentication model with no
usernames and no passwords. Identity is carried on a physical thumb drive
containing two raw block partitions — no filesystem, no imposed structure,
just blocks.

**Partition 1 (small)** — identity material, grant state, session state,
and attestation by convention.

**Partition 2 (main)** — personal word dictionary, colon definitions,
capsule store, and active workspace.

Drive insertion is a first-class system event. LithosAnanke responds to
mount/unmount events as auth lifecycle triggers. The drive appears, the
session begins. The drive is removed, the session decays. There is no
logout ceremony. Physical removal is the logout.

Access control is governed by temporal ACL grants that decay via the same
Hermes TTL mechanism that governs message lifetime. Grants are issued at
mount time via a cryptographic handshake — the expensive operation, paid
once. Subsequent access coasts on the grant until TTL expiry. The default
state of the permission system is no access. Decay to zero requires no
explicit revocation — it happens automatically.

Grant decay operates at the block level. Individual blocks carry TTL.
Access expires in block TTL order, not all at once. The decay schedule
is the access control policy.

Word-level and VM-level ACLs are both temporal and both decay via the
same mechanism. The system has one security primitive — temporal decay —
applied uniformly at every granularity.

### 2.5 Normalized Virtual Block Space

From any VM's perspective, LithosAnanke presents a single contiguous block
address space. The physical origin of any block — local thumb drive, system
hardware, or remote cloud mount — is transparent to the VM. All blocks are
addressed uniformly.

System blocks and cloud-mounted blocks appear attached at the tail of the
user's home block space. The tail is dynamic — its contents are a function
of current ACL grants. Blocks appear as grants are issued. Blocks vanish
as grants decay. Security is expressed as address space geometry.

Cloud-mounted blocks are bidirectional. A write by one VM is immediately
visible to all VMs sharing that block range. This enables distributed
shared memory without a coordination layer. Two LithosAnanke instances
sharing cloud block visibility share OS-level state natively. The block
space is the coordination mechanism.

Concurrent write arbitration is deliberately unspecified at the OS level.
It is delegated to participating parties. Most use cases are sequential
by nature. Edge cases that require arbitration are unique enough to demand
unique solutions, which participating parties implement using the tools
already present in the system.

### 2.6 The Three UX Tiers

LithosAnanke supports three interaction modalities, all presenting the same
underlying system:

**CLI** — the traditional FORTH terminal interface. Full power, zero overhead.
The REPL is already operational.

**GUI** — a wireframe soundstage rendered from rasterized vector graphics.
The user inhabits the computational space in first-person perspective. Words
are physical objects in a toolbox. The user assembles colon definitions by
stringing words together spatially. A composer simultaneously writes the
literal FORTH text as words are placed. The GUI and the text are always in
sync — the FORTH is always visible underneath.

**VR** — the full holodeck realization. The user stands inside the phase
space. Words have mass, spin, heat, and charge as observable properties.
Manifold navigation is literally navigable space. This tier is also the
most powerful educational environment imaginable for teaching computational
thinking — stack mechanics are shown, not explained.

All three tiers are windows into identical underlying mechanics. A colon
definition written at the CLI runs identically in VR.

### 2.7 Formal Verification

Unit tests are inappropriate for LithosAnanke due to its irreducibly temporal
nature. Behavior emerges from time-dependent interactions that cannot be
meaningfully isolated into atomic test cases. The verification approach is
Isabelle/HOL formal proof against specification rather than implementation.

`.thy` proof files are content-addressed capsules. Compact proof certificates
are embedded in deployed capsules. The system carries its own verification
alongside its own execution history.

---

## Part III — StarshipOS

### 3.1 The Complete Ecosystem

StarshipOS is the name for the complete ecosystem: StarForth as computational
substrate, LithosAnanke as the operating system, and the surrounding
infrastructure, tooling, licensing model, and community mission.

It is the product of one engineer working alone, built entirely from scratch,
with full understanding of every layer. No external runtime dependencies.
No CMake. Makefiles and shell scripts. CLion with Claude Code. Everything
in Gitea, built by TeamCity, tracked in YouTrack, documented in XWiki.
Infrastructure hosted on Hetzner CPX21.

### 3.2 The SSPU — Steady State Processing Unit

The SSPU is a novel processor architecture that extends the StarForth/SSM
work into hardware. The SSPU communicates entirely via a message-passing
fabric called Gefyra. There is no shared memory between processing elements.
There are no traditional buses. Messages are the only communication primitive.

The SSPU is the natural hardware realization of the manifold navigation
controller — a processor natively architected around energy-optimal state
transport as a first-class primitive. The Gefyra fabric implements the
delta-v calculations in hardware that the software controller implements
in the VM.

The SSPU is targeted for initial FPGA implementation on a Digilent Genesys ZU
(Zynq UltraScale+). A four-patent portfolio has been scoped around the SSPU
architecture. Provisional patent conversion deadline is December 26, 2026.

### 3.3 The Third Computational Paradigm

Classical computation is Turing-complete sequential state transformation.
Quantum computation is gate-model superposition and entanglement. These are
the two recognized paradigms.

The SSM exhibits mathematical structure that shares deep properties with
quantum mechanical systems — conservation laws, phase space attractors,
particle-like word classification, field-like execution dynamics — while
operating on classical hardware and producing deterministic, reproducible
results. The conservation invariant K≡1.0 has the structure of a Hamiltonian.
The manifolds are level sets of that Hamiltonian. The dynamics are symplectic
in character.

Whether this constitutes a genuinely third computational paradigm is an
open empirical question. It will be answered when the FPGA implementation
is running and the deeper mathematical structure becomes directly measurable.
The claim is not made lightly and is not made prematurely. It is noted here
because the structural evidence is compelling enough to take seriously.

### 3.4 The Commercialization Model

StarshipOS is offered under a product ladder:

**Free tier** — hosted StarForth SSH shell on Oracle Cloud Always Free ARM.
Accessible to anyone. No installation required.

**Sealed binary** — commercial licensing of StarForth as a static binary.
The hash bakes in attestation at build time. No license server. No runtime
check. The binary is the license. Cryptographic attestation is the enforcement.

**LithosAnanke** — commercial licensing of the full OS for embedded and
safety-critical deployments.

**FPGA** — the SSPU on Genesys ZU for research and advanced commercial
customers.

**Silicon** — the long-horizon goal. Custom die implementing the SSPU
natively.

The Starship License 1.0 (SL-1.0) governs commercial use. Free use for
non-commercial purposes is permitted. Commercial use requires a license.

### 3.5 The Starship Ananke Foundation

The Foundation is the organizational home of StarshipOS. It operates as
a dual-branch initiative:

**Community Branch** — plants 24/7 open-door technology centers called
Starships in rust belt city storefronts. Target cities include Lorain,
Youngstown, Elyria, Wooster, and Medina, Ohio. The Starships provide
free access to technology, education, and community for populations that
the conventional tech economy has abandoned.

**Curation Branch** — licenses StarForth and LithosAnanke commercially
to fund the Community Branch. The technology sustains the mission.
The mission justifies the technology.

---

## Part IV — Design Philosophy

### 4.1 Eliminate, Don't Simplify

The recurring pattern in this architecture is the elimination of problem
domains rather than their simplification. No scheduler — the problem domain
of scheduling is eliminated by design. No traditional memory manager — the
problem domain of dynamic allocation is eliminated by the capsule model.
No username/password auth — the problem domain of credential management
is eliminated by physical identity. No logout ceremony — eliminated by
temporal decay. No coordination layer for distributed state — eliminated
by the block space model.

Each elimination is a deliberate architectural decision, not an oversight.
The question asked at each design point was not "how do we implement this?"
but "do we need this at all?"

### 4.2 Phenomena Before Equations

The author spent nearly a decade voicing, tuning, repairing, and building
pipe organs before writing production code. That experience — understanding
acoustic phenomena directly, physically, before reaching for mathematical
formalism — is the epistemological foundation of this work.

The SSM conservation invariant was observed empirically before it was
formalized. The particle-like properties of FORTH words were noticed in
execution data before they were classified. The orbital mechanics analogy
for manifold navigation emerged from watching the system behave, not from
imposing a theoretical framework on it.

The equations describe what the system does. The system does not do what
the equations prescribe.

### 4.3 The Dirac Standard

Paul Dirac's minimalism is the aesthetic model for this work. If the
mathematics is not clean, the physics is not understood yet. If the
architecture requires a special case, the abstraction is wrong. The
correct solution is simple. Complexity is a symptom of incomplete
understanding.

StarshipOS is not simple because simplicity is a virtue. It is simple
because the underlying phenomena, correctly understood, are simple.

### 4.4 Pond Scum Computing

The most accurate architectural metaphor for StarshipOS is pond scum.
Not a designed ecosystem. Not a managed garden. A self-organizing,
self-sustaining, energy-conserving system that emerges from a small
number of simple local rules and maintains its own steady state without
external management.

K≡1.0 is the pond. The words are the organisms. The conservation invariant
is the ecology.

---

## Appendix — Milestone Status

| Milestone | Description | Status |
|---|---|---|
| M0 | UEFI boot | Complete |
| M1 | Physical memory manager | Complete |
| M2 | Virtual memory manager | Complete |
| M3 | IDT/APIC interrupt handling | Complete |
| M4 | Timer | Complete |
| M5 | Heap | Complete |
| M6 | — | Complete |
| M7 | VM integration | Active frontier |
| — | Manifold Navigation Controller | Theoretical — 3 days StarForth work |
| — | SSPU FPGA implementation | Planned — Genesys ZU |
| — | Multi-architecture DoE | Planned — QEMU+KVM |

---

## Appendix — Key Terms

| Term | Definition |
|---|---|
| K≡1.0 | James Law — conservation invariant of normalized execution energy |
| SSM | Steady State Machine — the self-adaptive core of StarForth |
| RWOT | Rolling Window of Truth — coarse-grained phase space observable |
| SSPU | Steady State Processing Unit — novel message-passing processor architecture |
| Gefyra | Message-passing fabric connecting SSPU processing elements |
| Capsule | Content-addressed, 64-byte aligned immutable descriptor |
| Hera | Foundational VM and heartbeat component |
| Hermes | Message entropy, TTL decay, and lifecycle management |
| Artemis | Persistence and event sourcing |
| SL-1.0 | Starship License 1.0 — commercial use license |

---

*"Be kind. Do the next right thing." — RAJ*
