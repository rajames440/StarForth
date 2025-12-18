# StarKernel Architecture Outline - Part I

## The Correct Mental Model

**There are no traditional processes.**

StarKernel is:
- **VM scheduler** - Allocates execution windows to VM instances
- **Hardware owner** - Manages physical resources (CPU, memory, devices)
- **Arbitration layer** - Mediates capability-based access and communication

Everything runnable is a **StarForth VM instance**.

No PIDs. No fork/exec. No "user vs kernel mode" in the POSIX sense.

Just:
- VM instances
- Capabilities
- Messages
- Time slices

**That's it.** Clean. Elegant. Dangerous in the good way.

---

## Terminology (Critical - Names Shape Architecture)

**Purge "process" from all documentation.** Use this vocabulary instead:

| Old OS Term | StarKernel Term |
|-------------|-----------------|
| Process | VM Instance |
| Thread | Execution Context (inside a VM) |
| Scheduler | VM Arbiter |
| Syscall | Kernel Word |
| Driver | Service VM |
| Kernel Task | Privileged VM |
| User Space | VM Space (all of it) |
| Context Switch | VM Transition |

This is not semantic bikeshedding. **Names shape architecture.**

If we say "process," we inherit 50 years of UNIX baggage.

If we say "VM instance," we create new design space.

---

## What "VM as the Execution Unit" REALLY Means

At boot:

1. **StarKernel initializes hardware** (GDT, IDT, PMM, VMM, timers, console)
2. **StarKernel creates VM[0]** - The primordial VM

VM[0]:
- Owns the dictionary root
- Defines core words
- May spawn other VMs (clones, forks, templates, whatever term you choose)

StarKernel only knows:
- **VM IDs** (not PIDs)
- **Memory regions** (ownership, capabilities)
- **Run state** (runnable, waiting, blocked, dead)
- **Capabilities** (what each VM is allowed to do)

**There is no concept of "kernel threads."**

StarKernel itself is mostly **inert** unless:
- Timer fires (heartbeat, scheduling quantum expires)
- Interrupt fires (hardware event)
- VM yields or traps (explicit control transfer)
- Arbitration is required (resource conflict, message delivery)

This is closer to:
- **Smalltalk** image semantics (everything is an object, VM is the world)
- **Erlang** actors (isolated, message-passing, supervision trees)
- **L4 microkernel** philosophy (minimal kernel, everything else is a service)

…but **more radical** because the VM is the execution substrate.

---

## Scheduling Model (VM-First)

Scheduling is **not**:
```
schedule threads → assign CPU time to thread → context switch
```

Scheduling **is**:
```
schedule VM execution windows → grant time quantum to VM → VM transition
```

Each VM gets:
- **Time quantum** - How long it runs before preemption
- **Memory envelope** - Which pages it can access
- **Capability vector** - What kernel words it can invoke

**Inside the VM:**
- You can do cooperative multitasking
- Or green threads (Forth tasks, coroutines)
- Or nothing at all (single-threaded execution)

**StarKernel doesn't care.** It schedules VMs, not code.

This lines up beautifully with your heartbeat / steady-state work:
- Each VM has its own execution heat map
- Each VM has its own rolling window of truth
- Each VM has its own physics state (decay, inference, etc.)

