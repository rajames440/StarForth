# Contributing to StarForth

Welcome to **StarForth** — a bare-metal, block-based, Forth-79 inspired system written in strict ANSI C99. This project is experimental, opinionated, and built for people who don’t half-ass shit. If you want to help, here’s how.

---

## Ground Rules

* **No glibc!**

* **Do It Right.**
  No shortcuts, no hacks, no “we’ll fix it later.” If it’s wrong, fix it now.

* **Standards Matter.**

  * Forth-79 semantics apply unless otherwise noted.
  * C code must compile clean with `-std=c99 -Wall -Wextra -Werror`.
  * Memory safety is not optional.

* **No Files.**
  This system is **block-based only**. No sneaking in filesystem dependencies.

* **Tests are the Gatekeepers.**
  All changes must pass the test runner. If your code breaks tests, it’s not done.

---

## Workflow

1. **Fork & Branch**

   * Fork the repo.
   * Work in a feature branch (`feature/<short-desc>`).

2. **Build & Test**
   Use the provided `Makefile`:

   ```bash
   make clean
   make
   ./build/starfotyh --run-tests optionally: --fail-fast --log-debug
   ```

3. **Coding Style**

   * 80-column soft limit.
   * Keep functions small and focused.
   * Comment Forth words with intended stack effect (`( n1 n2 -- n3 )`).

4. **Commit Messages**

   * Present tense, imperative mood.
     Example: `Add dictionary search for vocabularies`
   * Reference test cases when relevant.

5. **Pull Requests**

   * Open a PR against `main`.
   * Describe what problem you’re solving.
   * Include test results.

---

## Adding New Words

* Implement in a `*_words.c` module under `src/source_words/`.
* Declare in matching `*_words.h`.
* Add functional tests in `src/test_runner/modules/`.
* Update the test harness (`test_runner.c`) if needed.

---

## Tests

Tests live under `src/test_runner/modules/`.
Each module has a `WordTestSuite[]` with clearly defined cases.

**Rule:**
If you add a word, you add at least one test. No exceptions.

---

## License & Attribution

All contributions are under **CC0-1.0 (Public Domain)**. By contributing, you agree to release your changes with the same dedication. No encumbrances, no bullshit.

---

## How to Get Started

* Look at open issues tagged **`good first word`** or **`bug hunt`**.
* Build it. Break it. Fix it. Repeat.

---

## Code of Conduct

* Respect the codebase.
* Respect contributors.
* Debate hard, don’t get personal.
* And remember: **We don’t ship crap.**
