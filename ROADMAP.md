# StarForth Roadmap: VM → Kernel → OS → FPGA

**Version**: 1.0
**Date**: 2025-12-14
**Timeline**: 2025-2028 (3.5 years)
**Status**: Phase 0 Complete (VM), Phase 1 Starting (HAL)

---

## VISION

**From**: Adaptive FORTH VM running on hosted OS (Linux)
**To**: Self-hosting operating system running on custom FPGA hardware

```
2025 (NOW)          2026                2027                2028
    │                 │                   │                   │
    V                 V                   V                   V
┌─────────┐      ┌──────────┐       ┌────────────┐      ┌──────────┐
│StarForth│  →   │StarKernel│   →   │ StarshipOS │  →   │   FPGA   │
│   VM    │      │(Bare Metal)      │(Self-hosting)     │ Hardware │
└─────────┘      └──────────┘       └────────────┘      └──────────┘
  COMPLETE         6-8 months          12-18 months       12-18 months
```

**Key Principle**: Each phase builds on the previous. No skipping steps.

---

## CURRENT STATE (Phase 0: COMPLETE ✅)

### What We Have

**StarForth VM** - Production-ready adaptive FORTH interpreter:
- ✅ FORTH-79 compliant (780+ tests passing)
- ✅ Adaptive runtime (7 feedback loops, 0% algorithmic variance)
- ✅ Platforms: Linux, L4Re/Fiasco.OC
- ✅ Performance: 100M iterations/sec, profile-guided optimization
- ✅ Quality: 9.2/10 codebase rating, strict ANSI C99
- ✅ Validation: Peer-reviewed, 90-run statistical proof
- ✅ Patent pending: Adaptive runtime mechanisms

**Documentation**:
- ✅ 121-page academic paper
- ✅ Scientific defense documents (9 files)
- ✅ Ontology & formal definitions
- ✅ HAL architecture (documented, not implemented)

**Infrastructure**:
- ✅ Make-based build system (40+ targets)
- ✅ DoE (Design of Experiments) framework
- ✅ Reproducibility guarantees
- ✅ CI-ready test suite

### What's Missing

**See**: [docs/GAP_ANALYSIS.md](docs/GAP_ANALYSIS.md) for complete list (37 gaps identified)

**Critical Blockers**:
1. HAL abstraction layer (8 components) - **Blocks everything**
2. StarKernel implementation (13 components)
3. StarshipOS subsystems (12 components)
4. FPGA feasibility & design (19 components)

---

## PHASE 1: HAL IMPLEMENTATION (Months 1-2)

**Goal**: Make StarForth platform-agnostic via Hardware Abstraction Layer

**Why**: HAL is the foundation for all future work. Without HAL, cannot build kernel or port to FPGA.

### Deliverables

| # | Component | LOC | Duration | Status |
|---|-----------|-----|----------|--------|
| 1.1 | Define HAL interfaces (6 headers) | 500 | 1 week | 📋 Planned |
| 1.2 | Implement Linux HAL | 1500 | 2 weeks | 📋 Planned |
| 1.3 | Migrate VM core to HAL | 800 | 2 weeks | 📋 Planned |
| 1.4 | Migrate physics subsystems | 300 | 1 week | 📋 Planned |
| 1.5 | Validate determinism (DoE) | Testing | 2 days | 📋 Planned |

**Total**: ~3,100 LOC, 6-7 weeks

### Success Criteria

- ✅ All HAL interface headers exist (`hal_time.h`, `hal_interrupt.h`, etc.)
- ✅ Linux HAL implementation complete
- ✅ VM core has zero direct platform dependencies
- ✅ All 780+ tests pass on HAL
- ✅ 0% algorithmic variance maintained
- ✅ No measurable performance regression

### Milestones

**Week 2**: HAL headers defined, Linux implementation started
**Week 4**: Linux HAL complete, VM migration started
**Week 6**: All tests passing, determinism validated

**Blocker Risk**: Low (well-understood refactoring)

---

## PHASE 2: STARKERNEL (Months 3-5)

**Goal**: Boot StarForth on bare metal (UEFI → kernel → "ok" prompt)

**Why**: Kernel enables true OS development and demonstrates platform independence.

