# StarForth Ontology, Taxonomy, and Lexicon

**Version**: 1.0
**Date**: 2025-12-13
**Purpose**: Formal conceptual framework for precise academic discourse

---

## I. ONTOLOGY (Conceptual Framework)

### 1.1 Core Concepts

```
StarForth Adaptive Runtime System
├── Execution Metrics
│   ├── Execution Frequency (primary measurable)
│   ├── Temporal Decay (derived quantity)
│   └── Transition Probability (derived quantity)
├── Adaptive Mechanisms
│   ├── Frequency-Based Caching
│   ├── Window-Based Inference
│   └── Decay-Based Pruning
├── Convergence Properties
│   ├── Deterministic Behavior
│   ├── Steady-State Equilibrium
│   └── Variance Reduction
└── Analysis Frameworks
    ├── Dynamical Systems View
    ├── Statistical Inference View
    └── Control Theory View
```

### 1.2 Conceptual Relationships

```
METAPHORICAL MAPPING (Thermodynamics → Execution):
  Thermal Energy        ≈ Execution Frequency (measurable count)
  Heat Dissipation      ≈ Exponential Decay (time-based reduction)
  Thermal Equilibrium   ≈ Steady-State Convergence (stable metrics)
  Temperature           ≈ Normalized Frequency Rank
  Cooling Rate          ≈ Decay Coefficient (λ)

LITERAL IMPLEMENTATIONS (No Metaphor):
  Frequency Counter     → Integer increment on execution
  Decay Function        → f(t) = f₀ * e^(-λt)
  Hot-Words Cache       → Top-K frequency-sorted entries
  Rolling Window        → Circular buffer of execution records
  ANOVA Test            → Statistical variance analysis (Levene's test)
```

---

## II. TAXONOMY (Hierarchical Classification)

### 2.1 Adaptive Runtime Components

```
1. Measurement Layer
   1.1 Execution Frequency Tracking
       └── Dictionary Entry Counter (per-word increment)
   1.2 Temporal Recording
       └── Rolling Window of Truth (circular buffer)
   1.3 Transition Tracking
       └── Word-to-Word Transition Matrix

2. Transformation Layer
   2.1 Decay Models
       2.1.1 Linear Decay (Loop #3)
             └── Δf = -k * Δt
       2.1.2 Exponential Decay (Loop #6 Inference)
             └── f(t) = f₀ * e^(-λt)
   2.2 Normalization
       2.2.1 Frequency Ranking
       2.2.2 Probability Calculation

3. Inference Layer
   3.1 Window Width Inference (Loop #5)
       3.1.1 Variance Analysis (Levene's Test)
       3.1.2 Binary Search (Variance Inflection Point)
   3.2 Decay Slope Inference (Loop #6)
       3.2.1 Exponential Regression
       3.2.2 Least Squares Fitting

4. Actuation Layer
   4.1 Hot-Words Cache (Loop #1)
       4.1.1 Frequency-Based Sorting
       4.1.2 Top-K Selection
       4.1.3 Fast-Path Dictionary Lookup
   4.2 Speculative Execution (Loop #4)
       4.2.1 Transition Probability Calculation
       4.2.2 Prefetch Decision

5. Coordination Layer
   5.1 Heartbeat System (Loop #7)
       5.1.1 Time-Driven Tick Generation
       5.1.2 Loop Orchestration
       5.1.3 Adaptive Tick Rate
```

### 2.2 Feedback Loop Taxonomy

```
Feedback Loops (7 Total)
├── Positive Loops (Amplifying)
│   ├── Loop #1: Execution Heat Tracking
│   │   └── More executions → Higher rank → More cache hits → More executions
│   └── Loop #4: Pipelining Metrics
│       └── More transitions → Better prediction → More prefetch hits
├── Negative Loops (Stabilizing)
│   ├── Loop #3: Linear Decay
│   │   └── High frequency → Faster decay → Lower frequency → Slower decay
│   ├── Loop #5: Window Width Inference
│   │   └── High variance → Smaller window → Lower variance
│   └── Loop #6: Decay Slope Inference
│       └── Unstable metrics → Steeper decay → Faster stabilization
├── Neutral Loops (Monitoring)
│   └── Loop #2: Rolling Window History
│       └── Execution occurs → Record in window → Historical data available
└── Meta-Loop (Adaptive Coordination)
    └── Loop #7: Adaptive Heartbeat
        └── Stable system → Slower ticks → Less overhead
```

### 2.3 Convergence Taxonomy

