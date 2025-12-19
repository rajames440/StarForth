# StarForth Physics Runtime — TASK_LIST.md

> **Read this first if you are an AI assistant (Claude, etc.).**

---

## 0. PRELUDE FOR AI AGENTS (MANDATORY)

You are assisting **Captain Bob** on the StarForth project.  
This codebase implements a **physics-driven FORTH-79 VM** with non-standard architecture.  
Your job is to implement **small, isolated tasks** from this file — **not** to redesign the system.

### 0.1. Codebase Context

Primary files involved in this work include (paths may vary slightly):

- `src/vm.c`
- `src/main.c`
- `src/physics_runtime.c`
- `src/physics_hotwords_cache.c`
- `src/physics_metadata.c`
- `src/physics_pipelining_metrics.c`
- `src/doe_metrics.c` (or equivalent)
- `src/platform_time.c` / `platform_time.h`
- `src/log.c` / `log.h`
- `src/test_runner/...` (test harness)

You must **inspect existing behavior** before changing anything.

### 0.2. Hard Rules

1. **ONE TASK AT A TIME**
   - Only work on a **single checkbox task** from this file per session.
   - Do **not** “helpfully combine” multiple tasks.

2. **NO COMMITS WITHOUT APPROVAL**
   - You are not allowed to commit any changes.
   - All changes must be presented as diffs or full files for **Captain Bob** to review first.

3. **INCREMENTAL, TESTED CHANGES**
   - Every change must:
     - Compile cleanly (no new warnings).
     - Run existing tests that are relevant to the touched subsystem.
   - If tests fail: **STOP**, report precisely what failed, and do **not** patch around it.

4. **DO NOT REWRITE ARCHITECTURE**
   - Do **not**:
     - “Simplify” the design.
     - Replace custom physics with generic algorithms.
     - Introduce new dependencies.
     - Convert C99 code to C++, Rust, Go, etc.
   - This architecture is intentionally weird. Respect it.

5. **KEEP CHANGES LOCAL**
   - Don’t refactor unrelated files “for style.”
   - Don’t rename functions or structs unless explicitly required by a task.
   - Don’t move code between files unless a task says so.

6. **NO HIDDEN MAGIC**
   - All behavior must remain:
     - Deterministic.
     - Explicit.
     - Transparent.
   - No background threads, no async I/O, no “smart” runtimes unless explicitly specified.

7. **WHEN IN DOUBT, ASK**
   - If you are unsure about:
     - Semantics.
     - Intent.
     - Factor definitions for DOE.
     - Interpretation of this file.
   - Then: **STOP** and ask Captain Bob for clarification.

---

## 1. Working Rules (For Any Human or AI Helper)

- [ ] **ALWAYS test each change independently.**
- [ ] **NEVER commit without Captain Bob’s explicit approval.**
- [ ] **NEVER batch multiple tasks into one commit.**
- [ ] **NEVER “refactor” outside the exact task.**
- [ ] **NEVER change VM semantics without written approval.**
- [ ] **If uncertain, STOP and ask.**
- [ ] **All tasks must compile with zero warnings (at existing warning level).**

---

## 2. Immediate Stability Tasks (Do These First)

These are purely safety / clarity tasks. No behavior changes unless absolutely necessary.

- [ ] Add clarifying comments to physics-related code paths (heat, decay, window, prefetch) explaining intent, **not** implementation detail.
- [ ] Audit the word execution path in `vm.c` for:
  - Null checks on `prev_word`, `transition_metrics`, and dictionary entries.
  - Safe handling of `word_id` range / bounds.
- [ ] Verify that `rolling_window`, `pipeline_metrics`, and `execution_heat` paths cannot dereference NULL or uninitialized pointers.
- [ ] Add compile-time guards (`#if ENABLE_*`) around physics features so they can be disabled without breaking the build.
- [ ] Ensure DOE mode (`--doe-experiment`) writes **only the CSV line** to `stdout` as its final result (logs can exist but must not break CSV parsing).

---

## 3. Unified VM Signal & Wire Identification (Documentation + Light Scaffolding)

Goal: make explicit where the VM *could* emit signals, without fully implementing the event bus yet.

