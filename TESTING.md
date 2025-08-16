# StarForth Test Suite

The **StarForth** virtual machine includes a comprehensive, built-in testing framework designed to validate the behavior
of standard Forth words across normal, edge, and error conditions. This ensures strict Forth-79/83 compliance and
provides a robust platform for extensibility and validation.

---

## ✅ Features

* Modular test suite organized by word group
* Covers stack manipulation, arithmetic, logical, I/O, memory, and return stack operations
* Differentiates between **normal**, **edge**, and **error** test cases
* Supports skipped/unimplemented test markers
* Preserves and restores VM state between tests
* Detailed logging with result categories: PASS, FAIL, SKIP, ERROR

---

## 🤖 Test Case Structure

Each test suite (`WordTestSuite`) contains:

* A **Forth word name** (e.g. `DUP`, `+`, `EMIT`)
* An array of **test cases** (`TestCase[]`):

    * `name`: A short test name
    * `input`: A Forth string to evaluate
    * `expected`: A human-readable description of expected behavior
    * `test_type`: One of `TEST_NORMAL`, `TEST_EDGE_CASE`, or `TEST_ERROR_CASE`
    * `should_error`: Whether the VM is expected to throw an error
    * `implemented`: Flag to skip tests if not implemented

---

## ⚙️ Running the Test Suite

Build the project first:

```bash
make clean && make
```

Then run the REPL binary with embedded tests:

```bash
./build/starforth --run-tests
```

*(Requires a command-line flag to be implemented if not already present)*

Or invoke `run_all_tests(&global_vm);` from `main()` for automated testing.

---

## 🤷 Sample Test Output

```
[INFO] Starting comprehensive FORTH-79 test suite...
[INFO] ==============================================

Testing word: DUP
------------------------
  [PASS] DUP.basic
  [PASS] DUP.zero
  [FAIL] DUP.max_int
  [SKIP] DUP.not_implemented_case
  [ERROR] DUP.empty_stack
  DUP: 2 passed, 1 failed, 1 skipped, 1 error

...

FINAL TEST SUMMARY:
  Total tests: 91
  Passed: 87
  Failed: 2
  Skipped: 1
  Errors: 1
[ERROR] 3 tests FAILED or had ERRORS!
```

---

## 🏛️ Categories

### Stack Manipulation

* `DUP`, `DROP`, `SWAP`, `OVER`, `ROT`

### Arithmetic

* `+`, `-`, `*`, `/`, `MOD`

### Logical / Comparison

* `=`, `<`

### I/O

* `EMIT`, `CR`

### Memory

* `!` (store), `@` (fetch)

### Return Stack

* `>R`, `R>`, `R@`

---

## ⚖️ Adding Tests

1. Locate the correct `WordTestSuite` in `src/test/test.c`
2. Append a new `TestCase` entry with:

```c
{"test_name", "forth string", "expectation", TEST_TYPE, should_error, 1},
```

3. Add additional suites if needed:

```c
{ "NEWWORD", { {"case1", "input", "desc", TEST_NORMAL, 0, 1}, ... }, N },
```

4. Rebuild and re-run the suite.

---

## 🛡️ Assertion Helpers

* `assert_stack_depth(vm, expected_depth)`
* `assert_stack_top(vm, expected_value)`
* `assert_vm_error(vm, should_have_error)`

These can be expanded into deeper validation logic in future iterations.

---

## 🔒 Future Enhancements

* CLI flag support for filtering test runs
* Colored terminal output for quick visual pass/fail review
* Output to JUnit XML or Markdown for CI integration
* Regression mode to re-run only failing tests

---

## 🚀 Why It Matters

This framework ensures **StarForth** remains deterministic, robust, and secure across future modifications and
extensions. It serves as the compliance layer for system-critical builds and embedded deployments.

Every word tested, every word trusted.

---

**Made with love and a strong stack discipline by R. A. James** — and sniff-verified by **Santino** 🐾
