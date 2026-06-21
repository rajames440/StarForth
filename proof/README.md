# proof/

Isabelle/HOL theory files providing machine-checkable proofs of determinism
and correctness for StarForth. 24 theory files covering all 7 feedback loops,
core word categories, and the word-level ACL system.

## Running proofs

```bash
isabelle build -D proof/
```

Requires an Isabelle installation. See `docs/01-getting-started/DEVELOPER.md`.

## Theory files

### Foundations
| File | Covers |
|------|--------|
| `StarForth_Base.thy` | Base definitions and type system |
| `StarForth_Correctness.thy` | Overall correctness |
| `StarForth_Transition.thy` | State transitions |
| `StarForth_Concurrent.thy` | Concurrency properties |
| `StarForth_Mutex.thy` | Mutual exclusion |

### Physics loops (1–7)
| File | Loop |
|------|------|
| `StarForth_Loop1_Heat.thy` | Execution heat tracking |
| `StarForth_Loop2_Window.thy` | Rolling window of truth |
| `StarForth_Loop3_Decay.thy` | Linear decay |
| `StarForth_Loop4_Pipeline.thy` | Word transition prediction |
| `StarForth_Loop5_WinInf.thy` | Window width inference |
| `StarForth_Loop6_DecayInf.thy` | Decay slope inference |
| `StarForth_Loop7_Heartrate.thy` | Adaptive heartrate |

### Word categories
`StarForth_Arithmetic_Words.thy`, `StarForth_Stack_Words.thy`,
`StarForth_Logical_Words.thy`, `StarForth_Memory_Words.thy`,
`StarForth_Return_Stack_Words.thy`, `StarForth_Q48_16.thy`

### ACL system (Phase 6)
| File | Property |
|------|---------|
| `ACL_Pin_Monotone.thy` | Pin is one-way |
| `ACL_Inherit_Clears_Pin.thy` | Inheritance clears pin |
| `ACL_TTL_Bounded.thy` | TTL stays within bounds |
| `ACL_Emergency_Bypass.thy` | Emergency console bypass |
| `ACL_No_Escalation.thy` | No privilege escalation |

## See also

- [`ROOT`](ROOT) — Isabelle project manifest
- [Project root](../README.md)