- [ ] In a new doc `PHYSICS_SIGNALS.md`, list all **potential** signal points:
  - Execution signals:
    - before execution
    - after execution
    - heat increment
    - linear decay applied
    - rolling window update
    - prefetch attempt
    - prefetch hit/miss
    - transition recorded
  - Temporal signals:
    - heartbeat tick start/end
    - heartbeat rate change
    - inference cycle start/end
    - window stabilized/unstable
  - DOE / metrics signals:
    - DOE run start
    - DOE run end
    - metrics snapshot taken
- [ ] Cross-reference each signal with its current location in code (file + function name).
- [ ] Confirm in the doc which signals are:
  - MUST-HAVE
  - HIGH-VALUE
  - FUTURE / OPTIONAL

(No runtime changes yet; this is design + mapping only.)

---

## 4. VM Event Bus Foundation (Minimal, Non-Intrusive)

Goal: introduce a small, efficient event bus API without wiring it into everything at once.

- [ ] Add `vm_event_type_t` enum in a new header `vm_events.h`:
  - Include only a **minimal** set to start:
    - `VM_EVENT_HEAT_UPDATED`
    - `VM_EVENT_DECAY_APPLIED`
    - `VM_EVENT_WINDOW_UPDATED`
    - `VM_EVENT_PREFETCH_STATS`
    - `VM_EVENT_INFERENCE_COMPLETED`
    - `VM_EVENT_TICK_RATE_CHANGED`
- [ ] Add `vm_event_t` struct in `vm_events.h` with:
  - `vm_event_type_t type`
  - `uint64_t timestamp_ns`
  - A small tagged union for payloads (keep it conservative).
- [ ] Implement `vm_publish_event(VM *vm, const vm_event_t *ev)` in a new `vm_events.c`:
  - For now: no queues, just a single optional callback hook.
- [ ] Add registration API:
  - `void vm_set_event_listener(VM *vm, void (*listener)(VM *, const vm_event_t *));`
- [ ] Write a simple test listener in the test harness that logs events when enabled.

**Important:** Do **not** wire every possible signal yet — just prove out the API.

---

## 5. Adaptive Heartbeat: Measurement Foundations

Goal: add **pressure metrics** without changing tick behavior yet.

- [ ] Identify and document in `PHYSICS_RUNTIME.md` the signals that should contribute to “pressure”:
  - heat_rate (change in total heat over time)
  - window_variance or window stability proxy
  - prefetch_error_rate (1 - hit_ratio)
  - stale_word_ratio (stale / total)
- [ ] Implement a small `physics_pressure_state` struct in `physics_runtime.c` (or a new module) to hold:
  - last_total_heat
  - last_tick_ns
  - smoothed_pressure (Q48.16 or double)
- [ ] Add a function:
  - `Q48_16 physics_compute_pressure(const VM *vm);`
  - For now, just compute a simple weighted linear combination and log it.
- [ ] Ensure pressure computation is side-effect-free and can be called safely from `vm_tick()`.

No adaptive tick rate yet — just measurement.

---

## 6. Adaptive Heartbeat: Rate Control (Wired but Disabled by Default)

Goal: implement the math and the hooks, but keep behavior unchanged unless explicitly enabled.

- [ ] In the heartbeat/worker code, introduce:
  - `uint64_t base_tick_ns;`
  - `uint64_t min_tick_ns;`
  - `uint64_t max_tick_ns;`
- [ ] Implement a function:
  - `uint64_t vm_compute_tick_interval_ns(const VM *vm);`
  - Use:
    - pressure metric
    - clamp to `[min_tick_ns, max_tick_ns]`
    - return `base_tick_ns` if adaptive mode is disabled.
- [ ] Add a config flag (compile-time or runtime) to **enable/disable adaptive heartbeat**.
- [ ] When adaptive mode is enabled, call `vm_compute_tick_interval_ns()` in the tick loop.
- [ ] Emit `VM_EVENT_TICK_RATE_CHANGED` when the tick interval changes.

By default, keep adaptive mode OFF so tests remain deterministic until approved.

---

## 7. Feedback Loops Standardization (FL1–FL4)

Goal: Make the four core loops explicit and documented:

- FL1: Heat accumulation & decay
- FL2: Hotwords cache promotion
- FL3: Inference → decay slope tuning
- FL4: Inference → rolling window tuning / prefetch ROI

Tasks:

- [ ] In `PHYSICS_RUNTIME.md`, create a section **“Feedback Loops (FL1–FL4)”** describing:
  - Inputs
  - Internal state
  - Outputs
  - Where in code the loop is implemented.
