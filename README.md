# StarForth

**StarForth** is a lightweight, modular, ANSI C–based Forth virtual machine designed for embedded, minimal OS, or
experimental environments. It follows a classical Forth model but adds modern modularity through a rich set of
word-source files and a clean internal API. StarForth targets environments without a traditional C library (e.g., L4Re),
with **no reliance on** `malloc`, `printf`, or glibc.

> **⚙️ Built from the ground up** for Forth‑79/83 semantics, vocabulary management, safety checks, and a block‑centric
> future.

---

## 💡 Features (1.0.0)

* 🧠 **Threaded VM** (direct vs. indirect depends on target)
* ⚡ **Inline assembly optimizations** for x86_64 and ARM64 (22% speedup on x86_64)
* 🗂️ **Dictionary & registration** via `word_registry`
* 🧩 **Modular word sources** (one category per file)
* 💾 **Persistent block storage** - 3-layer architecture with FILE/RAM/L4Re backends
* 🧱 **Block‑style memory model** - RAM blocks (1-1023) + Disk blocks (1024+)
* 🧾 **64‑bit cells** by default (`cell_t`) with stack/logical/arithmetic/IO sets
* 🧪 **REPL** and an in‑process test runner
* 🧯 **No glibc**: suitable for tiny kernels and libc‑free targets
* 🏗️ **Multi-architecture**: Native x86_64, ARM64/AArch64 cross-compilation

**Control flow implemented**

* `IF … ELSE … THEN` with forward‑patched `(0BRANCH)` and unmatched‑THEN checks
* `BEGIN … WHILE … REPEAT`, plus `UNTIL` and `AGAIN` (byte‑relative branches)
* `DO … LOOP` / `+LOOP` with indices `I` and `J`
* `?DO` (conditional entry) with **(limit index --)** semantics
* **`LEAVE`** compiled via a **separate leave‑site stack** (does not poison IF/ELSE/THEN state)

**Runtime/compile‑time hygiene**

* All branch offsets are **byte‑relative** to their literal cell
* Return‑stack and data‑stack under/overflow checks with consistent logging
* Loop parameter stack: `(limit, index, loop_start_ip)` with strict bounds
* Compile‑time control‑flow (CF) stack tagged as `CF_BEGIN`, `CF_IF`, `CF_ELSE`, `CF_DO`, `CF_WHILE`
* CF “epoch” reset across mode transitions to prevent stale frames between `:` and `;`

---

## 🗂️ Project Structure

```text
.
├── build
├── include
│   ├── io.h
│   ├── log.h
│   ├── vm.h
│   └── word_registry.h
├── LICENSE
├── Makefile
├── README.md
├── src
│   ├── io.c
│   ├── log.c
│   ├── main.c
│   ├── test_runner
│   │   ├── include
│   │   │   ├── test_common.h
│   │   │   └── test_runner.h
│   │   ├── modules
│   │   │   ├── arithmetic_words_test.c
│   │   │   ├── block_words_test.c
│   │   │   ├── control_words_test.c
│   │   │   ├── defining_words_tests.c
│   │   │   ├── dictionary_manipulation_words_test.c
│   │   │   ├── dictionary_words_test.c
│   │   │   ├── double_words_test.c
│   │   │   ├── editor_words_test.c
│   │   │   ├── format_words_test.c
│   │   │   ├── io_words_test.c
│   │   │   ├── logical_words_test.c
│   │   │   ├── memory_words_test.c
│   │   │   ├── mixed_arithmetic_words_test.c
│   │   │   ├── return_stack_words_test.c
│   │   │   ├── stack_words_test.c
│   │   │   ├── string_words_test.c
│   │   │   ├── system_words_test.c
│   │   │   └── vocabulary_words.c
│   │   ├── test_common.c
│   │   └── test_runner.c
│   ├── vm.c
│   ├── word_registry.c
│   └── word_source
│       ├── arithmetic_words.c
│       ├── block_words.c
│       ├── control_words.c
│       ├── defining_words.c
│       ├── dictionary_manipulation_words.c
│       ├── dictionary_words.c
│       ├── double_words.c
│       ├── editor_words.c
│       ├── format_words.c
│       ├── include
│       │   ├── arithmetic_words.h
│       │   ├── block_words.h
│       │   ├── control_words.h
│       │   ├── defining_words.h
│       │   ├── dictionary_manipulation_words.h
│       │   ├── dictionary_words.h
│       │   ├── double_words.h
│       │   ├── editor_words.h
│       │   ├── format_words.h
│       │   ├── io_words.h
│       │   ├── logical_words.h
│       │   ├── memory_words.h
│       │   ├── mixed_arithmetic_words.h
│       │   ├── return_stack_words.h
│       │   ├── stack_words.h
│       │   ├── string_words.h
│       │   ├── system_words.h
│       │   └── vocabulary_words.h
│       ├── io_words.c
│       ├── logical_words.c
│       ├── memory_words.c
│       ├── mixed_arithmetic_words.c
│       ├── return_stack_words.c
│       ├── stack_words.c
│       ├── string_words.c
│       ├── system_words.c
│       └── vocabulary_words.c
└── TESTING.md
```