```
Convergence Properties
├── Determinism
│   ├── Algorithmic Determinism (same inputs → same outputs)
│   ├── Temporal Determinism (time-invariant steady state)
│   └── Statistical Determinism (0% variance)
├── Steady-State Characteristics
│   ├── Fixed Point (attractors in phase space)
│   ├── Periodic Behavior (limit cycles)
│   └── Chaotic Behavior (sensitive dependence - avoided)
└── Variance Metrics
    ├── Inter-Run Variance (across multiple executions)
    ├── Intra-Run Variance (within single execution)
    └── Temporal Variance (across time windows)
```

---

## III. LEXICON (Precise Definitions)

### 3.1 Core Terms (Alphabetical)

**Adaptive Heartbeat**
*Definition*: Time-driven coordination mechanism that orchestrates feedback loop execution at dynamically-adjusted intervals.
*Formal*: Thread T executing `vm_tick()` at frequency f_tick, where f_tick ∈ [f_min, f_max] adapts based on system stability.
*Measurement*: Tick period in nanoseconds (configurable: HEARTBEAT_TICK_NS).
*Category*: Coordination mechanism.

**Attractor**
*Definition*: Stable equilibrium point or region in phase space toward which execution trajectories converge.
*Formal*: Fixed point x* where F(x*) = x* for dynamical system x_{t+1} = F(x_t).
*Measurement*: Coordinates in (window_size, decay_slope, variance) space.
*Category*: Dynamical systems concept.

**Decay Coefficient (λ)**
*Definition*: Rate parameter controlling exponential reduction in execution frequency over time.
*Formal*: λ in f(t) = f₀ * e^(-λt), units of [1/time].
*Measurement*: Derived via exponential regression; stored as Q48.16 fixed-point.
*Category*: Transformation parameter.

**Deterministic Convergence**
*Definition*: Property whereby repeated executions of identical workloads produce statistically indistinguishable steady-state metrics.
*Formal*: ∀ executions i,j: |metric_i - metric_j| / σ < ε, where ε → 0 as t → ∞.
*Measurement*: Coefficient of variation CV = σ/μ → 0%.
*Category*: Convergence property.

**Execution Frequency**
*Definition*: Count of times a dictionary entry has been executed since VM initialization, optionally adjusted by decay.
*Formal*: f = Σ executions - ∫ decay(t) dt.
*Measurement*: Unsigned 64-bit integer (`uint64_t execution_heat`).
*Category*: Primary measurable quantity.
*Note*: "Heat" is metaphorical naming; actual quantity is frequency.

**Exponential Decay**
*Definition*: Mathematical function modeling reduction in execution frequency proportional to current value.
*Formal*: f(t) = f₀ * e^(-λt), where f₀ is initial frequency.
*Measurement*: Applied periodically by heartbeat system.
*Category*: Transformation function.
*Metaphor*: Analogous to radioactive decay or thermal dissipation.

**Hot-Words Cache**
*Definition*: Fixed-size array storing pointers to the K most frequently executed dictionary entries for O(1) lookup acceleration.
*Formal*: Cache C = {e_1, e_2, ..., e_K} where f(e_i) ≥ f(e_{i+1}) ∀ i.
*Measurement*: Cache size K (configurable: HOTWORDS_CACHE_SIZE).
*Category*: Frequency-based optimization.

**Levene's Test**
*Definition*: Non-parametric statistical test for homogeneity of variance across groups.
*Formal*: H₀: σ₁² = σ₂² = ... = σ_k² (variances equal).
*Measurement*: F-statistic with p-value threshold (typically 0.05).
*Category*: Statistical inference method.
*Application*: Used to detect variance changes when adjusting window size.

**Phase Space**
*Definition*: Multi-dimensional coordinate system where each axis represents a system state variable.
*Formal*: Space S = {(w, λ, σ²) | w ∈ ℕ, λ ∈ ℝ⁺, σ² ∈ ℝ⁺}.
*Measurement*: Coordinates: (window_size, decay_slope_Q48, variance_Q48).
*Category*: Dynamical systems representation.

**Rolling Window of Truth**
*Definition*: Circular buffer recording recent execution history for deterministic metric seeding.
*Formal*: Buffer B[i] = word_id at execution event i mod |B|.
*Measurement*: Buffer size (configurable: ROLLING_WINDOW_SIZE = 4096).
*Category*: Temporal recording mechanism.
*Purpose*: Ensures identical initial conditions for reproducibility.

**Steady-State Equilibrium**
*Definition*: Condition where adaptive system metrics stabilize within bounded oscillation.
*Formal*: ∃ t_0: ∀ t > t_0, |x(t) - x*| < δ for small δ.
*Measurement*: Variance CV < 0.1% over 1000-tick window.
*Category*: Convergence property.
*Metaphor*: Analogous to thermodynamic equilibrium.

**Thermodynamic Metaphor**
*Definition*: Conceptual mapping between thermodynamic quantities and execution metrics.
*Mapping*:
  - Heat ↔ Execution Frequency
  - Temperature ↔ Normalized Rank
  - Cooling ↔ Exponential Decay
  - Equilibrium ↔ Steady State
