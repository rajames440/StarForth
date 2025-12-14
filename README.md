# StarForth

```
   ███████╗████████╗ █████╗ ██████╗ ███████╗ ██████╗ ██████╗ ██████╗██╗  ██╗
   ██╔════╝╚══██╔══╝██╔══██╗██╔══██╗██╔════╝██╔═══██╗██╔══██╗═██╔══╝██║  ██║
   ███████╗   ██║   ███████║██████╔╝█████╗  ██║   ██║██████╔╝ ██║   ███████║
   ╚════██║   ██║   ██╔══██║██╔══██╗██╔══╝  ██║   ██║██╔══██╗ ██║   ██╔══██║
   ███████║   ██║   ██║  ██║██║  ██║██╗     ╚██████╔╝██║  ██║ ██║   ██║  ██║
   ╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝      ╚═════╝ ╚═╝  ╚═╝ ╚═╝   ╚═╝  ╚═╝
```

<p align="center">
  <strong>A Physics-Driven Adaptive FORTH-79 Virtual Machine</strong><br>
  The Foundation for StarKernel and StarshipOS
</p>

<p align="center">
  <a href="#-key-innovation">Key Innovation</a> •
  <a href="#-quick-start">Quick Start</a> •
  <a href="#-documentation">Documentation</a> •
  <a href="#-roadmap">Roadmap</a> •
  <a href="#-patent-pending">Patent Pending</a>
</p>

---

## ⚠️ PATENT PENDING

**This technology is subject to a provisional patent application filed with the USPTO.**

**Patent Application:** Physics-Grounded Self-Adaptive Runtime System for Virtual Machines
**Filing Date:** December 2024
**Applicant:** Robert A. James
**Status:** Provisional Patent Pending

The physics-driven adaptive runtime, execution heat model, rolling window of truth, and deterministic inference mechanisms described in this repository are proprietary innovations covered by pending patent protection.

**For licensing inquiries, contact:** [Patent licensing contact information]

---

## 🎯 What is StarForth?

**StarForth** is a FORTH-79 compliant virtual machine written in strict ANSI C99, featuring the world's first **physics-grounded self-adaptive runtime** with formally proven deterministic behavior. It achieves what conventional wisdom says is impossible: **adaptive optimization without sacrificing reproducibility**.

**Key Achievement:** **0% algorithmic variance** across 90 experimental runs, proving that adaptive systems can be deterministic and formally verifiable.

### The Innovation

StarForth introduces a novel approach to VM optimization based on a **thermodynamic metaphor**:
- **Execution Heat Model** - Words that execute frequently "heat up"
- **Rolling Window of Truth** - Deterministic execution history capture
- **Inference Engine** - Statistical adaptation using ANOVA and Levene's test
- **Hot-Words Cache** - Frequency-driven dictionary reordering

**Result:** 25.4% performance improvement while maintaining perfect determinism.

### The Vision

StarForth is not just a VM—it's the foundation for an entirely new computing stack:

```
StarForth (NOW)      →  StarKernel (NEXT)     →  StarshipOS (FUTURE)
FORTH-79 VM              UEFI-bootable kernel      Full operating system
Physics runtime          Forth as kernel shell     Storage, networking
Linux, L4Re              Bare metal                Self-hosting
```

---

## 🚀 Key Innovation

### Physics-Driven Adaptive Runtime (Formally Proven)

StarForth implements **seven physics feedback loops** that drive self-optimization while maintaining determinism:

| Loop | Mechanism | Type | Impact |
|------|-----------|------|--------|
| #1 | Execution Heat Tracking | Positive | Identifies hot words |
| #2 | Rolling Window History | Neutral | Deterministic metric seeding |
| #3 | Linear Decay | Negative | Prevents heat accumulation |
| #4 | Pipelining Metrics | Positive | Word transition prediction |
| #5 | Window Width Inference | Adaptive | Levene's test optimization |
| #6 | Decay Slope Inference | Adaptive | Exponential regression tuning |
| #7 | Adaptive Heartrate | Adaptive | System load response |

### Four Formal Theorems (Empirically Validated)

