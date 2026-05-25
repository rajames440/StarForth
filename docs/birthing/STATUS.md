# Birthing — Work Status and Context

**Branch:** `birthing` (off `lithosananke`)  
**Last updated:** 2026-05-25  
**Current phase:** Phase 0 — awaiting Captain Bob plan approval

---

## Execution Model — Single Process, VM Graph

**Hard constraints:**
- One OS process. No threads. No fork. No shared memory between processes.
- All VMs live in the same heap, same address space.
- The C call stack IS the execution stack.

**The graph:**

VMs are nodes. Edges are relationships — birth (who created whom) and execution (who
STARTed whom). These are distinct:

```
Birth graph (static, ownership):        Execution stack (dynamic, runtime):

        Hera                                 C call stack:
       /    \                                  hera_interp()
   Hermes  Artemis                              └─ vm_start("Hermes")
                                                    └─ hermes_interp()
                                                        └─ vm_start("Artemis")
                                                            └─ artemis_interp()
                                                                └─ STOP → unwind
```

**`START` = push.** Calling `S" Hermes" START` from Hera literally calls Hermes's
interpreter loop from within Hera's C execution context. Hera blocks in the `vm_start()`
call until Hermes returns.

**`STOP` = pop.** A VM stops itself — execution returns up the C call stack to whoever
called `vm_start()`. In Phase 1, STOP is always self-stop. Cross-VM STOP (via identity
assumption) is a future milestone.

**`BIRTH` does not equal `START`.** BIRTH allocates and initializes the VM. START runs it.
A VM can be birthed and left dormant.

**`USE` is orthogonal.** It redirects the REPL I/O independently of which VM is currently
executing. It does not affect the C call stack.

**Any VM can birth sub-VMs.** Hermes can call `S" Companion" BIRTH` — it is not Hera's
exclusive privilege. This makes the birth graph a true directed graph (rooted at Hera),
not a strict two-level tree.

---

## What This Branch Is

We are designing and implementing the multi-VM birth system for LithosAnanke. Hera (the
primary Forth VM) gains the ability to birth, name, start, stop, and kill peer VMs. All
VMs run inside the same process — no threads, no OS processes.

---

## The Three Named VMs

| VM | Role | Capsule |
|----|------|---------|
| **Hera** | Mama VM — orchestrates all others | `capsules/init.4th` (root, exclusive) |
| **Hermes** | Messaging stratum — pub/sub | `capsules/hermes/init.4th` |
| **Artemis** | Storage and memory manager | `capsules/artemis/init.4th` |

**Hades** = kernel + HAL substrate beneath all VMs.

---

## The Five Forth Primitives (C-level, in every VM's dictionary)

All words take a counted string `( c-addr u -- )` and leave the stack clean.

| Word | Behavior |
|------|----------|
| `BIRTH` | Birth a VM from `<name>:init.4th`. Idempotent — if healthy VM exists, skip + log. |
| `KILL` | Destroy a named VM unconditionally. Free resources, clear registry slot. |
| `START` | Begin or resume a named VM. Runs synchronously (Phase 1). |
| `STOP` | Suspend a named VM. Any VM can STOP any other. Saves execution state. |
| `USE` | System-wide REPL/console redirect to the named VM. Not capsule-scoped. |

Example usage:
```forth
S" Artemis" BIRTH
S" Artemis" START
S" Artemis" USE
S" Artemis" STOP
S" Artemis" KILL
```

---

## Resolved Decisions

| # | Decision | Resolution |
|---|----------|-----------|
| D1 | BIRTH name collision | Idempotent — healthy VM: skip and log. No error, no replace. |
| D2 | Capsule production flag | All capsules get both `FLAG_PRODUCTION` + `FLAG_EXPERIMENT`. Birth not gated on flag type. |
| D3 | BIRTH return value | Nothing — stack clean. Pure side-effect word. |
| D4 | USE scope | System-wide REPL redirect. Not bound to any capsule or identity. |
| D5 | STOP authority | Any VM can STOP any other. Identity assumption (future) adds nuance. |
| D6 | Max VMs | Dynamic — Hera allocates as needed. No fixed ceiling. |
| D7 | Log prefix format | `[Name]` — e.g., `[Hera]`, `[Hermes]`, `[Artemis]` |

---

## Capsule Namespace Convention

`mkcapsule` encodes relative paths by replacing `/` with `:`.

| File | Capsule name | Used by |
|------|-------------|---------|
| `capsules/init.4th` | `init.4th` | Hera only |
| `capsules/hermes/init.4th` | `hermes:init.4th` | Hermes |
| `capsules/artemis/init.4th` | `artemis:init.4th` | Artemis |

