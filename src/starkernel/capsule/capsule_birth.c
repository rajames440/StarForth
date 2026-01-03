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
*/

/**
 * capsule_birth.c - VM Birth Protocol Implementation (M7.1)
 *
 * Implements the birth protocol for Mama init, baby VMs, and experiments.
 * Freestanding - no libc dependency.
 */

#include "starkernel/capsule_birth.h"
#include "starkernel/capsule.h"
#include "starkernel/capsule_run.h"

/*===========================================================================
 * VM Execution Hooks
 *===========================================================================*/

static CapsuleExecFn vm_exec_fn = 0;
static CapsuleDictHashFn vm_dict_hash_fn = 0;
static CapsuleVMAllocFn vm_alloc_fn = 0;

void capsule_birth_set_hooks(
    CapsuleExecFn exec_fn,
    CapsuleDictHashFn dict_hash_fn,
    CapsuleVMAllocFn vm_alloc_fn_arg)
{
    vm_exec_fn = exec_fn;
    vm_dict_hash_fn = dict_hash_fn;
    vm_alloc_fn = vm_alloc_fn_arg;
}

/*===========================================================================
 * VM Registry
 *===========================================================================*/

#define MAX_VMS 64

static VMRegistryEntry vm_registry[MAX_VMS];
static uint32_t vm_registry_count = 0;
static uint32_t next_vm_id = 1;  /* VM 0 is reserved for Mama */

void capsule_vm_registry_init(void) {
    vm_registry_count = 0;
    next_vm_id = 1;

    /* Zero the registry */
    for (uint32_t i = 0; i < MAX_VMS; i++) {
        vm_registry[i].vm_id = 0;
        vm_registry[i].state = VM_STATE_EMBRYO;
        vm_registry[i].birth_capsule_id = 0;
        vm_registry[i].birth_timestamp_ns = 0;
        vm_registry[i].birth_dict_hash = 0;
        vm_registry[i].flags = 0;
        vm_registry[i].reserved = 0;
    }

    /* Register Mama as VM 0 */
    vm_registry[0].vm_id = 0;
    vm_registry[0].state = VM_STATE_LIVE;
    vm_registry_count = 1;
}

static VMRegistryEntry *vm_registry_alloc(void) {
    if (vm_registry_count >= MAX_VMS) {
        return (void *)0;
    }
    return &vm_registry[vm_registry_count++];
}

int capsule_vm_registry_get(uint32_t vm_id, VMRegistryEntry *out) {
    if (!out) {
        return -1;
    }

    for (uint32_t i = 0; i < vm_registry_count; i++) {
        if (vm_registry[i].vm_id == vm_id) {
            *out = vm_registry[i];
            return 0;
        }
    }

    return -1;
}

uint32_t capsule_vm_registry_count(void) {
    return vm_registry_count;
}

/*===========================================================================
 * Mama Init
 *===========================================================================*/

CapsuleRunResult capsule_birth_mama(
    void *mama_vm,
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    const uint8_t *arena)
{
    if (!mama_vm || !dir || !descs || !arena) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    if (!vm_exec_fn || !vm_dict_hash_fn) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Find Mama's init capsule */
    const CapsuleDesc *mama_cap = capsule_find_mama_init(dir, descs);
    if (!mama_cap) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Validate capsule */
    CapsuleValidateResult vr = capsule_validate(
        mama_cap, arena, dir->arena_size, 1);
    if (vr != CAPSULE_VALID) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Get pre-execution dictionary hash */
    uint64_t pre_dict_hash = vm_dict_hash_fn(mama_vm);

    /* Get payload */
    const uint8_t *payload = capsule_get_payload(mama_cap, arena);
    if (!payload) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Execute Mama's init */
    int exec_result = vm_exec_fn(mama_vm, (const char *)payload, mama_cap->length);
    if (exec_result != 0) {
        return CAPSULE_RUN_ERR_EXEC_FAIL;
    }

    /* Get post-execution dictionary hash */
    uint64_t post_dict_hash = vm_dict_hash_fn(mama_vm);

    /* Log parity record */
    capsule_parity_log_mama_init(
        mama_cap->capsule_id,
        mama_cap->content_hash,
        post_dict_hash
    );

    /* Update Mama's registry entry */
    vm_registry[0].birth_capsule_id = mama_cap->capsule_id;
    vm_registry[0].birth_dict_hash = post_dict_hash;

    return CAPSULE_RUN_OK;
}

/*===========================================================================
 * Baby Birth
 *===========================================================================*/

