/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/**
 * inference_words.c — Module 26: SSM inference engine + Jacquard FORTH words
 *
 * Exposes the SSM physics engine to FORTH:
 *   - Variance / decay-slope / window-width inference on arbitrary arrays
 *   - Full inference engine run on this VM's rolling window
 *   - L8 Jacquard mode selector: update, query, apply
 *   - Bayesian latency posteriors for cache-hit and bucket-search latencies
 *   - Rolling window diversity (entropy metric)
 *   - Readable inference output fields from vm->last_inference_outputs
 *
 * All Q48.16 values pushed as cell_t (int64_t), reinterpreted as uint64_t.
 *
 * Words registered:
 *   Q.VARIANCE        ( addr u -- q )  variance of u cells at addr (Q48.16)
 *   INFER-DECAY-SLOPE ( addr u -- q )  exponential decay slope (Q48.16)
 *   INFER-WINDOW-WIDTH ( addr u -- n ) optimal window width from inflection
 *   WINDOW-DIVERSITY  ( -- u )         rolling window diversity (entropy)
 *   INFER-RUN         ( -- )           run full inference, update vm->last_inference_outputs
 *   INFER-WINDOW@     ( -- u )         last adaptive_window_width
 *   INFER-DECAY@      ( -- q )         last adaptive_decay_slope (Q48.16)
 *   INFER-VARIANCE@   ( -- q )         last window_variance_q48
 *   INFER-FIT@        ( -- q )         last slope_fit_quality_q48
 *   INFER-EARLY-EXIT@ ( -- flag )      1 if last INFER-RUN used ANOVA early-exit
 *   L8-MODE           ( -- n )         current Jacquard mode (0-15)
 *   L8-UPDATE         ( entropy_q cv_q temporal_q stability_q -- )
 *   L8-APPLY          ( -- )           apply current mode to vm->ssm_config
 *   BAYES-CACHE-MEAN  ( -- q )         Bayesian mean latency for cache hits (Q48.16)
 *   BAYES-CACHE-LOWER ( -- q )         95% credible lower bound, cache hits
 *   BAYES-CACHE-UPPER ( -- q )         95% credible upper bound, cache hits
 *   BAYES-BUCKET-MEAN ( -- q )         Bayesian mean latency for bucket searches
 *   BAYES-BUCKET-LOWER ( -- q )        95% credible lower bound, bucket searches
 *   BAYES-BUCKET-UPPER ( -- q )        95% credible upper bound, bucket searches
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "vm.h"
#include "word_registry.h"
#include "q48_16.h"
#include "inference_engine.h"
#include "ssm_jacquard.h"
#include "rolling_window_of_truth.h"
#include "physics_hotwords_cache.h"
#include "platform_alloc.h"
#include "platform_lock.h"

/* ── helpers ──────────────────────────────────────────────────────────── */

static inline q48_16_t q48_pop_inf(VM *vm) { return (q48_16_t)(uint64_t)VM_POP(vm); }
static inline void     q48_push_inf(VM *vm, q48_16_t q) { VM_PUSH(vm, (cell_t)(int64_t)q); }

/* Translate Q48.16 integer to double for ssm_l8_metrics_t (double-based). */
static inline double q48_to_dbl(q48_16_t q) { return (double)q / 65536.0; }

/* ── array-based inference primitives ───────────────────────────────── */

/*
 * Validate a FORTH array reference: addr is a vaddr_t, u is cell count.
 * Returns pointer to data or NULL on bounds error (sets vm->error).
 */
static const uint64_t *array_ptr(VM *vm, vaddr_t addr, cell_t u)
{
    if (u <= 0 || addr >= (vaddr_t)VM_MEMORY_SIZE) {
        vm->error = 1;
        return NULL;
    }
    size_t bytes = (size_t)u * sizeof(cell_t);
    if ((size_t)addr + bytes > VM_MEMORY_SIZE) {
        vm->error = 1;
        return NULL;
    }
    return (const uint64_t *)(vm->memory + addr);
}

/* Q.VARIANCE ( addr u -- q )  — variance of u uint64_t cells at addr */
static void infer_word_q_variance(VM *vm)
{
    cell_t u    = VM_POP(vm);
    vaddr_t addr = (vaddr_t)VM_POP(vm);
    const uint64_t *data = array_ptr(vm, addr, u);
    if (!data) { q48_push_inf(vm, 0); return; }
    q48_push_inf(vm, compute_variance_q48(data, (uint64_t)u));
}

/* INFER-DECAY-SLOPE ( addr u -- q )  — decay slope via linear regression */
static void infer_word_decay_slope(VM *vm)
{
    cell_t u    = VM_POP(vm);
    vaddr_t addr = (vaddr_t)VM_POP(vm);
    const uint64_t *data = array_ptr(vm, addr, u);
    if (!data) { q48_push_inf(vm, 0); return; }
    q48_push_inf(vm, (q48_16_t)infer_decay_slope_q48(data, (uint64_t)u));
}