StarKernel tracks VM-level metrics:
- Execution time consumed
- Heat entropy (how "hot" is this VM's dictionary)
- Message queue depth
- Page fault rate

---

## IPC / Communication

**No shared memory by default.**

Communication is:
- **Message passing** (asynchronous, queued)
- **Event queues** (interrupt-driven notifications)
- **Optional shared regions** (only if explicitly granted via capabilities)

Think:
```
VM A  --msg-->  VM B
```

**Not**:
```
task_struct → mm → fd → sighand → files → signal → 🤮
```

Example message flow:

1. VM A invokes `SEND ( vmid msg -- )` kernel word
2. StarKernel validates:
   - Does VM A have `CAP_SEND` to VM B?
   - Is msg buffer within VM A's memory envelope?
3. StarKernel copies message to VM B's receive queue
4. VM B wakes (if blocked on `RECV`)
5. VM B invokes `RECV ( -- msg )` to dequeue message

**Message format is opaque to StarKernel.** It's just bytes.

VMs negotiate protocols among themselves (contracts, schemas, Forth words).

---

## Memory Model (This is Where You're Different)

Each VM gets:
- **Mapped region(s)** - Virtual address ranges backed by physical pages
- **Optional shared pages** - Explicitly granted (e.g., DMA buffers, framebuffer)
- **Entropy / heat metadata** - Your secret sauce (execution heat, pipelining metrics)

StarKernel tracks:
- **Ownership** - Which VM owns which pages
- **Residency** - RAM vs backing store (later: swap, persistent storage)
- **Migration hints** - Which pages are hot/cold for future swapping

**The VM does not see "virtual memory" as an abstraction.**

It sees:
- **Objects** (dictionary entries, blocks, data structures)
- **Blocks** (1KB logical storage units)
- **Cells** (word-sized values)

**That's already how StarForth thinks.**

StarKernel just enforces boundaries:
- VM A cannot read VM B's pages (unless explicitly shared)
- VM A cannot execute non-executable pages
- VM A cannot allocate more memory than its envelope allows

Kernel words for memory:
```forth
MAP-MEM     ( size flags -- addr )      \ Allocate mapped memory
UNMAP-MEM   ( addr size -- )            \ Release memory
SHARE-MEM   ( addr size vmid -- )       \ Share page with another VM
PROTECT     ( addr size flags -- )      \ Change page protections
```

---

## Kernel Words = Contract Boundary

**Instead of syscalls, the kernel exposes words.**

Examples (conceptual):

```forth
\ Time & Scheduling
TICKS        ( -- u64 )                 \ Monotonic time in nanoseconds
YIELD        ( -- )                     \ Voluntarily give up time quantum
SLEEP        ( ns -- )                  \ Block for at least ns nanoseconds

\ VM Lifecycle
SPAWN-VM     ( image caps -- vmid )     \ Create new VM instance
KILL-VM      ( vmid -- )                \ Terminate VM instance
VM-STATUS    ( vmid -- status )         \ Query VM run state

\ Communication
SEND         ( vmid msg -- )            \ Send message to VM
RECV         ( -- msg )                 \ Receive message (blocking)
RECV?        ( -- msg f )               \ Non-blocking receive (f=true if msg available)

\ Memory
MAP-MEM      ( size flags -- addr )     \ Allocate memory region
UNMAP-MEM    ( addr size -- )           \ Release memory region
SHARE-MEM    ( addr size vmid -- )      \ Share memory with another VM

\ Capabilities
GRANT-CAP    ( vmid cap -- )            \ Grant capability to VM
REVOKE-CAP   ( vmid cap -- )            \ Revoke capability from VM
HAS-CAP?     ( cap -- f )               \ Check if current VM has capability

\ Device I/O (mediated by capabilities)
MMIO-READ    ( paddr -- u64 )           \ Read memory-mapped I/O (requires CAP_MMIO)
MMIO-WRITE   ( u64 paddr -- )           \ Write memory-mapped I/O
IRQ-REGISTER ( irq handler -- )         \ Register interrupt handler (requires CAP_IRQ)
```

**A VM can only invoke what its capability vector allows.**

No capability? Kernel word **faults**. VM dies or degrades gracefully (supervisor decision).

This is **capability-based security** at the language level.

No ambient authority. No UID/GID. No ACLs.

Just: "Do you have the capability token? Yes? Proceed."

---

## Boot Phases (Revised)

Let's rewrite the boot story cleanly:

### Phase 0: Firmware
- UEFI initializes hardware
- Loads `BOOTX64.EFI` (StarKernel boot loader)
- Provides: memory map, ACPI tables, framebuffer

### Phase 1: StarKernel Core
- Boot loader collects `BootInfo` from UEFI
- Exits UEFI boot services
- Jumps to `kernel_main()`

### Phase 2: HAL Initialization
- CPU initialization (GDT, IDT, interrupts)
- Memory subsystem (PMM, VMM, heap)
- Time subsystem (TSC, HPET, APIC timer)
- Console (UART + framebuffer)
- Interrupt controller (APIC)

### Phase 3: VM Arbiter Initialization
- Initialize VM scheduler state
- Set up quantum timer (APIC periodic interrupt)
- Prepare VM ID allocator

### Phase 4: Primordial VM Instantiation
- Allocate VM[0] control block
- Grant VM[0] root capabilities (CAP_SPAWN, CAP_MMIO, CAP_IRQ)
- Map VM[0] memory envelope (e.g., 16MB initial)
- Hydrate dictionary:
  - Core words (arithmetic, stack, control flow)
  - Kernel words (TICKS, YIELD, SPAWN-VM, SEND, RECV, etc.)
  - Service words (device drivers, filesystems, networking - later)

### Phase 5: Start Scheduler
- Enqueue VM[0] as runnable
- Enter VM arbiter loop:
  ```c
  while (1) {
      VM *current = vm_arbiter_next();  // Select next VM to run
      vm_transition(current);           // Load VM state, jump to VM
      // VM runs until:
      //   - Quantum expires (timer interrupt)
      //   - VM yields (YIELD word)
      //   - VM blocks (RECV word, no messages)
      //   - VM faults (invalid memory access, missing capability)
      vm_save_state(current);           // Save VM state
  }
  ```

### Phase 6: Primordial VM Execution
- VM[0] prints: `ok` (the holy grail)
- VM[0] may spawn service VMs:
  - Console service (handles keyboard/screen)
  - Block device service (disk I/O)
  - Network service (NIC driver)
- VM[0] becomes the **supervisor** (init equivalent)

**At no point do "processes" exist.**

Only VM instances, running in time-sliced windows, communicating via messages.

---

## Security Model (Capability-Based)

Traditional OS security:
```
if (uid == 0) { /* you can do anything */ }
else if (gid == disk) { /* you can access /dev/sda */ }
else { /* you're fucked */ }
```

StarKernel security:
```forth
: TRY-MMIO-READ  ( paddr -- u64 )
  HAS-CAP? CAP_MMIO IF
    MMIO-READ
  ELSE
    ." Access denied" CR
    0  \ Return sentinel value
  THEN ;
```

Capabilities are **unforgeable tokens** held by VMs.

Examples:
- `CAP_SPAWN` - Can create new VM instances
- `CAP_KILL` - Can terminate other VMs
- `CAP_MMIO` - Can access memory-mapped I/O
- `CAP_IRQ` - Can register interrupt handlers
- `CAP_SEND(vmid)` - Can send messages to specific VM
- `CAP_TIME_ADMIN` - Can adjust system time (RTC, etc.)

Capabilities can be:
- **Granted** at VM creation (by parent VM)
- **Delegated** (VM A gives CAP_X to VM B)
- **Revoked** (supervisor removes capability)
- **Attenuated** (CAP_MMIO(0x1000..0x2000) - limited range)

**No ambient authority.** Every action requires an explicit capability check.

This is **fundamentally different** from UNIX permissions.

---

## Why This Architecture Matters

### 1. **Avoids POSIX Entirely**
You're not emulating Linux. You're not "UNIX-like."

You're something new: **VM-native execution substrate.**

### 2. **Avoids Linux Emulation Traps**
No need to implement:
- `fork()` / `exec()` (just spawn VM instances)
- `mmap()` / `mprotect()` (VM memory envelopes are first-class)
- `signal()` / `sigaction()` (messages + event queues)
- `pthread` (Forth tasks inside VMs, or cooperative multitasking)

### 3. **Matches Your Research Instincts**
- Execution heat is per-VM
- Rolling window of truth is per-VM
- Physics metrics are per-VM
- Deterministic behavior is VM-local (externally observable)

### 4. **Scales Elegantly**
```
Single VM on bare metal
  ↓
Multiple VMs on one machine (time-slicing)
  ↓
VM migration across machines (later: distributed StarKernel)
  ↓
VM swarm across nodes (Erlang-style supervision trees)
```

### 5. **Patent-able Architecture**
This is **not** a Linux remix.

This is:
- Forth VM as the process model
- Capability-based security at the language level
- Thermodynamically-grounded adaptive runtime per VM instance
- Message-passing IPC with zero-copy shared memory opt-in

**Nobody else is doing this.**

---

## Immediate Action Items

### 1. **Purge "Process" Language**
Search all docs for:
```bash
grep -r "process" docs/
```

Replace with appropriate StarKernel terminology.

### 2. **Update Boot Documentation**
Revise `docs/starkernel/hal/starkernel-integration.md`:
- Remove references to "init system"
- Replace with "primordial VM instantiation"
- Clarify VM arbiter vs traditional scheduler

### 3. **Define Kernel Word Interface**
Create `docs/starkernel/kernel-words.md`:
- Formal specification of each kernel word
- Capability requirements
- Error semantics (what happens on fault)

### 4. **Revise HAL Docs**
Update HAL interface docs to clarify:
- HAL serves StarKernel, not "processes"
- HAL serves VMs indirectly (via kernel words)
- HAL is StarKernel's hardware abstraction, not VM's

### 5. **Update CLAUDE.md**
Add StarKernel architectural vision:
- VM instances are the sole schedulable entities
- No processes, threads, or traditional OS abstractions
- Capability-based security model

---

## Flag Planted on the Hill

**Core Architectural Principle:**

> **StarForth VM instances are the sole schedulable entities in StarKernel.**
>
> There are no processes. There are no threads.
>
> There are only VM instances, capabilities, and messages.
>
> StarKernel is the arbiter, not the executor.

This sentence is your stake in the ground.

Everything else derives from this.

---

## What's Next (Part II will cover)

- **VM Control Block** structure (`struct VMInstance`)
- **Capability Vector** representation
- **VM Arbiter Algorithm** (scheduling policy)
- **Message Queue** implementation
- **VM Lifecycle States** (runnable, blocked, waiting, dead)
- **Interrupt Handling** in VM context
- **Service VMs** (drivers as privileged VM instances)
- **Dictionary Hydration** (ROM → RAM, or live construction)

---

## References / Inspiration

- **L4 Microkernel Family** - Minimal kernel, capability-based IPC
- **seL4** - Formally verified microkernel, capability security
- **Singularity OS** (Microsoft Research) - Type-safe OS, no hardware memory protection
- **EROS / CapROS** - Pure capability systems
- **Smalltalk-80** - Image-based execution, everything is an object
- **Erlang/OTP** - Actor model, supervision trees, message passing
- **Barrelfish OS** - Multikernel, message-passing between cores

**But more radical:** VM as the fundamental unit of execution, Forth as the native language.

---

*Document Version: 1.0*
*Date: 2025-12-17*
*Author: Compiled from architectural clarity session*
