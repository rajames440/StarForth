# StarForth Dictionary Test Coverage — Gap Audit
**Branch:** `lithosananke` | **StarForth v3.0.3 / LithosAnanke v1.0.8**
**Audited:** 2026-06-03

---

## Summary

| Metric | Count |
|--------|-------|
| Total registered words | ~230 |
| Words with at least one test | ~115 |
| Words with **no** test | ~115 |
| Test files | 20 |
| Untested word categories | 5 (editor, inference, log, physics-diag, q48) |

Coverage is solid for the foundational FORTH-79 core but has critical holes
in control-flow completeness, format output, memory ops, and the S" word
just added in v3.0.3.

---

## Priority 1 — FORTH-79 Core: Missing Critical Words

These are standard FORTH-79 words used in virtually all real programs.
Missing tests here are compliance failures.

| Word(s) | Source file | Why critical |
|---------|-------------|--------------|
| `1+` `1-` `2+` `2-` `2*` `2/` | arithmetic_words.c | Shorthand increment/shift ops; used in almost every program. Arithmetic test only covers `/MOD` variants, not these. |
| `I` `J` | control_words.c | Loop index access. Every `DO...LOOP` that reads the index uses `I`; nested loops need `J`. No test at all. |
| `EXIT` | control_words.c | Return from word mid-execution. Essential for early-return patterns. No test. |
| `LEAVE` | control_words.c | Early exit from `DO...LOOP`. No test. |
| `UNLOOP` | control_words.c | Required to clean the return stack before `EXIT` inside a loop. Omitting it is a stack-corruption bug; no test exercises this. |
| `AGAIN` | control_words.c | `BEGIN...AGAIN` infinite loop. Common pattern, completely untested. |
| `CASE` `OF` `ENDOF` `ENDCASE` | control_words.c | Full `CASE` structure. Entire construct untested. |
| `+!` `-!` | memory_words.c | Modify-in-place. The single most common memory idiom in FORTH after `@` and `!`. No test. |
| `FILL` `MOVE` `ERASE` | memory_words.c | Bulk memory primitives. Used by string ops, block subsystem, array initialisation. No test. |
| `2!` | memory_words.c | Store double/address pair. `2@` is tested; its store partner is not. |

---

## Priority 2 — S" (New in v3.0.3) — Urgent

`S"` was added as a proper IMMEDIATE primitive in this release. It has zero
test coverage. Regression risk is high since the implementation is new.

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `S"` | string_words.c | No test for interpret-mode push (c-addr u), compile-mode inline (`(s")` runtime), or use inside a `:` definition. |
| `(s")` | string_words.c | Internal runtime for compiled `S"`. Exercised only by compiled `S"` — needs a compile-mode test. |
| `BL` | string_words.c | Space-character constant. Trivial but untested; often paired with `S"`. |
| `SOURCE` `>IN` | string_words.c | Input stream state. `SOURCE` returns ( c-addr u ); `>IN` is the offset. Both untested. |

---

## Priority 3 — Format / Output Words

`format_words.c` has 19 registered words. Only `.` and `BASE`/`DECIMAL` are
touched (from io_words_test.c). The pictured numeric output group is
completely dark.

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `HEX` `OCTAL` | format_words.c | Base switching. Used constantly in systems work. No test. |
| `.S` | format_words.c | Print entire stack without consuming it. Core debugging primitive. No test. |
| `?` | format_words.c | Print cell at address (` @ . `). Heavily used. No test. |
| `.R` `U.` `U.R` | format_words.c | Right-justified and unsigned output. No test. |
| `D.` `D.R` | format_words.c | Double-number output. No test. |
| `<#` `#` `#S` `#>` `HOLD` `SIGN` | format_words.c | Pictured numeric output group. Entire 6-word subsystem untested. |
| `DUMP` | format_words.c | Memory hex dump. No test. |

---

## Priority 4 — Logical / Comparison Gaps

