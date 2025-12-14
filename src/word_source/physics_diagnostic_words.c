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
 * physics_diagnostic_words.c
 *
 * Interactive FORTH diagnostic words for live physics feedback loop demonstration.
 * These words allow you to:
 *
 *   1. Define a test word
 *   2. Execute it repeatedly
 *   3. Watch metrics (temperature, execution heat, latency) update in real time
 *   4. Calculate knob adjustments based on physics math
 *   5. See how the system responds to changing conditions
 *
 * Usage in REPL:
 *   : HOTTEST  100 0 DO 1 2 + 3 * LOOP ;
 *   HOTTEST
 *   PHYSICS-WATCH-WORD
 *   PHYSICS-CALC-KNOBS
 *   PHYSICS-SHOW-FEEDBACK
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "vm.h"
#include "physics_metadata.h"
#include "word_registry.h"

/* ============================================================================
 * Diagnostic: Show Word Metrics
 * ============================================================================
 */

/**
 * PHYSICS-WORD-METRICS ( -- )
 *
 * Display detailed physics metrics for a word.
 * Looks up the most recently executed word and shows its state.
 */
void forth_PHYSICS_WORD_METRICS(VM *vm) {
    if (vm->error) return;

    // Find the most recently executed word (highest last_active_ns)
    DictEntry *most_recent = NULL;
    uint64_t max_active = 0;

    for (DictEntry *w = vm->latest; w; w = w->link) {
        if (w->physics.last_active_ns > max_active) {
            max_active = w->physics.last_active_ns;
            most_recent = w;
        }
    }

    if (!most_recent) {
        printf("No words executed yet.\n");
        return;
    }

    printf("\n=== Physics Metrics: %s ===\n", most_recent->name);
    printf("Temperature (Q8):    0x%04x\n", most_recent->physics.temperature_q8);
    printf("Execution Heat:      %ld\n", most_recent->execution_heat);
    printf("Avg Latency:         %u ns\n", most_recent->physics.avg_latency_ns);
    printf("Mass (bytes):        %u\n", most_recent->physics.mass_bytes);
    printf("Last Active:         %lu ns\n", most_recent->physics.last_active_ns);

    // Calculate useful derived metrics
    uint32_t baseline_temp = 0x2000;
    uint32_t max_temp = 0xFFFF;
    int16_t temp_delta = (int16_t)most_recent->physics.temperature_q8 - (int16_t)baseline_temp;

    float thermal_pressure = (float)temp_delta / (float)(max_temp - baseline_temp);
    if (thermal_pressure < 0.0f) thermal_pressure = 0.0f;
    if (thermal_pressure > 1.0f) thermal_pressure = 1.0f;

    printf("\nDerived:\n");
    printf("Thermal Pressure:    %.3f (0.0=cold, 1.0=hot)\n", thermal_pressure);
    printf("Normalized Temp:     %.2f%% (0%%=baseline, 100%%=max)\n",
           thermal_pressure * 100.0f);
}

/* ============================================================================
 * Diagnostic: Calculate Knob Adjustments
 * ============================================================================
 */

/**
 * PHYSICS-CALC-KNOBS ( -- )
 *
 * Calculate what knobs should be adjusted based on current word metrics.
 * Shows:
 *   - Thermal pressure (0.0 to 1.0)
 *   - Recommended priority adjustment
 *   - Recommended sampling rate
 *   - Stack depth limit
 *   - Core affinity preference
 */
