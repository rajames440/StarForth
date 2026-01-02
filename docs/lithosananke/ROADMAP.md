# LithosAnanke Roadmap

**Branch:** `lithosananke`
**Current:** M7 Complete

---

## Milestone Overview

```
M0  UEFI Boot           ████████████████████ COMPLETE
M1  PMM                 ████████████████████ COMPLETE
M2  VMM                 ████████████████████ COMPLETE
M3  IDT                 ████████████████████ COMPLETE
M4  APIC                ████████████████████ COMPLETE
M5  Timer               ████████████████████ COMPLETE
M6  Heap                ████████████████████ COMPLETE
M7  VM Parity           ████████████████████ COMPLETE
M7.1 Capsules           ████████████░░░░░░░░ DESIGN COMPLETE
M8  REPL                ░░░░░░░░░░░░░░░░░░░░ PLANNED
M9  Block I/O           ░░░░░░░░░░░░░░░░░░░░ PLANNED
M10 Networking          ░░░░░░░░░░░░░░░░░░░░ FUTURE
```

---

## Phase 1: Boot Foundation (M0-M6)

### M0: UEFI Boot + Serial Output

**Goal:** Kernel prints "LithosAnanke booting..." to serial

**Deliverables:**
- [x] UEFI loader (`uefi_loader.c`)
- [x] BootInfo handoff (memory map, ACPI, framebuffer)
- [x] ExitBootServices
- [x] Serial console output

**Validation:**
```
StarKernel UEFI Loader
Loading...
RAW SERIAL UP
```

---

### M1: Physical Memory Manager

**Goal:** Track and allocate physical page frames

**Deliverables:**
- [x] Bitmap allocator
- [x] Parse UEFI memory map
- [x] `pmm_alloc_pages()` / `pmm_free_pages()`
- [x] Statistics reporting

**Validation:**
```
PMM initialized.
PMM statistics:
  Total pages: 249450
  Free pages : 247921
```

---

### M2: Virtual Memory Manager

**Goal:** 4-level paging, identity + higher-half mapping

**Deliverables:**
- [x] PML4 → PDPT → PD → PT setup
- [x] CR3 switch
- [x] `vmm_map()` / `vmm_unmap()`
- [x] Self-test

**Validation:**
```
VMM initialized (mapped RAM, CR3 switched)
VMM self-test: mapped OK at 0xffff800000000000
```

---

### M3: Interrupt Descriptor Table

**Goal:** CPU exceptions and IRQ handling

**Deliverables:**
- [x] 256-entry IDT
- [x] ISR stubs (asm)
- [x] Exception handlers (div-by-zero, page fault, etc.)
- [x] IRQ routing framework

**Validation:**
```
IDT installed.
```

---

### M4: APIC Timer

**Goal:** Local APIC initialization and timer IRQs

**Deliverables:**
- [x] Local APIC enable
- [x] APIC timer configuration
- [x] Spurious interrupt vector

**Validation:**
```
APIC: init...
APIC enabled (SIVR=0xFF).
APIC: init done
```

---

### M5: Timer Calibration

**Goal:** Accurate time measurement

**Deliverables:**
- [x] TSC frequency detection
- [x] HPET calibration (when available)
- [x] PM Timer fallback
- [x] Relative vs. absolute trust levels

**Validation:**
```
Timer: init...
Timer: trust=1 (0=NONE,1=REL,2=ABS), TSC=0 Hz
Timer: init done
```

---

### M6: Kernel Heap

**Goal:** `kmalloc()` / `kfree()` working

**Deliverables:**
- [x] Heap initialization
- [x] Allocation tracking
- [x] Statistics

**Validation:**
```
Kernel heap initialized.
Heap statistics:
  Total bytes: 16777176
  Free  bytes: 16777176
```

---

## Phase 2: VM Integration (M7)

### M7: VM Parity Validation

**Goal:** StarForth VM boots with reproducible dictionary hash

**Deliverables:**
- [x] VMHostServices abstraction
- [x] VM arena allocation (5 MB)
- [x] FORTH-79 word registration (295 words)
- [x] Parity checkpoint logging
- [x] Heartbeat thread start

