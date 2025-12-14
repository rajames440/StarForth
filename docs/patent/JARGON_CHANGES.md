# Jargon Removal and Precision Language Changes
## Date: 2025-12-10

### Purpose
Replaced metaphorical/jargon terminology with precise technical language or added explicit disclaimers explaining analogies. This prevents misunderstanding and makes claims more defensible.

---

## Key Term Replacements

### 1. "Execution Heat" → "Execution Frequency Counters"
**BEFORE (vague/metaphorical):**
```
Execution Heat: A scalar quantity that increases when an instruction executes
and decays over time.
```

**AFTER (precise):**
```
Execution Frequency Counters ("Execution Heat"): A scalar quantity that
increments when an instruction or word executes and decrements over time
according to a time-based decay function. The term "heat" is used by analogy
to thermal systems but refers to a dimensionless counter value.
```

**Why:** "Heat" is a thermal/physics term. We're actually counting executions. The disclaimer clarifies it's an analogy.

---

### 2. "Entropy" → "Variability Measure" or "Statistical Variance"
**BEFORE (borrowed from information theory):**
```
Entropy Window: A sliding distribution reflecting the variability of execution heat.
```

**AFTER (precise):**
```
Variability Measure ("Entropy Window"): A sliding statistical distribution
reflecting the variability of execution frequency counters. The term "entropy"
is used by analogy to information theory but refers to statistical variance.
```

**Why:** "Entropy" has specific meaning in physics/information theory. We're measuring statistical variance. Disclaimer clarifies the analogy.

---

### 3. "Pipeline Pressure" → "Instruction Queue Depth"
**BEFORE (metaphorical):**
```
Pipeline Pressure: A measurement of lookup latency, structural hazards, or
contention in the interpreter pipeline.
```

**AFTER (precise):**
```
Instruction Queue Depth ("Pipeline Pressure"): A measurement of lookup latency,
structural hazards, or contention in the interpreter execution pipeline.
Elevated queue depth may indicate an opportunity for caching or prefetch adaptation.
```

**Why:** "Pressure" is a fluid dynamics term. We're measuring queue depth. Clarified as a metaphor.

---

### 4. "Decay" → "Time-Based Counter Decay" or "Decay Coefficient"
**BEFORE (vague):**
```
Decay Rate: A tunable parameter governing how quickly heat dissipates.
```

**AFTER (precise):**
```
Decay Rate Parameter: A tunable coefficient governing the rate at which
execution frequency counters decrease over time.
```

**Why:** "Dissipates" is a thermal term. We're applying a decay coefficient to counters. Made the mechanism explicit.

---

### 5. "Temporal Markers" → "Time Indices"
**BEFORE (vague):**
```
Temporal Markers: Counters, clock signals, or window indices used by decay
functions and inference weighting logic.
```

**AFTER (precise):**
```
Time Indices: Integer counters, timestamp values, or circular buffer indices
used by time-based decay functions and statistical weighting algorithms.
```

**Why:** "Markers" is vague. These are integer indices/timestamps. Made the data types explicit.

---

### 6. "Stability Score" → "Stability Metric"
**BEFORE (vague):**
```
Stability Score: A derived metric computed from recent execution timing variance.
```

**AFTER (precise):**
```
Stability Metric: A derived metric computed from recent execution timing variance,
typically expressed as coefficient of variation (CV = standard deviation / mean).
```

**Why:** "Score" implies subjective rating. It's a quantitative metric (CV). Added the formula.

---

## Feedback Loop Name Changes

### L1: "Heat Accumulation" → "Execution Frequency Tracking"
**Why:** "Accumulation" is vague. We're tracking execution frequencies via counters.

### L2: "Statistical Inference" → "Pattern Recognition"
**Why:** "Inference" is overly broad. We're doing pattern matching on execution history.

### L3: "Temporal Decay" → "Time-Based Counter Decay"
**Why:** "Temporal Decay" is metaphorical. Made the mechanism explicit: time-based coefficient applied to counters.

### L4: "Pipeline Optimization" → "Lookup Optimization"
**Why:** More precise - we're optimizing dictionary lookups and cache strategies.

### L5: "Window Stabilization" → "Measurement Window Adaptation"
**Why:** We're not "stabilizing" the window, we're adaptively sizing the observation window.

### L6: "Inference Weighting" → "Pattern Confidence Weighting"
**Why:** More precise - we're assigning confidence levels to pattern recognition outputs.

### L7: "Baseline Stabilizer" → "Stability Guarantor"
**Why:** More precise - it enforces stability bounds rather than "stabilizing a baseline."

---

## Mode Name Changes

### "Inference Mode" → "Pattern Recognition Mode"
**Why:** "Inference" is broad. It's specifically doing pattern matching, not general inference.