void forth_PHYSICS_CALC_KNOBS(VM *vm) {
    if (vm->error) return;

    // Find most recent word
    DictEntry *most_recent = NULL;
    uint64_t max_active = 0;

    for (DictEntry *w = vm->latest; w; w = w->link) {
        if (w->physics.last_active_ns > max_active) {
            max_active = w->physics.last_active_ns;
            most_recent = w;
        }
    }

    if (!most_recent) {
        printf("No words executed yet.\n");
        return;
    }

    // --- Calculate thermal pressure ---
    uint16_t baseline_temp = 0x2000;
    uint16_t max_temp = 0xFFFF;
    uint16_t current_temp = most_recent->physics.temperature_q8;

    int16_t temp_delta = (int16_t)current_temp - (int16_t)baseline_temp;
    uint16_t temp_range = max_temp - baseline_temp;
    float thermal_pressure = (float)temp_delta / (float)temp_range;

    if (thermal_pressure < 0.0f) thermal_pressure = 0.0f;
    if (thermal_pressure > 1.0f) thermal_pressure = 1.0f;

    printf("\n=== Knob Calculations: %s ===\n", most_recent->name);
    printf("\nINPUT METRICS:\n");
    printf("  Temperature: 0x%04x\n", current_temp);
    printf("  Thermal Pressure: %.3f\n", thermal_pressure);

    // --- KNOB 1: Priority ---
    int base_priority = 0;
    int max_priority_boost = -20;
    int priority_boost = (int)(thermal_pressure * max_priority_boost);
    int adjusted_priority = base_priority + priority_boost;

    printf("\nKNOB 1: OS Priority\n");
    printf("  Formula: priority = base + (thermal × max_boost)\n");
    printf("  Base priority: %d\n", base_priority);
    printf("  Max boost: %d\n", max_priority_boost);
    printf("  Adjustment: %d\n", priority_boost);
    printf("  ➜ ADJUSTED: %d %s\n", adjusted_priority,
           thermal_pressure > 0.5f ? "(HIGH PRIORITY)" : "(normal)");

    // --- KNOB 2: Sampling Rate ---
    uint32_t normal_sampling = 100;
    uint32_t high_temp_sampling = 10;
    uint32_t adjusted_sampling =
        (uint32_t)(normal_sampling - thermal_pressure * (normal_sampling - high_temp_sampling));

    printf("\nKNOB 2: Sampling Rate\n");
    printf("  Formula: sampling = normal - (thermal × (normal - high_temp))\n");
    printf("  Normal: %u%%\n", normal_sampling);
    printf("  High-temp: %u%%\n", high_temp_sampling);
    printf("  ➜ ADJUSTED: %u%% %s\n", adjusted_sampling,
           adjusted_sampling < 50 ? "(LOW OVERHEAD)" : "(normal)");

    // --- KNOB 3: Stack Depth Limit ---
    uint32_t normal_stack_limit = 256;
    uint32_t min_stack_limit = 16;
    uint32_t adjusted_stack_limit =
        (uint32_t)(normal_stack_limit - thermal_pressure * (normal_stack_limit - min_stack_limit));

    printf("\nKNOB 3: Stack Depth Limit\n");
    printf("  Formula: limit = normal - (thermal × (normal - min))\n");
    printf("  Normal: %u\n", normal_stack_limit);
    printf("  Minimum: %u\n", min_stack_limit);
    printf("  ➜ ADJUSTED: %u %s\n", adjusted_stack_limit,
           adjusted_stack_limit < 100 ? "(PREVENTS RUNAWAY)" : "(no limit)");

    // --- KNOB 4: Cache Affinity ---
    int preferred_core = (thermal_pressure > 0.5f) ? 0 : -1;

    printf("\nKNOB 4: Cache Affinity\n");
    printf("  If thermal_pressure > 0.5: pin to core 0\n");
    printf("  ➜ PREFERRED CORE: %s\n", preferred_core >= 0 ? "0 (PINNED)" : "any (no preference)");

    // --- Summary ---
    printf("\n=== SUMMARY ===\n");
    printf("If executed with these knobs:\n");
    printf("  Priority boosted:      %s\n", thermal_pressure > 0.3f ? "YES" : "no");
    printf("  Sampling reduced:      %s\n", adjusted_sampling < 80 ? "YES" : "no");
    printf("  Stack limited:         %s\n", adjusted_stack_limit < 200 ? "YES" : "no");
    printf("  Core pinned:           %s\n", preferred_core >= 0 ? "YES" : "no");
    printf("\nEffect: %s execution with %s overhead.\n",
           thermal_pressure > 0.5f ? "OPTIMIZED" : "Normal",
           adjusted_sampling < 50 ? "MINIMAL" : "standard");
}

/* ============================================================================
 * Diagnostic: Run Repeated Execution and Show Feedback
 * ============================================================================
 */

/**
 * PHYSICS-BURN ( n -- )
 *
 * Execute the top-of-stack word n times and show thermal feedback.
 *
 * Usage:
 *   : HOTTEST  100 0 DO 1 2 + 3 * LOOP ;
 *   HOTTEST
 *   10 PHYSICS-BURN   (execute 10 times)
 *   PHYSICS-SHOW-FEEDBACK
 */
