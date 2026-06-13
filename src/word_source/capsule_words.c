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
 * capsule_words.c - FORTH words bridging the hosted VM to the capsule SDK
 *
 * Words defined here let a user (or doe.4th) drive:
 *   - EXEC:         load a capsule .4th file from CAPSULES_DIR
 *   - L8-UPDATE/L8-APPLY/L8-MODE:  full L8 Jacquard mode selector bridge
 *   - INFER-*:      expose the inference engine outputs to Forth
 *   - WINDOW-DIVERSITY: rolling-window diversity metric (Q48.16)
 *   - BAYES-*:      hot-words cache Bayesian latency means (Q48.16)
 */

#include "include/capsule_words.h"
#include "../../include/capsule_exec.h"
#include "../../include/ssm_jacquard.h"
#include "../../include/inference_engine.h"
#include "../../include/rolling_window_of_truth.h"
#include "../../include/physics_hotwords_cache.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/platform_alloc.h"
#include "../../include/platform_lock.h"

#include <string.h>

/* ------------------------------------------------------------------
 * EXEC ( c-addr u -- )
 *
 * Execute a capsule file.  The string on the stack is the filename
 * (no path) relative to CAPSULES_DIR.
 * ------------------------------------------------------------------ */
static void forth_exec(VM *vm)
{
    if (!vm || vm->dsp < 1) {
        log_message(LOG_ERROR, "EXEC: stack underflow");
        vm->error = 1;
        return;
    }

    cell_t u    = vm_pop(vm);
    cell_t addr = vm_pop(vm);

    if (u <= 0 || u > 255) {
        log_message(LOG_ERROR, "EXEC: invalid filename length %ld", (long)u);
        vm->error = 1;
        return;
    }

    if (!vm_addr_ok(vm, (vaddr_t)addr, (size_t)u)) {
        log_message(LOG_ERROR, "EXEC: address out of range");
        vm->error = 1;
        return;
    }

    const char *name = (const char *)(vm->memory + (vaddr_t)addr);
    if (capsule_exec_file(vm, name, (size_t)u) != 0) {
        /* capsule_exec_file already logged the error */
        vm->error = 1;
    }
}

/* ------------------------------------------------------------------
 * L8-UPDATE ( ent-q cv-q tmp-q stb-q -- )
 *
 * Feed four Q48.16 metrics (as integers × 65536) into the L8 mode
 * selector.  Stack order: entropy, cv, temporal_decay, stability
 * (pushed left-to-right, so stb is on top).
 * ------------------------------------------------------------------ */
static void forth_l8_update(VM *vm)
{
    if (!vm || vm->dsp < 3) {
        log_message(LOG_ERROR, "L8-UPDATE: stack underflow");
        vm->error = 1;
        return;
    }

    cell_t stb_q = vm_pop(vm);
    cell_t tmp_q = vm_pop(vm);
    cell_t cv_q  = vm_pop(vm);
    cell_t ent_q = vm_pop(vm);

    ssm_l8_metrics_t metrics;
    metrics.entropy        = (double)ent_q / 65536.0;
    metrics.cv             = (double)cv_q  / 65536.0;
    metrics.temporal_decay = (double)tmp_q / 65536.0;
    metrics.stability_score = (double)stb_q / 65536.0;

    if (!vm->ssm_l8_state) {
        log_message(LOG_ERROR, "L8-UPDATE: L8 state not initialized");
        vm->error = 1;
        return;
    }

    ssm_l8_update(&metrics, (ssm_l8_state_t *)vm->ssm_l8_state);
}

/* ------------------------------------------------------------------
 * L8-APPLY ( -- )
 *
 * Apply the current L8 mode to vm->ssm_config (sets L2/L3/L5/L6 bits).
 * ------------------------------------------------------------------ */
static void forth_l8_apply(VM *vm)
{
    if (!vm->ssm_l8_state || !vm->ssm_config) {
        log_message(LOG_ERROR, "L8-APPLY: L8 state not initialized");
        vm->error = 1;
        return;
    }

    ssm_apply_mode((const ssm_l8_state_t *)vm->ssm_l8_state,
                   (ssm_config_t *)vm->ssm_config);
}

/* ------------------------------------------------------------------
 * L8-MODE ( -- mode )
 *
 * Push the current L8 mode index (0-15) onto the stack.
 * ------------------------------------------------------------------ */