---

## 🚀 Building

### Dependencies

None. The project is ANSI **C99** and compiles with `gcc` out of the box.

For ARM64 cross-compilation, install: `gcc-aarch64-linux-gnu`

### Build Commands

**Native build (auto-detects x86_64 or ARM64):**
```bash
make clean && make
```

**ARM64 cross-compile (from x86_64):**

```bash
make rpi4-cross      # Raspberry Pi 4 optimized
make rpi4-fastest    # Maximum optimizations
```

**Performance optimized builds:**

```bash
make turbo    # -O3 optimizations
make fast     # ASM + direct threading
make fastest  # ASM + direct threading + LTO
```

### Output

Build artifacts go into the `build/` directory.

### Architecture Support

- **x86_64**: Native compilation with inline assembly optimizations (22% faster)
- **ARM64**: Cross-compilation supported (Raspberry Pi 4, Apple Silicon, AWS Graviton)

---

## 🥪 Running

To launch the Forth VM REPL:

```bash
./build/starforth
```

You should see:

```
ok>
```

---

## 🧰 Usage / Command‑line options

> These are the built‑in switches exposed by the 1.0.0 `main.c`.

```
Usage: ./build/starforth [OPTIONS]
StarForth - A lightweight Forth virtual machine

Options:
  --run-tests       Run the comprehensive test suite before starting REPL
                    (automatically enables TEST logging if no log level set)
  --benchmark [N]   Run performance benchmarks (default: 1000 iterations)
                    (exits after benchmarking, does not start REPL)
  --log-error       Set logging level to ERROR (only errors)
  --log-warn        Set logging level to WARN (warnings and errors)
  --log-info        Set logging level to INFO (default)
  --log-test        Set logging level to TEST (test results only)
  --log-debug       Set logging level to DEBUG (all messages)
  --fail-fast       Stop test suite immediately on first failure
  --help, -h        Show this help message

Examples:
  ./build/starforth                          # Start REPL with INFO logging
  ./build/starforth --run-tests              # Run tests with TEST logging, then start REPL
  ./build/starforth --benchmark              # Run benchmarks with 1000 iterations
  ./build/starforth --benchmark 5000         # Run benchmarks with 5000 iterations
  ./build/starforth --log-debug --run-tests  # Run tests with DEBUG logging
```

**Notes**

* After `--run-tests`, the process **continues into the REPL** (same VM image).
* `--fail-fast` aborts the suite on first failing test (non‑zero exit).
* Exit codes: `0` (clean) • `1` (VM error or test failure) • `2` (bad CLI usage).

---

## ⚗️ Quick smoke (copy/paste)

IF/ELSE:

```
: T1 IF 42 ELSE 24 THEN ;
-1 T1 . CR   \ -> 42
0  T1 . CR   \ -> 24
```

Loops and indices:

```
: T2 5 0 DO I . LOOP ;   T2 CR      \ -> 0 1 2 3 4
: T3 10 0 DO I . 2 +LOOP ; T3 CR    \ -> 0 2 4 6 8
```

Conditional loop + LEAVE:

```
: T4 0 5 ?DO I DUP 2 = IF LEAVE THEN . LOOP ;  T4 CR
\ -> 0 1
```

Descending with +LOOP:

```
: T5 0 10 DO I . -1 +LOOP ;  T5 CR
\ -> 10 9 8 7 6 5 4 3 2 1 0
```

---

## 📊 Tests

* Control words and core mechanics: **green**.
* Latest known run:

    * **709 total** • **658 passed** • **0 failed** • **49 skipped** • **0 errors**
    * REPL starts automatically (`ok>`)

See `src/test_runner/` for the harness and individual suites.

---

## 🔬 Implementation Notes

* Strict **C99**. No C++isms.
* Compile‑time CF stack handles IF/ELSE/THEN and BEGIN/WHILE/REPEAT.
  `LEAVE` sites tracked on a separate side‑stack keyed to the current `DO`.
* Runtime loop frames live on a dedicated small stack and are popped on `LEAVE`.
* Branch offsets are byte‑relative; literals are cell‑aligned.

---

## 🔐 Roadmap

* [ ] `UNLOOP` and **`EXIT` behaves as `UNLOOP EXIT`** inside loops
* [ ] Dictionary inspector + `SEE`
* [ ] Block I/O backend (persistence), snapshots, replay
* [ ] Minimal ROMFS + boot path (L4Re target)
* [ ] Per‑word ACLs / vocabulary isolation
* [ ] Sample programs (sieve, Mandelbrot, ANSI art)

---

## 🛡️ License

**CC0‑1.0 (No warranty).** See `LICENSE`.

---

## 🧙‍♂️ Author

**R. A. James (rajames)** — part of the StarshipOS stack.
Repo: [https://github.com/rajames440](https://github.com/rajames440)

---

## 🐾 Mascot

Proudly supervised by **Santino** 🐕. All commits are sniff‑tested.

---

### Release 1.0.0 Notes (tagged)

First stable cut: full control flow and loops, `?DO`, clean `LEAVE`, byte‑relative branches, verbose logs, and a living
REPL. Tests green; ready for experiments, ports, and shenanigans.