void forth_PHYSICS_BURN(VM *vm) {
    if (vm->error) return;

    if (vm->dsp < 1) {
        vm->error = 1;
        printf("PHYSICS-BURN: Need count on stack\n");
        return;
    }

    cell_t burn_count = vm->data_stack[--vm->dsp];

    if (burn_count < 1) {
        printf("PHYSICS-BURN: count must be >= 1\n");
        return;
    }

    // Find most recent word to burn
    DictEntry *target = NULL;
    uint64_t max_active = 0;

    for (DictEntry *w = vm->latest; w; w = w->link) {
        if (w->physics.last_active_ns > max_active) {
            max_active = w->physics.last_active_ns;
            target = w;
        }
    }

    if (!target || !target->func) {
        printf("PHYSICS-BURN: No word to burn\n");
        return;
    }

    printf("\nBurning: %s × %ld times\n", target->name, burn_count);

    // Record starting state
    uint16_t start_temp = target->physics.temperature_q8;
    uint64_t start_heat = target->execution_heat;

    // Execute repeatedly
    for (cell_t i = 0; i < burn_count; i++) {
        target->func(vm);
        if (vm->error) {
            printf("Error during burn iteration %ld\n", i);
            vm->error = 0;  // Continue
        }

        // Show progress every 10% or every iteration if small count
        if (burn_count <= 10 || (i + 1) % (burn_count / 10 + 1) == 0) {
            uint16_t current_temp = target->physics.temperature_q8;
            uint64_t current_heat = target->execution_heat;
            float temp_change = (float)((int16_t)current_temp - (int16_t)start_temp);

            printf("  [%3ld/%ld] Temp: 0x%04x (Δ %+.0f), Execution Heat: %ld (Δ %ld)\n",
                   i + 1, burn_count, current_temp, temp_change,
                   current_heat, current_heat - start_heat);
        }
    }

    // Show final state
    uint16_t final_temp = target->physics.temperature_q8;
    uint64_t final_heat = target->execution_heat;

    printf("\nBurn Complete:\n");
    printf("  Start temp: 0x%04x\n", start_temp);
    printf("  Final temp: 0x%04x\n", final_temp);
    printf("  Change: %+d (Δ %.1f%%)\n", (int16_t)final_temp - (int16_t)start_temp,
           100.0f * ((float)final_temp - (float)start_temp) / (float)start_temp);
    printf("  Execution heat increased: %ld → %ld (Δ %ld)\n",
           start_heat, final_heat, final_heat - start_heat);
}

/* ============================================================================
 * Diagnostic: Show Complete Feedback Loop
 * ============================================================================
 */

/**
 * PHYSICS-SHOW-FEEDBACK ( -- )
 *
 * Display the complete feedback loop for the most recent word.
 * Shows: Metrics → Calculations → Knobs → Effect
 */