static void forth_l8_mode(VM *vm)
{
    if (!vm->ssm_l8_state) {
        vm_push(vm, 0);
        return;
    }

    ssm_l8_state_t *state = (ssm_l8_state_t *)vm->ssm_l8_state;
    vm_push(vm, (cell_t)state->current_mode);
}

/* ------------------------------------------------------------------
 * Shared inference runner — mirrors vm_time.c run_inference_engine().
 * Allocates last_inference_outputs if not yet done, collects dictionary
 * metrics, and runs inference_engine_run().
 * ------------------------------------------------------------------ */
static void run_inference(VM *vm)
{
    rolling_window_service(&vm->rolling_window);

    sf_mutex_lock(&vm->tuning_lock);
    if (!vm->last_inference_outputs) {
        vm->last_inference_outputs = sf_malloc(sizeof(InferenceOutputs));
        if (!vm->last_inference_outputs) {
            sf_mutex_unlock(&vm->tuning_lock);
            log_message(LOG_ERROR, "INFER-RUN: alloc failed");
            vm->error = 1;
            return;
        }
        memset(vm->last_inference_outputs, 0, sizeof(InferenceOutputs));
    }
    sf_mutex_unlock(&vm->tuning_lock);

    uint64_t hot_word_count   = 0;
    uint64_t stale_word_count = 0;
    uint64_t total_heat       = 0;
    uint32_t word_count       = 0;

    sf_mutex_lock(&vm->dict_lock);
    for (DictEntry *e = vm->latest; e != NULL; e = e->link) {
        if (e->execution_heat > HOTWORDS_EXECUTION_HEAT_THRESHOLD)
            hot_word_count++;
        else if (e->execution_heat > 0 && e->execution_heat < 10)
            stale_word_count++;
        total_heat += e->execution_heat;
        word_count++;
    }
    sf_mutex_unlock(&vm->dict_lock);

    uint64_t tlen = (vm->rolling_window.window_pos > 0)
                    ? vm->rolling_window.window_pos
                    : vm->rolling_window.total_executions;

    /* inference_engine_run requires is_warm and trajectory_length > 1 */
    if (!vm->rolling_window.is_warm || tlen <= 1) {
        log_message(LOG_DEBUG, "INFER-RUN: window not warm yet (tlen=%llu)", (unsigned long long)tlen);
        return;
    }

    InferenceInputs inputs = {
        .window            = &vm->rolling_window,
        .vm                = vm,
        .trajectory_length = tlen,
        .prefetch_hits     = vm->pipeline_metrics.prefetch_hits,
        .prefetch_attempts = vm->pipeline_metrics.prefetch_attempts,
        .hot_word_count    = hot_word_count,
        .stale_word_count  = stale_word_count,
        .total_heat        = total_heat,
        .word_count        = word_count,
        .last_total_heat   = vm->total_heat_at_last_check,
        .last_stale_count  = vm->stale_word_count_at_check,
    };

    inference_engine_run(&inputs, vm->last_inference_outputs);
}

/* ------------------------------------------------------------------
 * INFER-RUN ( -- )
 *
 * Trigger one inference engine pass.  Results are cached in
 * vm->last_inference_outputs and read by INFER-WINDOW@ etc.
 * ------------------------------------------------------------------ */
static void forth_infer_run(VM *vm)
{
    run_inference(vm);
}

/* ------------------------------------------------------------------
 * WINDOW-DIVERSITY ( -- q )
 *
 * Push rolling-window pattern diversity as Q48.16.
 * diversity = last_pattern_diversity / effective_window_size (clamped 0..1)
 * ------------------------------------------------------------------ */
static void forth_window_diversity(VM *vm)
{
    uint64_t diversity  = vm->rolling_window.last_pattern_diversity;
    uint32_t win        = vm->rolling_window.effective_window_size;

    cell_t q = 0;
    if (win > 0) {
        /* Clamp to 1.0 = 65536 in Q48.16 */
        if (diversity >= (uint64_t)win)
            q = 65536;
        else
            q = (cell_t)((diversity * 65536ULL) / (uint64_t)win);
    }

    vm_push(vm, q);
}

/* ------------------------------------------------------------------
 * INFER-WINDOW@ ( -- n )
 * Push the last inferred window width (cells, not Q48.16).
 * ------------------------------------------------------------------ */
static void forth_infer_window(VM *vm)
{
    cell_t val = 0;
    if (vm->last_inference_outputs)
        val = (cell_t)vm->last_inference_outputs->adaptive_window_width;
    vm_push(vm, val);
}