*Category*: Conceptual framework (not literal physics).
*Warning*: Must qualify as metaphor in academic writing.

**Transition Probability**
*Definition*: Conditional probability that word B is executed immediately after word A.
*Formal*: P(B|A) = count(A→B) / count(A).
*Measurement*: Stored as Q48.16 fixed-point in transition matrix.
*Category*: Derived metric for speculative execution.

**Variance Inflection Point**
*Definition*: Window size w* where variance begins to increase when window shrinks below w*.
*Formal*: w* = arg min_w { Var(w) | w < w_current }.
*Measurement*: Found via binary search with Levene's test.
*Category*: Inference target.
*Purpose*: Optimal window size for stable metrics.

**Word Transition**
*Definition*: Sequential execution of word B immediately following word A in threaded code.
*Formal*: Event (A, B, t) where A executes at time t and B at time t+ε.
*Measurement*: Recorded in transition_metrics.history[] circular buffer.
*Category*: Execution event.

---

### 3.2 Avoid/Deprecated Terms

| ❌ Avoid | ✅ Use Instead | Reason |
|---------|---------------|--------|
| "Physics-based" | "Thermodynamically-inspired metaphor" | Not literal physics |
| "Execution heat" (in formal writing) | "Execution frequency with decay" | "Heat" is metaphorical |
| "Temperature" | "Normalized frequency rank" | No actual thermal quantity |
| "Quantum-inspired" | N/A | No quantum mechanics involved |
| "AI-driven" | "Statistically-inferred" | No neural networks or ML |
| "Learning" | "Adaptive inference" | Not machine learning |
| "Training" | "Convergence to steady state" | Not supervised learning |

---

## IV. MATHEMATICAL FORMALISM

### 4.1 Execution Frequency Evolution

**Discrete-time update**:
```
f[t+1] = f[t] + Δexec[t] - Δdecay[t]

where:
  Δexec[t]  = 1 if word executed at tick t, else 0
  Δdecay[t] = λ * f[t] * Δt  (exponential decay approximation)
```

**Continuous-time model**:
```
df/dt = r(t) - λf(t)

where:
  r(t) = execution rate [executions/second]
  λ    = decay coefficient [1/second]

Solution:
  f(t) = e^(-λt) * [f₀ + ∫₀ᵗ r(τ)e^(λτ) dτ]
```

### 4.2 Hot-Words Cache Selection

**Cache membership criterion**:
```
e ∈ Cache ⟺ rank(e) ≤ K

where:
  rank(e) = |{e' ∈ Dictionary : f(e') > f(e)}| + 1
  K       = cache size (constant)
```

### 4.3 Window Width Inference

**Objective function**:
```
w* = arg min { Var(w) : w ∈ [w_min, w_current] }

Subject to:
  Levene(w, w_current) → F-statistic
  p-value(F) < α  (typically α = 0.05)
```

### 4.4 Decay Slope Inference

**Exponential regression**:
```
Given: {(t_i, f_i)}_{i=1}^N from rolling window

Model: f(t) = f₀ * e^(-λt)

Log-transform: ln(f) = ln(f₀) - λt

Least squares:
  λ* = arg min Σ [ln(f_i) - (ln(f₀) - λt_i)]²
```

### 4.5 Transition Probability

**Maximum likelihood estimate**:
```
P(B|A) = count(A→B) / count(A)

where:
  count(A→B) = # times B executed immediately after A
  count(A)   = # times A executed
```

### 4.6 Convergence Metric

**Coefficient of Variation**:
```
CV = σ / μ

where:
  σ = √(Var[metric]) = standard deviation
  μ = E[metric]      = mean

Convergence achieved when: CV → 0
```

---

## V. RELATIONSHIP DIAGRAM

### 5.1 Component Dependencies

```
┌─────────────────────────────────────────────────────────────┐
│                     Execution Event                          │
│                     (word executed)                          │
└──────────────────────┬──────────────────────────────────────┘
                       │
           ┌───────────┴───────────┐
           │                       │
           ▼                       ▼
  ┌────────────────┐      ┌──────────────────┐
  │  Frequency     │      │ Rolling Window   │
  │  Increment     │      │  Recording       │
  │  (Loop #1)     │      │  (Loop #2)       │
  └────────┬───────┘      └─────────┬────────┘
           │                        │
           │                        │
           ▼                        ▼
  ┌────────────────┐      ┌──────────────────┐
  │  Decay         │      │  Inference       │
  │  Application   │      │  Engine          │
  │  (Loop #3,#6)  │      │  (Loop #5,#6)    │
  └────────┬───────┘      └─────────┬────────┘
           │                        │
           └───────────┬────────────┘
                       │
                       ▼
           ┌───────────────────────┐
           │   Hot-Words Cache     │
           │   Reorganization      │
           └───────────┬───────────┘
                       │
                       ▼
           ┌───────────────────────┐
           │  Optimized Lookup     │
           │  (O(1) cache hit)     │
           └───────────────────────┘
```

