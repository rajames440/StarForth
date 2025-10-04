![assetts/banner.png](banner.png)

```
   ███████╗████████╗ █████╗ ██████╗ ███████╗ ██████╗ ██████╗ ██████╗██╗  ██╗
   ██╔════╝╚══██╔══╝██╔══██╗██╔══██╗██╔════╝██╔═══██╗██╔══██╗═██╔══╝██║  ██║
   ███████╗   ██║   ███████║██████╔╝█████╗  ██║   ██║██████╔╝ ██║   ███████║
   ╚════██║   ██║   ██╔══██║██╔══██╗██╔══╝  ██║   ██║██╔══██╗ ██║   ██╔══██║
   ███████║   ██║   ██║  ██║██║  ██║██╗     ╚██████╔╝██║  ██║ ██║   ██║  ██║
   ╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝      ╚═════╝ ╚═╝  ╚═╝ ╚═╝   ╚═╝  ╚═╝
                   A Clean FORTH-79 VM for the Modern Age — Fast. Lean. Amber.
```

# 🧠 **StarForth**

### *Come see how I `stack up`* 🧱⚡

> “Born in 1979. Rebuilt for 2025.
> The fastest rat rod in Ohio — and maybe your next favorite VM.”

---

## 🚀 **What is StarForth?**

**StarForth** is a clean, modern, **FORTH-79** implementation — written entirely in **strict ANSI C99**, designed for *
*speed**, **predictability**, and **surgical control** over memory and execution.

It’s the beating heart of [StarshipOS](https://github.com/starshipos) 🛸, but it’s also a **standalone, embeddable
virtual machine**. You can run it on bare metal, inside L4Re, in QEMU, or integrate it into your own projects.

StarForth isn’t bloated.
StarForth isn’t cute.
**StarForth is a goddamn precision engine.**

---

## 🧰 **Core Features**

| Feature                           | Description                                                                                     |
|-----------------------------------|-------------------------------------------------------------------------------------------------|
| 🧱 **FORTH-79 Core**              | Full compliance with the Forth-79 standard, no compromises, no “toy” shortcuts.                 |
| ⚡ **Direct-Threaded Interpreter** | 64-bit aligned, aggressively optimized for low-latency execution.                               |
| 💾 **Block Storage Subsystem**    | Supports RAM-disk and disk-image backed storage. Works with NVMe, SATA, eMMC, and QEMU devices. |
| 🧠 **Strict Pointer Discipline**  | `STRICT_PTR` mode enforces hard pointer safety checks — no sloppy segfault roulette.            |
| 🌀 **Modular Word Dictionary**    | 18+ modules, from arithmetic to defining words, easily extendable.                              |
| 🔬 **Entropy-Ready Internals**    | Hooks for runtime statistical profiling and ML-assisted memory placement.                       |
| 💻 **Amber Glow**                 | Retro-inspired terminal aesthetics. When you see that amber code block, you know you’re home.   |

---

## 📂 **Repository Layout**

```
starforth/
 ├─ src/               # Core VM and subsystems
 ├─ include/           # Public headers (strict C99)
 ├─ word_source/       # Modular word definitions (Forth-79 vocabulary)
 ├─ tests/             # 675+ functional tests (fail-fast harness)
 ├─ disks/             # Disk images for RAM/disk-backed experiments
 └─ docs/              # Full DocBook + dark-themed HTML/PDF/EPUB manuals
```

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

---

## 🧪 **Testing**

We don’t play “maybe it works.”
StarForth ships with a **full test harness**: 675+ tests covering arithmetic, stack ops, control structures, dictionary
internals, block I/O, and more.

```bash
make test
```

Fail-fast. Log-rich. Deterministic.

---

## 💾 **Running**

You can run StarForth directly, with or without a backing disk image:

```bash
./build/starforth --disk-img=disks/starforth-dev.img --ram-disk=10
```

If the disk image is missing, StarForth auto-allocates the RAM disk and bootstraps a clean block environment.

---

## 📜 **Documentation**

StarForth has **real documentation** built with DocBook 4.5 + Pandoc + Calibre, themed in glorious dark mode with amber
retro terminals.

Build all formats:

```bash
cd docs
make
```

Outputs:

* 📄 `build/starforth-manual.html`
* 📘 `build/StarForth-Manual.epub`
* 🖨️ `build/StarForth-Manual.pdf`
* 📚 `build/StarForth-Manual.azw3` (Kindle-ready)

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
