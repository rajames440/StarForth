/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  Licensed under the Starship License 1.0 (SL-1.0)
*/

#ifndef TEST_CONTRACTS_H
#define TEST_CONTRACTS_H

#include "vm.h"   /* resolved via -Iinclude */
#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Word Contract System — Runtime witnesses for Isabelle/HOL axioms
 *
 * Two axioms require runtime verification:
 *   A1  heartbeat_exec_neutral   — vm_tick() must not write exec-equiv fields
 *   A4' word_physics_transparent — word results must be independent of physics state
 *
 * Contracts attach at three levels; resolution order (first non-NONE wins):
 *   individual TestCase > WordTestSuite.suite_contract > module_contract arg
 *
 * Contract checking only runs when compiled with -DCONTRACTS_ENABLED=1.
 * When CONTRACTS_ENABLED=0 (default), all contract functions are no-ops.
 * ============================================================================ */

typedef uint32_t contract_flags_t;

#define CONTRACT_NONE                 0u
/* A4': re-run word with perturbed physics; assert identical exec result */
#define CONTRACT_PHYSICS_TRANSPARENT  (1u << 0)
/* A1: call vm_tick() and assert exec-equiv fields are unchanged */
#define CONTRACT_HEARTBEAT_SAFE       (1u << 1)

/* Number of physics perturbation rounds per test case (0 = use default) */
#define CONTRACT_DEFAULT_FUZZ_ROUNDS  4

typedef struct {
    contract_flags_t flags;
    int fuzz_rounds;  /* 0 = use CONTRACT_DEFAULT_FUZZ_ROUNDS */
} WordContract;

/* Exec-domain snapshot — exactly the four fields in exec_equiv */
typedef struct {
    cell_t stack[STACK_SIZE];
    int    dsp;
    int    rsp;
    int    error;
} ExecResult;

/* Physics field snapshot for save/restore around A4' checks */
typedef struct {
    uint64_t decay_slope_q48;
    uint32_t eff_window_size;
    uint64_t tick_target_ns;
    cell_t   heat_25th;
    cell_t   heat_50th;
    cell_t   heat_75th;
    uint64_t prefetch_attempts;
    uint64_t total_executions;
} PhysicsSnapshot;

/* Global counter incremented on each contract violation (CONTRACTS_ENABLED only) */
extern int global_contract_violations;

/*
 * capture_exec_result — snapshot the four exec-equiv fields from vm into r.
 */
void capture_exec_result(VM *vm, ExecResult *r);

/*
 * exec_results_equal — 1 if a and b agree on dsp, rsp, error, and live stack.
 */
int exec_results_equal(const ExecResult *a, const ExecResult *b);

/*
 * save_physics / restore_physics — snapshot and restore key physics scalars.
 */
void save_physics(VM *vm, PhysicsSnapshot *s);
void restore_physics(VM *vm, const PhysicsSnapshot *s);

/*
 * perturb_physics — replace key physics scalars with fresh random values.
 * round is used to vary the RNG seed across multiple rounds.
 */
void perturb_physics(VM *vm, int round);

/*
 * check_physics_transparent — A4' contract check for a single test case.
 *
 * Runs input with the current physics state (baseline), then runs it again
 * fuzz_rounds times with physics perturbed, asserting identical ExecResult
 * each time.  Returns 1 if all rounds match, 0 on first violation (which is
 * also logged as an error).  Skips error-expected and unimplemented tests.
 *
 * NOTE: input must not define dictionary words; the dict is not snapshotted
 * inside this function.  Callers that cover defining-words tests should
 * save/restore dict state around this call.
 */
int check_physics_transparent(VM *vm, const char *word_name,
                               const char *input,
                               int should_error, int implemented,
                               int fuzz_rounds);

/*
 * assert_heartbeat_safe — A1 contract check.
 *
 * Snapshots exec-equiv fields, calls vm_tick(), then asserts all four fields
 * are unchanged.  Returns 1 if the assertion holds, 0 on violation.
 */
int assert_heartbeat_safe(VM *vm);

#endif /* TEST_CONTRACTS_H */
