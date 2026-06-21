# src/test_runner/

POST (Power-On Self Test) harness — 1000 tests across all word categories.
Tests run in order: unit → dictionary → integration → stress → adversarial.

## Harness files

| File | Purpose |
|------|---------|
| `test_runner.c` | Orchestration — discovers and runs all suites |
| `test_common.c` | Shared utilities: `run_test_suite`, `run_test_suite_m` |
| `test_contracts.c` | Contract-based testing helpers |
| `include/` | Internal headers for the test harness |

## Test modules (`modules/`)

| File | Category |
|------|---------|
| `arithmetic_words_test.c` | `+` `-` `*` `/` `MOD` `ABS` `MIN` `MAX` |
| `stack_words_test.c` | `DUP` `DROP` `SWAP` `ROT` `OVER` |
| `logical_words_test.c` | `AND` `OR` `XOR` `NOT` `=` `<` `>` `>=` `<=` |
| `control_words_test.c` | `IF` `DO` `LOOP` `BEGIN` `WHILE` |
| `defining_words_tests.c` | `:` `;` `VARIABLE` `CONSTANT` `DEFER` `IS` `DEFER@` |
| `memory_words_test.c` | `@` `!` `C@` `C!` `MOVE` `FILL` |
| `return_stack_words_test.c` | `>R` `R>` `R@` `RDROP` |
| `double_words_test.c` | `2DUP` `2DROP` `2SWAP` `D+` `D-` |
| `string_words_test.c` | `S"` `TYPE` string operations |
| `io_words_test.c` | `EMIT` `CR` `SPACE` |
| `format_words_test.c` | `.` `HEX` `DECIMAL` |
| `block_words_test.c` | `BLOCK` `BUFFER` `LOAD` `FLUSH` |
| `dictionary_words_test.c` | `FIND` `WORDS` |
| `dictionary_manipulation_words_test.c` | `FORGET` |
| `vocabulary_words_test.c` | `VOCABULARY` `DEFINITIONS` |
| `system_words_test.c` | `BYE` `ABORT` |
| `mixed_arithmetic_words_test.c` | Mixed-type arithmetic |
| `mama_forth_words_test.c` | Mama VM words (`BIRTH` `RUN` `USE`) |
| `acl_words_test.c` | ACL system words |
| `starforth_words_test.c` | StarForth extensions |
| `integration_tests.c` | Cross-category integration |
| `stress_tests.c` | Stack depth, loop count, memory pressure |
| `break_me_tests.c` | Adversarial / fuzzing cases |

## Test DSL

Each suite uses a `WordTestSuite` / `TestCase` struct:

```c
TestCase cases[] = {
    { "description", "input FORTH", "expected output",
      TEST_NORMAL, /*should_error=*/false, /*implemented=*/true },
};
run_test_suite("SUITE-NAME", cases, ARRAY_SIZE(cases), vm);
```

Use `run_test_suite_m` with `CONTRACT_PHYSICS_TRANSPARENT` for physics-aware words.

## See also

- [`src/README.md`](../README.md) — parent directory
- [Project root](../../README.md)