logical_words_test.c covers the common cases but misses several FORTH-79 words.

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `0<>` | logical_words.c | Not-equal-to-zero. Symmetric counterpart to `0=`. No test. |
| `U<` `U>` | logical_words.c | Unsigned comparison. Critical for address arithmetic and char comparisons. No test. |
| `WITHIN` | logical_words.c | Range test. No test. |
| `TRUE` `FALSE` | logical_words.c | Boolean constants. Untested (though they're trivially `0` and `-1`). |

---

## Priority 5 — Stack Word Gaps

stack_words_test.c covers DUP DROP SWAP OVER ROT DEPTH PICK ROLL.
Two standard words missing:

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `?DUP` | stack_words.c | Conditional dup (dup only if non-zero). Common idiom. No test. |
| `-ROT` | stack_words.c | Reverse rotate. Tested implicitly via `ROT` but never directly. |

---

## Priority 6 — Double Number Gaps

double_words_test.c is thorough but misses:

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `S>D` | double_words.c | Convert single to double. The entry point for all `D`-word arithmetic. No test. |
| `D2*` `D2/` | double_words.c | Double arithmetic shifts. No test. |
| `2>R` `2R>` `2R@` | double_words.c | Double-number return stack. No test. |

---

## Priority 7 — Mixed Arithmetic Gap

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `M-` | mixed_arithmetic_words.c | `M+` and `M*` are tested; `M-` is not. |

---

## Priority 8 — Compilation / Meta Words

These are used to build user-defined IMMEDIATE words and to manipulate the
dictionary at compile time. None are tested.

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `COMPILE` `[COMPILE]` | defining_words.c | Used to force compilation of words inside IMMEDIATE definitions. No test. |
| `[']` | string_words.c | Compile-time version of `'` (tick). Returns XT of next word at compile time. No test. |
| `LIT` | defining_words.c | Internal compile-time literal push. No test. |
| `STATE` | defining_words.c / dictionary_manipulation_words.c | Dual-registered. Compile/interpret switch. No test. |
| `ALIGN` | dictionary_words.c | Align `HERE` to cell boundary. No test. |
| `'` (TICK) | dictionary_manipulation_words.c | Fetch execution token of named word. Core of `[']` and `EXECUTE`. No test. |
| `LITERAL` `[LITERAL]` | string_words.c | Compile a literal at runtime / inside definitions. Partially touched in io test but no dedicated test. |

---

## Priority 9 — Dictionary Traversal Words

These enable introspection of the dictionary structure. No tests at all.

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `>NAME` `NAME>` | dictionary_manipulation_words.c | Convert between CFA and NFA. No test. |
| `>LINK` `LINK>` | dictionary_manipulation_words.c | Navigate dictionary linked list. No test. |
| `CFA` `NFA` `LFA` `PFA` | dictionary_manipulation_words.c | Classic field-access words. No test. |
| `TRAVERSE` | dictionary_manipulation_words.c | Walk dictionary chain. No test. |
| `INTERPRET` | dictionary_manipulation_words.c | The outer interpreter. No test. |

---

## Priority 10 — System Words

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `ABORT"` `(ABORT")` | system_words.c | Conditional abort with message. Used in error handling. No test. |
| `WORDS` `VLIST` | system_words.c | Dictionary listing. No test. |
| `SEE` | system_words.c | Decompiler. No test. |
| `COLD` `WARM` | system_words.c | System restart/reinit. No test (understandably risky to test directly). |
| `QUIT` | system_words.c | Outer interpreter restart. No test. |

---

## Priority 11 — Block Word Gap

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `-->` | block_words.c | Chain-load next block. Used in multi-block definitions. No test. |

---

## Priority 12 — Lifecycle / Capsule Words

Capsule-BIRTH/KILL/PAUSE/RESUME/USE have partial coverage from mama_forth tests
but direct functional tests are missing.

| Word(s) | Source file | Gap |
|---------|-------------|-----|
| `KILL` `PAUSE` `RESUME` | lifecycle_words_hosted.c | Capsule lifecycle management. No direct test. |
| `USE` | lifecycle_words_hosted.c | Switch active VM. No test. |

---

## Priority 13 — Entire Untested Categories

These word groups have **zero test coverage**. Lower urgency than gaps above
because they are StarForth-specific extensions (not FORTH-79 compliance),
but they should be ticketed.

| Category | Source file | Word count | Notes |
|----------|-------------|------------|-------|
| Q48 fixed-point | q48_words.c | 21 | POST unit tests exist for the C layer but no FORTH-word tests |
| Inference | inference_words.c | 19 | Physics adaptive engine; complex to test deterministically |
| Log words | log_words.c | 7 | LOG-ERROR … LOG-LEVEL@; simple but untested |
| Physics diagnostics | physics_diagnostic_words.c | 4 | PHYSICS-WORD-METRICS etc. |
| Physics freeze | physics_freeze_words.c | 9 | FREEZE-WORD, HEAT@, etc. |
| Physics benchmark | physics_benchmark_words.c | 6 | BENCH-DICT-LOOKUP etc. |
| Physics pipelining diag | physics_pipelining_diagnostic_words.c | 6 | PIPELINING-SHOW-STATS etc. |
| Editor | editor_words.c | 4 | L, S, SHOW, EDIT — block editor; very low priority |

---

## Work Order (suggested sequence)

```
1. [ ] P1a  arithmetic_words_test.c  — add 1+ 1- 2+ 2- 2* 2/
2. [ ] P1b  control_words_test.c     — add I J EXIT LEAVE UNLOOP AGAIN
3. [ ] P1c  control_words_test.c     — add CASE OF ENDOF ENDCASE
4. [ ] P1d  memory_words_test.c      — add +! -! 2! FILL MOVE ERASE
5. [ ] P2   string_words_test.c      — add S" (s") BL SOURCE >IN
6. [ ] P3a  format_words_test.c      — new file: HEX OCTAL .S ? .R U. U.R D. D.R
7. [ ] P3b  format_words_test.c      — add <# # #S #> HOLD SIGN DUMP
8. [ ] P4   logical_words_test.c     — add 0<> U< U> WITHIN TRUE FALSE
9. [ ] P5   stack_words_test.c       — add ?DUP -ROT
10.[ ] P6   double_words_test.c      — add S>D D2* D2/ 2>R 2R> 2R@
11.[ ] P7   mixed_arithmetic_words_test.c — add M-
12.[ ] P8a  defining_words_tests.c   — add COMPILE [COMPILE] LIT STATE
13.[ ] P8b  dictionary_manipulation_words_test.c — add ' ['] ALIGN LITERAL
14.[ ] P9   dictionary_manipulation_words_test.c — add >NAME NAME> CFA NFA LFA PFA TRAVERSE INTERPRET
15.[ ] P10  system_words_test.c      — new file: ABORT" WORDS VLIST SEE
16.[ ] P11  block_words_test.c       — add -->
17.[ ] P12  lifecycle capsule tests  — add KILL PAUSE RESUME USE
18.[ ] P13a q48_words_test.c         — new file: all 21 Q48 FORTH words
19.[ ] P13b log_words_test.c         — new file: LOG-ERROR … LOG-LEVEL@
20.[ ] P13c physics words tests      — new files as needed
```
