# StarForth Physics Runtime

## PHYSIOLOGICAL MONITORING PRINCIPLES

### (Non-Perturbative, t≈0, History+Present Feedback Model)

**Author:** Captain Bob
**Purpose:** Canonical rules for ALL AI assistants and contributors
**Scope:** StarForth’s physics runtime, monitoring subsystem, inference engine, DOE integration

---

# 1. Overview

The StarForth VM is built as a **physics-driven organism**, not a traditional interpreter.
It contains several continuous, interacting feedback loops:

- Heat accumulation & decay
- Rolling Window of Truth
- Hotwords cache dynamics
- Prefetch heuristics
- Transition graph formation
- Adaptive decay slope tuning
- Adaptive heartbeat interval control

These loops **must exist and operate** before any monitoring or metrics can be safely attached.

Monitoring is *non-perturbative* and *observational only*.
No sensor may inject latency, allocations, reordering, contention, or architectural changes.

This document defines the **fundamental principles** for where, when, and how monitoring occurs.

---

# 2. Physiology Before Instrumentation

Instrumentation must **never** be built before the runtime’s physiology is wired.

**Required order of operations:**

1. Build core runtime loops
   - Heat & decay
   - Window updates
   - Prefetch logic
   - Inference heuristics
   - Tick/heartbeat loop
   - Transition metrics
2. Let the system run and reach first-cycle stability
3. Only then attach:
   - Sensors
   - DOE metrics
   - Event bus listeners
   - Observability points

**Why?**
Before the loops run, there *is no signal* to observe; metrics would produce garbage or perturb the VM.

---

# 3. Monitoring Begins at t≈0 (Earliest Stable Cycle)

## 3.1 Window Bootstrapping: Empty Window Is VALID

The Rolling Window of Truth **must begin empty** at VM startup.

This is intentional and required.

**Principle:**

> The VM must be able to operate with no history and adapt “with what it has,”
> allowing inference mechanisms to form their earliest predictions from sparse data.

This means:

- An empty window is **not an error condition**
- No artificial seeding or fake signal insertion is allowed
- Inference must be able to:
  - detect missing history
  - treat first values as baseline
  - adapt gradually as the window fills
- Early fluctuations are **normal and desired**
- Decay slope tuning and window-size tuning must behave conservatively when history is thin
- Prefetch heuristics must NOT assume a non-empty window
- No subsystem may depend on the window being pre-populated

This startup period is the system’s **initial learning phase**,analogous to:

- the warm-up cycle of a PID controller
- the first few samples of a strip-chart recorder
- the startup of a biological homeostasis loop

The window will fill naturally as the VM runs, and all feedback loops will stabilize accordingly.

Monitoring should begin at:

> **The earliest moment when all feedback loops have completed at least one internal cycle, but before any long-term adaption occurs.**

This is referred to as **t≈0**:

- *Not literal time zero*
- But the **first physiologically meaningful moment**

At t≈0:

- The Rolling Window of Truth has its first value
- Heat has been incremented at least once
- Cache promotion may have occurred
- Decay slope has an initialized baseline
- Prefetch tracking exists
- Transition graph has ≥ 1 edge
- Heartbeat loop has run once

From this point forward, the VM has **true physics** to observe.

>Where seed values are needed, they will be provided buy the builder when they configure their build. 
---

# 4. Non-Perturbative Observation

Monitoring must **never disturb** the VM or affect:

- dictionary ordering
- bucket heat
- window width
- inference tuning
- lookup strategy
- word execution timing
- tick interval
- cache hits/misses

### Absolute rules:

- No malloc/free
- No locks (use read-only pointers)
- No dictionary traversal
- No reordering
- No I/O to stdout during VM operations
- No touching any structure that the physics feedback loops mutate

**Monitors observe.
They never participate.**

---

# 5. History + Present → Inference → Adjustment

This is the **golden rule** governing all inference cycles.

Every adaptive subsystem:

- decay slope tuning
- window width adjustment
- prefetch ROI
- hotword promotion heuristics
- adaptive heartbeat pressure

…must base its decisions on:

### 1. **History** (integral component)

Examples:

- Rolling Window of Truth
- Previous decay slope
- Heat momentum
- Prefetch historical hit/miss ratio
- Transition graph edges
- Long-term pressure average

### 2. **Current Value** (proportional component)

Examples:

- current heat spike
- current window entry
- instant tick-time pressure
- current transition
- current hit/miss event

### 3. **Inference Process**

- smoothing
- numerical weighting
- confidence scoring
- stochastic biasing (deterministic, seeded)
- bounded slope tuning
- bounded window expansion/contraction

### 4. **Adjustment (output)**

- updated decay slope
- updated window width
- updated prefetch heuristics
- updated heartbeat interval (if enabled)
- updated hotword promotion threshold

This mirrors a **PID controller** but tuned for StarForth’s information thermodynamics.

---

# 6. Monitoring Points Must Be Explicit and Side-Effect-Free

For every feedback loop (FL1–FL4), there should be **explicit, isolated sampling points**:

- heat updated
- window updated
- decay slope recalculated
- inference cycle completed
- prefetch stat updated
- transition recorded
- heartbeat tick completed

These sampling points must:

- Use only **const VM*** (no mutation)
- Read-only access to all internal structures
- Never influence scheduling or ordering
- Never produce allocations
- Never block the VM

---

# 7. Monitoring Feeds DOE, Not the VM

All monitoring output goes to one of three sinks:

1. **Event Bus**

   - Optional, listener-based, non-intrusive
2. **DOE Metrics Collector**

   - Single-line CSV output
   - Deterministic
   - No logs on stdout
3. **Optional Debug Logging**

   - stderr only
   - never in CICD
   - never in DOE mode without explicit flag

Monitoring **informs** experimentation.
It does **not** control the VM.

The VM’s physiology controls itself.

---

# 8. Monitoring Must Not Create New Feedback Loops

The system already has four core loops:

- **FL1:** Heat accumulation & decay
- **FL2:** Hotword promotion
- **FL3:** Inference → decay slope tuning
- **FL4:** Inference → window width tuning

Monitoring must **not** introduce:

- FL5 feedback contamination
- logging-based slowdown
- mechanical timing noise
- heat changes due to sampling
- cache busting due to instrumentation

Monitoring *describes*.
It does not *shape*.

---

# 9. Summary Checklist

### Mandatory:

- [X]  Physiology wired before instrumentation
- [X]  Monitoring begins at first stable cycle (t≈0)
- [X]  Monitors are non-perturbative
- [X]  Metrics use history+present → inference → adjustment
- [X]  Metrics never modify physics state
- [X]  DOE output is isolated and side-effect-free

### Forbidden:

- [ ]  Sampling inside locked dictionary regions
- [ ]  Allocating memory during monitoring
- [ ]  Reordering dictionary due to instrumentation
- [ ]  Logging to stdout during VM operation
- [ ]  Creating new feedback loops via metrics

---

# 10. Closing Notes

StarForth is not a “VM with some metrics.”
It is a **self-regulating cybernetic organism** with measurable physics.

These monitoring principles are **doctrinal**.
Any assistant, contributor, or future tool must follow them **without modification**.

Misunderstanding this document = breaking StarForth.
