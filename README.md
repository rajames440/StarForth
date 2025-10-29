nownow ![assetts/banner.png](banner.png)

```
   ███████╗████████╗ █████╗ ██████╗ ███████╗ ██████╗ ██████╗ ██████╗██╗  ██╗
   ██╔════╝╚══██╔══╝██╔══██╗██╔══██╗██╔════╝██╔═══██╗██╔══██╗═██╔══╝██║  ██║
   ███████╗   ██║   ███████║██████╔╝█████╗  ██║   ██║██████╔╝ ██║   ███████║
   ╚════██║   ██║   ██╔══██║██╔══██╗██╔══╝  ██║   ██║██╔══██╗ ██║   ██╔══██║
   ███████║   ██║   ██║  ██║██║  ██║██╗     ╚██████╔╝██║  ██║ ██║   ██║  ██║
   ╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝      ╚═════╝ ╚═╝  ╚═╝ ╚═╝   ╚═╝  ╚═╝
                   A Clean FORTH-79 VM for the Modern Age — Fast. Lean. Better machine.
```

# 🧠 **StarForth**

### *Come see how I `stack up`* 🧱⚡

> “Born in 199. Rebuilt for 2025.
> The fastest rat rod in Ohio — and maybe your next favorite VM.”

---

## 🚀 **What is StarForth?**

**StarForth** is a clean, modern, **FORTH-79** implementation — written entirely in **strict ANSI C99**, designed for *
*speed**, **predictability**, and **surgical control** over memory and execution.