CapsuleRunResult capsule_birth_baby(
    uint64_t capsule_id,
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    const uint8_t *arena,
    uint32_t *out_vm_id,
    void **out_vm_ctx)
{
    if (!dir || !descs || !arena) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    if (!vm_exec_fn || !vm_dict_hash_fn || !vm_alloc_fn) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Find the capsule */
    const CapsuleDesc *cap = capsule_find_by_id(dir, descs, capsule_id);
    if (!cap) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Check it's a production capsule */
    if (!CAPSULE_BIRTH_ELIGIBLE(cap->flags)) {
        return CAPSULE_RUN_ERR_NOT_ELIGIBLE;
    }

    /* Validate capsule */
    CapsuleValidateResult vr = capsule_validate(
        cap, arena, dir->arena_size, 1);
    if (vr != CAPSULE_VALID) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Allocate registry entry */
    VMRegistryEntry *entry = vm_registry_alloc();
    if (!entry) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    uint32_t vm_id = next_vm_id++;
    entry->vm_id = vm_id;
    entry->state = VM_STATE_EMBRYO;
    entry->birth_capsule_id = capsule_id;

    /* Allocate VM */
    void *new_vm = vm_alloc_fn();
    if (!new_vm) {
        entry->state = VM_STATE_STILLBORN;
        capsule_parity_log_birth_failed(vm_id, capsule_id,
            CAPSULE_RUN_ERR_STILLBORN, 0);
        return CAPSULE_RUN_ERR_STILLBORN;
    }

    /* Get payload */
    const uint8_t *payload = capsule_get_payload(cap, arena);
    if (!payload) {
        entry->state = VM_STATE_STILLBORN;
        capsule_parity_log_birth_failed(vm_id, capsule_id,
            CAPSULE_RUN_ERR_INVALID, 0);
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Execute init on new VM */
    int exec_result = vm_exec_fn(new_vm, (const char *)payload, cap->length);
    if (exec_result != 0) {
        uint64_t partial_hash = vm_dict_hash_fn(new_vm);
        entry->state = VM_STATE_STILLBORN;
        entry->birth_dict_hash = partial_hash;
        capsule_parity_log_birth_failed(vm_id, capsule_id,
            CAPSULE_RUN_ERR_EXEC_FAIL, partial_hash);
        return CAPSULE_RUN_ERR_EXEC_FAIL;
    }

    /* Success - VM is born */
    uint64_t dict_hash = vm_dict_hash_fn(new_vm);
    entry->state = VM_STATE_LIVE;
    entry->birth_dict_hash = dict_hash;

    /* Log parity record */
    capsule_parity_log_birth(vm_id, capsule_id, cap->content_hash, dict_hash);

    /* Return results */
    if (out_vm_id) *out_vm_id = vm_id;
    if (out_vm_ctx) *out_vm_ctx = new_vm;

    return CAPSULE_RUN_OK;
}

/*===========================================================================
 * Experiment Execution
 *===========================================================================*/

CapsuleRunResult capsule_run_experiment(
    void *mama_vm,
    uint64_t capsule_id,
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    const uint8_t *arena,
    uint64_t *out_run_id)
{
    if (!mama_vm || !dir || !descs || !arena) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    if (!vm_exec_fn || !vm_dict_hash_fn) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Find the capsule */
    const CapsuleDesc *cap = capsule_find_by_id(dir, descs, capsule_id);
    if (!cap) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Check it's an experiment capsule */
    if (!CAPSULE_DOE_ELIGIBLE(cap->flags)) {
        return CAPSULE_RUN_ERR_NOT_ELIGIBLE;
    }

    /* Validate capsule */
    CapsuleValidateResult vr = capsule_validate(
        cap, arena, dir->arena_size, 1);
    if (vr != CAPSULE_VALID) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Get pre-execution dictionary hash */
    uint64_t pre_dict_hash = vm_dict_hash_fn(mama_vm);

    /* Get payload */
    const uint8_t *payload = capsule_get_payload(cap, arena);
    if (!payload) {
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* Execute experiment on Mama */
    int exec_result = vm_exec_fn(mama_vm, (const char *)payload, cap->length);

    /* Get post-execution dictionary hash */
    uint64_t post_dict_hash = vm_dict_hash_fn(mama_vm);

    /* Log run record */
    CapsuleRunRecord record = {
        .run_id = 0,  /* Will be assigned by log */
        .vm_id = 0,   /* Mama is VM 0 */
        .reserved = 0,
        .capsule_id = capsule_id,
        .capsule_hash = cap->content_hash,
        .pre_dict_hash = pre_dict_hash,
        .post_dict_hash = post_dict_hash,
        .started_ns = 0,  /* TODO: timestamps */
        .ended_ns = 0,
        .result_code = (exec_result == 0) ? CAPSULE_RUN_OK : CAPSULE_RUN_ERR_EXEC_FAIL,
        .flags = cap->flags
    };

    uint64_t run_id = capsule_run_log_record(&record);

    /* Log parity record */
    capsule_parity_log_run(0, run_id, capsule_id, pre_dict_hash, post_dict_hash);

    if (out_run_id) *out_run_id = run_id;

    return (exec_result == 0) ? CAPSULE_RUN_OK : CAPSULE_RUN_ERR_EXEC_FAIL;
}