# L8 Control Wiring Implementation Plan

**Status**: Infrastructure exists but execution paths are not gated
**Date**: 2025-12-05
**Issue**: L8 Jacquard mode selector computes 4-bit control signals (L2/L3/L5/L6) but feedback loops ignore them

---

## Executive Summary

The L8 Steady State Machine (SSM) infrastructure is **90% complete** but the final 10% is critical:

✅ **Complete**:
- L8 state machine (`ssm_jacquard.c`) computes 4-bit mode (C0-C15) based on entropy/CV/temporal metrics
- `ssm_apply_mode()` sets L2/L3/L5/L6 flags in `vm->ssm_config`
- `vm_heartbeat_update_l8()` updates L8 state every heartbeat cycle (lines 993-1045 in `vm.c`)
- Mode transitions logged with debug info

❌ **Missing**:
- Feedback loop execution paths don't check `ssm_config` flags before running
- L2/L3/L5/L6 run unconditionally despite L8 setting mode bits
- Result: L8 is a "supervisor without authority"

---

## Current Architecture (From DoE Analysis)

From `Makefile:112-127` and `ssm_jacquard.h:32-51`:

| Loop | Name | Status | Control |
|------|------|--------|---------|
| L1 | Heat Tracking | ALWAYS OFF | Disabled (harmful in 86% of configs) |
| L2 | Rolling Window | **NEEDS GATING** | L8 bit 3 (entropy-driven) |
| L3 | Linear Decay | **NEEDS GATING** | L8 bit 2 (temporal locality) |
| L4 | Pipelining Metrics | ALWAYS OFF | Disabled (harmful in 100% of configs) |
| L5 | Window Inference | **NEEDS GATING** | L8 bit 1 (CV-driven) |
| L6 | Decay Inference | **NEEDS GATING** | L8 bit 0 (CV + temporal) |
| L7 | Adaptive Heartrate | ALWAYS ON | Enabled (beneficial in 71% of configs) |
| L8 | Jacquard Selector | **IMPLEMENTED** | 4-bit mode controller (C0-C15) |

**Key Insight**: L1 and L4 are compile-time disabled. L7 is always-on. **L2/L3/L5/L6 need runtime gating** based on L8 mode bits.

---

## Required Changes (4 Locations)

### 1. L2: Rolling Window Recording (src/rolling_window_of_truth.c)

**Current Code** (`rolling_window_record_execution()` line 181):
```c
int rolling_window_record_execution(RollingWindowOfTruth* window, uint32_t word_id)
{
    if (!window || !window->execution_history)
        return -1;

    window->execution_history[window->window_pos] = word_id;
    // ... always executes
}
```

**Required Change**:
Add check at function entry or call site. Two options:

**Option A** (Preferred - Clean separation):
Modify call site in `src/vm.c:1652`:
```c
// Before (line 1652):
rolling_window_record_execution(&vm->rolling_window, word_id);

// After:
if (vm->ssm_config && ((ssm_config_t*)vm->ssm_config)->L2_rolling_window) {
    rolling_window_record_execution(&vm->rolling_window, word_id);
}
```

**Option B** (Alternative - Gate inside function):
Modify `rolling_window_record_execution()` to accept VM pointer and check internally.

**Files to modify**:
- `src/vm.c:1652` - Call site in `execute_word()`
- `src/rolling_window_of_truth.c:181` - Consider adding VM* parameter for internal check
- `include/rolling_window_of_truth.h` - Update function signature if Option B chosen

---

### 2. L3: Linear Decay Application (src/physics_metadata.c)

**Current Code** (line 176):
```c
/* L8 FINAL INTEGRATION: L3 linear decay is always-on */
void physics_metadata_apply_linear_decay(DictEntry *entry, uint64_t elapsed_ns, VM *vm) {
    if (!entry || !vm) {
        return;
    }

    /* Frozen words do not decay */
    if (entry->flags & WORD_FROZEN) {
        return;
    }

    /* Don't bother with insignificant time intervals */
    if (elapsed_ns < DECAY_MIN_INTERVAL) {
        return;
    }

    // ... decay logic always executes
}
```

