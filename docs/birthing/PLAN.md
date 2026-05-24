# Birthing Plan: Child VMs Born of Hera

**Status:** DRAFT — requires Captain Bob approval before any implementation begins  
**Branch:** `birthing` (off `lithosananke`)  
**Authors:** Claude Code + Captain Bob  
**Date:** 2026-05-24

---

## 1. Vision

Hera is the primary VM — the eternal, always-running Forth interpreter inside LithosAnanke.
Hades is the kernel + HAL substrate upon which everything runs.

Hera can give birth to **child VMs** — lightweight Forth execution contexts that live entirely
inside Hera's process space. Children are not threads, not processes, not OS tasks. They are
**mortal sub-interpreters** woven into Hera's own fabric. Hera is their world; Hades is the
ground beneath that world.

The system must support **thousands of concurrent children** within a single process.

---

## 2. Hard Constraints

These are fixed by the execution model — not by Hera's current implementation. Hera herself
can and will be expanded as the design requires.

| Constraint | Implication |
|------------|-------------|
| Same process as Hera | No `fork()`, no `pthread_create()` |
| No threads | All scheduling is cooperative, inside the interpreter loop |
| No OS processes | Children share Hera's address space, file descriptors, signal handlers |
| ANSI C99, `-Wall -Werror` | No GNU extensions, no C++ features |
| Hades is the substrate | I/O, timing, and memory services come from Hades (the kernel+HAL) |

**What is NOT a hard constraint:** Hera's current memory size, struct layout, or any
implementation detail of the existing VM. Hera grows to meet the design. We design for
what's correct and rich; Hera expands to support it.

---

## 3. Memory Architecture

### 3.1 Hera's Expansion Model

The current VM struct is a starting point, not a ceiling. Hera can be expanded in any of
these directions as the child system demands:

- **Larger or elastic memory arena** — Hera's 5 MB `memory[]` becomes a dynamically grown
  heap or a large fixed arena (e.g., 512 MB on hosted platforms, as much as Hades allows
  on kernel targets)
- **Child memory pool** — a dedicated allocation arena carved from Hera's expanded memory,
  from which child stacks, local dictionaries, and private memory slices are sub-allocated
- **VM struct refactor** — the `VM` struct can be split into a "core execution state" and
  separately managed subsystems (physics, heartbeat, dictionary) so children can compose
  exactly what they need

### 3.2 Child Richness Classes

Design for three classes. Which class a given child uses is a decision for Captain Bob (D2).

| Class | What Child Owns | Memory per Child | Max at 4 GB |
|-------|----------------|-----------------|-------------|
| **Thin** (task) | IP + stacks (256 cells each) + metadata | ~4 KB | ~1,000,000 |
| **Medium** (worker) | Thin + private memory slice (e.g., 256 KB) | ~260 KB | ~15,000 |
| **Full** (VM) | Complete VM state — own dict, physics, memory | ~6–64 MB | ~60–700 |

All three are valid. The system need not be uniform — Hera can birth thin, medium, and full
children simultaneously depending on the use case.

### 3.3 What Every Child Owns (Minimum)

Regardless of class, every child independently owns:

1. **Data stack** — isolated (size TBD; see D2)
2. **Return stack** — isolated (same size)
3. **Instruction pointer (IP)** — current execution position
4. **Mode** — interpret vs. compile
5. **Input buffer + parse state** — for Forth words it reads
6. **Identity** — name or handle in Hera's child registry
7. **Status** — alive / suspended / dying / dead
8. **Class** — thin / medium / full
9. **Parent reference** — back to Hera

### 3.4 What a Child May Share with Hera (Class-Dependent)

