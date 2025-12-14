/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

/*
                                  ***   StarForth   ***

  rolling_window_knobs.h - Tunable Adaptive Shrinking Control

  Knobs that control how the rolling window automatically learns and adapts
  its size during execution. These are NOT the initial window size - they
  control the self-tuning behavior.

  All knobs are tunable via Makefile:
    make ADAPTIVE_SHRINK_RATE=30     (shrink by 30% instead of default)
    make ADAPTIVE_MIN_WINDOW_SIZE=512 (never shrink below 512)
    etc.

*/

#ifndef ROLLING_WINDOW_KNOBS_H
#define ROLLING_WINDOW_KNOBS_H

/* ============================================================================
 * Knob #8: ADAPTIVE_SHRINK_RATE - How aggressively to shrink the window
 * ============================================================================
 *
 * When pattern diversity plateaus, the window shrinks by this percentage.
 *
 * Value: 0-99 (percentage to retain, not discard)
 *   ADAPTIVE_SHRINK_RATE=75  → shrink to 75% of current size (discard 25%)
 *   ADAPTIVE_SHRINK_RATE=50  → shrink to 50% of current size (discard 50%)
 *   ADAPTIVE_SHRINK_RATE=90  → shrink to 90% of current size (discard 10%)
 *
 * Rationale:
 *   Higher (90+):   Conservative, slow learning, larger final window
 *   Default (75):   Balanced - learns in 3-4 shrink cycles
 *   Lower (50):     Aggressive, fast learning, minimal final window
 *
 * Trade-off:
 *   Higher = safer (more pattern capture margin, slower adaptation)
 *   Lower = leaner (less memory overhead, faster optimization)
 *
 * Default: 75 (shrink to 75%, discard 25% each cycle)
 * Range: 50-95 (below 50 is too aggressive, above 95 barely shrinks)
 *
 * Tuning: make ADAPTIVE_SHRINK_RATE=80
 */
#ifndef ADAPTIVE_SHRINK_RATE
#define ADAPTIVE_SHRINK_RATE 75
#endif

/* ============================================================================
 * Knob #9: ADAPTIVE_MIN_WINDOW_SIZE - Minimum window size (safety floor)
 * ============================================================================
 *
 * The window will never shrink below this size, even if diversity plateaus.
 * Prevents over-optimization that might miss emerging patterns.
 *
 * Value: 64-2048 (in word IDs, not bytes)
 *   ADAPTIVE_MIN_WINDOW_SIZE=256  → don't shrink below 256
 *   ADAPTIVE_MIN_WINDOW_SIZE=128  → don't shrink below 128
 *   ADAPTIVE_MIN_WINDOW_SIZE=512  → don't shrink below 512
 *
 * Rationale:
 *   Smaller (128):  Lean optimization, accept lower pattern capture
 *   Default (256):  Balanced - typical workload needs 200-250 patterns
 *   Larger (512):   Conservative, handles complex workloads
 *
 * Trade-off:
 *   Smaller = less memory, risk of missing edge-case patterns
 *   Larger = more memory, safer for unpredictable workloads
 *
 * Default: 256 (reasonable for most FORTH programs)
 * Range: 64-1024
 *
 * Tuning: make ADAPTIVE_MIN_WINDOW_SIZE=512
 */
#ifndef ADAPTIVE_MIN_WINDOW_SIZE
#define ADAPTIVE_MIN_WINDOW_SIZE 256
#endif