- [ ] Add inline comments in the corresponding C functions labeling them:
  - `/* FL1: Heat accumulation & decay */`
  - `/* FL2: Hotwords cache promotion */`
  - `/* FL3: Inference → decay slope */`
  - `/* FL4: Inference → window tuning */`
- [ ] Ensure FL1–FL4 each have at least one optional `vm_publish_event` call (behind a compile-time or runtime flag).
- [ ] Confirm that enabling/disabling physics features does not leave loops partially active.

---

## 8. DOE Subsystem Overhaul

Goal: Make DOE runs clean, deterministic, and data-rich.

### 8.1 CLI and Mode Handling

- [ ] Confirm `--doe-experiment` flag is handled in `cli.c` / `main.c`.
- [ ] Add an option `--doe-header` to print CSV header row once.
- [ ] Ensure DOE mode:
  - Resets all physics metrics
  - Does **not** start the REPL
  - Exits with status 0 on success

### 8.2 Metrics Extraction

- [ ] In `doe_metrics.c` (or similar), define a clear `DoeMetrics` struct with named fields:
  - workload_duration_ns
  - cpu_temp_delta
  - cpu_freq_delta
  - total_heat
  - hot_word_count
  - stale_word_count
  - effective_window_size
  - prefetch_attempts
  - prefetch_hits
  - prefetch_hit_ratio
  - decay_slope_q48
  - tick_interval_ns (if adaptive enabled)
  - any other core fields currently in the CSV row
- [ ] Implement:
  - `DoeMetrics metrics_from_vm(const VM *vm, uint64_t workload_duration_ns, int32_t cpu_temp_delta, int32_t cpu_freq_delta);`
- [ ] Implement:
  - `void metrics_write_csv_header(FILE *fp);`
  - `void metrics_write_csv_row(FILE *fp, const DoeMetrics *m);`
- [ ] Ensure CSV field order is stable and documented in a comment.

---

## 9. Metrics & Logging Hygiene

Goal: Make logs and metrics compatible and non-chaotic.

- [ ] Standardize log prefixes for physics/inference/DOE messages.
- [ ] Ensure DOE CSV is **single-line per run**, no stray prints.
- [ ] Verify logs go to stderr when in DOE mode, and CSV to stdout (or vice versa, but clearly separated).
- [ ] Add an option to disable all logs except errors during DOE.

---

## 10. Future Pub/Sub & Multi-VM (Design-Only for Now)

Goal: Prepare for pub/sub and multi-VM sync without implementing it fully.

- [ ] Create `PUBSUB_DESIGN.md` documenting:
  - VM event bus as the internal publisher
  - Potential external subscribers:
    - MamaForth
    - BastardForth
    - Monitoring/visualization tools
  - Message types for future:
    - heat sync
    - entropy deltas
    - inference parameter sync
    - tick-rate harmonization
- [ ] Sketch how multiple VMs could register into a global registry and share physics signals.

---

## 11. Testing & Validation

- [ ] Add unit tests for:
  - event dispatch with a dummy listener
  - pressure metric computation
  - tick interval computation (with mock pressure)
- [ ] Add regression tests for:
  - `--doe-experiment` mode:
    - produces exactly one CSV line per run
    - no crashes, no leaks
- [ ] Add a simple script (documented, not necessarily committed) to:
  - run N DOE runs
  - append to a CSV file
  - verify consistent column count.

---

## 12. Documentation

- [ ] `PHYSICS_RUNTIME.md` — overview of physics components and feedback loops.
- [ ] `PHYSICS_SIGNALS.md` — list of signals and where they live in the code.
- [ ] `PUBSUB_DESIGN.md` — future-facing multi-VM/pub-sub design.
- [ ] Update any existing high-level docs to mention:
  - adaptive heartbeat
  - physics runtime
  - DOE integration as first-class citizens.

---

## 13. Final Integration Checklist

- [ ] Adaptive heartbeat compiles and runs with the feature disabled by default.
- [ ] Event bus compiles and runs even with no listeners.
- [ ] DOE mode works end-to-end and is documented.
- [ ] No regressions in existing tests.
- [ ] All new behavior is behind flags and does not surprise users.
- [ ] Captain Bob has reviewed and approved each major step before any commit.

---

**Remember:**  
You are not here to make it “normal.”  
You are here to help build something **no one’s seen before** — one tiny, deterministic step at a time.
Sec 1 & 2 done.