| Resource | Thin | Medium | Full |
|----------|------|--------|------|
| Memory arena | Shared (executes in Hera's memory) | Private slice | Own arena |
| Dictionary | Shared (read-only or vocab-scoped) | Shared | Own copy |
| Physics/SSM | Hera observes all | Hera observes all | Per-child SSM |
| Heartbeat | Hera's heartbeat | Hera's heartbeat | Shared or own |
| Hades access | Via Hera | Via Hera | Direct or via Hera |

### 3.5 What a Child Cannot Do (Regardless of Class)

- Corrupt Hera's own stacks or IP
- Block Hera (must yield)
- Modify Hera's core VM state directly
- Exceed its allocated memory class without Hera's explicit permission

---

## 5. Child VM Lifecycle

```
BIRTH
  │
  ▼
RUNNING ──── yields ────► SUSPENDED
  │                           │
  │                        resumed
  │                           │
  │◄──────────────────────────┘
  │
  ▼
DYING (word returns or calls DONE/EXIT)
  │
  ▼
DEAD (context reclaimed by Hera)
```

**Birth:** Hera allocates a child context and places it in the child registry.  
**Running:** Hera's scheduler gives the child a turn in the interpreter loop.  
**Suspended:** Child has yielded; context saved; Hera (or another child) runs.  
**Dying:** Child's top-level word returned, or child called a termination word.  
**Dead:** Child's stacks and context are freed; slot returns to the pool.

---

## 6. Spawn Mechanism — Options for Decision

Three candidate models. Captain Bob selects one (or a hybrid).

### Option A: `SPAWN <word-name>`
Hera executes the Forth word `SPAWN`, which takes the name of an existing defined word and
creates a child that will execute it. The child starts with empty stacks and runs the word
from its beginning.

```
: my-job  42 EMIT ;    \ define a job
SPAWN my-job            \ birth a child that runs my-job
```

**Pros:** Simple, explicit, composable. Children are named Forth words — they're documented
in the dictionary naturally.  
**Cons:** Children can only do what's already compiled. No runtime-parameterized behavior
without additional words.

### Option B: `BEGET` (inline definition)
Hera compiles a small anonymous word at birth time and assigns it to the child. Similar to
a lambda. The child's entire behavior is defined at the `BEGET` call site.

```
BEGET [ 42 EMIT ] AS my-child
```

**Pros:** Flexible, parameterized at call site.  
**Cons:** More complex compiler integration. Anonymous words pollute the dictionary unless
they're cleaned up on child death.

### Option C: `BIRTH <capsule-name>`
Children are born from capsule scripts (`.4th` files or in-memory capsule buffers) — the
same mechanism used for Hera's own boot init. A capsule defines the child's full behavior.

```
BIRTH "capsule/worker.4th"
```

**Pros:** Clean separation of child behavior into files. Aligns with existing capsule
infrastructure. Easy to version and swap behaviors.  
**Cons:** Requires capsule loading at runtime; adds I/O dependency at birth time.

### Recommendation (for Captain Bob's review)
**Option A as the primary mechanism, Option C as the boot-time variant.** `SPAWN` is the
runtime word; `BIRTH` loads from a capsule for complex workers. Option B (inline) deferred
until the simpler mechanisms are proven.

---

## 7. Scheduling Model

### 7.1 Cooperative, Inside the Interpreter Loop

No preemption. A child runs until it calls `YIELD` (or an equivalent implicit yield point).
Hera's interpreter loop maintains a **child run queue** — a circular list of live children.

```
Hera's interpreter loop:
  1. Execute one step of the current context (Hera or a child)
  2. If current context yields (or completes a word), rotate to next in queue
  3. If queue is empty, Hera resumes its own execution
```

### 7.2 The Yield Contract

A child must yield if it:
- Completes one "unit of work" (definition TBD — could be N opcodes, or explicit `YIELD`)
- Calls a blocking primitive (I/O wait, timer wait)
- Is done (returns from its top-level word)

**Open question:** Explicit `YIELD` word vs. automatic yield after N Forth words executed?
Explicit is safer and more deterministic. Automatic requires an opcode counter (adds cost to
the inner loop).

### 7.3 Child Registry

Hera maintains a **child registry** — a fixed-size pool of child context slots. The pool
size is a compile-time constant (e.g., `MAX_CHILDREN 4096`). Slots are allocated at `SPAWN`
and freed at child death. The scheduler walks the pool round-robin, skipping dead/suspended slots.

---

## 8. Dictionary and Namespace Strategy

### Option A: Shared Dictionary, No Isolation
All children look up words in Hera's dictionary. Children cannot define words (or can define
words that are global). Simple. Dangerous if children define conflicting words.