**Validation:**
```
VM: bootstrap parity...
[HAL][host] VMHostServices table registered
VM arena allocated: 0xffff900000000000 (5 MB)
Registering FORTH-79 Standard word set...
PARITY:M7.1a word_count=295 here=0x30 latest_id=294 hash=0x684bbf2fa1d96d55
PARITY:OK
VM: parity bootstrap complete
Starting heartbeat...
APIC Timer: started
```

**Commit:** `6f350bc` — M7: StarForth VM integration with parity validation

---

## Phase 3: Capsule Architecture (M7.1)

### M7.1: Init Capsule System

**Goal:** Content-addressed, immutable init capsules for VM birth

**Status:** Design Complete (see [M7.1.md](M7.1.md))

**Core Concepts:**

| Concept | Description |
|---------|-------------|
| **DOMAIN** | Mama-only construction space — never visible to babies |
| **PERSONALITY** | Baby-only identity — result of executing (p) INIT |
| **(p) Production** | Truth-bearing capsules that birth VMs |
| **(e) Experiment** | Mama-only workloads for DoE |

**Birth Protocol:**
1. Mama selects one production `(p)` capsule by content hash
2. Mama validates eligibility (ACTIVE, PRODUCTION, not REVOKED)
3. Mama allocates new VM
4. INIT blocks copied to execution window (RAM blocks 0–2047)
5. INIT blocks executed sequentially
6. Execution window cleared
7. VM begins life with PERSONALITY imprinted
8. Mama logs `PARITY:BIRTH vm_id=N capsule_id=X mode=p ...`
9. Mama increments `capsule.birth_count`

**Deliverables:**
- [ ] `CapsuleDesc` struct (64 bytes, cache-aligned)
- [ ] `CapsuleDirHeader` struct
- [ ] xxHash64 implementation (freestanding)
- [ ] `capsule_validate()` function
- [ ] Birth protocol (`PARITY:BIRTH` logging)
- [ ] DoE run logging (`CapsuleRunRecord`)
- [ ] `mkcapsule` build tool

**Key Design Decisions:**
- Content-addressed: `capsule_id == content_hash`
- (p) Production vs (e) Experiment modes
- One truth per VM — no shared/implicit base INITs
- Mama holds all truths — (e) capsules never touch babies
- Twins/variants are just VMs with same/similar capsules

---

## Phase 4: Interactive Forth (M8)

### M8: REPL + Interactive Forth

**Goal:** Type Forth at the kernel, get `ok` prompt

**Deliverables:**
- [ ] Keyboard input (PS/2 or USB HID)
- [ ] REPL loop integration
- [ ] Line editing (backspace, minimal)
- [ ] Word execution from console

**Validation:**
```
LithosAnanke v0.3.0
ok 1 2 + .
3 ok
```

---

## Phase 5: Persistence (M9)

### M9: Block Storage

**Goal:** Read/write blocks to disk

**Deliverables:**
- [ ] AHCI driver (SATA)
- [ ] Block device abstraction
- [ ] `BLOCK` / `BUFFER` / `UPDATE` / `FLUSH` words
- [ ] Persistent dictionary

---

## Future Milestones

### M10: Networking
- VirtIO-net driver
- TCP/IP stack (minimal)
- DHCP client

### M11: Process Model
- Forth tasks
- Scheduling
- IPC

### M12: Self-Hosting
- Compile Forth on LithosAnanke
- Edit/assemble/link cycle

---

## Validation Commands

```bash
# Build
make -f Makefile.starkernel ARCH=amd64 STARFORTH_ENABLE_VM=1

# Run QEMU
make -f Makefile.starkernel ARCH=amd64 STARFORTH_ENABLE_VM=1 qemu

# Clean
make -f Makefile.starkernel ARCH=amd64 clean-kernel
```

---

## Branch Relationship

```
master (hosted StarForth)
    │
    ├── lithosananke (kernel branch)
    │       │
    │       └── M7 complete, M7.1 design complete
    │
    └── starkernel-junkyard (legacy, can delete)
```

---

## Success Criteria

LithosAnanke is successful when:

1. **M7 Parity** — VM dictionary hash reproducible across boots
2. **M7.1 Capsules** — Birth protocol enforced, provenance logged
3. **M8 REPL** — Interactive Forth at bare metal
4. **M9 Persistence** — State survives reboot

---

*The foundation is laid. The necessity is clear.*
