# Academic Wording Guidelines for StarForth

**Version**: 1.0
**Date**: 2025-12-14
**Purpose**: Maintain academic rigor and avoid causation claims

---

## Critical Rule: Avoid Implied Causation

When noting mathematical similarities between StarForth's empirical results and equations from physics, **always** use language that describes structural similarity, not equivalence.

---

## ✅ APPROVED PHRASES (Use These)

### Describing Mathematical Relationships

**Best (Most Defensive):**
```markdown
"We evaluated multiple candidate models and found that the functional form
τ = τ₀/√(1-β²) provided the best empirical fit across workloads."
```

**Good:**
- "fits the functional form"
- "shares structural similarity with"
- "is mathematically isomorphic to"
- "exhibits the same mathematical structure as"
- "the empirical data conforms to"
- "matches the pattern of"

### Describing Observations

**Good:**
- "We observed that..."
- "The data reveals..."
- "Measurements indicate..."
- "Empirical analysis shows..."
- "The experimental results demonstrate..."

### Noting Physics Parallels

**Good:**
- "shares dimensional similarity with"
- "the functional form also appears in [physics context]"
- "this mathematical structure is also found in"
- "bears mathematical resemblance to"

---

## ❌ FORBIDDEN PHRASES (Never Use)

### Direct Equivalence Claims

**Never:**
- "is equivalent to"
- "is the same as"
- "proves that"
- "demonstrates that [physics concept] applies"

### Unqualified Analogies

**Avoid (unless in clearly-marked interpretation sections):**
- "is analogous to"
- "corresponds to" (without "structurally" or similar qualifier)
- "behaves as" (without qualification)
- "mirrors" (implies direct correspondence)
- "exactly as predicted by"

### Causation Implications

**Never:**
- "because of [physics principle]"
- "due to [physics concept]"
- "governed by [physics law]"
- "obeys [physics equation]"

---

## Examples: Before & After

### Example 1: Performance Scaling

❌ **WRONG:**
```
"Performance scales exactly as predicted by special relativity's time dilation."
```

✅ **CORRECT:**
```
"We evaluated multiple candidate performance models. The empirical data fits
the functional form τ = τ₀/√(1-β²), which shares structural similarity with
the Lorentz transformation from special relativity."
```

**Why**: The correct version:
1. States we tried multiple models (not cherry-picking)
2. Uses "fits the functional form" (empirical observation)
3. Uses "shares structural similarity" (mathematical fact, not causation)

---

### Example 2: Window Capacity Law

❌ **WRONG:**
```
"The system obeys a cosmological constant law Λ(DoF) = 4096/(DoF+1)."
```

✅ **CORRECT:**
```
"Empirical measurements reveal that window capacity follows the relationship
Λ(DoF) = 4096/(DoF+1) with 0.00% coefficient of variation. The symbol Λ is
borrowed from cosmology for mathematical convenience."
```

**Why**: The correct version:
1. "Empirical measurements reveal" (observation, not theory)
2. "follows the relationship" (describes pattern, not obedience to law)
3. Explicitly states Λ is "borrowed... for mathematical convenience" (notation, not physics)

---

### Example 3: Geodesic Convergence

❌ **WRONG:**
```
"Workloads converge along geodesics analogous to general relativity."
```

✅ **CORRECT:**
```
"All workload trajectories converge to the same attractor basin regardless of
starting conditions. This pattern shares structural similarity with geodesic
motion in curved spacetime."
```

**Why**: The correct version:
1. Describes observed behavior first (empirical fact)
2. "shares structural similarity" (mathematical comparison, not physical claim)
3. No causation implied

---

## Defensive Language Patterns

### Pattern 1: Model Evaluation Statement

**Template:**
```
"We evaluated [N] candidate models including [list]. The functional form
[equation] provided the best fit with R² = [value]."
```

**Example:**
```
"We evaluated linear, polynomial, exponential, and power-law models. The
functional form f(t) = f₀×e^(-λt) provided the best fit with R² = 0.96."
```

**Defense**: Shows you didn't cherry-pick; you tested alternatives.

---

### Pattern 2: Empirical Observation + Mathematical Note

**Template:**
```
"[Empirical observation]. This [relationship/pattern/structure] also appears
in [context] as [reference]."
```