### Option B: Shared Dictionary, Per-Child Vocabulary
Each child has an active **vocabulary** (Forth-79 vocabulary stack). Children can push a
local vocabulary onto the stack, define words into it, and those words are visible only while
the child's vocabulary is active. On child death, the vocabulary is popped and garbage-collected.

### Option C: Read-Only Dictionary for Children
Children can only *call* words defined in Hera's dictionary. They cannot define new words.
New word definition is a privilege reserved for Hera.

### Recommendation
**Option C first** (safest, simplest). Promote to Option B once Option C is stable. Option A
is a footgun at scale.

---

## 9. Inter-Child and Child-Hera Communication

### 9.1 Message Passing via Shared Channels
Hera maintains a set of named **channels** — simple FIFO queues in shared memory. A child
posts a cell (or word reference) to a channel; another child or Hera reads from it.

```
CHANNEL workers-out    \ Hera creates a channel
SPAWN my-job           \ child writes to workers-out
workers-out RECEIVE    \ Hera reads from it
```

### 9.2 Shared Variables in Hera's Memory
Children can read and write `VARIABLE` words defined in Hera's dictionary — the standard
Forth `@` / `!` memory model. Coordination is via Forth-level protocol (no locks in the
no-threads model).

### 9.3 Direct Stack Handoff (at birth/death)
Hera pushes values onto its own stack before `SPAWN`; the child inherits those values on its
data stack at birth. On child death, the child's top-of-stack can be posted back to Hera.

---

## 10. Hades Interface — Options for Decision

Hades is the kernel + HAL. Two design philosophies:

### Option A: Children Have No Direct Hades Access
All Hades calls (I/O, timer, memory) are mediated by Hera. A child calls a Hera-defined
word like `HERA-EMIT`, which validates and proxies to Hades. Children are fully sandboxed
behind Hera.

**Pros:** Hera can audit, throttle, or deny any child's access to Hades. Clean capability model.  
**Cons:** All I/O goes through Hera — potential bottleneck if Hera itself is doing other work.

### Option B: Children Share Hera's Hades Capability
Children inherit Hera's Hades access. Calling `EMIT`, `KEY`, `BLOCK`, etc. in a child goes
directly to Hades — same as calling it from Hera. No proxy overhead.

**Pros:** Simple — no proxy layer. Consistent behavior.  
**Cons:** No isolation. A misbehaving child can flood I/O or trigger kernel calls Hera didn't
authorize.

### Option C: Capability Tokens at Birth
At `SPAWN` time, Hera grants the child a **capability set** — a bitmask of what Hades calls
it may make. The child's Hades interface checks the capability before proceeding.

**Pros:** Fine-grained, auditable.  
**Cons:** Most complex to implement.

### Recommendation
**Option A initially** (simplest sandboxing). Promote to Option C once the threat model
for what children can do is better understood. Option B is acceptable if children are
trusted workers rather than untrusted code.

---

## 11. Physics / SSM Integration

### The Key Question
Does each child get its own L8 Jacquard mode and rolling window? Or does Hera's SSM observe
all children collectively?

### Option A: Global SSM (Hera observes everything)
Hera's rolling window, heartbeat, and L8 mode apply to the whole system — all word executions
by all children are logged into Hera's rolling window. The SSM sees the aggregate workload.

**Pros:** No per-child physics overhead. Consistent with the current single-VM architecture.  
**Cons:** Individual child behavior is invisible to the SSM. A noisy child could perturb
Hera's mode selector.

### Option B: Per-Child SSM (each child has its own physics)
Each child gets a lightweight SSM context — its own entropy/CV/temporal metrics. L8 mode
is computed per-child. The heartbeat coordinates across all children.

**Pros:** Fine-grained adaptive tuning per workload.  
**Cons:** Per-child SSM overhead at thousands of children is significant. Heartbeat
coordination across thousands of SSM contexts is an unsolved design problem.

### Option C: Hybrid — Hera SSM + Child Sampling
Hera's SSM runs globally. Per-child, a lightweight "heat token" accumulates hot words. On
child death, the heat token is merged back into Hera's global heat map. No per-child L8.