**Required Change**:
Add L3 gate check after frozen/interval checks:
```c
/* L8 FINAL INTEGRATION: L3 linear decay - runtime gated by Jacquard */
void physics_metadata_apply_linear_decay(DictEntry *entry, uint64_t elapsed_ns, VM *vm) {
    if (!entry || !vm) {
        return;
    }

    /* Frozen words do not decay */
    if (entry->flags & WORD_FROZEN) {
        return;
    }

    /* Don't bother with insignificant time intervals */
    if (elapsed_ns < DECAY_MIN_INTERVAL) {
        return;
    }

    /* L8 GATE: Check if L3 (linear decay) is enabled by Jacquard mode */
    if (vm->ssm_config) {
        ssm_config_t *config = (ssm_config_t*)vm->ssm_config;
        if (!config->L3_linear_decay) {
            return;  /* L3 disabled by L8 mode selector */
        }
    }

    // ... existing decay logic
}
```

**Files to modify**:
- `src/physics_metadata.c:176` - Add L3 gate check (line ~188-193)
- Comment update at line 175 to reflect runtime gating

**Call sites** (no changes needed, gate is internal):
- `src/vm.c:737` - `vm_tick_apply_background_decay()` (heartbeat loop)
- Other call sites inherit the gate

---

### 3. L5: Window Width Inference (src/inference_engine.c)

**Current Code** (line 538):
```c
/* === PHASE 2C: Window Width Inference === */
/* L8 FINAL INTEGRATION: Loop #5 always-on */
uint32_t inferred_width = find_variance_inflection(
    trajectory,
    traj_len,
    current_variance
);
```

**Required Change**:
Gate inference execution with conditional block:
```c
/* === PHASE 2C: Window Width Inference === */
/* L8 FINAL INTEGRATION: Loop #5 runtime-gated by Jacquard */
uint32_t inferred_width = outputs->adaptive_window_width;  /* Keep previous value as default */

if (inputs->vm && inputs->vm->ssm_config) {
    ssm_config_t *config = (ssm_config_t*)inputs->vm->ssm_config;
    if (config->L5_window_inference) {
        /* L5 enabled - run inference */
        inferred_width = find_variance_inflection(
            trajectory,
            traj_len,
            current_variance
        );
    }
    /* else: L5 disabled, keep previous adaptive_window_width */
}
```

**Files to modify**:
- `src/inference_engine.c:538-544` - Add L5 conditional gate
- Comment update at line 539 to reflect runtime gating

---

### 4. L6: Decay Slope Inference (src/inference_engine.c)

**Current Code** (line 546):
```c
/* === PHASE 2D: Decay Slope Inference === */
/* L8 FINAL INTEGRATION: Loop #6 always-on */
uint64_t inferred_slope = infer_decay_slope_q48(trajectory, traj_len);
```

**Required Change**:
Gate inference execution with conditional block:
```c
/* === PHASE 2D: Decay Slope Inference === */
/* L8 FINAL INTEGRATION: Loop #6 runtime-gated by Jacquard */
uint64_t inferred_slope = outputs->adaptive_decay_slope;  /* Keep previous value as default */

if (inputs->vm && inputs->vm->ssm_config) {
    ssm_config_t *config = (ssm_config_t*)inputs->vm->ssm_config;
    if (config->L6_decay_inference) {
        /* L6 enabled - run inference */
        inferred_slope = infer_decay_slope_q48(trajectory, traj_len);
    }
    /* else: L6 disabled, keep previous adaptive_decay_slope */
}
```

**Files to modify**:
- `src/inference_engine.c:546-548` - Add L6 conditional gate
- Comment update at line 547 to reflect runtime gating

---

## Implementation Order (Recommended)

1. **L3 (Decay)** - Simplest, internal to one function
   - Modify `src/physics_metadata.c:176-220`
   - Add gate check after line 188
   - Test with mode C0 (all off) vs C4 (L3 on)