/* INFER-WINDOW-WIDTH ( addr u -- n )  — optimal window width */
static void infer_word_window_width(VM *vm)
{
    cell_t u    = VM_POP(vm);
    vaddr_t addr = (vaddr_t)VM_POP(vm);
    const uint64_t *data = array_ptr(vm, addr, u);
    if (!data) { VM_PUSH(vm, 0); return; }
    q48_16_t var = compute_variance_q48(data, (uint64_t)u);
    uint32_t w = find_variance_inflection(data, (uint64_t)u, var);
    VM_PUSH(vm, (cell_t)(int64_t)w);
}

/* ── rolling-window stats ─────────────────────────────────────────────── */

/* WINDOW-DIVERSITY ( -- u ) */
static void infer_word_window_diversity(VM *vm)
{
    uint64_t d = rolling_window_measure_diversity(&vm->rolling_window);
    VM_PUSH(vm, (cell_t)(int64_t)d);
}

/* ── full inference run ────────────────────────────────────────────────── */

/*
 * INFER-RUN ( -- )
 * Runs the full inference engine on this VM's rolling window and dictionary
 * heat, updating vm->last_inference_outputs.  Allocates the outputs struct
 * on first call (matches the pattern in vm_time.c).
 */
static void infer_word_run(VM *vm)
{
    /* Allocate outputs struct if not yet done */
    if (!vm->last_inference_outputs) {
        vm->last_inference_outputs = (InferenceOutputs *)sf_malloc(sizeof(InferenceOutputs));
        if (!vm->last_inference_outputs) { vm->error = 1; return; }
        memset(vm->last_inference_outputs, 0, sizeof(InferenceOutputs));
    }

    /* Walk dictionary to collect heat stats (mirror of vm_time.c) */
    uint64_t hot_word_count  = 0;
    uint64_t stale_word_count = 0;
    uint64_t total_heat       = 0;
    uint32_t word_count       = 0;

    sf_mutex_lock(&vm->dict_lock);
    DictEntry *e = vm->latest;
    while (e) {
        if (e->execution_heat > HOTWORDS_EXECUTION_HEAT_THRESHOLD)
            hot_word_count++;
        else if (e->execution_heat > 0 && e->execution_heat < 10)
            stale_word_count++;
        total_heat += e->execution_heat;
        word_count++;
        e = e->link;
    }
    sf_mutex_unlock(&vm->dict_lock);

    uint64_t traj_len = (vm->rolling_window.window_pos > 0)
                        ? vm->rolling_window.window_pos
                        : vm->rolling_window.total_executions;

    InferenceInputs inputs;
    memset(&inputs, 0, sizeof(inputs));
    inputs.vm               = vm;
    inputs.window           = &vm->rolling_window;
    inputs.trajectory_length = traj_len;
    inputs.prefetch_hits    = vm->pipeline_metrics.prefetch_hits;
    inputs.prefetch_attempts = vm->pipeline_metrics.prefetch_attempts;
    inputs.hot_word_count   = hot_word_count;
    inputs.stale_word_count = stale_word_count;
    inputs.total_heat       = total_heat;
    inputs.word_count       = word_count;
    inputs.last_total_heat  = vm->total_heat_at_last_check;
    inputs.last_stale_count = vm->stale_word_count_at_check;

    inference_engine_run(&inputs, vm->last_inference_outputs);
}

/* ── inference output accessors ───────────────────────────────────────── */

/* INFER-WINDOW@ ( -- u ) */
static void infer_word_window_fetch(VM *vm)
{
    uint32_t w = vm->last_inference_outputs
                 ? vm->last_inference_outputs->adaptive_window_width : 0;
    VM_PUSH(vm, (cell_t)(int64_t)w);
}

/* INFER-DECAY@ ( -- q ) */
static void infer_word_decay_fetch(VM *vm)
{
    uint64_t d = vm->last_inference_outputs
                 ? vm->last_inference_outputs->adaptive_decay_slope : 0;
    q48_push_inf(vm, (q48_16_t)d);
}

/* INFER-VARIANCE@ ( -- q ) */
static void infer_word_variance_fetch(VM *vm)
{
    uint64_t v = vm->last_inference_outputs
                 ? vm->last_inference_outputs->window_variance_q48 : 0;
    q48_push_inf(vm, (q48_16_t)v);
}

/* INFER-FIT@ ( -- q ) */
static void infer_word_fit_fetch(VM *vm)
{
    uint64_t f = vm->last_inference_outputs
                 ? vm->last_inference_outputs->slope_fit_quality_q48 : 0;
    q48_push_inf(vm, (q48_16_t)f);
}

/* INFER-EARLY-EXIT@ ( -- flag ) */
static void infer_word_early_exit_fetch(VM *vm)
{
    uint32_t ex = vm->last_inference_outputs
                  ? vm->last_inference_outputs->early_exited : 0;
    VM_PUSH(vm, (cell_t)(int64_t)ex);
}

/* ── L8 Jacquard ──────────────────────────────────────────────────────── */

/* L8-MODE ( -- n ) */
static void infer_word_l8_mode(VM *vm)
{
    ssm_l8_state_t *l8 = (ssm_l8_state_t *)vm->ssm_l8_state;
    int mode = l8 ? (int)l8->current_mode : 0;
    VM_PUSH(vm, (cell_t)mode);
}