### 5.2 Feedback Loop Interactions

```
Execution → Frequency ↑ → Cache Rank ↑ → Lookup Speed ↑ → More Execution
            ▲                                                 │
            │                                                 │
            └─────────────── (Positive Feedback) ────────────┘

Frequency ↑ → Decay ↑ → Frequency ↓ → Decay ↓ → Frequency Stabilizes
              ▲                        │
              │                        │
              └──── (Negative Feedback) ────┘

Variance ↑ → Window Shrink → Variance ↓ → Window Stable
             ▲                           │
             │                           │
             └──── (Negative Feedback) ──┘
```

---

## VI. ONTOLOGICAL COMMITMENTS

### 6.1 Foundational Assumptions

1. **Frequency as Proxy for Importance**
   - Assumption: Frequently executed code is more important to optimize
   - Justification: Empirical validation (Zipf's law in execution patterns)

2. **Decay Models Temporal Relevance**
   - Assumption: Recent executions are more relevant than distant past
   - Justification: Locality of reference (temporal locality principle)

3. **Determinism Through Convergence**
   - Assumption: Adaptive systems can converge to deterministic steady states
   - Justification: Fixed-point theorems for contractive mappings

4. **Statistical Inference Validity**
   - Assumption: Execution patterns are statistically analyzable
   - Justification: Central Limit Theorem for large sample sizes

### 6.2 Scope Limitations

**What This Ontology DOES Cover**:
- ✓ Execution frequency measurement and decay
- ✓ Adaptive caching and inference mechanisms
- ✓ Dynamical systems characterization
- ✓ Statistical convergence properties

**What This Ontology DOES NOT Cover**:
- ✗ Actual thermodynamic processes (metaphor only)
- ✗ Machine learning or neural networks (no training)
- ✗ Quantum computing (no quantum effects)
- ✗ Biological neural systems (no biomimicry)

---

## VII. USAGE GUIDELINES

### 7.1 Academic Writing

**In Abstracts/Titles**: Use precise, non-metaphorical terms
```
✅ "Thermodynamically-Inspired Adaptive Runtime"
❌ "Physics-Based Virtual Machine"
```

**In Technical Sections**: Qualify metaphors explicitly
```
✅ "We employ a thermodynamic metaphor where execution frequency
    is treated as 'heat' that dissipates over time..."
❌ "The physics model applies heat decay..."
```

**In Formalism**: Use mathematical definitions, not analogies
```
✅ "Frequency evolves as f(t) = f₀ * e^(-λt)"
❌ "Heat cools exponentially like in Newton's law"
```

### 7.2 Code Comments

**In Source Code**: Use concrete terms from lexicon
```c
// ✅ GOOD: Increment execution frequency counter
entry->execution_heat++;

// ❌ BAD: Increase temperature of word
entry->execution_heat++;  // heat up!
```

### 7.3 Presentation/Talks

**Slides**: Use metaphor for intuition, then formalize
```
Slide 1: "Think of execution frequency like heat..."
Slide 2: "Formally: f(t) = f₀ * e^(-λt)"
```

---

## VIII. REFERENCES

### 8.1 Foundational Concepts

- **Dynamical Systems**: Strogatz, S. (2015). *Nonlinear Dynamics and Chaos*
- **Control Theory**: Åström, K. & Murray, R. (2008). *Feedback Systems*
- **Statistical Inference**: Casella, G. & Berger, R. (2002). *Statistical Inference*
- **Exponential Decay**: Standard mathematical function (e^(-λt))

### 8.2 Related Work

- **Trace-based JIT**: Bolz et al. (2009). "Tracing the Meta-Level: PyPy's Tracing JIT Compiler"
- **Adaptive Systems**: Garlan et al. (2004). "Rainbow: Architecture-Based Self-Adaptation"
- **Forth Optimization**: Ertl, M.A. (1996). "Stack Caching for Interpreters"

---

## IX. VERSION HISTORY

**v1.0** (2025-12-13):
- Initial ontology, taxonomy, and lexicon
- Formal mathematical definitions
- Relationship diagrams
- Usage guidelines

---

## X. ACKNOWLEDGMENTS

This ontology provides the formal conceptual framework for the StarForth project.
It aims to eliminate ambiguity and enable precise academic discourse while
acknowledging the metaphorical nature of certain conceptual mappings.

**License**: See ./LICENSE