/* ------------------------------------------------------------------
 * INFER-DECAY@ ( -- q )
 * Push the last inferred decay slope as Q48.16.
 * ------------------------------------------------------------------ */
static void forth_infer_decay(VM *vm)
{
    cell_t val = 0;
    if (vm->last_inference_outputs)
        val = (cell_t)vm->last_inference_outputs->adaptive_decay_slope;
    vm_push(vm, val);
}

/* ------------------------------------------------------------------
 * INFER-VARIANCE@ ( -- q )
 * Push the last inferred window variance as Q48.16.
 * ------------------------------------------------------------------ */
static void forth_infer_variance(VM *vm)
{
    cell_t val = 0;
    if (vm->last_inference_outputs)
        val = (cell_t)vm->last_inference_outputs->window_variance_q48;
    vm_push(vm, val);
}

/* ------------------------------------------------------------------
 * INFER-EARLY-EXIT@ ( -- flag )
 * Push 1 if the last inference pass used ANOVA early-exit, else 0.
 * ------------------------------------------------------------------ */
static void forth_infer_early_exit(VM *vm)
{
    cell_t val = 0;
    if (vm->last_inference_outputs)
        val = (cell_t)vm->last_inference_outputs->early_exited;
    vm_push(vm, val);
}

/* ------------------------------------------------------------------
 * INFER-FIT@ ( -- q )
 * Push the last inference fit quality (R² proxy) as Q48.16.
 * ------------------------------------------------------------------ */
static void forth_infer_fit(VM *vm)
{
    cell_t val = 0;
    if (vm->last_inference_outputs)
        val = (cell_t)vm->last_inference_outputs->slope_fit_quality_q48;
    vm_push(vm, val);
}

/* ------------------------------------------------------------------
 * BAYES-CACHE-MEAN ( -- q )
 *
 * Push the hot-words cache mean hit latency as Q48.16.
 * = cache_hit_total_ns_q48 / cache_hit_samples (already in Q48.16 units)
 * ------------------------------------------------------------------ */
static void forth_bayes_cache_mean(VM *vm)
{
    cell_t val = 0;
    if (vm->hotwords_cache) {
        HotwordsStats *s = &vm->hotwords_cache->stats;
        if (s->cache_hit_samples > 0)
            val = (cell_t)(s->cache_hit_total_ns_q48 / (int64_t)s->cache_hit_samples);
    }
    vm_push(vm, val);
}

/* ------------------------------------------------------------------
 * BAYES-BUCKET-MEAN ( -- q )
 *
 * Push the dictionary bucket-search mean latency as Q48.16.
 * = bucket_search_total_ns_q48 / bucket_search_samples
 * ------------------------------------------------------------------ */
static void forth_bayes_bucket_mean(VM *vm)
{
    cell_t val = 0;
    if (vm->hotwords_cache) {
        HotwordsStats *s = &vm->hotwords_cache->stats;
        if (s->bucket_search_samples > 0)
            val = (cell_t)(s->bucket_search_total_ns_q48 / (int64_t)s->bucket_search_samples);
    }
    vm_push(vm, val);
}

/* ------------------------------------------------------------------
 * register_capsule_words
 * ------------------------------------------------------------------ */
void register_capsule_words(VM *vm)
{
    /* Capsule loader */
    register_word(vm, "EXEC", forth_exec);

    /* L8 Jacquard mode selector bridge */
    register_word(vm, "L8-UPDATE", forth_l8_update);
    register_word(vm, "L8-APPLY",  forth_l8_apply);
    register_word(vm, "L8-MODE",   forth_l8_mode);

    /* Inference engine observability */
    register_word(vm, "INFER-RUN",        forth_infer_run);
    register_word(vm, "WINDOW-DIVERSITY", forth_window_diversity);
    register_word(vm, "INFER-WINDOW@",    forth_infer_window);
    register_word(vm, "INFER-DECAY@",     forth_infer_decay);
    register_word(vm, "INFER-VARIANCE@",  forth_infer_variance);
    register_word(vm, "INFER-EARLY-EXIT@", forth_infer_early_exit);
    register_word(vm, "INFER-FIT@",       forth_infer_fit);

    /* Hot-words cache Bayesian latency means */
    register_word(vm, "BAYES-CACHE-MEAN",  forth_bayes_cache_mean);
    register_word(vm, "BAYES-BUCKET-MEAN", forth_bayes_bucket_mean);
}