/* ============================================================================
 * Knob #10: ADAPTIVE_CHECK_FREQUENCY - How often to check diversity
 * ============================================================================
 *
 * The system checks pattern diversity every N executions.
 * More frequent checks = faster response to saturation, more overhead.
 *
 * Value: 32-1024 (execution count between checks)
 *   ADAPTIVE_CHECK_FREQUENCY=128  → check after every 128 executions
 *   ADAPTIVE_CHECK_FREQUENCY=256  → check after every 256 executions
 *   ADAPTIVE_CHECK_FREQUENCY=512  → check after every 512 executions
 *
 * Rationale:
 *   More frequent (128):   Responsive learning, detects saturation quickly
 *   Default (256):        Balanced - good responsiveness, minimal overhead
 *   Less frequent (512):  Lazy learning, smaller monitoring overhead
 *
 * Trade-off:
 *   More = faster adaptation, slightly more computation
 *   Less = slower adaptation, minimal overhead
 *
 * Default: 256 (balanced)
 * Range: 32-1024
 *
 * Tuning: make ADAPTIVE_CHECK_FREQUENCY=128
 */
#ifndef ADAPTIVE_CHECK_FREQUENCY
#define ADAPTIVE_CHECK_FREQUENCY 256
#endif

/* ============================================================================
 * Knob #11: ADAPTIVE_GROWTH_THRESHOLD - Pattern growth rate that signals saturation
 * ============================================================================
 *
 * When pattern diversity growth drops below this rate, window can shrink.
 * Expressed as percentage (0-100).
 *
 * Value: 0-10 (as percentage, 0=0%, 10=10%)
 *   ADAPTIVE_GROWTH_THRESHOLD=1  → shrink when growth < 1% (aggressive)
 *   ADAPTIVE_GROWTH_THRESHOLD=2  → shrink when growth < 2% (default, balanced)
 *   ADAPTIVE_GROWTH_THRESHOLD=5  → shrink when growth < 5% (conservative)
 *
 * Rationale:
 *   Lower (0.5%):  Very aggressive, assumes patterns found early
 *   Default (1%):  Balanced - empirically tested, works well
 *   Higher (5%):   Conservative, waits for stronger saturation signal
 *
 * Trade-off:
 *   Lower = eager shrinking, risk of missing late-arriving patterns
 *   Higher = cautious shrinking, longer time to final window size
 *
 * Default: 1 (1% growth rate = saturation signal)
 * Range: 0-10
 *
 * Note: Internally, this is applied as: growth_rate < (threshold / 100.0)
 *   E.g., ADAPTIVE_GROWTH_THRESHOLD=1 means growth_rate < 0.01
 *
 * Tuning: make ADAPTIVE_GROWTH_THRESHOLD=2
 */
#ifndef ADAPTIVE_GROWTH_THRESHOLD
#define ADAPTIVE_GROWTH_THRESHOLD 1
#endif

/* ============================================================================
 * Summary Table: How Knobs Interact
 * ============================================================================
 *
 * Scenario 1: "Fast Learning, Lean Final Size"
 *   ADAPTIVE_SHRINK_RATE=50
 *   ADAPTIVE_MIN_WINDOW_SIZE=128
 *   ADAPTIVE_CHECK_FREQUENCY=128
 *   ADAPTIVE_GROWTH_THRESHOLD=0
 *   → Shrinks aggressively, frequently, to minimal size
 *   → Best for: Memory-constrained systems, simple workloads
 *
 * Scenario 2: "Balanced (Default)"
 *   ADAPTIVE_SHRINK_RATE=75
 *   ADAPTIVE_MIN_WINDOW_SIZE=256
 *   ADAPTIVE_CHECK_FREQUENCY=256
 *   ADAPTIVE_GROWTH_THRESHOLD=1
 *   → Good balance, typical FORTH programs
 *   → Best for: General-purpose systems
 *
 * Scenario 3: "Conservative, Safe"
 *   ADAPTIVE_SHRINK_RATE=90
 *   ADAPTIVE_MIN_WINDOW_SIZE=512
 *   ADAPTIVE_CHECK_FREQUENCY=512
 *   ADAPTIVE_GROWTH_THRESHOLD=5
 *   → Slow, cautious shrinking, large safety margin
 *   → Best for: Complex workloads, unpredictable patterns
 *
 */

#endif /* ROLLING_WINDOW_KNOBS_H */
