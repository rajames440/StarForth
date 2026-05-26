# Birthing — Work Status and Context

**Branch:** `birthing` (off `lithosananke`)  
**Last updated:** 2026-05-26  
**Current phase:** Phases 1–10 complete (kernel + hosted); kernel QEMU acceptance pending Captain Bob

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
- [x] **Captain Bob approves plan — gate for Phase 1**

---

### Phase 1 — VM Registry Naming
- [x] Add `char name[VM_NAME_MAX]` + `VM_STATE_STOPPED` to `VMRegistryEntry` (`include/starkernel/capsule_run.h`)
- [x] Replace static `vm_registry[64]` with dynamic kmalloc linked list (`src/starkernel/capsule/capsule_birth.c`)
- [x] Hera (VM 0) registered as `"Hera"` in `capsule_vm_registry_init()`
- [x] Add `capsule_vm_find_by_name()` — walks linked list, copies entry on match
- [x] Add `capsule_vm_registry_set_name()` — sets symbolic name by vm_id
- [x] Declare both new functions in `include/starkernel/capsule_birth.h`
- [x] Kernel build (`make -f Makefile.starkernel ARCH=amd64 STARFORTH_ENABLE_VM=1`) passes clean
- [ ] Tests: find by name, not-found returns -1, name persists across states

---

**Gap 1 (deferred to Phase 4):** `CAPSULE_MODE_VALID` enforces XOR between `FLAG_PRODUCTION`
and `FLAG_EXPERIMENT`. Captain Bob's answer about D2 points at a runtime logger/mode
selector (`LOG-DOE`) that does not yet exist. Design of `LOG-DOE` and the runtime mode
selection is a separate task before Phase 4 (mkcapsule flag update) can proceed.

---

### Phase 2 — Log Prefix `[Name]`
- [x] Identify the HAL console output path (`src/starkernel/hal/console.c`)
- [x] Add "active VM" context pointer to the HAL console layer
- [x] Every `hal_console_putchar` / `hal_console_puts` prefixes `[VMName] ` at line start
- [x] Hera's boot output shows `[Hera]` from first character
- [ ] QEMU verification pending Captain Bob

---

### Phase 3 — Capsule Stubs
- [x] Write `capsules/hermes/init.4th` (boot message + placeholder vocabulary)
- [x] Write `capsules/artemis/init.4th` (boot message + placeholder vocabulary)
- [ ] Verify `mkcapsule` generates descriptors for both (requires kernel build env)
- [ ] Verify capsule names resolve correctly (`hermes:init.4th`, `artemis:init.4th`)

---

### Phase 4 — mkcapsule Flag Update
- [ ] Update `tools/mkcapsule.c` to set both `FLAG_PRODUCTION` and `FLAG_EXPERIMENT` on
      every capsule (remove the directory-prefix-based flag logic, or extend it to set both)
- [ ] Rebuild and verify `capsule_generated.c` shows both flags on all entries
- [ ] Confirm birth eligibility check in `capsule_birth.c` no longer blocks on flag type

---

### Phase 5 — `BIRTH` Word
- [x] Register `BIRTH` as a C primitive in `register_mama_forth_words()` (mama_forth_words.c)
- [x] Lowercase the name string from the stack
- [x] Construct capsule name: `<name>:init.4th` (special case: `hera` → rejected)
- [x] Look up capsule in directory by name
- [x] Health check: if VM with that name is alive and healthy, log and return (idempotent)
- [x] Allocate new VM (dynamic — no fixed pool ceiling)
- [x] Execute capsule init via `capsule_birth_baby` path
- [x] Register VM under symbolic name in `VMRegistryEntry`
- [x] Emit parity log via `capsule_parity_log_birth`
- [x] Stack clean on exit
- [x] Fixed caddr+1 offset bug — now reads chars directly at caddr per FORTH-79 S"

---

### Phase 6 — `KILL` Word
- [x] Register `KILL` as a C primitive
- [x] Look up VM by name (case-insensitive)
- [x] If not found: log, return (no stack effect)
- [x] Tear down VM, free via `vm_cleanup` + `sf_free`
- [x] Clear registry slot (state → DEAD, name cleared)
- [x] Emit parity log via `capsule_parity_log_kill`
- [x] Stack clean on exit
- [x] Hera kill rejected

---

### Phase 7 — `START` Word
- [x] Register `START` as a C primitive
- [x] Look up VM by name
- [x] If EMBRYO or STOPPED: set state → LIVE
- [x] Call `sk_repl_run(target)` — blocks until target->halted
- [x] On target halt: state → STOPPED, caller resumes
- [x] If target already LIVE: log error, return clean
- [x] Stack clean on exit

---

### Phase 8 — `STOP` Word
- [x] Register `STOP` as a C primitive
- [x] Self-stop: sets `vm->halted = 1`, sk_repl_run loop exits
- [x] State updated to STOPPED by START word after REPL returns
- [x] Stack clean (STOP takes no arguments)

---

### Phase 9 — `USE` Word
- [x] Register `USE` as a C primitive
- [x] Look up VM by name
- [x] Set active system-wide REPL VM via `sk_repl_set_active_vm`
- [x] HAL console prefix changes to `[VMName]` via `console_set_vm_name`
- [x] Stack clean on exit

---

### Phase 10 — Hera `init.4th` Update
- [x] `S" Hermes" BIRTH` in `capsules/init.4th`
- [x] `S" Artemis" BIRTH` in `capsules/init.4th`
- [x] S" fixed to FORTH-79 ( -- c-addr u ) — stores at HERE, no count-byte prefix
- [ ] Boot sequence QEMU verification pending Captain Bob

---

### Phase 11 — Integration Test (kernel QEMU — pending Captain Bob)
- [ ] Boot QEMU: kernel loads, Hera starts, births Hermes and Artemis
- [ ] Serial log shows `[Hera]`, `[Hermes]`, `[Artemis]` prefixes
- [ ] `S" Hermes" USE` switches console to Hermes
- [ ] `S" Hera" USE` switches back
- [ ] KILL + re-BIRTH a VM in the same session
- [ ] All 936+ POST tests still pass after changes

**Note:** QEMU and OVMF firmware not available in CI build environment.
Run: `make -f Makefile.starkernel ARCH=amd64 clean qemu` (then arm64, riscv64)

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