/*
 * L8-UPDATE ( entropy_q cv_q temporal_q stability_q -- )
 * Takes four Q48.16 values, converts to double, calls ssm_l8_update().
 * Stack order: stability TOS, temporal, cv, entropy at bottom.
 */
static void infer_word_l8_update(VM *vm)
{
    ssm_l8_state_t *l8 = (ssm_l8_state_t *)vm->ssm_l8_state;
    if (!l8) { VM_POP(vm); VM_POP(vm); VM_POP(vm); VM_POP(vm); return; }

    ssm_l8_metrics_t metrics;
    metrics.stability_score = q48_to_dbl(q48_pop_inf(vm));
    metrics.temporal_decay  = q48_to_dbl(q48_pop_inf(vm));
    metrics.cv              = q48_to_dbl(q48_pop_inf(vm));
    metrics.entropy         = q48_to_dbl(q48_pop_inf(vm));
    ssm_l8_update(&metrics, l8);
}

/* L8-APPLY ( -- ) */
static void infer_word_l8_apply(VM *vm)
{
    ssm_l8_state_t *l8 = (ssm_l8_state_t *)vm->ssm_l8_state;
    ssm_config_t *cfg  = (ssm_config_t *)vm->ssm_config;
    if (!l8 || !cfg) return;
    ssm_apply_mode(l8, cfg);
}

/* ── Bayesian latency posteriors ──────────────────────────────────────── */

static BayesianLatencyPosterior cache_posterior(VM *vm)
{
    BayesianLatencyPosterior zero;
    memset(&zero, 0, sizeof(zero));
    if (!vm->hotwords_cache) return zero;
    return hotwords_posterior_cache_hits(&vm->hotwords_cache->stats);
}

static BayesianLatencyPosterior bucket_posterior(VM *vm)
{
    BayesianLatencyPosterior zero;
    memset(&zero, 0, sizeof(zero));
    if (!vm->hotwords_cache) return zero;
    return hotwords_posterior_bucket_searches(&vm->hotwords_cache->stats);
}

static void infer_word_bayes_cache_mean(VM *vm)
{
    q48_push_inf(vm, (q48_16_t)(int64_t)cache_posterior(vm).mean_ns_q48);
}

static void infer_word_bayes_cache_lower(VM *vm)
{
    q48_push_inf(vm, (q48_16_t)(int64_t)cache_posterior(vm).credible_lower_95);
}

static void infer_word_bayes_cache_upper(VM *vm)
{
    q48_push_inf(vm, (q48_16_t)(int64_t)cache_posterior(vm).credible_upper_95);
}

static void infer_word_bayes_bucket_mean(VM *vm)
{
    q48_push_inf(vm, (q48_16_t)(int64_t)bucket_posterior(vm).mean_ns_q48);
}

static void infer_word_bayes_bucket_lower(VM *vm)
{
    q48_push_inf(vm, (q48_16_t)(int64_t)bucket_posterior(vm).credible_lower_95);
}

static void infer_word_bayes_bucket_upper(VM *vm)
{
    q48_push_inf(vm, (q48_16_t)(int64_t)bucket_posterior(vm).credible_upper_95);
}

/* ── registration ─────────────────────────────────────────────────────── */

void register_inference_words(VM *vm)
{
    register_word(vm, "Q.VARIANCE",         infer_word_q_variance);
    register_word(vm, "INFER-DECAY-SLOPE",  infer_word_decay_slope);
    register_word(vm, "INFER-WINDOW-WIDTH", infer_word_window_width);
    register_word(vm, "WINDOW-DIVERSITY",   infer_word_window_diversity);
    register_word(vm, "INFER-RUN",          infer_word_run);
    register_word(vm, "INFER-WINDOW@",      infer_word_window_fetch);
    register_word(vm, "INFER-DECAY@",       infer_word_decay_fetch);
    register_word(vm, "INFER-VARIANCE@",    infer_word_variance_fetch);
    register_word(vm, "INFER-FIT@",         infer_word_fit_fetch);
    register_word(vm, "INFER-EARLY-EXIT@",  infer_word_early_exit_fetch);
    register_word(vm, "L8-MODE",            infer_word_l8_mode);
    register_word(vm, "L8-UPDATE",          infer_word_l8_update);
    register_word(vm, "L8-APPLY",           infer_word_l8_apply);
    register_word(vm, "BAYES-CACHE-MEAN",   infer_word_bayes_cache_mean);
    register_word(vm, "BAYES-CACHE-LOWER",  infer_word_bayes_cache_lower);
    register_word(vm, "BAYES-CACHE-UPPER",  infer_word_bayes_cache_upper);
    register_word(vm, "BAYES-BUCKET-MEAN",  infer_word_bayes_bucket_mean);
    register_word(vm, "BAYES-BUCKET-LOWER", infer_word_bayes_bucket_lower);
    register_word(vm, "BAYES-BUCKET-UPPER", infer_word_bayes_bucket_upper);
}