**Theorem 1: Algorithmic Determinism**
- Cache decisions **identical across all runs** (p < 10^-30)
- **0% algorithmic variance** - not noise, mathematical certainty
- Completely reproducible behavior

**Theorem 2: Adaptive Convergence**
- **25.4% performance improvement** via physics-driven optimization
- Early runs: 10.20 ms → Late runs: 7.61 ms
- Measurable, reproducible gains

**Theorem 3: Environmental Robustness**
- Algorithm variance: **0%** | OS noise variance: 60-70%
- Deterministic decisions independent of OS load
- Formal guarantees across deployment environments

**Theorem 4: Predictable Performance**
- 95% confidence intervals enable **formal SLA specification**
- Production systems can guarantee performance bounds
- Statistical validation via Central Limit Theorem

**See the complete study:** [Peer Review Materials](docs/archive/phase-1/Reference/physics_experiment/PEER_REVIEW_SUBMISSION/) (formal theorems, experimental methodology, variance analysis)

---

## ✨ Features

### FORTH-79 Compliance
- ✅ **936+ tests** validating complete FORTH-79 standard compliance
- ✅ **18 word modules** from arithmetic to control structures
- ✅ **Direct-threaded interpreter** for low-latency execution
- ✅ **Block storage subsystem** (RAM-disk and disk-backed)

### Quality & Validation
- ✅ **Strict ANSI C99** - Zero GNU extensions, zero warnings
- ✅ **Platform-agnostic** - Runs on Linux, L4Re/Fiasco.OC, bare metal
- ✅ **Deterministic behavior** - Formally validated, reproducible
- ✅ **Production-ready** - 9.2/10 codebase quality rating

### Performance
- ⚡ **100M iterations/sec** (Fibonacci benchmark)
- ⚡ **Profile-Guided Optimization** (PGO) support
- ⚡ **Architecture-specific assembly** optimizations (x86_64, ARM64, RISC-V)
- ⚡ **LTO + direct threading** for maximum performance

### Developer Experience
- 🛠️ **40+ Make targets** - build, test, benchmark, profile
- 📚 **Comprehensive documentation** - 121-page academic paper + full API docs
- 🧪 **Fail-fast testing** - Quick smoke tests to full test harness
- 🔬 **DoE mode** - Design of Experiments for reproducible validation

---

## 📦 Quick Start

### Build

```bash
# Clone repository
git clone https://github.com/rajames440/StarForth.git
cd StarForth

# Standard build
make

# Maximum performance build
make fastest

# Run interactive REPL
./build/amd64/standard/starforth
```

### Test

```bash
# Run all 936+ tests
make test

# Quick smoke test
make smoke

# Run DoE validation
./build/amd64/fastest/starforth --doe
```

### Verify Determinism

```bash
# Run experiments multiple times
for i in {1..5}; do
    ./build/amd64/fastest/starforth --doe > run_$i.csv
done

# All CSV files should be identical (0% variance)
md5sum run_*.csv
```

---

## 📚 Documentation

### Getting Started
- **[Quick Start Guide](docs/01-getting-started/README.md)** - Installation and first steps
- **[Contributing Guide](CONTRIBUTING.md)** - How to contribute
- **[Developer Setup](docs/01-getting-started/DEVELOPER.md)** - Development environment

### Architecture
- **[Architecture Overview](docs/03-architecture/OVERVIEW.md)** - Complete system architecture
- **[HAL Documentation](docs/03-architecture/hal/)** - Hardware Abstraction Layer
- **[Physics Engine](docs/03-architecture/physics-engine/)** - Adaptive runtime details
- **[Heartbeat System](docs/03-architecture/heartbeat-system/)** - Time-driven coordination

### Research & Validation
- **[Peer Review Materials](docs/archive/phase-1/Reference/physics_experiment/PEER_REVIEW_SUBMISSION/)** - Formal theorems and experimental validation
- **[Experiments](docs/02-experiments/)** - DoE protocols and results
- **[Quality Assurance](docs/04-quality/)** - Testing and validation

### API Reference
```bash
# Generate documentation
make book        # LaTeX → PDF
make book-html   # HTML with dark theme
```

---

## 🗺️ Roadmap