### "Full Adaptive Mode" → "Full Adaptive Mode (no change needed)"
**But clarified:** "Enables multiple feedback loops (L2, L3, L5, L6) simultaneously"
**Why:** Made explicit which loops are active rather than vague "advanced combinations."

---

## Other Critical Clarifications

### "Rolling Window of Truth" → "Execution History Buffer"
**BEFORE (dramatic name):**
```
the rolling window of truth
```

**AFTER (technical description):**
```
the execution history buffer (a circular buffer storing recent execution events)
```

**Why:** "Rolling Window of Truth" is a dramatic name. Clarified it's a circular buffer storing execution history.

---

### "Jacquard Mode Selector" → "Supervisory Mode Selector"
**BEFORE (historical reference):**
```
The Jacquard Mode Selector (L8) examines the state vector...
```

**AFTER (clarified):**
```
The Supervisory Mode Selector (L8)... The name "Jacquard" refers by historical
analogy to the Jacquard loom's pattern selection mechanism, but the controller
operates via algorithmic decision logic.
```

**Why:** "Jacquard" is a historical reference that might confuse readers. Clarified it's an analogy to automated pattern selection.

---

### Workload Characterization
**BEFORE (vague descriptors):**
```
- Stable: low entropy, repetitive patterns
- Volatile: unpredictable bursts or chaotic behavior
```

**AFTER (quantitative definitions):**
```
- Stable: low coefficient of variation, repetitive execution patterns
- Volatile: high coefficient of variation, unpredictable execution bursts
```

**Why:** Replaced subjective terms ("low entropy," "chaotic") with quantitative metrics (CV).

---

## Summary Section Changes

### State Vector Description
**BEFORE:**
```
representing execution heat (a measure of instruction frequency), entropy
(workload variability), temporal decay behavior, pipeline pressure...
```

**AFTER:**
```
representing execution frequency counters (referred to as "execution heat"
by analogy), workload variability metrics (statistical variance, referred to
as "entropy" by analogy to information theory), time-based decay parameters,
instruction queue depth...
```

**Why:** Every metaphorical term now has a precise definition and explicit disclaimer.

---

## What This Achieves

### 1. **Precision**
- Every term has a clear technical meaning
- No ambiguity about what's being measured or computed

### 2. **Defensibility**
- Patent examiners won't misunderstand metaphorical terms
- No risk of rejection due to vague or misleading language

### 3. **Honesty**
- Analogies are explicitly labeled as analogies
- We're not pretending counters are "heat" or variance is "entropy"

### 4. **Accessibility**
- Engineers can understand exactly what the system does
- No need to decode metaphors to understand the mechanism

---

## Terms That WERE Kept (With Disclaimers)

These terms were retained because they're useful shorthand, but now have explicit disclaimers:

1. **"Execution Heat"** - kept but defined as "execution frequency counters" with analogy disclaimer
2. **"Entropy"** - kept but defined as "statistical variance" with information theory analogy disclaimer
3. **"Pipeline Pressure"** - kept but defined as "instruction queue depth" with metaphor disclaimer
4. **"Jacquard"** - kept but explained as "historical analogy to automated pattern selection"

---

## Terms That WERE Removed

These terms were removed entirely because they were too metaphorical:

1. ~~"Thermal-like"~~ → just say what it actually is
2. ~~"Dissipates"~~ → "decreases over time"
3. ~~"Chaotic"~~ → "high coefficient of variation"
4. ~~"Markers"~~ → "indices"

---

## Example: Before and After Comparison

### BEFORE (metaphorical jargon):
```
The system measures execution heat, which accumulates as words execute and
dissipates through temporal decay. Rising entropy signals volatile workloads.
Pipeline pressure triggers cache optimization. The Jacquard selector evaluates
thermal signals and chooses execution modes to stabilize the system.
```

### AFTER (precise technical language):
```
The system maintains execution frequency counters (referred to as "execution
heat" by analogy), which increment when instructions execute and decrease via
time-based decay coefficients. Increasing statistical variance signals high
workload variability. Elevated instruction queue depth triggers cache optimization.
The supervisory mode selector (L8) evaluates the state vector and selects
execution modes to reduce timing variance.
```

**Result:** Same meaning, but defensible and precise. No risk of examiner confusion or rejection due to vague terminology.

---

## Files Modified

1. `patent/sections/06_detailed_description.tex` - Runtime State Vector, Feedback Loops, Modes, K-statistic
2. `patent/sections/04_summary.tex` - State vector description, feedback loops, mode selector
3. `patent/sections/03_background.tex` - Workload characterization language

---

## Verification

✅ Every metaphorical term now has explicit disclaimer
✅ Every measurement has quantitative definition
✅ Every algorithm has clear mechanism description
✅ No remaining thermal/physics jargon without disclaimer
✅ No vague terms like "stabilize," "optimize" without specifics

**Result:** Patent language is now precise, defensible, and honest about what the system actually does.
