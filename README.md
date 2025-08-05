# StarForth

**StarForth** is a lightweight, modular, ANSI C-based Forth virtual machine designed for embedded, minimal OS, or experimental environments. It follows a classical Forth model but adds modern modularity through a rich set of word source files and a clean internal API. StarForth targets environments without a traditional C library (e.g. L4Re), with no reliance on `malloc`, `printf`, or glibc.

> **⚙️ Built from the ground up** to support strict Forth-79/83 compatibility, vocabulary management, memory safety, and eventual block-based persistence.

---

## 💡 Features

* 🧠 **Fully threaded VM architecture** (direct-threaded or indirect-threaded depending on target)
* 🗂️ **Dictionary-based word management**
* 🛠️ **Vocabulary support** with segregated word source files
* 🛡️ **Block-based memory model** for fileless operation
* 🔐 **Access control planned** (per-word ACLs)
* 📏 Support for **64-bit values**, **stack manipulation**, **logic**, **math**, and **I/O**
* 🧪 Includes REPL and test hooks
* 📦 Clean modular source layout

---

## 🗂️ Project Structure

```text
StarForth/
├── Makefile                  # Build script
├── build/                    # Output artifacts
├── include/                 # Header files for all subsystems
│   ├── io.h
│   ├── log.h
│   ├── vm.h
│   └── word_registry.h
└── src/                     # C source files
    ├── main.c               # Entry point (REPL)
    ├── vm.c / io.c / log.c
    ├── word_registry.c      # Central registry of Forth words
    ├── test/                # Testing logic
    └── word_source/         # Modular Forth word implementations
        ├── arithmetic_words.c
        ├── block_words.c
        ├── dictionary_words.c
        ├── editor_words.c
        ├── io_words.c
        ├── logical_words.c
        ├── memory_words.c
        ├── return_stack_words.c
        └── ...many more
```

---

## 🚀 Building

### Dependencies

None. The project is ANSI C (C90 or optionally C99) and compiles with `gcc` out of the box.

### Build Command

```bash
make clean && make
```

### Output

Build artifacts go into the `build/` directory.

---

## 🥪 Running

To launch the Forth VM REPL:

```bash
./build/starforth
```

(Assumes `starforth` is the output binary.)

---

## 🧠 Word Set

Words are implemented in separate files in `src/word_source/`, grouped by category:

* `arithmetic_words.c`
* `logical_words.c`
* `stack_words.c`
* `defining_words.c`
* `vocabulary_words.c`
* `editor_words.c`
* `system_words.c`
* `block_words.c` *(simulated persistent storage model)*
* ...and many more

Each file registers its own words via the `word_registry.c` mechanism at startup.

---

## 🔐 Roadmap

* [ ] Add per-word access control (ACLs)
* [ ] Entropy tagging for memory residency
* [ ] VM snapshotting and replay
* [ ] Minimal ROMFS + boot integration (targeting L4Re)
* [ ] Block I/O backend for persistence
* [ ] Port to bare-metal platform or ISO boot

---

## 🛡️ License

Creative Commons 1.0 — see `LICENSE` file for details.

---

## 🧙‍♂️ Author

Created by **R. A. James** — part of the [StarshipOS](https://github.com/rajames440) experimental system stack.

---

## 🐾 Mascot

StarForth is proudly supervised by **Santino**, the StarshipOS mascot 🐕. All commits are thoroughly sniff-tested.
