/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.
*/

/**
 * capsule_birth.c - VM Birth Protocol Implementation (M7.1)
 *
 * Mama init, baby birth, and experiment execution.
 * Freestanding - no libc dependency.
 *
 * Birth sequence for a baby VM:
 *   1. Find capsule by name (capsule_find_by_name)
 *   2. Assert CAPSULE_BIRTH_ELIGIBLE
 *   3. Validate content hash
 *   4. vm_alloc_hook() — fresh VM
 *   5. vm_exec_hook(payload) — IDENTITY (init capsule from Hera's store)
 *   6. vm_exec_hook("1 LOAD") — PERSONALITY (baby's personal init.4th from block 1, if present)
 *   7. Log parity record
 */

#include "starkernel/capsule_birth.h"
#include "starkernel/capsule.h"
#include "starkernel/capsule_run.h"

/*===========================================================================
 * VM Execution Hooks
 *===========================================================================*/

static CapsuleExecFn     vm_exec_fn      = 0;
static CapsuleDictHashFn vm_dict_hash_fn = 0;
static CapsuleVMAllocFn  vm_alloc_fn     = 0;

void capsule_birth_set_hooks(
    CapsuleExecFn     exec_fn,
    CapsuleDictHashFn dict_hash_fn,
    CapsuleVMAllocFn  vm_alloc_fn_arg)
{
    vm_exec_fn      = exec_fn;
    vm_dict_hash_fn = dict_hash_fn;
    vm_alloc_fn     = vm_alloc_fn_arg;
}

/*===========================================================================
 * VM Registry
 *===========================================================================*/

#define MAX_VMS 64

static VMRegistryEntry vm_registry[MAX_VMS];
static uint32_t        vm_registry_count = 0;
static uint32_t        next_vm_id = 1;   /* VM 0 is reserved for Mama */

void capsule_vm_registry_init(void) {
    vm_registry_count = 0;
    next_vm_id = 1;

    uint32_t i;
    for (i = 0; i < MAX_VMS; i++) {
        vm_registry[i].vm_id              = 0;
        vm_registry[i].state              = VM_STATE_EMBRYO;
        vm_registry[i].birth_capsule_id   = 0;
        vm_registry[i].birth_timestamp_ns = 0;
        vm_registry[i].birth_dict_hash    = 0;
        vm_registry[i].flags              = 0;
        vm_registry[i].reserved           = 0;
    }

    /* Mama is always VM 0 */
    vm_registry[0].vm_id  = 0;
    vm_registry[0].state  = VM_STATE_LIVE;
    vm_registry_count     = 1;
}

static VMRegistryEntry *vm_registry_alloc(void) {
    if (vm_registry_count >= MAX_VMS) return (void *)0;
    return &vm_registry[vm_registry_count++];
}