### Recommendation
**Option A for the initial architecture.** Option C is worth revisiting once children are
stable and we have empirical data on how child execution affects Hera's SSM modes. Option B
is deferred — too much complexity before the basic birth/death cycle works.

---

## 12. Open Decisions — Requires Captain Bob Approval

The following must be decided before any code is written:

| # | Decision | Options | Recommendation |
|---|----------|---------|----------------|
| D1 | Spawn mechanism | A: SPAWN, B: BEGET, C: BIRTH | A primary, C for capsule-boot |
| D2 | Default child class | Thin (task) / Medium (worker) / Full (VM) / Mixed | Captain Bob decides; system can support all three |
| D3 | Child stack size | 256 / 512 / 1024 / configurable per class | Captain Bob decides; not constrained by Hera's current size |
| D4 | Max children | 1024 / 4096 / dynamic (Hera allocates as needed) | Dynamic preferred; Hera expands her pool on demand |
| D5 | Dictionary access | A: shared, B: per-vocabulary, C: read-only | Captain Bob decides per class (thin=C, full=own) |
| D6 | Yield model | Explicit YIELD word vs. automatic every N ops | Explicit YIELD (deterministic) |
| D7 | Hades access | A: Hera proxies, B: direct, C: capability tokens | Captain Bob decides; class-dependent makes sense |
| D8 | SSM scope | A: global, B: per-child, C: hybrid | Global for thin/medium; per-child for full VMs |
| D9 | Child naming | Anonymous handles vs. named (registered in dict) | Named (registered in Hera's namespace) |
| D10 | Child definition word | SPAWN vs. some other Forth word name | Captain Bob decides |
| D11 | Hera expansion strategy | Elastic arena / large fixed pool / multi-segment | Captain Bob decides; drives how Hera's VM struct is refactored |

---

## 13. What This Is NOT

To keep the plan honest:

- **Not a thread scheduler.** There are no threads. This is cooperative Forth-level multitasking.
- **Not an OS process model.** Children share Hera's address space (thin/medium) or are isolated sub-VMs (full class).
- **Not a security boundary by default.** Without capability enforcement (D6-C), children are trusted workers.
- **Not a priority system.** All children are equal in the round-robin queue (initially).
- **Not constrained by Hera's current implementation.** Hera expands as needed — the plan drives the code, not the reverse.

---

## 14. Proposed Implementation Phases

These phases are for planning reference only. No code written until Captain Bob approves the plan.

| Phase | Scope | Deliverable |
|-------|-------|-------------|
| 0 | Design approval | This document, signed off by Captain Bob |
| 1 | Hera expansion | Refactor Hera's VM struct and memory arena per D11; establish child pool infrastructure |
| 2 | Child context struct | `ChildVM` struct covering all three classes; lifecycle states; allocator |
| 3 | Scheduler | Round-robin queue in Hera's interpreter loop; `YIELD` word |
| 4 | `SPAWN` word | Birth a thin child from a named Forth word |
| 5 | Child death | Cleanup, stack reclaim, registry slot free |
| 6 | Communication | Named channels, stack handoff at birth/death |
| 7 | `BIRTH` word | Capsule-based child initialization |
| 8 | Medium children | Private memory slices, per-vocabulary dictionary |
| 9 | Full children | Complete sub-VM; per-child SSM if D8-B chosen |
| 10 | Hades interface | Proxy or capability layer per D7 |
| 11 | Scale testing | Thin: 10K+, Medium: 1K+, Full: 100+ child stress tests |

---

## 15. Mythological Framing Reference

| Name | Role |
|------|------|
| **Hades** | Kernel + HAL — the underworld substrate; eternal, unchanging |
| **Hera** | Primary VM — queen, eternal, gives birth to children, governs them |
| **Children** | Child VMs — born of Hera, mortal, do work, return to Hera on death |
| **Capsules** | The divine scripts (`.4th` init files) — a child's destiny at birth |
| **SPAWN / BIRTH** | The Forth words that enact creation |
| **YIELD** | A child's act of deference — stepping aside for Hera or siblings |
| **DONE / die** | A child's natural death — completing its purpose |

---

*This document is a planning artifact only. No code changes have been made.  
Captain Bob's approval of the open decisions in §12 is required before Phase 1 begins.*