### Architecture

```
┌─────────────────────────────────────────────┐
│ UEFI Firmware                               │
│ • Memory map, ACPI, framebuffer             │
└────────────────┬────────────────────────────┘
                 │ ExitBootServices()
                 ▼
┌─────────────────────────────────────────────┐
│ StarKernel Boot (boot/uefi_loader.c)        │
│ • Collect BootInfo                          │
│ • Jump to kernel_main()                     │
└────────────────┬────────────────────────────┘
                 ▼
┌─────────────────────────────────────────────┐
│ StarKernel HAL (platform/kernel/)           │
│ • CPU init (GDT, IDT, interrupts)           │
│ • Memory (PMM, VMM, heap)                   │
│ • Time (TSC, HPET, APIC timer)              │
│ • Console (UART + framebuffer)              │
└────────────────┬────────────────────────────┘
                 ▼
┌─────────────────────────────────────────────┐
│ StarForth VM                                │
│ • vm_create()                               │
│ • Physics subsystems                        │
│ • REPL → "ok" prompt                        │
└─────────────────────────────────────────────┘
```

### Deliverables

| # | Component | LOC | Duration | Status |
|---|-----------|-----|----------|--------|
| 2.1 | UEFI boot loader | 800 | 2 weeks | 📋 Planned |
| 2.2 | Physical memory manager (PMM) | 1200 | 2 weeks | 📋 Planned |
| 2.3 | Virtual memory manager (VMM) | 1500 | 2 weeks | 📋 Planned |
| 2.4 | Kernel heap (kmalloc) | 800 | 1 week | 📋 Planned |
| 2.5 | CPU initialization (GDT/IDT) | 1000 | 1 week | 📋 Planned |
| 2.6 | UART driver (serial console) | 600 | 1 week | 📋 Planned |
| 2.7 | Time subsystem (TSC/HPET) | 1000 | 2 weeks | 📋 Planned |
| 2.8 | APIC interrupt controller | 1200 | 2 weeks | 📋 Planned |
| 2.9 | Framebuffer driver | 800 | 1 week | 📋 Planned |
| 2.10 | ACPI parsing | 600 | 1 week | 📋 Planned |
| 2.11 | HAL implementation (kernel) | 1000 | 2 weeks | 📋 Planned |
| 2.12 | Build system integration | 500 | 1 week | 📋 Planned |

**Total**: ~11,000 LOC, 12-14 weeks

### Success Criteria

- ✅ UEFI boots on QEMU + OVMF
- ✅ Serial console prints "StarKernel booting..."
- ✅ PMM/VMM allocate memory correctly
- ✅ Kernel heap works (kmalloc/kfree)
- ✅ Interrupts and timers functional
- ✅ VM starts and prints "ok" prompt
- ✅ Can execute simple FORTH code (`1 2 + .` → `3`)
- ✅ Physics subsystems operational on bare metal

### Milestones

**Month 3**: UEFI boot + serial output working
**Month 4**: Memory managers complete, VM starts
**Month 5**: Full HAL working, "ok" prompt on QEMU

**Blocker Risk**: Medium (complex low-level code, but well-documented)

### Testing Strategy

**Development**: QEMU with OVMF (UEFI firmware)
**Validation**: Physical hardware (Intel NUC or similar)
**Automation**: QEMU headless testing in CI

---

## PHASE 3: STARSHIPOS (Months 6-18)

**Goal**: Self-hosting operating system (compiles itself, runs applications)

