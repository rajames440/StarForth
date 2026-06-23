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
#include "starkernel/kmalloc.h"
#include "starkernel/console.h"
#include "vm.h"
#include "platform_alloc.h"

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
 * VM Registry — dynamic linked list, heap-allocated via kmalloc
 *===========================================================================*/

typedef struct vm_node {
    VMRegistryEntry  entry;
    uint32_t         parent_vm_id;  /* VM 0 = Hera for direct children */
    struct vm_node  *next;
} vm_node_t;

static vm_node_t *vm_registry_head  = (void *)0;
static uint32_t   vm_registry_count = 0;
static uint32_t   next_vm_id        = 1;  /* VM 0 reserved for Mama */

/* Copy at most VM_NAME_MAX-1 chars, always null-terminate */
static void vm_name_copy(char *dst, const char *src) {
    uint32_t i;
    for (i = 0; i < (VM_NAME_MAX - 1u) && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/* Case-sensitive equality test (no libc) */
static int vm_name_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* Internal: return mutable pointer into registry node for vm_id */
static VMRegistryEntry *vm_find_entry_ptr(uint32_t vm_id) {
    vm_node_t *node = vm_registry_head;
    while (node) {
        if (node->entry.vm_id == vm_id) return &node->entry;
        node = node->next;
    }
    return (void *)0;
}

void capsule_vm_registry_init(void *mama_vm_ptr) {
    uint32_t   i;
    vm_node_t *node;
    vm_node_t *next;
    vm_node_t *mama;

    /* Free any nodes from a previous init (defensive) */
    node = vm_registry_head;
    while (node) {
        next = node->next;
        kfree(node);
        node = next;
    }

    vm_registry_head  = (void *)0;
    vm_registry_count = 0;
    next_vm_id        = 1;

    /* Mama is always VM 0 */
    mama = (vm_node_t *)kmalloc(sizeof(vm_node_t));
    if (!mama) return;

    mama->entry.vm_id              = 0;
    mama->entry.state              = VM_STATE_LIVE;
    mama->entry.birth_capsule_id   = 0;
    mama->entry.birth_timestamp_ns = 0;
    mama->entry.birth_dict_hash    = 0;
    mama->entry.flags              = 0;
    mama->entry.reserved           = 0;
    mama->entry.vm_ptr             = mama_vm_ptr;
    for (i = 0; i < VM_NAME_MAX; i++) mama->entry.name[i] = '\0';
    vm_name_copy(mama->entry.name, "Hera");
    mama->parent_vm_id = 0;
    mama->next = (void *)0;

    vm_registry_head  = mama;
    vm_registry_count = 1;

    /* From this point on all console output is prefixed [Hera] */
    console_set_vm_name("Hera");
}

static VMRegistryEntry *vm_registry_alloc(void) {
    uint32_t   i;
    vm_node_t *node;
    vm_node_t *tail;

    node = (vm_node_t *)kmalloc(sizeof(vm_node_t));
    if (!node) return (void *)0;

    node->entry.vm_id              = 0;
    node->entry.state              = VM_STATE_EMBRYO;
    node->entry.birth_capsule_id   = 0;
    node->entry.birth_timestamp_ns = 0;
    node->entry.birth_dict_hash    = 0;
    node->entry.flags              = 0;
    node->entry.reserved           = 0;
    node->entry.vm_ptr             = (void *)0;
    for (i = 0; i < VM_NAME_MAX; i++) node->entry.name[i] = '\0';
    node->parent_vm_id = 0;
    node->next = (void *)0;

    /* Append to tail */
    if (!vm_registry_head) {
        vm_registry_head = node;
    } else {
        tail = vm_registry_head;
        while (tail->next) tail = tail->next;
        tail->next = node;
    }

    vm_registry_count++;
    return &node->entry;
}

int capsule_vm_registry_get(uint32_t vm_id, VMRegistryEntry *out) {
    VMRegistryEntry *entry;
    if (!out) return -1;
    entry = vm_find_entry_ptr(vm_id);
    if (!entry) return -1;
    *out = *entry;
    return 0;
}

uint32_t capsule_vm_registry_count(void) {
    return vm_registry_count;
}

int capsule_vm_find_by_name(const char *name, VMRegistryEntry *out) {
    vm_node_t *node;
    if (!name || !out) return -1;
    node = vm_registry_head;
    while (node) {
        if (vm_name_eq(node->entry.name, name)) {
            *out = node->entry;
            return 0;
        }
        node = node->next;
    }
    return -1;
}

/* Fold ASCII letter to lowercase (no libc) */
static char vm_to_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* Case-insensitive ASCII equality (no libc) */
static int vm_name_eq_nocase(const char *a, const char *b) {
    while (*a && *b) {
        if (vm_to_lower(*a) != vm_to_lower(*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

int capsule_vm_find_by_name_nocase(const char *name, VMRegistryEntry *out) {
    vm_node_t *node;
    if (!name || !out) return -1;
    node = vm_registry_head;
    while (node) {
        if (vm_name_eq_nocase(node->entry.name, name)) {
            *out = node->entry;
            return 0;
        }
        node = node->next;
    }
    return -1;
}

void capsule_vm_set_state(uint32_t vm_id, uint32_t state) {
    VMRegistryEntry *entry = vm_find_entry_ptr(vm_id);
    if (entry) entry->state = state;
}

void capsule_vm_registry_set_name(uint32_t vm_id, const char *name) {
    VMRegistryEntry *entry;
    if (!name) return;
    entry = vm_find_entry_ptr(vm_id);
    if (!entry) return;
    vm_name_copy(entry->name, name);
}

/*===========================================================================
 * Internal: init.4th dispatch (PERSONALITY layer)
 *
 * After a baby VM runs its identity capsule, attempt to execute block 1.
 * Block 1 is the PERSONALITY layer — the baby's personal init.4th.
 * Failure is silent: the block may not exist, which is normal.
 *===========================================================================*/

static void dispatch_init_forth(void *vm_ctx) {
    /* M9: run baby's personal init.4th from block 1 once per-VM block
     * storage is isolated.  Until then this is a no-op to avoid executing
     * Mama's block 1 content on every child VM. */
    (void)vm_ctx;
}

/*===========================================================================
 * VM Kill
 *===========================================================================*/

int capsule_vm_kill(const char *name) {
    VMRegistryEntry *entry;
    VM              *vm;
    uint32_t         vm_id;
    uint32_t         i;

    if (!name) return -1;

    /* Locate by name (case-insensitive) */
    {
        vm_node_t *node = vm_registry_head;
        entry = (VMRegistryEntry *)0;
        while (node) {
            if (vm_name_eq_nocase(node->entry.name, name)) {
                entry = &node->entry;
                break;
            }
            node = node->next;
        }
    }

    if (!entry) {
        console_puts("KILL: ");
        console_puts(name);
        console_println(" not found");
        return -1;
    }

    /* Hera cannot be killed */
    if (entry->vm_id == 0) {
        console_println("KILL: cannot kill Hera");
        return -1;
    }

    /* Already dead — idempotent */
    if (entry->state == VM_STATE_DEAD) {
        console_puts("KILL: ");
        console_puts(name);
        console_println(" already dead");
        return 0;
    }

    vm_id = entry->vm_id;
    vm    = (VM *)entry->vm_ptr;

    /* Tear down and free */
    if (vm) {
        vm_cleanup(vm);
        sf_free(vm);
    }

    entry->vm_ptr = (void *)0;
    entry->state  = VM_STATE_DEAD;
    for (i = 0; i < VM_NAME_MAX; i++) entry->name[i] = '\0';

    capsule_parity_log_kill(vm_id, name);

    console_puts("KILL: ");
    console_puts(name);
    console_println(" dead");

    return 0;
}

void capsule_vm_kill_all_nonmama(void) {
    vm_node_t *node;
    VM        *vm;
    uint32_t   vm_id;

    node = vm_registry_head;
    while (node) {
        if (node->entry.vm_id == 0 || node->entry.state == VM_STATE_DEAD) {
            node = node->next;
            continue;
        }
        vm_id = node->entry.vm_id;
        vm    = (VM *)node->entry.vm_ptr;
        if (vm) {
            vm->halted = 1;
            vm_cleanup(vm);
            sf_free(vm);
        }
        node->entry.vm_ptr = (void *)0;
        node->entry.state  = VM_STATE_DEAD;
        capsule_parity_log_kill(vm_id, node->entry.name);
        node = node->next;
    }
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

    {
        VMRegistryEntry *mama_entry = vm_find_entry_ptr(0);
        if (mama_entry) {
            mama_entry->birth_capsule_id = mama_cap->capsule_id;
            mama_entry->birth_dict_hash  = post_dict_hash;
        }
    }

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
    entry->vm_ptr = new_vm;
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

    /* PERSONALITY: per-VM block storage is M9 scope; no-op until then */
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
