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

/*
 * Accessor functions — same-TU direct access avoids R_X86_64_REX_GOTPCRELX
 * double-dereference in -fPIC PE builds where GOTPCRELX is not relaxed to LEA.
 */
/**
 * @brief Return the current count of accumulated contract violations.
 *
 * Reads @c global_contract_violations directly; same-TU access avoids
 * GOT double-dereference in @c -fPIC PE32+ builds where
 * @c R_X86_64_REX_GOTPCRELX is not relaxed to a LEA instruction.
 *
 * @return Number of contract violations recorded since last reset.
 */
int  contract_get_violation_count(void)  { return global_contract_violations; }

/**
 * @brief Reset the contract violation counter to zero.
 *
 * Call before a fresh round of contract checks so that counts from
 * earlier test phases do not accumulate across module boundaries.
 */
void contract_reset_violation_count(void) { global_contract_violations = 0; }

/* ============================================================================
 * xorshift64 RNG — deterministic, seeded per-round so each perturbation
 * produces a distinct physics state.
 * ============================================================================ */

static uint64_t rng_state = 0xDEADBEEFCAFEBABEULL;

/**
 * @brief Generate the next pseudo-random 64-bit value via xorshift64.
 *
 * Uses the global @c rng_state. The xorshift64 parameters (13, 7, 17)
 * produce a full-period sequence over all non-zero 64-bit states.
 * Caller must seed via @c rng_seed() before starting a new fuzz round
 * to ensure different perturbation sequences across rounds.
 *
 * @return Next pseudo-random 64-bit integer.
 */
static uint64_t contract_rand(void) {
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}

/**
 * @brief Seed the RNG for a given fuzz round.
 *
 * Derives a deterministic seed from the round index mixed with a
 * Fibonacci multiplier (@c 0x9E3779B97F4A7C15) so each round produces
 * a distinct, reproducible perturbation sequence regardless of how many
 * rounds have previously run.
 *
 * @param round  Zero-based fuzz round index.
 */
static void rng_seed(int round) {
    rng_state = 0xDEADBEEFCAFEBABEULL ^ ((uint64_t)(unsigned)round * 0x9E3779B97F4A7C15ULL);
}

/* ============================================================================
 * Exec-domain capture and comparison
 * ============================================================================ */

/**
 * @brief Snapshot the execution-domain fields of a VM into an @c ExecResult.
 *
 * Captures @c dsp, @c rsp, @c error, and the live portion of the data
 * stack (indices 0 … dsp−1). The remainder of @c r->stack is zeroed for
 * deterministic @c memcmp() comparisons in @c exec_results_equal().
 *
 * Only execution-domain fields are captured — physics scalars, heartbeat
 * state, and rolling-window counters are deliberately excluded.
 *
 * @param vm  VM instance to snapshot.
 * @param r   Output @c ExecResult struct to populate.
 */
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

/**
 * @brief Compare two @c ExecResult snapshots for execution-domain equality.
 *
 * Returns 1 only if @c dsp, @c rsp, @c error, and the live stack contents
 * are all identical. Stack comparison uses @c memcmp() over the live
 * portion (0 … dsp−1 cells); the zeroed tail guarantees no false
 * positives from uninitialised padding.
 *
 * @param a  First snapshot.
 * @param b  Second snapshot.
 * @return   1 if execution-domain is identical, 0 if any field differs.
 */
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

/**
 * @brief Save the key physics scalars touched by the seven feedback loops.
 *
 * Captures the eight fields that @c perturb_physics() will randomise:
 * @c decay_slope_q48, @c effective_window_size, @c tick_target_ns,
 * the three heat percentile thresholds, @c prefetch_attempts, and
 * @c total_executions. Used as a bracket pair with @c restore_physics()
 * around each fuzz round so the VM returns to a known state.
 *
 * @param vm  VM instance to snapshot.
 * @param s   Output @c PhysicsSnapshot to populate.
 */
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

/**
 * @brief Restore the physics scalars from a previously saved snapshot.
 *
 * Writes back the eight fields saved by @c save_physics(), undoing any
 * perturbations made by @c perturb_physics(). Called at the end of each
 * fuzz round and after the full @c check_physics_transparent() loop so
 * the VM state is clean for the next contract check.
 *
 * @param vm  VM instance to restore.
 * @param s   Source @c PhysicsSnapshot to restore from.
 */
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

/**
 * @brief Randomly perturb key physics scalars to expose A4' violations.
 *
 * Seeds the RNG via @c rng_seed(@p round) for deterministic but distinct
 * perturbations per round, then overwrites the eight tracked physics
 * fields with random values that still satisfy their well-formedness
 * invariants:
 *
 *  - @c decay_slope_q48: LSB forced to 1 (must be > 0 per @c slope_wf)
 *  - @c effective_window_size: [1, ROLLING_WINDOW_SIZE] (never 0)
 *  - @c tick_target_ns: LSB forced to 1 (must be > 0 per @c hb_state_wf)
 *  - Heat thresholds: arbitrary positive values (no strict invariant)
 *  - @c prefetch_attempts / @c total_executions: arbitrary counts
 *
 * After perturbation, running the same FORTH input must produce an
 * identical @c ExecResult; divergence is an A4' violation.
 *
 * @param vm     VM instance to perturb.
 * @param round  Fuzz round index (used to seed the RNG).
 */
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

/**
 * @brief A4' contract check — verify a word is transparent to physics state.
 *
 * Axiom A4' (physics transparency): a FORTH word's execution-domain
 * result (@c ExecResult) must be identical regardless of the values of
 * the seven physics feedback-loop scalars.
 *
 * Protocol:
 *  1. Run @p input with current (unperturbed) physics; capture baseline.
 *  2. For each of @p fuzz_rounds rounds: seed RNG, perturb physics,
 *     re-run @p input, capture result, compare to baseline.
 *  3. On divergence, log the violation and increment
 *     @c global_contract_violations.
 *  4. Always restore original physics state before returning.
 *
 * Error-expected tests (@p should_error) and unimplemented words
 * (@p implemented == 0) are skipped: exec-equivalent divergence is
 * legitimate for error paths.
 *
 * @param vm          VM instance.
 * @param word_name   Human-readable word name for diagnostic messages.
 * @param input       FORTH source string to execute (same each round).
 * @param should_error  Non-zero if the test is expected to raise an error.
 * @param implemented   Non-zero if the word is implemented and should be checked.
 * @param fuzz_rounds   Number of perturbation rounds; 0 → CONTRACT_DEFAULT_FUZZ_ROUNDS.
 * @return            1 if all rounds passed (or test skipped), 0 on any violation.
 */
int check_physics_transparent(VM *vm, const char *word_name,
                               const char *input,
                               int should_error, int implemented,
                               int fuzz_rounds) {
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
}

/* ============================================================================
 * A1 contract check
 * ============================================================================ */

/**
 * @brief A1 contract check — verify @c vm_tick() does not modify the execution domain.
 *
 * Axiom A1 (heartbeat isolation): the background heartbeat tick must never
 * alter stacks, error state, or any other execution-domain field observable
 * by FORTH words. This function captures an @c ExecResult before and after
 * one @c vm_tick() call and compares them with @c exec_results_equal().
 *
 * On violation, logs @c [A1 CONTRACT] VIOLATION with before/after dsp and
 * rsp values, and increments @c global_contract_violations.
 *
 * @param vm  VM instance; stacks and error state must already be in a known
 *            reference state before calling.
 * @return    1 if the heartbeat left the execution domain unchanged, 0 on violation.
 */
int assert_heartbeat_safe(VM *vm) {
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
}