### Phase 1: StarForth VM ✅ COMPLETE
**Status:** Production-ready, formally validated

- ✅ FORTH-79 compliant interpreter
- ✅ Physics-driven adaptive runtime
- ✅ 0% algorithmic variance proven
- ✅ 936+ tests passing
- ✅ Runs on Linux, L4Re/Fiasco.OC

### Phase 2: HAL Migration 🔄 IN PROGRESS
**Status:** Architecture defined, implementation underway

- 🔄 Hardware Abstraction Layer design complete
- 🔄 Platform abstraction (Linux, L4Re, Kernel)
- 🔄 Migration plan documented (7 phases)
- 📋 Validation of determinism across platforms

**Timeline:** 2-4 weeks

### Phase 3: StarKernel 📋 PLANNED
**Status:** Design complete, ready for implementation

StarKernel transforms StarForth into a **UEFI-bootable kernel** with Forth as the native control plane:

- 📋 **UEFI boot loader** - Exit boot services, handoff to kernel
- 📋 **Freestanding C environment** - No libc, direct hardware access
- 📋 **Memory management** - PMM (Physical), VMM (Virtual), kmalloc
- 📋 **Time subsystem** - TSC, HPET, APIC timer
- 📋 **Console** - UART 16550 + GOP framebuffer
- 📋 **Interrupts** - IDT, Local APIC, IOAPIC
- 📋 **Boot to `ok` prompt** - Forth as kernel shell

**Key Features:**
- Forth words expose kernel services (`NOW`, `HB-ON`, `PCI.LS`)
- Physics runtime at kernel level (zero OS jitter)
- Deterministic behavior on bare metal
- Foundation for StarshipOS

**Documentation:** [docs/03-architecture/hal/starkernel-integration.md](docs/03-architecture/hal/starkernel-integration.md)

### Phase 4: StarshipOS 🔮 FUTURE
**Status:** Conceptual design, long-term vision

StarshipOS builds on StarKernel to create a complete operating system:

- 🔮 **Storage** - AHCI, NVMe drivers + FAT32/ext2 filesystem
- 🔮 **Networking** - TCP/IP stack, VirtIO-net, DHCP/DNS
- 🔮 **Process Model** - Forth tasks with scheduling
- 🔮 **Device Model** - Unified block/net/char subsystem
- 🔮 **Security** - Capabilities, ACL, Forth-based access control
- 🔮 **Self-hosting** - Build StarshipOS on StarshipOS

**Vision:** The first physics-native, formally verifiable operating system where Forth is not an app—it's the OS.

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 1: VM Core + Physics Subsystems                      │
│  • FORTH-79 interpreter                                     │
│  • Execution heat model                                     │
│  • Rolling window of truth                                  │
│  • Hot-words cache                                          │
│  • Inference engine                                         │
│  • Heartbeat coordinator                                    │
│                                                              │
│  ↓ Platform-agnostic (calls HAL only)                       │
├─────────────────────────────────────────────────────────────┤
│  Layer 2: Hardware Abstraction Layer (HAL)                  │
│  • hal_time.h - Timing & timers                             │
│  • hal_interrupt.h - IRQ management                         │
│  • hal_memory.h - Memory allocation                         │
│  • hal_console.h - I/O                                      │
│  • hal_cpu.h - CPU control                                  │
│                                                              │
│  ↓ Platform-specific implementations                        │
├─────────────────────────────────────────────────────────────┤
│  Layer 3: Platform Implementations                          │
│  ┌───────────────┬──────────────┬──────────────────────┐   │
│  │ Linux         │ L4Re         │ StarKernel (planned) │   │
│  │ (POSIX)       │ (microkernel)│ (freestanding)       │   │
│  ├───────────────┼──────────────┼──────────────────────┤   │
│  │ clock_gettime │ L4Re::Clock  │ TSC + HPET + APIC    │   │
│  │ malloc/free   │ dataspaces   │ PMM + VMM + kmalloc  │   │
│  │ stdin/stdout  │ L4Re console │ UART + framebuffer   │   │
│  └───────────────┴──────────────┴──────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