**Why**: Demonstrates complete independence from Linux/POSIX, enables FORTH-native ecosystem.

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    StarshipOS Layers                     │
├─────────────────────────────────────────────────────────┤
│ Userland                                                │
│ • Shell, editors, compilers, tools                      │
├─────────────────────────────────────────────────────────┤
│ System Services                                         │
│ • Filesystem (FAT32 or custom)                          │
│ • Networking (TCP/IP stack)                             │
│ • Process manager, scheduler                            │
├─────────────────────────────────────────────────────────┤
│ Drivers                                                 │
│ • Storage (AHCI, NVMe)                                  │
│ • Network (VirtIO, E1000)                               │
│ • Block/char device abstraction                         │
├─────────────────────────────────────────────────────────┤
│ StarKernel (from Phase 2)                               │
│ • CPU, memory, time, interrupts                         │
└─────────────────────────────────────────────────────────┘
```

### Deliverables (Staged)

#### Stage 3A: Storage (Months 6-9)

| # | Component | LOC | Duration | Status |
|---|-----------|-----|----------|--------|
| 3.1 | AHCI driver | 2500 | 1 month | 📋 Planned |
| 3.2 | NVMe driver | 2000 | 1 month | 📋 Planned |
| 3.3 | Block device abstraction | 1500 | 2 weeks | 📋 Planned |
| 3.4 | FAT32 filesystem | 3000 | 1 month | 📋 Planned |
| 3.5 | VFS layer | 2000 | 2 weeks | 📋 Planned |

**Subtotal**: ~11,000 LOC, 3-4 months

**Success Criteria**:
- ✅ Read/write files to disk
- ✅ Boot from hard drive (not just RAM)
- ✅ Persistent FORTH definitions

---

#### Stage 3B: Networking (Months 10-12)

| # | Component | LOC | Duration | Status |
|---|-----------|-----|----------|--------|
| 3.6 | VirtIO-net driver | 2000 | 1 month | 📋 Planned |
| 3.7 | E1000 driver | 1500 | 2 weeks | 📋 Planned |
| 3.8 | lwIP TCP/IP stack (port) | 8000 | 2 months | 📋 Planned |
| 3.9 | Socket API (FORTH-native) | 1500 | 2 weeks | 📋 Planned |

**Subtotal**: ~13,000 LOC, 3-4 months

**Success Criteria**:
- ✅ Send/receive packets
- ✅ TCP connections work
- ✅ Simple HTTP server in FORTH

---

#### Stage 3C: Process Model (Months 13-15)

| # | Component | LOC | Duration | Status |
|---|-----------|-----|----------|--------|
| 3.10 | FORTH task abstraction | 2000 | 1 month | 📋 Planned |
| 3.11 | Scheduler (round-robin) | 1500 | 2 weeks | 📋 Planned |
| 3.12 | IPC mechanism | 2500 | 1 month | 📋 Planned |
| 3.13 | System call interface | 1000 | 2 weeks | 📋 Planned |

**Subtotal**: ~7,000 LOC, 2-3 months

**Success Criteria**:
- ✅ Multiple concurrent tasks
- ✅ Task switching works
- ✅ Tasks can communicate (send/receive)

---

#### Stage 3D: Self-Hosting (Months 16-18)

| # | Component | LOC | Duration | Status |
|---|-----------|-----|----------|--------|
| 3.14 | FORTH compiler (meta-compilation) | 3000 | 1 month | 📋 Planned |
| 3.15 | Assembler (FORTH-hosted) | 2000 | 2 weeks | 📋 Planned |
| 3.16 | Linker/loader | 1500 | 2 weeks | 📋 Planned |
| 3.17 | Shell & core utilities | 2500 | 1 month | 📋 Planned |
| 3.18 | Build system (FORTH-based) | 1000 | 2 weeks | 📋 Planned |

**Subtotal**: ~10,000 LOC, 2-3 months

**Success Criteria**:
- ✅ StarshipOS compiles itself
- ✅ No dependency on Linux toolchain
- ✅ Can build applications entirely in FORTH

---

**Phase 3 Total**: ~41,000 LOC, 12-15 months

### Milestones

**Month 9**: Persistent storage working, boot from disk
**Month 12**: Network stack operational, HTTP demo
**Month 15**: Multitasking working, IPC functional
**Month 18**: Self-hosting achieved, OS compiles itself

**Blocker Risk**: High (large scope, complex subsystems)

**De-risking Strategy**: Port existing code where possible (lwIP for TCP/IP, FAT32 reference)

---

## PHASE 4: FPGA FEASIBILITY (Months 6-7)

**Goal**: Determine if FPGA implementation is worth pursuing

**Why**: FPGA is a massive investment (12-18 months). Need go/no-go decision before committing.

### Feasibility Study

| # | Task | Duration | Deliverable |
|---|------|----------|-------------|
| 4.1 | Survey existing FORTH FPGA cores | 1 week | Comparison table |
| 4.2 | Estimate resource usage (LUTs, BRAMs) | 3 days | Resource budget |
| 4.3 | Select target FPGA | 3 days | FPGA selection rationale |
| 4.4 | Prototype stack machine (Verilog) | 2 weeks | Minimal stack machine |
| 4.5 | Synthesize & test on FPGA board | 1 week | Synthesis report |
| 4.6 | Performance analysis | 3 days | Speed vs. software |
| 4.7 | Cost/benefit analysis | 2 days | Go/no-go recommendation |

**Total**: 5-6 weeks

### Decision Criteria

**GO if**:
- ✅ Fits in affordable FPGA (~$200-500 board)
- ✅ Performance ≥ 50 MHz clock (faster than software?)
- ✅ Adaptive runtime feasible in hardware (critical!)
- ✅ Development time justified by benefits

**NO-GO if**:
- ❌ Too large for affordable FPGAs
- ❌ Slower than software implementation
- ❌ Adaptive runtime not synthesizable
- ❌ >18 months to implement (opportunity cost too high)

### Prototype Spec

**Minimal Stack Machine** (proof of concept):
- Data stack (16 entries × 64-bit)
- Return stack (16 entries × 64-bit)
- Basic operations: +, -, *, /, DUP, DROP, SWAP
- Dictionary ROM (256 entries)
- Simple UART I/O

**Target**: Prove concept works in hardware, get resource usage numbers

---

## PHASE 5: FPGA IMPLEMENTATION (Months 8-24)

**Status**: CONDITIONAL (only if Phase 4 says GO)

**Goal**: Hardware FORTH processor on FPGA

### Architecture

```
┌────────────────────────────────────────────────┐
│               FPGA Top Module                  │
├────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐           │
│  │ Stack Machine│  │   Memory     │           │
│  │  • Data stack│  │  Controller  │           │
│  │  • Return stk│  │  • Dict ROM  │           │
│  │  • ALU       │  │  • RAM       │           │
│  └──────────────┘  └──────────────┘           │
├────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐           │
│  │     UART     │  │    Timers    │           │
│  │  IP Core     │  │  Counters    │           │
│  └──────────────┘  └──────────────┘           │
├────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────┐ │
│  │   Adaptive Runtime (if feasible)         │ │
│  │   • Execution counters                   │ │
│  │   • Hot-word cache                       │ │
│  │   • Hardware decay (?)                   │ │
│  └──────────────────────────────────────────┘ │
└────────────────────────────────────────────────┘
```

### Deliverables

#### Stage 5A: Core Implementation (Months 8-13)

| # | Component | LOC (HDL) | Duration | Status |
|---|-----------|-----------|----------|--------|
| 5.1 | Data stack (16 deep) | 500 | 2 weeks | 📋 Conditional |
| 5.2 | Return stack (16 deep) | 500 | 2 weeks | 📋 Conditional |
| 5.3 | ALU (arithmetic/logic ops) | 2000 | 1 month | 📋 Conditional |
| 5.4 | Dictionary memory (ROM) | 1000 | 2 weeks | 📋 Conditional |
| 5.5 | RAM controller | 1500 | 3 weeks | 📋 Conditional |
| 5.6 | Instruction decoder | 2000 | 1 month | 📋 Conditional |
| 5.7 | Control FSM | 1500 | 3 weeks | 📋 Conditional |

**Subtotal**: ~9,000 lines Verilog, 4-5 months

---

#### Stage 5B: I/O & Peripherals (Months 14-16)

| # | Component | LOC (HDL) | Duration | Status |
|---|-----------|-----------|----------|--------|
| 5.8 | UART transmitter/receiver | 800 | 2 weeks | 📋 Conditional |
| 5.9 | Timer/counter IP cores | 600 | 1 week | 📋 Conditional |
| 5.10 | Interrupt controller | 1000 | 2 weeks | 📋 Conditional |
| 5.11 | Memory-mapped I/O bus | 1200 | 3 weeks | 📋 Conditional |

**Subtotal**: ~3,600 lines Verilog, 2-3 months

---

#### Stage 5C: Adaptive Runtime (Months 17-20)

**CRITICAL UNCERTAINTY**: Can we synthesize this?

| # | Component | LOC (HDL) | Duration | Status |
|---|-----------|-----------|----------|--------|
| 5.12 | Execution frequency counters | 1500 | 1 month | 📋 Conditional |
| 5.13 | Hot-word cache (top-K) | 2000 | 1 month | 📋 Conditional |
| 5.14 | Hardware decay logic | 1000 | 2 weeks | 📋 Conditional |
| 5.15 | Transition tracking | 1500 | 3 weeks | 📋 Conditional |

**Subtotal**: ~6,000 lines Verilog, 3-4 months

**Fallback**: If adaptive runtime doesn't synthesize well, implement in software on soft-core CPU (MicroBlaze/NIOS II) alongside FORTH core.

---

#### Stage 5D: Validation & Optimization (Months 21-24)

| # | Task | Duration | Deliverable |
|---|------|----------|-------------|
| 5.16 | Testbench (all modules) | 2 months | 100% coverage |
| 5.17 | Formal verification (critical paths) | 1 month | Proofs |
| 5.18 | Synthesis optimization | 1 month | Timing closure |
| 5.19 | Hardware-in-loop testing | 2 weeks | Physical validation |
| 5.20 | Performance tuning | 2 weeks | Final optimizations |

**Subtotal**: 4-5 months

---

**Phase 5 Total**: ~18,600 lines Verilog + testbenches, 15-18 months

### Target FPGAs (Options)

| FPGA | Cost | Resources | Speed | Notes |
|------|------|-----------|-------|-------|
| **Xilinx Zynq-7000** | $300 | 85K LUTs, ARM core | 200 MHz | **Recommended**: Hybrid ARM+FPGA |
| Intel Cyclone V | $250 | 77K LEs | 150 MHz | Good alternative |
| Lattice iCE40 UP5K | $50 | 5K LUTs | 50 MHz | Budget option, slower |
| Xilinx Artix-7 | $200 | 33K LUTs | 200 MHz | Pure FPGA, no ARM |

**Recommendation**: Zynq-7000 (ARM + FPGA fabric)
- Run StarshipOS on ARM
- Offload FORTH core to FPGA fabric
- Best of both worlds

### Success Criteria

- ✅ FPGA boots, prints "ok" via UART
- ✅ Can execute FORTH code
- ✅ Performance ≥ software on same workload
- ✅ Adaptive runtime functional (or gracefully degraded)
- ✅ 780+ tests pass on hardware
- ✅ 0% algorithmic variance maintained (if adaptive works)

### Milestones

**Month 13**: Core stack machine working in simulation
**Month 16**: I/O functional, boots on real FPGA
**Month 20**: Adaptive runtime implemented (or fallback decided)
**Month 24**: Full validation, production-ready FPGA core

**Blocker Risk**: VERY HIGH
- HDL expertise required
- Timing closure can be unpredictable
- Adaptive runtime may not be synthesizable
- Resource constraints on affordable FPGAs

---

## INTEGRATION TIMELINE

### Parallel Paths

**Months 1-5**: Sequential (HAL → Kernel)
**Months 6-18**: **Parallel** (OS + FPGA feasibility)
**Months 8-24**: **Conditional** (FPGA impl if feasible)

```
2025        2026                          2027                2028
  │           │                             │                   │
  V           V                             V                   V