`BIRTH` maps `S" Artemis"` → lowercase → `artemis:init.4th`.

`capsules/hermes/` and `capsules/artemis/` currently exist but contain only `.gitkeep`.
Stub `init.4th` files must be written before the kernel build runs `mkcapsule`.

---

## Implementation Checklist

### Phase 0 — Design and Approval
- [x] `birthing` branch created off `lithosananke` HEAD
- [x] `docs/birthing/PLAN.md` written
- [x] All seven decisions resolved with Captain Bob
- [x] `docs/birthing/STATUS.md` created (this file)
- [ ] **Captain Bob approves plan — gate for Phase 1**

---

### Phase 1 — VM Registry Naming
- [ ] Add `char name[64]` to `VMRegistryEntry` in `src/starkernel/capsule/`
- [ ] Add `vm_find_by_name()` lookup function
- [ ] Ensure VM 0 (Hera) is registered as `"Hera"` at bootstrap
- [ ] Tests: find by name, not-found returns NULL, name persists across states

---

### Phase 2 — Log Prefix `[Name]`
- [ ] Identify the HAL console output path (`src/starkernel/hal/console.c`)
- [ ] Add "active VM" context pointer to the HAL console layer
- [ ] Every `hal_console_putchar` / `hal_console_puts` prefixes `[VMName] ` at line start
- [ ] Hera's boot output shows `[Hera]` from first character
- [ ] Tests: prefix appears on all output types (`.`, `EMIT`, `TYPE`, `CR`)

---

### Phase 3 — Capsule Stubs
- [ ] Write `capsules/hermes/init.4th` (boot message + placeholder vocabulary)
- [ ] Write `capsules/artemis/init.4th` (boot message + placeholder vocabulary)
- [ ] Verify `mkcapsule` generates descriptors for both
- [ ] Verify capsule names resolve correctly (`hermes:init.4th`, `artemis:init.4th`)

---

### Phase 4 — mkcapsule Flag Update
- [ ] Update `tools/mkcapsule.c` to set both `FLAG_PRODUCTION` and `FLAG_EXPERIMENT` on
      every capsule (remove the directory-prefix-based flag logic, or extend it to set both)
- [ ] Rebuild and verify `capsule_generated.c` shows both flags on all entries
- [ ] Confirm birth eligibility check in `capsule_birth.c` no longer blocks on flag type

---

### Phase 5 — `BIRTH` Word
- [ ] Register `BIRTH` as a C primitive in the base word set (registered before capsule init runs)
- [ ] Lowercase the name string from the stack
- [ ] Construct capsule name: `<name>:init.4th` (special case: `hera` → `init.4th`)
- [ ] Look up capsule in directory by name
- [ ] Health check: if VM with that name is alive and healthy, log and return (idempotent)
- [ ] Allocate new VM (dynamic — no fixed pool ceiling)
- [ ] Execute capsule init via existing `capsule_exec_init` path
- [ ] Register VM under symbolic name in extended `VMRegistryEntry`
- [ ] Emit parity log: `PARITY:BIRTH name=X vm_id=N capsule_id=Y`
- [ ] Stack clean on exit
- [ ] Tests: BIRTH Hermes, BIRTH Artemis, double-BIRTH idempotent, unknown name logs error

---

### Phase 6 — `KILL` Word
- [ ] Register `KILL` as a C primitive
- [ ] Look up VM by name
- [ ] If not found: log, return (no stack effect)
- [ ] Forcibly terminate any pending execution
- [ ] Free VM stacks, dictionary, memory
- [ ] Clear registry slot (state → DEAD, name cleared)
- [ ] Emit parity log: `PARITY:KILL name=X vm_id=N`
- [ ] Stack clean on exit
- [ ] Tests: KILL live VM, KILL stopped VM, KILL unknown name

---

### Phase 7 — `START` Word
- [ ] Register `START` as a C primitive
- [ ] Look up VM by name
- [ ] If EMBRYO or STOPPED: set state → LIVE
- [ ] Call the target VM's interpreter loop directly (C function call — no threads)
      The calling VM blocks inside `vm_start()` until the target returns
- [ ] On target self-STOP or normal exit: `vm_start()` returns, calling VM resumes
- [ ] If target is already LIVE: log error, return clean (cannot double-enter)
- [ ] Stack clean on exit
- [ ] Tests: START Hermes, Hermes runs and returns; START already-live errors; nested
      START (Hera→Hermes→Artemis) unwinds correctly