**See:** [docs/03-architecture/OVERVIEW.md](docs/03-architecture/OVERVIEW.md) for complete architecture documentation.

---

## 🧪 Validation & Governance

### Academic Rigor
- **90 experimental runs** with statistical validation
- **Design of Experiments (DoE)** methodology
- **ANOVA, Levene's test** for statistical inference
- **Peer-review ready** materials (formal theorems, experimental methodology, proofs)

### Code Quality
- **ANSI C99 compliance** - strict, no warnings
- **936+ unit tests** - comprehensive coverage
- **Determinism validation** - 0% algorithmic variance
- **9.2/10 quality rating** - production-ready

### Standards Compliance
- **FORTH-79 standard** - 100% compliant
- **ISO/IEC best practices** - governance structure
- **Formal verification** - Isabelle/HOL ready

---

## 🤝 Contributing

We welcome contributions aligned with:
- Deterministic behavior and reproducibility
- ANSI C99 standards compliance
- Validation-first engineering
- Formal reasoning and verification

**Before contributing:**
1. Read [CONTRIBUTING.md](CONTRIBUTING.md)
2. Review [docs/CLAUDE.md](docs/CLAUDE.md) for project overview
3. Check [docs/03-architecture/](docs/03-architecture/) for architecture

**Ways to contribute:**
- 🐛 Bug reports and fixes
- 📚 Documentation improvements
- 🧪 Additional test coverage
- ⚡ Performance optimizations (with benchmarks)
- 🔬 Experimental validation
- 🌐 Platform ports (HAL implementations)

---

## 📜 License

**StarForth is released under the Starship License 1.0 (SL-1.0).**

This is a permissive license that grants broad freedoms:
- ✅ **Use** for any purpose (personal, commercial, research, embedded)
- ✅ **Modify** and create derivative works
- ✅ **Distribute** in source or binary form
- ✅ **Relicense** under different terms

**Sole requirement:** Permanent attribution to **R.A. James (Captain Bob)** must be preserved in all distributions and derivative works.

**Patent Notice:** This license does **not** grant patent rights. The physics-driven adaptive runtime innovations are subject to **pending patent protection**. See [Patent Pending](#-patent-pending) section above for licensing inquiries.

**Third-party components:**
- Fiasco.OC and L4Re remain **GPLv2** (external dependencies)

**Full license:** See [LICENSE](LICENSE) file for complete terms.

---

## 👤 Author

**Robert A. James (Captain Bob)**
Systems Engineer, Hacking Since 1973

Built with precision, tested with rigor, documented with care.

---

## 🔗 Related Projects

- **StarshipOS** - Full operating system built on StarKernel (future)
- **StarForth Governance** - Validation and standards compliance repository

---

## 📞 Contact

**For technical questions:**
- Open a [GitHub Issue](https://github.com/rajames440/StarForth/issues)
- See [CONTRIBUTING.md](CONTRIBUTING.md) for development questions

**For patent licensing:**
- [Patent licensing contact information]

**For research collaboration:**
- [Research collaboration contact information]

---

## 🌟 Why StarForth Matters

Modern VMs sacrifice determinism for performance. Adaptive systems are treated as black boxes. Formal verification is considered incompatible with runtime optimization.

**StarForth proves this is wrong.**

This is not just a FORTH implementation—it's a **proof of concept** that adaptive systems can be:
- ✅ Deterministic and reproducible
- ✅ Formally verifiable
- ✅ High-performance
- ✅ Production-ready

The implications extend beyond FORTH:
- **Runtime systems** - Deterministic JIT compilation
- **Operating systems** - Verifiable kernel optimization
- **Real-time systems** - Formal SLA guarantees
- **Safety-critical software** - Adaptive behavior with certification

**StarForth is heritage with a V8 engine. Retro aesthetics, modern science, future-proof architecture.**

---

<p align="center">
  <strong>StarForth: Where Physics Meets Computing</strong><br>
  Deterministic. Adaptive. Verifiable.
</p>

```
╔═════════════════════════════════════════════════════╗
║  STARFORTH: THE FOUNDATION FOR STARKERNEL & BEYOND  ║
╚═════════════════════════════════════════════════════╝
```