int capsule_vm_registry_get(uint32_t vm_id, VMRegistryEntry *out) {
    uint32_t i;
    if (!out) return -1;
    for (i = 0; i < vm_registry_count; i++) {
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
 * Internal: init.4th dispatch (PERSONALITY layer)
 *
 * After a baby VM runs its identity capsule, attempt to execute block 1.
 * Block 1 is the PERSONALITY layer — the baby's personal init.4th.
 * Failure is silent: the block may not exist, which is normal.
 *===========================================================================*/

static void dispatch_init_forth(void *vm_ctx) {
    /* "1 LOAD" — load and execute block 1.  If the block subsystem has no
     * block 1, LOAD will set vm->error which the exec hook clears. */
    vm_exec_fn(vm_ctx, "1 LOAD", 6);
    /* error from a missing personal init.4th is expected and intentional */
}

/*===========================================================================
 * Mama Init
 *===========================================================================*/

CapsuleRunResult capsule_birth_mama(
    void                   *mama_vm,
    const CapsuleDirHeader *dir,
    const CapsuleDesc      *descs,
    const CapsuleNameEntry *names,
    const uint8_t          *arena)
{
    if (!mama_vm || !dir || !descs || !names || !arena)
        return CAPSULE_RUN_ERR_INVALID;
    if (!vm_exec_fn || !vm_dict_hash_fn)
        return CAPSULE_RUN_ERR_INVALID;

    const CapsuleDesc *mama_cap = capsule_find_mama_init(dir, descs);
    if (!mama_cap) return CAPSULE_RUN_ERR_INVALID;

    CapsuleValidateResult vr = capsule_validate(mama_cap, arena, dir->arena_size, 1);
    if (vr != CAPSULE_VALID) return CAPSULE_RUN_ERR_INVALID;

    uint64_t pre_dict_hash = vm_dict_hash_fn(mama_vm);
    (void)pre_dict_hash;

    const uint8_t *payload = capsule_get_payload(mama_cap, arena);
    if (!payload) return CAPSULE_RUN_ERR_INVALID;

    int exec_result = vm_exec_fn(mama_vm, (const char *)payload, mama_cap->length);
    if (exec_result != 0) return CAPSULE_RUN_ERR_EXEC_FAIL;

    uint64_t post_dict_hash = vm_dict_hash_fn(mama_vm);

    capsule_parity_log_mama_init(
        mama_cap->capsule_id,
        mama_cap->content_hash,
        post_dict_hash);

    vm_registry[0].birth_capsule_id = mama_cap->capsule_id;
    vm_registry[0].birth_dict_hash  = post_dict_hash;

    return CAPSULE_RUN_OK;
}

/*===========================================================================
 * Baby Birth
 *===========================================================================*/

CapsuleRunResult capsule_birth_baby(
    const char             *capsule_name,
    const CapsuleDirHeader *dir,
    const CapsuleDesc      *descs,
    const CapsuleNameEntry *names,
    const uint8_t          *arena,
    uint32_t               *out_vm_id,
    void                  **out_vm_ctx)
{
    if (!capsule_name || !dir || !descs || !names || !arena)
        return CAPSULE_RUN_ERR_INVALID;
    if (!vm_exec_fn || !vm_dict_hash_fn || !vm_alloc_fn)
        return CAPSULE_RUN_ERR_INVALID;

    /* Locate by name */
    const CapsuleDesc *cap = capsule_find_by_name(dir, descs, names, capsule_name);
    if (!cap) return CAPSULE_RUN_ERR_INVALID;

    if (!CAPSULE_BIRTH_ELIGIBLE(cap->flags)) return CAPSULE_RUN_ERR_NOT_ELIGIBLE;

    CapsuleValidateResult vr = capsule_validate(cap, arena, dir->arena_size, 1);
    if (vr != CAPSULE_VALID) return CAPSULE_RUN_ERR_INVALID;

    VMRegistryEntry *entry = vm_registry_alloc();
    if (!entry) return CAPSULE_RUN_ERR_INVALID;

    uint32_t vm_id = next_vm_id++;
    entry->vm_id            = vm_id;
    entry->state            = VM_STATE_EMBRYO;
    entry->birth_capsule_id = cap->capsule_id;

    /* Allocate baby VM */
    void *new_vm = vm_alloc_fn();
    if (!new_vm) {
        entry->state = VM_STATE_STILLBORN;
        capsule_parity_log_birth_failed(vm_id, cap->capsule_id,
            CAPSULE_RUN_ERR_STILLBORN, 0);
        return CAPSULE_RUN_ERR_STILLBORN;
    }

    const uint8_t *payload = capsule_get_payload(cap, arena);
    if (!payload) {
        entry->state = VM_STATE_STILLBORN;
        capsule_parity_log_birth_failed(vm_id, cap->capsule_id,
            CAPSULE_RUN_ERR_INVALID, 0);
        return CAPSULE_RUN_ERR_INVALID;
    }

    /* IDENTITY: run init capsule */
    int exec_result = vm_exec_fn(new_vm, (const char *)payload, cap->length);
    if (exec_result != 0) {
        uint64_t partial_hash = vm_dict_hash_fn(new_vm);
        entry->state            = VM_STATE_STILLBORN;
        entry->birth_dict_hash  = partial_hash;
        capsule_parity_log_birth_failed(vm_id, cap->capsule_id,
            CAPSULE_RUN_ERR_EXEC_FAIL, partial_hash);
        return CAPSULE_RUN_ERR_EXEC_FAIL;
    }

    /* PERSONALITY: run baby's personal init.4th from block 1 if present */
    dispatch_init_forth(new_vm);

    uint64_t dict_hash = vm_dict_hash_fn(new_vm);
    entry->state           = VM_STATE_LIVE;
    entry->birth_dict_hash = dict_hash;

    capsule_parity_log_birth(vm_id, cap->capsule_id, cap->content_hash, dict_hash);

    if (out_vm_id)  *out_vm_id  = vm_id;
    if (out_vm_ctx) *out_vm_ctx = new_vm;

    return CAPSULE_RUN_OK;
}

/*===========================================================================
 * Experiment Execution
 *===========================================================================*/

CapsuleRunResult capsule_run_experiment(
    void                   *mama_vm,
    const char             *capsule_name,
    const CapsuleDirHeader *dir,
    const CapsuleDesc      *descs,
    const CapsuleNameEntry *names,
    const uint8_t          *arena,
    uint64_t               *out_run_id)
{
    if (!mama_vm || !capsule_name || !dir || !descs || !names || !arena)
        return CAPSULE_RUN_ERR_INVALID;
    if (!vm_exec_fn || !vm_dict_hash_fn)
        return CAPSULE_RUN_ERR_INVALID;

    const CapsuleDesc *cap = capsule_find_by_name(dir, descs, names, capsule_name);
    if (!cap) return CAPSULE_RUN_ERR_INVALID;

    if (!CAPSULE_DOE_ELIGIBLE(cap->flags)) return CAPSULE_RUN_ERR_NOT_ELIGIBLE;

    CapsuleValidateResult vr = capsule_validate(cap, arena, dir->arena_size, 1);
    if (vr != CAPSULE_VALID) return CAPSULE_RUN_ERR_INVALID;

    uint64_t pre_dict_hash = vm_dict_hash_fn(mama_vm);

    const uint8_t *payload = capsule_get_payload(cap, arena);
    if (!payload) return CAPSULE_RUN_ERR_INVALID;

    int exec_result = vm_exec_fn(mama_vm, (const char *)payload, cap->length);
    uint64_t post_dict_hash = vm_dict_hash_fn(mama_vm);

    CapsuleRunRecord record;
    record.run_id         = 0;
    record.vm_id          = 0;
    record.reserved       = 0;
    record.capsule_id     = cap->capsule_id;
    record.capsule_hash   = cap->content_hash;
    record.pre_dict_hash  = pre_dict_hash;
    record.post_dict_hash = post_dict_hash;
    record.started_ns     = 0;
    record.ended_ns       = 0;
    record.result_code    = (exec_result == 0) ? CAPSULE_RUN_OK : CAPSULE_RUN_ERR_EXEC_FAIL;
    record.flags          = cap->flags;

    uint64_t run_id = capsule_run_log_record(&record);

    capsule_parity_log_run(0, run_id, cap->capsule_id, pre_dict_hash, post_dict_hash);

    if (out_run_id) *out_run_id = run_id;

    return (exec_result == 0) ? CAPSULE_RUN_OK : CAPSULE_RUN_ERR_EXEC_FAIL;
}