It’s the beating heart of [StarshipOS](https://github.com/rajames440/StarshipOS) 🛸, but it’s also a **standalone, embeddable
virtual machine**. You can run it on bare metal, inside L4Re, in QEMU, or integrate it into your own projects.

StarForth isn't bloated.
StarForth isn't cute.
**StarForth is a goddamn precision engine.**

---

## 📋 **Comprehensive Codebase Review**

**[→ Read the Complete Technical Review](.README_ADDENDUM.md)**

An in-depth analysis of the entire StarForth codebase including:

- Architecture analysis (VM design, execution model, memory management)
- Implementation quality assessment (ANSI C99 compliance, error handling, safety)
- Test infrastructure review (936 tests across 18 modules)
- Build system evaluation (40+ Make targets, PGO, cross-compilation)
- Governance & compliance (Tier I/II validation, standards compliance)
- Performance analysis (benchmarks, optimizations)

**Overall Rating: 9.2/10 - PRODUCTION-READY**

---

## 🚀 **Getting Started**

### 📚 **Complete Documentation**

**[Read the Full Documentation →](docs/src/README.adoc)**

Our comprehensive documentation covers everything from quick-start guides to deep architecture dives:

- **Quick Start & Installation** – Build StarForth on Linux, L4Re, Raspberry Pi, and more
- **Architecture Deep-Dive** – Understand the VM internals, block storage, and word system
- **API Reference** – Auto-generated from Doxygen with navigation to all source files
- **Building & Testing** – Detailed guides for PGO, cross-compilation, and optimization
- **Platform Integration** – L4Re, seL4, and platform abstraction layer documentation

### 🤝 **Contributing**

Want to improve StarForth? **[Start Here →](CONTRIBUTE.md)**

Our contribution guide covers:

- How to set up your development environment
- Code standards and review process
- Architecture and design principles
- Testing requirements and workflows
- How to report issues effectively

---

## ✅ **Governance & Validation**

StarForth is governed and validated through the [StarForth Governance Repository](https://github.com/rajames440/StarForth-Governance):

- **FORTH-79 Compliance:** 675+ test suite validates 100% standard compliance
- **ANSI C99:** Strict compilation with zero warnings
- **Portability:** Proven across Linux, L4Re/Fiasco.OC, seL4, and embedded systems
- **Component Independence:** Platform abstraction layer enables use in multiple hypervisors

See [StarshipOS Governance](https://github.com/rajames440/StarshipOS-Governance) for how StarForth integrates across three phases:

- **Phase 1 (NOW):** L4Re/Fiasco.OC foundation
- **Phase 2 (COMING):** seL4 formally-verified microkernel
- **Phase 3 (FUTURE):** Standalone microkernel/OS environment

---

## 🧰 **Core Features**

| Feature                          | Description                                                                                     |
|----------------------------------|-------------------------------------------------------------------------------------------------|
| 🧱**FORTH-79 Core**              | Full compliance with the Forth-79 standard, no compromises, no “toy” shortcuts.                 |
| ⚡**Direct-Threaded Interpreter** | 64-bit aligned, aggressively optimized for low-latency execution.                               |
| 💾**Block Storage Subsystem**    | Supports RAM-disk and disk-image backed storage. Works with NVMe, SATA, eMMC, and QEMU devices. |
| 🧠**Strict Pointer Discipline**  | `STRICT_PTR` mode enforces hard pointer safety checks — no sloppy segfault roulette.            |
| 🌀**Modular Word Dictionary**    | 18+ modules, from arithmetic to defining words, easily extendable.                              |
| 🔬**Entropy-Ready Internals**    | Hooks for runtime statistical profiling and ML-assisted memory placement.                       |
| 💻**Amber Glow**                 | Retro-inspired terminal aesthetics. When you see that amber code block, you know you’re home.   |

---

## 📂 **Repository Layout**

```
starforth/
 ├─ src/               # Core VM and subsystems
 ├─ include/           # Public headers (strict C99)
 ├─ word_source/       # Modular word definitions (Forth-79 vocabulary)
 ├─ tests/             # 675+ functional tests (fail-fast harness)
 ├─ disks/             # Disk images for RAM/disk-backed experiments
 ├─ docs/              # Full AsciiDoc + LaTeX documentation
 └─ StarForth-Governance/ # [READ-ONLY] Governance policies (submodule)
```

**⚠️ Note on StarForth-Governance:** This is a read-only git submodule. Only maintainers can modify governance files. If
you need to propose governance changes, create a GitHub issue. See [CONTRIBUTE.md](CONTRIBUTE.md) for details.

---

## 🧱 **Architecture Overview**

```text
            +-------------------+       +---------------------------+
            |  Command-Line UX  |       |  Test Runner / Benchmarks |
            |  (REPL & CLI)     |       |  (src/test_runner)        |
            +---------+---------+       +---------------+-----------+
                      |                                 |
                      v                                 v
            +-------------------+       +---------------------------+
            |   VM Front Door   |       |   Word Registry & Loader  |
            |   (src/main.c)    |       |   (word_source/*)         |
            +---------+---------+       +---------------+-----------+
                      |                                 |
                      v                                 v
            +-----------------------------------------------+
            |            Core VM Engine (src/vm.c)          |
            |  • Data/return stacks     • Dictionary state   |
            |  • Interpreter & compiler • Numeric base       |
            +-------------------+---------------------------+
                                |
                                v
+-------------------------------+-------------------------------------+
| Memory & Block Subsystems                                         |
| • Dictionary allocator (memory_management.c)                       |
| • Block I/O factory (blkio_*.c)                                    |
| • Logical→physical mapper (block_subsystem.c)                      |
+-------------------------------+-------------------------------------+
                                |
                                v
                    +-------------------------------+
                    | Platform Glue (src/platform/) |
                    | • POSIX timers / minimal init |
                    | • Optional L4Re bindings      |
                    +-------------------------------+
```

**Onboarding Flow**

- Start at `src/main.c` to see VM setup, CLI flags, and how the test harness attaches.
- Inspect `src/vm.c` for execution lifecycle; it links directly to the word registry declared in `word_registry.h`.
- Follow calls into `memory_management.c` and `block_subsystem.c` to understand how dictionary space and blocks are
  managed.
- Review `src/platform/linux/` for host integration and `src/test_runner/` to learn how modules exercise words
  end-to-end.

---

## ⚙️ **Building**

```bash
make clean
make
```

Optional flags:

* `STRICT_PTR=1` – enforce pointer safety
* `USE_ASM_OPT=1` – enable architecture-specific assembler optimizations
* `ARCH_X86_64=1` – target 64-bit x86 (default)
* `ARCH_ARM64=1` – target ARM (experimental)

Example:

```bash
make CFLAGS="-DSTRICT_PTR=1 -DUSE_ASM_OPT=1 -DARCH_X86_64=1 -O3" build/starforth
```

### Portable Build Tips

- Override `CFLAGS` with `-O0 -g -static` removed (`make CFLAGS="$(BASE_CFLAGS) -O2" LDFLAGS=""`) when targeting glibc
  variants that dislike static linking.
- Use `make STRICT_PTR=0` only when porting; restore the default once platform issues are resolved to keep bounds checks
  active.
- Cross-compiles should set `CC` and append architecture defines rather than editing the Makefile, e.g.
  `make CC=clang CFLAGS="$(BASE_CFLAGS) -target armv7a-none-eabi"`.
- For non-GNU linkers, pass a clean `LDFLAGS="-L/path/to/lib"` to avoid `-fuse-linker-plugin`; add `USE_ASM_OPT=0`
  temporarily if inline assembly is unsupported.

---

## 🧪 **Testing**

We don’t play “maybe it works.”
StarForth ships with a **full test harness**: 675+ tests covering arithmetic, stack ops, control structures, dictionary
internals, block I/O, and more.

```bash
make test
```

Fail-fast. Log-rich. Deterministic.

Need a sub-second sanity check? Run:

```bash
make smoke
```

This pipes `1 2 + .` through the VM with logging disabled and verifies the output is `3`.

---

## 💾 **Running**

You can run StarForth directly, with or without a backing disk image:

```bash
./build/starforth --disk-img=disks/starforth-dev.img --ram-disk=10
```

If the disk image is missing, StarForth auto-allocates the RAM disk and bootstraps a clean block environment.

---

## 📜 **Documentation**

StarForth has **comprehensive documentation** built from LaTeX and HTML sources of authority, themed in glorious dark
mode with amber CRT code blocks.

### Build Complete Manual

**LaTeX → PDF** (the gold standard):

```bash
make book
```

Outputs:

* 📝 `docs/build/latex/StarForth-Manual.tex` (editable LaTeX source)
* 🖨️ `docs/build/StarForth-Manual-LaTeX.pdf` (comprehensive PDF)

**HTML** (single-page + multi-page with dark.css):

```bash
make book-html
```

Outputs:

* 📄 `docs/build/html/StarForth-Manual.html` (single-page scrollable)
* 📚 `docs/build/html/book/index.html` (multi-page navigable)

### What's Included

Both formats contain **EVERYTHING**:

* Doxygen API documentation (generated fresh)
* Man pages (embedded)
* GNU Info docs (referenced)
* All markdown documentation (README first!)
* Architecture, testing, gap analysis

From these sources you can generate PDF, ePub, Mobi, DocBook, paper books, or any format you need!

---

## 🧠 **Why StarForth Exists**

Modern VMs are obese.
Scripting languages are bloated.
Operating systems treat FORTH like a novelty.

StarForth is a **back-to-basics, forward-looking** response:

* Minimalism without masochism
* Old-school stack discipline with modern performance
* FORTH not as a gimmick — but as **the core architecture**

It’s retro, yes. But it’s *fast*, *tight*, and *built for today’s hardware*.

---

## 🔥 **Come See How I `stack up`**

StarForth is already integrated as the core VM for **StarshipOS**, where it will power a full fileless, event-driven
operating system.

But you can use it independently for:

* Embedded systems
* Virtual machine experiments
* Retro computing projects
* Language runtime research
* Microkernel services

This isn’t nostalgia — it’s **heritage with a V8 engine**.

---

## 🧑‍🚀 **Captain Bob**

Built and maintained by **Captain Bob**, a systems engineer hacking since 1973.
He doesn’t do Python dependency hell. He does **tight loops**, **inline assembly**, and **terminal glow**.

---

## 🪪 **License**

**CC0 / Public Domain** —
Take it. Fork it. Embed it. Burn rubber.
Just don’t blame us when your VM goes supersonic.

---

## 🌟 **StarForth — Where Retro Meets Relentless**

```
╔═══════════════════════════════════════════╗
║  STARFORTH: STACK-BASED VM, BUILT TO RUN  ║
╚═══════════════════════════════════════════╝
```
