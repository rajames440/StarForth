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
.
├── build
├── include
│   ├── io.h
│   ├── log.h
│   ├── vm.h
│   └── word_registry.h
├── LICENSE
├── Makefile
├── README.md
├── src
│   ├── io.c
│   ├── log.c
│   ├── main.c
│   ├── test_runner
│   │   ├── include
│   │   │   ├── test_common.h
│   │   │   └── test_runner.h
│   │   ├── modules
│   │   │   ├── arithmetic_words_test.c
│   │   │   ├── block_words_test.c
│   │   │   ├── control_words_test.c
│   │   │   ├── defining_words_tests.c
│   │   │   ├── dictionary_manipulation_words_test.c
│   │   │   ├── dictionary_words_test.c
│   │   │   ├── double_words_test.c
│   │   │   ├── editor_words_test.c
│   │   │   ├── format_words_test.c
│   │   │   ├── io_words_test.c
│   │   │   ├── logical_words_test.c
│   │   │   ├── memory_words_test.c
│   │   │   ├── mixed_arithmetic_words_test.c
│   │   │   ├── return_stack_words_test.c
│   │   │   ├── stack_words_test.c
│   │   │   ├── string_words_test.c
│   │   │   ├── system_words_test.c
│   │   │   └── vocabulary_words.c
│   │   ├── test_common.c
│   │   └── test_runner.c
│   ├── vm.c
│   ├── word_registry.c
│   └── word_source
│       ├── arithmetic_words.c
│       ├── block_words.c
│       ├── control_words.c
│       ├── defining_words.c
│       ├── dictionary_manipulation_words.c
│       ├── dictionary_words.c
│       ├── double_words.c
│       ├── editor_words.c
│       ├── format_words.c
│       ├── include
│       │   ├── arithmetic_words.h
│       │   ├── block_words.h
│       │   ├── control_words.h
│       │   ├── defining_words.h
│       │   ├── dictionary_manipulation_words.h
│       │   ├── dictionary_words.h
│       │   ├── double_words.h
│       │   ├── editor_words.h
│       │   ├── format_words.h
│       │   ├── io_words.h
│       │   ├── logical_words.h
│       │   ├── memory_words.h
│       │   ├── mixed_arithmetic_words.h
│       │   ├── return_stack_words.h
│       │   ├── stack_words.h
│       │   ├── string_words.h
│       │   ├── system_words.h
│       │   └── vocabulary_words.h
│       ├── io_words.c
│       ├── logical_words.c
│       ├── memory_words.c
│       ├── mixed_arithmetic_words.c
│       ├── return_stack_words.c
│       ├── stack_words.c
│       ├── string_words.c
│       ├── system_words.c
│       └── vocabulary_words.c
└── TESTING.md
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
