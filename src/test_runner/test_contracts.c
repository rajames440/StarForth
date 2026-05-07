/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  Licensed under the Starship License 1.0 (SL-1.0)
*/

/*
 * test_contracts.c — Runtime witnesses for Isabelle/HOL axioms A1 and A4'.
 *
 * All contract checks are compiled away when CONTRACTS_ENABLED != 1 so that
 * normal `make test` builds carry zero overhead.
 */

#include "test_contracts.h"
#include "log.h"     /* log_message, LOG_ERROR, LOG_TEST */
#include "vm.h"      /* VM, vm_interpret, vm_tick, ROLLING_WINDOW_SIZE */
#include <string.h>  /* memcpy, memset, memcmp */

/* Global violation counter (defined here; declared extern in test_contracts.h) */
int global_contract_violations = 0;

/* ============================================================================
 * xorshift64 RNG — deterministic, seeded per-round so each perturbation
 * produces a distinct physics state.
 * ============================================================================ */

static uint64_t rng_state = 0xDEADBEEFCAFEBABEULL;

static uint64_t contract_rand(void) {
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}

static void rng_seed(int round) {
    rng_state = 0xDEADBEEFCAFEBABEULL ^ ((uint64_t)(unsigned)round * 0x9E3779B97F4A7C15ULL);
}

/* ============================================================================
 * Exec-domain capture and comparison
 * ============================================================================ */

void capture_exec_result(VM *vm, ExecResult *r) {
    r->dsp   = vm->dsp;
    r->rsp   = vm->rsp;
    r->error = vm->error;
    /* copy only the live portion; zero the rest for deterministic memcmp */
    int depth = (vm->dsp > 0) ? vm->dsp : 0;
    memcpy(r->stack, vm->data_stack, (size_t)depth * sizeof(cell_t));
    if (depth < STACK_SIZE)
        memset(&r->stack[depth], 0, (size_t)(STACK_SIZE - depth) * sizeof(cell_t));
}

int exec_results_equal(const ExecResult *a, const ExecResult *b) {
    if (a->dsp   != b->dsp)   return 0;
    if (a->rsp   != b->rsp)   return 0;
    if (a->error != b->error) return 0;
    int depth = (a->dsp > 0) ? a->dsp : 0;
    return memcmp(a->stack, b->stack, (size_t)depth * sizeof(cell_t)) == 0;
}

/* ============================================================================
 * Physics snapshot / restore / perturb
 *
 * These cover the key scalars touched by the seven feedback loops.  A word
 * that reads any of these will produce a different ExecResult after perturb,
 * exposing a violation of A4'.
 * ============================================================================ */

void save_physics(VM *vm, PhysicsSnapshot *s) {
    s->decay_slope_q48  = vm->decay_slope_q48;
    s->eff_window_size  = vm->rolling_window.effective_window_size;
    s->tick_target_ns   = vm->heartbeat.tick_target_ns;
    s->heat_25th        = vm->heat_threshold_25th;
    s->heat_50th        = vm->heat_threshold_50th;
    s->heat_75th        = vm->heat_threshold_75th;
    s->prefetch_attempts = vm->pipeline_metrics.prefetch_attempts;
    s->total_executions = vm->rolling_window.total_executions;
}

void restore_physics(VM *vm, const PhysicsSnapshot *s) {
    vm->decay_slope_q48                      = s->decay_slope_q48;
    vm->rolling_window.effective_window_size = s->eff_window_size;
    vm->heartbeat.tick_target_ns             = s->tick_target_ns;
    vm->heat_threshold_25th                  = s->heat_25th;
    vm->heat_threshold_50th                  = s->heat_50th;
    vm->heat_threshold_75th                  = s->heat_75th;
    vm->pipeline_metrics.prefetch_attempts   = s->prefetch_attempts;
    vm->rolling_window.total_executions      = s->total_executions;
}