**Example:**
```
"Performance scales nonlinearly with degrees of freedom. This functional form
also appears in special relativity as the Lorentz factor."
```

**Defense**: Observation is separate from physics reference; no causation implied.

---

### Pattern 3: Metaphor Disclosure

**Template:**
```
"We employ [physics concept] as a conceptual metaphor, where [mapping]. The
actual implementation uses [literal description]."
```

**Example:**
```
"We employ thermodynamic cooling as a conceptual metaphor, where execution
frequency decreases over time. The actual implementation uses exponential
decay f(t) = f₀×e^(-λt) applied periodically."
```

**Defense**: Explicitly labels metaphor as metaphor; provides literal implementation.

---

## Section-Specific Guidelines

### In Abstracts/Titles

**Use**: Mathematical/empirical language only
- ✅ "Frequency-Based Adaptive Runtime"
- ❌ "Physics-Driven Adaptive Runtime"

### In Introductions

**First**: State empirical observations
**Then**: Note mathematical similarities
**Never**: Claim physics explains computation

### In Methods/Implementation

**Use**: Literal descriptions only
- ✅ "Frequency counter incremented on execution"
- ❌ "Temperature increases when word heats up"

### In Results

**Use**: Statistical language
- ✅ "CV = 0.00% (p < 10^-30)"
- ❌ "Perfect thermodynamic equilibrium achieved"

### In Discussion/Interpretation Sections

**Allowed**: Exploratory language, **IF** properly qualified
- ✅ "One interpretation is that... However, causation is not established."
- ✅ "This pattern suggests possible structural similarities with..."
- ❌ "This proves that computation follows physics laws"

---

## Academic Reviewer Defense Scenarios

### Scenario 1: "You're claiming physics!"

**Response:**
"No. Section [X] explicitly states: 'We employ [concept] as a conceptual
metaphor.' All empirical claims are mathematical: the functional form
f(t) = f₀×e^(-λt) fits our performance data with R² = [value]. That this
same functional form appears in physics is noted as a mathematical
similarity, not a physical claim."

### Scenario 2: "You cherry-picked equations!"

**Response:**
"Section [X] documents that we evaluated [N] candidate models: [list]. The
reported functional form was selected based on empirical fit quality
(R² = [value], AIC = [value]), not theoretical preference."

### Scenario 3: "This is pseudoscience!"

**Response:**
"All claims are empirically verifiable:
1. [Claim 1]: See [data file], line [X]
2. [Claim 2]: See [experimental results]
3. [Claim 3]: Reproduction protocol in [file]

The physics references describe mathematical similarities, clearly documented
in ONTOLOGY.md Section [X] as metaphorical framing, not physical causation."

---

## Checklist Before Publication

Before submitting any paper, presentation, or public documentation:

- [ ] Search for "is equivalent to" → replace with "fits the functional form"
- [ ] Search for "is analogous to" → replace with "shares structural similarity"
- [ ] Search for "corresponds to" → add qualifier like "structurally"
- [ ] Search for "mirrors" → replace with "matches the pattern of"
- [ ] Search for "exactly as" → replace with "according to the functional form"
- [ ] Search for "obeys" → replace with "follows the relationship"
- [ ] Search for "governed by" → replace with "characterized by"
- [ ] Verify all metaphors are explicitly labeled in text
- [ ] Confirm empirical claims have data references
- [ ] Check that all physics parallels use "shares similarity" language

---

## Cross-Reference

See also:
- **ONTOLOGY.md** - Section 3.2 "Avoid/Deprecated Terms"
- **ONTOLOGY.md** - Section 7.1 "Academic Writing Guidelines"
- **ONTOLOGY.md** - Section 1.2 "Conceptual Relationships" (metaphor mapping)

---

## Summary

**Golden Rule**: Describe what you **observed**, note where it **mathematically resembles** known equations, but **never claim** the physics causes the behavior.

**Your Position**:
- "We measured these relationships in our system"
- "They fit these mathematical forms"
- "These forms also appear in physics"
- "This similarity may provide useful modeling frameworks"

**NOT Your Position**:
- "Our system implements physics"
- "Physics laws govern our system"
- "This proves computation is physical"

---

**Maintain this discipline rigorously, and no academic reviewer can challenge your scientific integrity.**

---

**License**: CC0 / Public Domain
