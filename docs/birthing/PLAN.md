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

These are non-negotiable given the target environment:

| Constraint | Implication |
|------------|-------------|
| Same process as Hera | No `fork()`, no `pthread_create()` |
| No threads | All scheduling is cooperative, inside the interpreter loop |
| No OS processes | Children share Hera's file descriptors, address space, signal handlers |
| Thousands of children | Per-child memory must be measured in KB, not MB |
| ANSI C99, `-Wall -Werror` | No GNU extensions, no dynamic language tricks |
| Hades is the substrate | I/O, timing, and memory services come from Hades (the kernel+HAL) |

---

## 3. Memory Reality Check

The current `VM` struct owns approximately:

| Resource | Size | Per-child fate |
|----------|------|----------------|
| `memory[5MB]` | 5 MB | Cannot clone — shared or sliced |
| `data_stack[1024]` + `return_stack[1024]` | 32 KB | **Must own** — each child needs isolated stacks |
| `rolling_window` history buffer | ~16 KB | Shared (Hera's physics observes all) |
| `heartbeat.tick_buffer` | ~800 KB | Shared (one heartbeat for the whole system) |
| `word_id_map[1024]` | 8 KB | Shared (children look up words in Hera's dictionary) |
| Struct metadata, input buffer, compile buffer | ~2 KB | **Must own** — per-child execution context |

**At full clone:** ~5.8 MB × 1,000 children = 5.8 GB. Not feasible.  
**At thin context (stacks + metadata only):** ~34 KB × 1,000 = 34 MB. Feasible.

**Conclusion:** Children must be thin. They share Hera's memory, dictionary, and physics
infrastructure. A child is a *saved execution context* — not a full VM.

---

## 4. The Child VM Model

### 4.1 What a Child Owns

A child VM owns exactly:

1. **Data stack** — isolated, fixed-size (size TBD; likely 256 or 512 cells)
2. **Return stack** — isolated, fixed-size (same)
3. **Instruction pointer (IP)** — where it is currently executing
4. **Mode** — interpret vs. compile (children probably always interpret)
5. **Input buffer + parse state** — for any Forth words it reads
6. **Identity** — a name or handle by which Hera tracks it
7. **Status** — alive, suspended, dying, dead
8. **Parent reference** — pointer back to Hera's VM

### 4.2 What a Child Shares with Hera

- **Memory arena** — children execute Forth code compiled into Hera's memory
- **Dictionary** — children look up and call words defined in Hera's dictionary
- **Hades interface** — Hades HAL calls go through the same kernel paths
- **Physics/SSM state** — Hera's L8 Jacquard and heartbeat observe all execution
- **I/O** — children emit to the same console as Hera (unless redirected)

### 4.3 What a Child Cannot Do

- Define new words that persist after its death (unless explicitly promoted to Hera's dictionary)
- Corrupt Hera's own stacks
- Block Hera (it must yield)
- Access memory outside its permitted slice (if isolation enforced)
- Directly call Hades without going through Hera's capability layer (TBD — see §8)

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
| D2 | Child stack size | 128 / 256 / 512 / 1024 cells | 256 cells (2 KB per stack, 4 KB per child total) |
| D3 | Max children | 256 / 1024 / 4096 / dynamic | 4096 (compile-time constant pool) |
| D4 | Dictionary access | A: shared, B: per-vocabulary, C: read-only | C first, then B |
| D5 | Yield model | Explicit YIELD word vs. automatic every N ops | Explicit YIELD (deterministic) |
| D6 | Hades access | A: Hera proxies, B: direct, C: capability tokens | A first |
| D7 | SSM scope | A: global, B: per-child, C: hybrid | A first |
| D8 | Child naming | Anonymous handles vs. named (registered in dict) | Named (registered in Hera's namespace) |
| D9 | Child definition word | SPAWN vs. some other Forth word name | Captain Bob decides |

---

## 13. What This Is NOT

To keep the plan honest:

- **Not a thread scheduler.** There are no threads. This is cooperative Forth-level multitasking.
- **Not an OS process model.** Children share Hera's address space completely.
- **Not a full VM clone.** Children do not get their own 5 MB memory arenas.
- **Not a security boundary.** Without explicit capability enforcement (D6-C), children are trusted.
- **Not a priority system.** All children are equal in the round-robin queue (initially).

---

## 14. Proposed Implementation Phases

These phases are for planning reference only. No code written until Captain Bob approves the plan.

| Phase | Scope | Deliverable |
|-------|-------|-------------|
| 0 | Design approval | This document, signed off |
| 1 | Child context struct | `ChildVM` struct, pool allocator, lifecycle states |
| 2 | Scheduler | Round-robin queue in Hera's interpreter loop, `YIELD` word |
| 3 | `SPAWN` word | Birth a child from a named Forth word |
| 4 | Child death | Cleanup, stack reclaim, registry slot free |
| 5 | Communication | Named channels, stack handoff at birth/death |
| 6 | `BIRTH` word | Capsule-based child initialization |
| 7 | Hades interface | Proxy layer (if D6-A chosen) |
| 8 | SSM integration | Verify Hera's physics are not perturbed by children |
| 9 | Scale testing | 1K, 4K child stress tests |

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