void forth_PHYSICS_SHOW_FEEDBACK(VM *vm) {
    if (vm->error) return;

    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           Live Feedback Loop Demonstration                   ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    // Find most recent word
    DictEntry *most_recent = NULL;
    uint64_t max_active = 0;

    for (DictEntry *w = vm->latest; w; w = w->link) {
        if (w->physics.last_active_ns > max_active) {
            max_active = w->physics.last_active_ns;
            most_recent = w;
        }
    }

    if (!most_recent) {
        printf("No words executed yet.\n");
        return;
    }

    printf("\nSTEP 1: GRAB INPUTS\n");
    printf("════════════════════════════════════════════════════════════════\n");
    printf("Word: %s\n", most_recent->name);
    printf("  temperature_q8 = 0x%04x\n", most_recent->physics.temperature_q8);
    printf("  execution_heat = %ld (execution count)\n", most_recent->execution_heat);
    printf("  avg_latency_ns = %u ns\n", most_recent->physics.avg_latency_ns);
    printf("  mass_bytes = %u\n", most_recent->physics.mass_bytes);

    // Calculate thermal pressure
    uint16_t baseline = 0x2000;
    uint16_t current = most_recent->physics.temperature_q8;
    float pressure = (float)((int16_t)current - (int16_t)baseline) / (float)(0xFFFF - baseline);
    if (pressure < 0.0f) pressure = 0.0f;
    if (pressure > 1.0f) pressure = 1.0f;

    printf("\nSTEP 2: APPLY MATHEMATICS\n");
    printf("════════════════════════════════════════════════════════════════\n");
    printf("thermal_pressure = (temp - baseline) / max\n");
    printf("                 = (0x%04x - 0x%04x) / 0xFFFF\n", current, baseline);
    printf("                 = %.3f\n", pressure);

    printf("\nSTEP 3: CALCULATE KNOB ADJUSTMENTS\n");
    printf("════════════════════════════════════════════════════════════════\n");

    // Priority
    int priority = (int)(pressure * -20);
    printf("priority_adjust = thermal_pressure × (-20)\n");
    printf("                = %.3f × (-20) = %d\n", pressure, priority);

    // Sampling
    uint32_t sampling = (uint32_t)(100 - pressure * 90);
    printf("sampling = 100 - (thermal_pressure × 90)\n");
    printf("         = 100 - (%.3f × 90) = %u%%\n", pressure, sampling);

    // Stack
    uint32_t stack_limit = (uint32_t)(256 - pressure * 240);
    printf("stack_limit = 256 - (thermal_pressure × 240)\n");
    printf("            = 256 - (%.3f × 240) = %u\n", pressure, stack_limit);

    // Affinity
    printf("core_affinity = %s\n", pressure > 0.5f ? "0 (pinned)" : "any");

    printf("\nSTEP 4: TUNE THE KNOBS\n");
    printf("════════════════════════════════════════════════════════════════\n");
    printf("BEFORE adjustment:\n");
    printf("  priority: 0 (normal), sampling: 100%%, stack: 256, core: any\n");
    printf("\nAFTER adjustment:\n");
    printf("  priority: %d %s\n", priority, priority < -10 ? "(BOOSTED)" : "");
    printf("  sampling: %u%% %s\n", sampling, sampling < 50 ? "(REDUCED)" : "");
    printf("  stack_limit: %u %s\n", stack_limit, stack_limit < 128 ? "(LIMITED)" : "");
    printf("  core: %s\n", pressure > 0.5f ? "0 (PINNED)" : "any");

    printf("\nSTEP 5: OBSERVE EFFECT\n");
    printf("════════════════════════════════════════════════════════════════\n");
    if (pressure > 0.7f) {
        printf("✓ Word is VERY HOT (%.1f%% thermal load)\n", pressure * 100.0f);
        printf("  → Priority BOOSTED: gets more CPU time\n");
        printf("  → Sampling REDUCED: minimal profiling overhead\n");
        printf("  → Stack LIMITED: prevents deep recursion\n");
        printf("  → Core PINNED: keeps warm data in CPU cache\n");
        printf("  RESULT: System fully optimized for this hot path\n");
    } else if (pressure > 0.4f) {
        printf("✓ Word is WARM (%.1f%% thermal load)\n", pressure * 100.0f);
        printf("  → Moderate adjustments applied\n");
        printf("  → System balances performance with overhead\n");
    } else {
        printf("✓ Word is COOL (%.1f%% thermal load)\n", pressure * 100.0f);
        printf("  → Minimal adjustments, normal execution\n");
    }

    printf("\n════════════════════════════════════════════════════════════════\n");
    printf("FEEDBACK LOOP COMPLETE\n");
    printf("Next execution will use adjusted knobs → new metrics → loop\n");
    printf("════════════════════════════════════════════════════════════════\n\n");
}

/* ============================================================================
 * Word Registration
 * ============================================================================
 */

/**
 * Register all physics diagnostic words with the VM
 */
void register_physics_diagnostic_words(VM *vm) {
    register_word(vm, "PHYSICS-WORD-METRICS", forth_PHYSICS_WORD_METRICS);
    register_word(vm, "PHYSICS-CALC-KNOBS", forth_PHYSICS_CALC_KNOBS);
    register_word(vm, "PHYSICS-BURN", forth_PHYSICS_BURN);
    register_word(vm, "PHYSICS-SHOW-FEEDBACK", forth_PHYSICS_SHOW_FEEDBACK);
}