2. **L5 (Window Inference)** - Medium complexity
   - Modify `src/inference_engine.c:538-544`
   - Preserve previous value when disabled
   - Test early-exit behavior with mode C0 vs C2

3. **L6 (Decay Inference)** - Similar to L5
   - Modify `src/inference_engine.c:546-548`
   - Preserve previous value when disabled
   - Test with mode C0 vs C1

4. **L2 (Rolling Window)** - Most complex (data structure)
   - Decide on Option A (call-site gate) vs Option B (internal gate)
   - Modify call site `src/vm.c:1652` OR function signature
   - Test with mode C0 vs C8 (high entropy)

---

## Testing Strategy

### Unit Tests
For each loop, verify:
1. **Mode C0 (0000)**: All loops disabled → function returns early or skips logic
2. **Single-bit modes**: Enable one loop at a time (C1, C2, C4, C8)
3. **Top 5% modes**: C4, C7, C9, C11, C12 work correctly

### Integration Tests
1. **Mode transitions**: L8 changes mode → loops respond correctly
2. **Hysteresis**: Mode doesn't flap rapidly (5-tick delay)
3. **Metrics**: Verify entropy/CV/temporal drive correct mode selection

### Regression Tests
1. Run existing 936+ test suite with different L8 modes
2. Verify deterministic behavior (no variance across runs)
3. Check performance matches DoE predictions

---

## Validation Checklist

After implementation, verify:

- [ ] L2: `rolling_window_record_execution()` only executes when L2 bit set
- [ ] L3: `physics_metadata_apply_linear_decay()` returns early when L3 bit clear
- [ ] L5: `find_variance_inflection()` only called when L5 bit set
- [ ] L6: `infer_decay_slope_q48()` only called when L6 bit set
- [ ] L8: Mode transitions logged in debug output
- [ ] All existing tests pass with default mode (C0 or C12)
- [ ] DoE experiment can reproduce 128 configurations via L8 mode forcing

---

## Impact Assessment

**Lines of code changed**: ~40-60 lines across 3 files
**Risk**: Low (adding guards, not changing logic)
**Performance**: Neutral to positive (skips work when disabled)
**Correctness**: High confidence (L8 already validated via DoE)

---

## Open Questions

1. **Default mode**: Should VM initialize with C0 (minimal) or C12 (DoE optimal)?
   - Current: `ssm_l8_init(..., SSM_MODE_C0)` in `vm.c:387`
   - Top 5%: C4, C7, C9, C11, C12
   - Recommendation: Start C0, let L8 adapt

2. **Mode override**: Should FORTH words exist to force L8 mode?
   - `L8-MODE!` to set mode manually
   - `L8-MODE@` to query current mode
   - Useful for debugging/benchmarking

3. **Metrics exposure**: Should heartbeat CSV export include L8 mode?
   - Currently exports: executions, heat, variance, slope
   - Add: `l8_mode` column (0-15 integer)

---

## Success Criteria

**Definition of Done**:
1. All 4 feedback loops check `ssm_config` flags before executing
2. L8 mode selector controls L2/L3/L5/L6 runtime behavior
3. Mode transitions visible in debug logs
4. Existing test suite passes with default L8 mode
5. Can reproduce DoE configurations by forcing L8 modes 0-15

**Verification**:
```bash
# Build with L8 control wired
make clean && make fastest

# Test mode C0 (all off) - should be fastest, least adaptive
./build/amd64/fastest/starforth --benchmark 1000

# Test mode C15 (all on) - should be slowest, most adaptive
# (requires word to set L8 mode or environment variable)

# Run full test suite
make test
```

---

## Next Steps

**Before coding**:
1. Review this plan with stakeholders
2. Decide on L2 implementation strategy (Option A vs B)
3. Choose default L8 mode (C0 vs C12)

**During implementation**:
1. Follow order: L3 → L5 → L6 → L2
2. Test each loop individually before moving to next
3. Update comments to reflect runtime gating

**After completion**:
1. Run full DoE validation (128 configs × 300 reps)
2. Verify 0% algorithmic variance maintained
3. Document L8 control API for future developers