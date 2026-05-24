# Birthing Plan: Child VMs Born of Hera

**Status:** DRAFT — requires Captain Bob approval before any implementation begins  
**Branch:** `birthing` (off `lithosananke`)  
**Authors:** Claude Code + Captain Bob  
**Date:** 2026-05-24

---

## 1. Vision

Three named VMs form the initial constellation of LithosAnanke:

| VM Name | Role |
|---------|------|
| **Hera** | Mama VM — orchestrates all other VMs; runs `capsules/init.4th` |
| **Hermes** | Messaging stratum — pub/sub between VMs; runs `capsules/hermes/init.4th` |
| **Artemis** | Storage and memory manager; runs `capsules/artemis/init.4th` |

**Hades** remains the kernel + HAL substrate beneath all of them.

For now, Hera's job is simply: birth, start, stop, and kill VM instances.
Concurrent scheduling, inter-VM messaging, and storage management are later milestones.

Every VM — including Hermes and Artemis — carries the same lifecycle primitives.
A VM can birth and manage other VMs. The infrastructure is uniform.

---

## 2. Hard Constraints

Fixed by the execution model. Hera's implementation is not constrained — she expands
as the design requires.

| Constraint | Implication |
|------------|-------------|
| Same process as Hera | No `fork()`, no `pthread_create()` |
| No threads | Scheduling is cooperative, inside the interpreter loop |
| No OS processes | VMs share Hera's address space |
| ANSI C99, `-Wall -Werror` | No GNU extensions |
| Hades is the substrate | All I/O, timing, and memory from Hades (kernel+HAL) |

---

## 3. VM Identity and Naming

### 3.1 Every VM Has a Name

The current `VMRegistryEntry` tracks VMs by numeric ID only. This must be extended with a
symbolic name field. A VM's name:

- Is set at birth (derived from the capsule namespace — see §5)
- Is displayed in every log line emitted by that VM
- Is the argument to all lifecycle words: `S" Artemis" BIRTH`, `S" Artemis" KILL`, etc.
- Is unique within a session (no two live VMs share a name)
- Is case-preserved but case-insensitive for lookup

### 3.2 VM Registry Extension

`VMRegistryEntry` needs a `name` field (at minimum `char name[64]`). The name is:
- Set by `BIRTH` at VM creation time
- Cleared on `KILL` (slot becomes reusable)
- Logged in every parity record alongside the numeric VM ID

### 3.3 Log Prefix — Critical

Every line of console output emitted by a VM is prefixed with its name:

```
[Hera]    ok
[Hermes]  pub/sub ready
[Artemis] storage initialized
[Hera]    2 VMs alive
```

This applies to `EMIT`, `TYPE`, `.`, `.S`, `CR`, and all I/O words. The prefix is injected
at the HAL console layer — the VM does not format it; Hades prints it automatically based
on the current active VM context.

---

## 4. The Five Primitive Forth Words

These words are **built-in C primitives** registered in every VM's dictionary at startup.
They are not defined in Forth — they are part of the base word set, available to all VMs.

### Stack signatures use the counted-string convention: `( c-addr u -- )`

---

### `BIRTH  ( c-addr u -- )`

Birth a new VM from the capsule whose namespace matches the given name.

**Mapping:** `S" Artemis"` → lowercase `"artemis"` → capsule name `artemis:init.4th`
(the standard capsule for a named VM is `<name>:init.4th`).

**Sequence:**
1. Lowercase the name string
2. Construct capsule name: `<name>:init.4th`
3. Look up capsule in the directory by name
4. Verify capsule is `FLAG_PRODUCTION` + `FLAG_ACTIVE` and not `FLAG_REVOKED`
5. Allocate a new VM context (full VM — Hera expands her pool as needed)
6. Execute the capsule init (via existing `capsule_exec_init` path)
7. Register VM under the symbolic name in the extended `VMRegistryEntry`
8. Log parity record: `PARITY:BIRTH name=Artemis vm_id=N capsule_id=X`
9. On success: push 0; on failure: push error code

**Open question D1:** What happens if a VM named "Artemis" is already alive?
(Error? Replace? Queue?) — Captain Bob decides.

---

### `KILL  ( c-addr u -- )`

Destroy a named VM. The VM's resources are freed and its registry slot is cleared.

**Sequence:**
1. Look up VM by name in registry
2. If not found: push error code, return
3. Forcibly terminate any pending execution in that VM
4. Free the VM's stacks, dictionary, and memory
5. Clear registry slot (name + state = DEAD)
6. Log parity record: `PARITY:KILL name=Artemis vm_id=N`
7. Push 0 on success

**Note:** KILL is unconditional. It does not wait for the VM to reach a yield point.
At this phase (no concurrent execution), the VM is idle when KILL is called.

---

### `START  ( c-addr u -- )`

Begin or resume execution of a named VM.

**At this phase:** `START` runs the VM to completion (or until it calls `STOP` on itself)
before returning control to the caller. Concurrent interleaved execution is a later milestone.

