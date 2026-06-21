# src/word_source/

FORTH-79 word implementations. Each file registers its words via
`register_*_words()` called from `vm_bootstrap.c`.

## Word files

| File | Words |
|------|-------|
| `arithmetic_words.c` | `+` `-` `*` `/` `MOD` `ABS` `MIN` `MAX` |
| `stack_words.c` | `DUP` `DROP` `SWAP` `ROT` `OVER` `NIP` `TUCK` |
| `control_words.c` | `IF` `ELSE` `THEN` `DO` `LOOP` `BEGIN` `UNTIL` `WHILE` |
| `defining_words.c` | `:` `;` `CREATE` `DOES>` `VARIABLE` `CONSTANT` `DEFER` `IS` `DEFER@` |
| `memory_words.c` | `@` `!` `C@` `C!` `MOVE` `FILL` |
| `return_stack_words.c` | `>R` `R>` `R@` `RDROP` `2>R` `2R@` `2R>` |
| `double_words.c` | `2DUP` `2DROP` `2SWAP` `2@` `2!` `D+` `D-` |
| `logical_words.c` | `AND` `OR` `XOR` `NOT` `INVERT` `LSHIFT` `RSHIFT` `=` `<>` `<` `>` `>=` `<=` |
| `io_words.c` | `EMIT` `KEY` `TYPE` `CR` `TAB` `SPACE` `ACCEPT` |
| `string_words.c` | `S"` `SLITERAL` and string operations |
| `block_words.c` | `BLOCK` `BUFFER` `LOAD` `THRU` `FLUSH` |
| `format_words.c` | `.` `.(` `.R` `.S` `HEX` `DECIMAL` `BASE` |
| `system_words.c` | `BYE` `ABORT` `INCLUDE` `STATE` |
| `dictionary_words.c` | `FIND` `SEARCH-WORDLIST` `WORDS` |
| `vocabulary_words.c` | `VOCABULARY` `DEFINITIONS` `FORTH-WORDLIST` |
| `dictionary_manipulation_words.c` | `FORGET` and related |
| `mixed_arithmetic_words.c` | Mixed-type arithmetic |
| `q48_16_words.c` | Q48.16 fixed-point word definitions |
| `q48_words.c` | Q48 supplemental words |
| `acl_words.c` | `ACL-PIN` `ACL-DENY` `ACL-ALLOW` `ACL-TTL` `ACL-INHERIT` |
| `starforth_words.c` | StarForth-specific extensions |
| `physics_benchmark_words.c` | Benchmark harness (L1–L7) |
| `physics_diagnostic_words.c` | `WORD-ENTROPY` and diagnostics |
| `physics_freeze_words.c` | `PHYSICS-FREEZE` / `PHYSICS-THAW` |
| `physics_pipelining_diagnostic_words.c` | Pipeline diagnostics |
| `inference_words.c` | Inference engine words |
| `dictionary_heat_diagnostic_words.c` | Heat diagnostic words |
| `log_words.c` | Log subsystem words |
| `editor_words.c` | Block editor words |
| `lifecycle_words_hosted.c` | Hosted-only lifecycle words |

## See also

- [`src/README.md`](../README.md) — parent directory
- [`include/vm.h`](../../include/vm.h) — `DictEntry` and VM state
- [Project root](../../README.md)