Phase 1   Phase 2                       Phase 3              Phase 5
  HAL     Kernel                           OS                FPGA
  (2m)     (3m)       ┌──────────────────(12m)────────────┐    (16m)
           │          │                                    │     │
           │    Phase 4: Feasibility (2m)                 │     │
           │          └─────┬─────────────────────────────┘     │
           │                │                                   │
           └────────────────┴──GO/NO-GO Decision───────────────┘
                            │
                      If NO-GO: Focus on OS only
                      If GO: Parallel OS + FPGA work
```

### Resource Allocation (Solo Development)

**Months 1-5** (HAL + Kernel): 100% focus
**Months 6-7** (Feasibility): 80% OS, 20% FPGA study
**Months 8-18** (Parallel): 70% OS, 30% FPGA (if GO)
**Months 19-24** (FPGA finish): 50% OS, 50% FPGA

**Community Contributions**: Welcomed at all stages (see CONTRIBUTING.md)

---

## RISK MITIGATION

### High-Risk Items

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **HAL breaks determinism** | Low | Critical | Extensive testing, DoE validation |
| **Kernel never boots** | Medium | Critical | Incremental dev, QEMU testing |
| **FPGA too slow** | Medium | High | Feasibility study gates investment |
| **Adaptive runtime unsynthesizable** | High | High | Software fallback on ARM core |
| **Project too large (solo)** | High | Medium | Community outreach, phased delivery |
| **FPGA cost too high** | Low | Medium | Target affordable boards ($200-500) |

### Fallback Plans

**If HAL migration fails**: Continue hosted-only, defer kernel
**If kernel too hard**: Focus on L4Re platform instead
**If OS scope too large**: Deliver minimal components (storage + network only)
**If FPGA infeasible**: Software-only path is still valuable

---

## DELIVERABLES SUMMARY

### By End of 2025 (Months 1-2)
- ✅ HAL implementation complete
- ✅ StarForth platform-agnostic
- ✅ All tests passing on HAL

### By Mid-2026 (Months 3-5)
- ✅ StarKernel boots to "ok" on QEMU
- ✅ Physics subsystems work on bare metal

### By End of 2026 (Months 6-12)
- ✅ Storage subsystem operational
- ✅ Network stack functional
- ✅ FPGA feasibility determined (GO/NO-GO)

### By Mid-2027 (Months 13-18)
- ✅ Process model working
- ✅ StarshipOS self-hosting
- ✅ (If FPGA GO): Prototype FPGA core functional

### By End of 2027 (Months 19-24)
- ✅ StarshipOS production-ready
- ✅ (If FPGA GO): FPGA core validated

### By 2028 (Future)
- 🎯 Ecosystem growth (applications, libraries)
- 🎯 FPGA optimizations & variants
- 🎯 Community-driven features

---

## SUCCESS METRICS

### Technical Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| **HAL overhead** | <5% | Benchmark comparison |
| **Kernel boot time** | <1 second | QEMU timing |
| **OS compile speed** | <10 min | Self-hosting build |
| **FPGA clock speed** | ≥50 MHz | Synthesis report |
| **Test pass rate** | 100% (780+) | All platforms |
| **Determinism** | 0% CV | DoE validation |

### Project Health Metrics

| Metric | Target | Current |
|--------|--------|---------|
| **Code quality** | ≥9.0/10 | 9.2/10 ✅ |
| **Test coverage** | ≥80% | ~90% ✅ |
| **Documentation** | Complete | Strong ✅ |
| **Community** | ≥10 contributors | 1 (solo) |
| **GitHub stars** | ≥500 | TBD |

---

## COMMUNITY & ECOSYSTEM

### How to Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**High-value contributions**:
1. HAL implementations (new platforms)
2. Kernel drivers (AHCI, NVMe, network)
3. FPGA expertise (Verilog, synthesis)
4. Testing & validation
5. Documentation improvements

### Seeking Collaborators

**Needed expertise**:
- Kernel development (UEFI, memory management)
- FPGA design (Verilog/VHDL)
- OS design (filesystems, networking)
- Formal verification (HDL proofs)

**Contact**: rajames440@gmail.com (Robert A. James)

---

## CONCLUSION

**This roadmap is ambitious but achievable.**

**Key Success Factors**:
1. ✅ Strong foundation (Phase 0 complete)
2. ✅ Clear milestones (phased delivery)
3. ✅ Risk mitigation (fallback plans)
4. ⚠️ Community growth (critical for timeline)
5. ⚠️ FPGA decision (feasibility gates investment)

**Realistic Timeline**:
- Solo: 3.5 years (42 months)
- With 3-5 contributors: 2 years (24 months)
- With 10+ contributors: 1.5 years (18 months)

**The vision is clear. The path is defined. Now we build.**

---

**Next Steps**:
1. Read [docs/GAP_ANALYSIS.md](docs/GAP_ANALYSIS.md) for detailed gaps
2. Start Phase 1: HAL implementation
3. Join the project (see CONTRIBUTING.md)

**License**: See ./LICENSE