void perturb_physics(VM *vm, int round) {
    rng_seed(round);

    /* decay_slope_q48: must be > 0 per slope_wf */
    vm->decay_slope_q48 = contract_rand() | 1ULL;

    /* effective_window_size: must be in [1, ROLLING_WINDOW_SIZE] */
    uint64_t w = contract_rand() % (uint64_t)ROLLING_WINDOW_SIZE;
    vm->rolling_window.effective_window_size = (uint32_t)(w == 0 ? 1 : w);

    /* tick_target_ns: must be > 0 per hb_state_wf */
    vm->heartbeat.tick_target_ns = contract_rand() | 1ULL;

    /* heat thresholds: arbitrary signed values */
    vm->heat_threshold_25th = (cell_t)(contract_rand() & 0x7FFF);
    vm->heat_threshold_50th = (cell_t)(contract_rand() & 0xFFFF);
    vm->heat_threshold_75th = (cell_t)(contract_rand() & 0x1FFFF);

    /* pipeline and window counters */
    vm->pipeline_metrics.prefetch_attempts    = contract_rand();
    vm->rolling_window.total_executions       = contract_rand();
}

/* ============================================================================
 * A4' contract check
 * ============================================================================ */

int check_physics_transparent(VM *vm, const char *word_name,
                               const char *input,
                               int should_error, int implemented,
                               int fuzz_rounds) {
#if !CONTRACTS_ENABLED
    (void)vm; (void)word_name; (void)input;
    (void)should_error; (void)implemented; (void)fuzz_rounds;
    return 1;
#else
    /* Skip error-expected tests: exec-equiv may legitimately differ */
    if (!implemented || should_error) return 1;

    int rounds = (fuzz_rounds > 0) ? fuzz_rounds : CONTRACT_DEFAULT_FUZZ_ROUNDS;

    /* --- Baseline run with current (unperturbed) physics --- */
    vm->error = 0;
    vm->dsp   = -1;
    vm->rsp   = -1;
    vm->exit_colon      = 0;
    vm->abort_requested = 0;
    vm_interpret(vm, input);

    ExecResult baseline;
    capture_exec_result(vm, &baseline);

    PhysicsSnapshot saved;
    save_physics(vm, &saved);

    int ok = 1;
    for (int r = 0; r < rounds && ok; r++) {
        perturb_physics(vm, r);

        vm->error = 0;
        vm->dsp   = -1;
        vm->rsp   = -1;
        vm->exit_colon      = 0;
        vm->abort_requested = 0;
        vm_interpret(vm, input);

        ExecResult fuzzed;
        capture_exec_result(vm, &fuzzed);

        if (!exec_results_equal(&baseline, &fuzzed)) {
            log_message(LOG_ERROR,
                "[A4' CONTRACT] VIOLATION: %s '%s' result changed under physics perturbation (round %d)",
                word_name, input, r);
            log_message(LOG_ERROR,
                "  baseline dsp=%d  fuzzed dsp=%d", baseline.dsp, fuzzed.dsp);
            ok = 0;
            global_contract_violations++;
        }

        vm->dsp   = -1;
        vm->rsp   = -1;
        vm->error = 0;
    }

    restore_physics(vm, &saved);
    vm->dsp   = -1;
    vm->rsp   = -1;
    vm->error = 0;
    return ok;
#endif /* CONTRACTS_ENABLED */
}

/* ============================================================================
 * A1 contract check
 * ============================================================================ */

int assert_heartbeat_safe(VM *vm) {
#if !CONTRACTS_ENABLED
    (void)vm;
    return 1;
#else
    ExecResult before, after;
    capture_exec_result(vm, &before);

    vm_tick(vm);

    capture_exec_result(vm, &after);

    if (!exec_results_equal(&before, &after)) {
        log_message(LOG_ERROR,
            "[A1 CONTRACT] VIOLATION: vm_tick() modified exec-equiv fields");
        log_message(LOG_ERROR,
            "  dsp before=%d after=%d  rsp before=%d after=%d",
            before.dsp, after.dsp, before.rsp, after.rsp);
        global_contract_violations++;
        return 0;
    }
    return 1;
#endif /* CONTRACTS_ENABLED */
}