**Sequence:**
1. Look up VM by name
2. If state is EMBRYO or STOPPED: set state to LIVE, transfer execution
3. If state is LIVE: error (already running)
4. On VM completion or self-STOP: return control to the calling VM
5. Push 0 on success

---

### `STOP  ( c-addr u -- )`

Suspend a named VM. Its execution state is preserved; `START` will resume it.

At this phase, a VM can only STOP itself (called from within the running VM's context).
Hera calling `S" Hermes" STOP` on a different VM is a future capability.

**Sequence:**
1. Verify name matches the current VM
2. Save execution state (IP, stacks)
3. Set state to STOPPED
4. Return control to the VM that called `START`

---

### `USE  ( c-addr u -- )`

Make a named VM the active console target. Subsequent REPL input and output is directed
to and from that VM until another `USE` call or the VM dies.

```forth
S" Hermes" USE    \ REPL now talks to Hermes
: send-test  S" hello" PUBLISH ;
S" Hera" USE      \ REPL returns to Hera
```

**Sequence:**
1. Look up VM by name
2. Set it as the active REPL/console VM
3. Log line prefix changes to `[VMName]`
4. Push 0

---

## 5. Capsule Namespace Convention

### 5.1 Directory Structure

Each named VM owns a subdirectory under `capsules/`:

```
capsules/
├── init.4th                  ← Hera's capsule ONLY (root level, no subdir)
├── hermes/
│   └── init.4th              ← Hermes' capsule
└── artemis/
    └── init.4th              ← Artemis' capsule
```

**`capsules/init.4th` is Hera's exclusive init.** No other VM uses the root-level init.
Do not place general capsules there. Do not recreate the old `conf/` directory.

### 5.2 How mkcapsule Names Capsules

`tools/mkcapsule.c` scans `capsules/` recursively. It encodes relative paths by replacing
`/` with `:`. So:

| File path | Capsule name |
|-----------|-------------|
| `capsules/init.4th` | `init.4th` |
| `capsules/hermes/init.4th` | `hermes:init.4th` |
| `capsules/artemis/init.4th` | `artemis:init.4th` |

### 5.3 BIRTH Name → Capsule Name Mapping

`BIRTH` lowercases the VM name and appends `:init.4th`:

| Forth call | Capsule looked up |
|------------|------------------|
| `S" Hera" BIRTH` | `init.4th` (special case — root) |
| `S" Hermes" BIRTH` | `hermes:init.4th` |
| `S" Artemis" BIRTH` | `artemis:init.4th` |
| `S" MyWorker" BIRTH` | `myworker:init.4th` |

The root-level `init.4th` is a special case: `S" Hera" BIRTH` is how a future VM would
re-birth Hera; at bootstrap, Hera boots from it automatically.

### 5.4 mkcapsule Flag Mapping

The capsule flag (`FLAG_PRODUCTION` vs `FLAG_EXPERIMENT`) is determined by the directory
prefix — **not the file name**. Under the current `mkcapsule` logic:

- `capsules/hermes/init.4th` → does not match `production:*`, `core:*`, or `domains:*`
  → currently gets `FLAG_EXPERIMENT`

**This is a gap.** Production VM capsules (Hermes, Artemis) must be `FLAG_PRODUCTION` to
be `BIRTH`-eligible. The mkcapsule flag logic must be updated to treat `capsules/<name>/`
subdirectories as production capsules, OR the `hermes/` and `artemis/` directories must be
moved under a `production/` prefix.

**Open question D2:** How do named VM capsules get `FLAG_PRODUCTION`?
- Option A: rename directories to `production/hermes/`, `production/artemis/`
  → capsule names become `production:hermes:init.4th`
- Option B: update `mkcapsule.c` to treat any `capsules/<name>/init.4th` (1 level deep,
  no special prefix) as production
- Option C: add an explicit production marker file or naming convention

Captain Bob decides.

---

## 6. Capsule Content (Stubs)

Hermes and Artemis capsule directories exist but are empty (`.gitkeep` only). They need
`init.4th` stubs before the system can BIRTH them.

### `capsules/hermes/init.4th` (stub)

Should:
- Print `[Hermes] initializing` (or similar boot message)
- Define Hermes' pub/sub vocabulary (placeholder for now)
- Leave Hermes in a known state (not just empty)

### `capsules/artemis/init.4th` (stub)

Should:
- Print `[Artemis] initializing`
- Define Artemis' storage/memory vocabulary (placeholder)

**These stubs must be written before `make -f Makefile.starkernel` is run, because
mkcapsule runs at build time and produces `capsule_generated.c` from whatever is in
`capsules/` at that moment.**

---

## 7. VM Lifecycle Model

```
(capsule exists in directory)
          │
          ▼
       [BIRTH]
          │
          ▼
      EMBRYO ──────────────────[KILL]──────► DEAD
          │                                    ▲
       [START]                                 │
          │                                 [KILL]
          ▼                                    │
      LIVE/RUNNING ──────────[STOP]──► STOPPED─┘
          │
       (completes)
          │
          ▼
        DEAD (natural)
```

States:
- **EMBRYO** — VM born (capsule executed) but `START` not yet called
- **LIVE** — executing (at this phase: running synchronously inside `START`)
- **STOPPED** — suspended; state saved; resumable via `START`
- **DEAD** — killed or naturally completed; slot reclaimable

---

## 8. Scheduling Model (Phase 1)

At this phase, scheduling is **sequential and explicit**:

- `START` runs a VM synchronously — the calling VM suspends while the started VM runs
- No round-robin, no time-slicing, no preemption
- The REPL is always in exactly one VM's context at a time
- `USE` switches the REPL/console target

**This is intentionally simple.** Cooperative concurrent scheduling (multiple VMs
interleaved in the interpreter loop) is a future milestone, designed separately after
the basic lifecycle works.

---

## 9. Dictionary and Namespace

The five lifecycle words (BIRTH, KILL, START, STOP, USE) are C primitives in **every VM's
base dictionary**. They are registered at VM initialization, before any capsule init runs.

A child VM born by Hermes or Artemis also has these words — all VMs can manage other VMs.

**Dictionary isolation:** Each VM has its own dictionary. Words defined by Artemis's
`init.4th` do not appear in Hera's dictionary or Hermes' dictionary. VMs communicate
through the messaging layer (Hermes), not shared dictionary state.

---

## 10. Hades Interface

All VMs access Hades through the same HAL interface. At this phase:
- No capability bitmask enforcement
- All VMs can call any HAL function (I/O, memory, time)
- The log prefix (`[VMName]`) is injected by the HAL console layer based on the currently
  executing VM context — not by the VM itself

Capability enforcement (limiting what a child VM can do in Hades) is a future milestone.

---

## 11. Physics / SSM Integration

Each VM has its own independent SSM/L8 Jacquard state — this is already the natural
consequence of each VM being a full VM instance with its own `VM` struct. No shared physics.

The heartbeat runs per-VM. Hera's SSM does not observe Hermes' or Artemis' execution.

---

## 12. Open Decisions — Requires Captain Bob Approval

| # | Decision | Options | Status |
|---|----------|---------|--------|
| D1 | BIRTH name collision | Error / replace / queue | Captain Bob decides |
| D2 | Production flag for named VM capsules | A: `production/` prefix, B: mkcapsule update, C: other | Captain Bob decides |
| D3 | `BIRTH` return value | Push 0/error vs. push VM ID vs. leave nothing | Captain Bob decides |
| D4 | `USE` scope | REPL-only vs. also redirects EMIT/TYPE globally | Captain Bob decides |
| D5 | STOP across VMs | Can Hera STOP Hermes directly, or only self-STOP? | Captain Bob decides |
| D6 | Max named VMs | Fixed pool (e.g., 64) vs. dynamic | Captain Bob decides |
| D7 | Log prefix format | `[Name]` vs. `Name:` vs. `(Name)` vs. other | Captain Bob decides |

---

## 13. What This Is NOT

- **Not a thread scheduler.** No preemption. Sequential execution at this phase.
- **Not a process model.** VMs share Hera's address space.
- **Not a security boundary yet.** No capability enforcement in this phase.
- **Not constrained by Hera's current implementation.** Hera expands as the design requires.

---

## 14. Implementation Phases

No code until Captain Bob approves the plan.

| Phase | Scope | Deliverable |
|-------|-------|-------------|
| 0 | Design approval | This document, signed off |
| 1 | VM registry naming | Add `name[64]` to `VMRegistryEntry`; name lookup functions |
| 2 | Log prefix | HAL console layer emits `[VMName]` prefix per active VM |
| 3 | Capsule stubs | Write `capsules/hermes/init.4th`, `capsules/artemis/init.4th` |
| 4 | mkcapsule flag fix | Resolve D2 — ensure named VM capsules get `FLAG_PRODUCTION` |
| 5 | BIRTH word | C primitive; name→capsule lookup; full VM alloc; registry entry |
| 6 | KILL word | C primitive; teardown; registry clear |
| 7 | START word | C primitive; synchronous execution hand-off |
| 8 | STOP word | C primitive; state save; return to caller |
| 9 | USE word | C primitive; REPL/console redirect |
| 10 | Hera init.4th update | Use new words to birth Hermes and Artemis at boot |
| 11 | Integration test | Boot sequence: Hera → BIRTH Hermes → BIRTH Artemis → logs show names |

---

## 15. Mythological Framing Reference

| Name | Role |
|------|------|
| **Hades** | Kernel + HAL — the underworld substrate |
| **Hera** | Mama VM — queen, orchestrator, eternal; `capsules/init.4th` |
| **Hermes** | Messenger — pub/sub stratum; `capsules/hermes/init.4th` |
| **Artemis** | Hunter/keeper — storage and memory; `capsules/artemis/init.4th` |
| **BIRTH** | The word that brings a new VM into being |
| **KILL** | The word that destroys a VM |
| **START** | The word that gives a VM its first breath (or resumes it) |
| **STOP** | The word that suspends a VM |
| **USE** | The word that shifts the speaker's attention to a VM |

---

*This document is a planning artifact only. No code changes have been made.  
Captain Bob's approval of the open decisions in §12 is required before Phase 1 begins.*