---

### Phase 8 — `STOP` Word
- [ ] Register `STOP` as a C primitive
- [ ] Phase 1: STOP is self-stop only — saves current VM state, returns from the C
      interpreter loop, unwinding back to whoever called `vm_start()`
- [ ] Save execution state: IP, dsp, rsp, stack contents
- [ ] Set state → STOPPED
- [ ] Return from the C interpreter loop (this is how the C call stack unwinds)
- [ ] Stack clean on exit (from the calling VM's perspective)
- [ ] Cross-VM STOP (any VM stops any other) deferred to identity-assumption milestone
- [ ] Tests: self-STOP returns to START caller; STOP saves state; re-START resumes;

---

### Phase 9 — `USE` Word
- [ ] Register `USE` as a C primitive
- [ ] Look up VM by name
- [ ] Set as the active system-wide REPL/console VM
- [ ] HAL console prefix changes to `[VMName]`
- [ ] All REPL input directed to that VM until next `USE` call
- [ ] Stack clean on exit
- [ ] Tests: USE Hermes, type words, USE Hera to return, USE unknown logs error

---

### Phase 10 — Hera `init.4th` Update
- [ ] Add `S" Hermes" BIRTH` to `capsules/init.4th`
- [ ] Add `S" Artemis" BIRTH` to `capsules/init.4th`
- [ ] Optionally: `S" Hermes" START` and `S" Artemis" START` at boot
- [ ] Boot sequence produces correct `[Name]` output for all three VMs
- [ ] Verify capsule hash changes (content-addressed — hash will change)

---

### Phase 11 — Integration Test
- [ ] Boot QEMU: kernel loads, Hera starts, births Hermes and Artemis
- [ ] Serial log shows `[Hera]`, `[Hermes]`, `[Artemis]` prefixes
- [ ] `S" Hermes" USE` switches console to Hermes
- [ ] `S" Hera" USE` switches back
- [ ] KILL + re-BIRTH a VM in the same session
- [ ] All 936+ POST tests still pass after changes

---

## Key Files

| File | Purpose |
|------|---------|
| `docs/birthing/PLAN.md` | Full architectural plan |
| `docs/birthing/STATUS.md` | This file — living checklist |
| `tools/mkcapsule.c` | Build-time capsule packager |
| `capsules/init.4th` | Hera's init (root, exclusive) |
| `capsules/hermes/init.4th` | Hermes stub (to be written) |
| `capsules/artemis/init.4th` | Artemis stub (to be written) |
| `src/starkernel/capsule/capsule_birth.c` | C-level VM birth logic |
| `src/starkernel/capsule/capsule_loader.c` | Capsule load + exec |
| `src/starkernel/capsule/capsule_registry.c` | Capsule directory |
| `src/starkernel/hal/console.c` | HAL console — add `[Name]` prefix here |
| `include/starkernel/capsule.h` | `CapsuleDesc`, `VMRegistryEntry` structs |
| `include/starkernel/capsule_birth.h` | Birth protocol headers |

---

## Branch and Commit History

```
birthing (HEAD)
  d2766d6  plan: lock all seven decisions into birthing plan
  ba27226  plan: overhaul birthing plan — named VMs, five Forth primitives, capsule namespacing
  d258b12  plan: update birthing plan — Hera is not constrained
  84e1c61  plan: add birthing plan for child VMs born of Hera
  91e0b3d  (lithosananke HEAD at branch point) docs: comprehensive CLAUDE.md update
```

---

## Notes and Open Threads

- **Identity assumption (D5 future):** Cross-VM STOP requires a VM to assume another's
  identity. Not in scope for current phases — design separately. Phase 1 STOP is self-stop.
- **Cooperative scheduling / YIELD:** The single-process graph model means VMs run one at a
  time (depth-first down the call stack). True cooperative interleaving (Hermes and Artemis
  sharing time while Hera waits) would require an explicit `YIELD` mechanism and a trampoline
  scheduler — future milestone, designed separately.
- **Graph depth — CONFIRMED unlimited:** Any VM can BIRTH sub-VMs at any depth. The birth
  graph is a true rooted directed graph. Deep graphs consume C stack space — note for kernel
  stack budgeting. No enforced limit; document safe depth in practice.
- **Hermes pub/sub and Artemis storage:** Their full vocabularies are future work. Current
  phase only needs stub `init.4th` files.
- **`capsules/hermes/` and `capsules/artemis/`** currently contain only `.gitkeep`. Stub
  `init.4th` files must exist before `make -f Makefile.starkernel` calls `mkcapsule`.
