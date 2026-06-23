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
 * capsule_birth.h - VM Birth Protocol (M7.1)
 *
 * Functions for birthing VMs from capsules:
 * - Mama init: Execute core/init.4th to establish Mama's PERSONALITY
 * - Baby birth: Create new VM from (p) capsule
 * - Experiment run: Execute (e) capsule on Mama
 */

#ifndef STARKERNEL_CAPSULE_BIRTH_H
#define STARKERNEL_CAPSULE_BIRTH_H

#include <stdint.h>
#include "starkernel/capsule.h"
#include "starkernel/capsule_run.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * VM Execution Hook
 *
 * The birth protocol needs to execute FORTH code on a VM.
 * This hook is provided by the VM layer.
 *===========================================================================*/

/**
 * VM execution function type
 *
 * @param vm_ctx     Opaque pointer to VM context
 * @param code       FORTH source code to execute
 * @param code_len   Length of code in bytes
 * @return 0 on success, non-zero on error
 */
typedef int (*CapsuleExecFn)(void *vm_ctx, const char *code, uint64_t code_len);

/**
 * Dictionary hash function type
 *
 * @param vm_ctx  Opaque pointer to VM context
 * @return 64-bit hash of dictionary state
 */
typedef uint64_t (*CapsuleDictHashFn)(void *vm_ctx);

/**
 * VM allocation function type (for baby birth)
 *
 * @return Opaque pointer to new VM context, or NULL on failure
 */
typedef void *(*CapsuleVMAllocFn)(void);

/**
 * capsule_birth_set_hooks - Configure VM execution hooks
 *
 * Must be called before any birth operations.
 *
 * @param exec_fn      Function to execute FORTH code on VM
 * @param dict_hash_fn Function to compute dictionary hash
 * @param vm_alloc_fn  Function to allocate new VM (for babies)
 */
void capsule_birth_set_hooks(
    CapsuleExecFn exec_fn,
    CapsuleDictHashFn dict_hash_fn,
    CapsuleVMAllocFn vm_alloc_fn
);

/*===========================================================================
 * Mama Init
 *===========================================================================*/

/**
 * capsule_birth_mama - Execute Mama's init capsule
 *
 * Finds the MAMA_INIT capsule by flag, validates it, executes it on Mama's VM.
 *
 * @param mama_vm    Mama's VM context
 * @param dir        Capsule directory header
 * @param descs      Capsule descriptor array
 * @param names      Capsule name entry array (parallel to descs)
 * @param arena      Capsule payload arena
 * @return CAPSULE_RUN_OK on success, error code otherwise
 */
CapsuleRunResult capsule_birth_mama(
    void *mama_vm,
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    const CapsuleNameEntry *names,
    const uint8_t *arena
);

/*===========================================================================
 * Baby Birth
 *===========================================================================*/

/**
 * capsule_birth_baby - Birth a new VM from a named (p) capsule
 *
 * Finds the capsule by colon-separated name, validates it, allocates a new
 * VM, executes the capsule payload as IDENTITY, then (if present) executes
 * unit.4th from the baby's block space as PERSONALITY.
 *
 * @param capsule_name  Colon-separated capsule name, e.g. "production:myvm.4th"
 * @param dir           Capsule directory header
 * @param descs         Capsule descriptor array
 * @param names         Capsule name entry array (parallel to descs)
 * @param arena         Capsule payload arena
 * @param out_vm_id     Output: assigned VM ID
 * @param out_vm_ctx    Output: new VM context
 * @return CAPSULE_RUN_OK on success, error code otherwise
 */
CapsuleRunResult capsule_birth_baby(
    const char *capsule_name,
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    const CapsuleNameEntry *names,
    const uint8_t *arena,
    uint32_t *out_vm_id,
    void **out_vm_ctx
);

/*===========================================================================
 * Experiment Execution
 *===========================================================================*/

/**
 * capsule_run_experiment - Execute a named (e) capsule on Mama
 *
 * Finds the experiment capsule by colon-separated name, validates it,
 * executes it on Mama's VM without creating a new VM.
 *
 * @param mama_vm      Mama's VM context
 * @param capsule_name Colon-separated capsule name, e.g. "experiments:doe-l8:init-l8-stable.4th"
 * @param dir          Capsule directory header
 * @param descs        Capsule descriptor array
 * @param names        Capsule name entry array (parallel to descs)
 * @param arena        Capsule payload arena
 * @param out_run_id   Output: assigned run ID
 * @return CAPSULE_RUN_OK on success, error code otherwise
 */
CapsuleRunResult capsule_run_experiment(
    void *mama_vm,
    const char *capsule_name,
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    const CapsuleNameEntry *names,
    const uint8_t *arena,
    uint64_t *out_run_id
);

/*===========================================================================
 * VM Hook Registration
 *===========================================================================*/

/**
 * capsule_vm_hooks_register - Wire concrete VM hooks into the capsule subsystem
 *
 * Registers capsule_exec_hook, capsule_dict_hash_hook, and capsule_vm_alloc_hook.
 * Must be called after vm_init() on Mama's VM and before any birth operations.
 */
void capsule_vm_hooks_register(void);

/*===========================================================================
 * VM Registry
 *===========================================================================*/

/**
 * capsule_vm_registry_init - Initialize VM registry
 *
 * Allocates Mama's registry node (VM 0, name "Hera") via kmalloc and
 * stores mama_vm_ptr so KILL can guard against destroying Mama.
 * Must be called after kmalloc_init().
 *
 * @param mama_vm_ptr  Pointer to Mama's VM object (e.g. &sk_mama_vm)
 */
void capsule_vm_registry_init(void *mama_vm_ptr);

/**
 * capsule_vm_kill - Destroy a named VM and release all its resources
 *
 * Looks up the VM by name (case-insensitive).  Hera (VM 0) cannot be
 * killed.  If the VM is already DEAD the call is a no-op.
 * On success: vm_cleanup + sf_free, state → VM_STATE_DEAD, name cleared.
 *
 * @param name  Symbolic name of the VM to kill (case-insensitive)
 * @return 0 on success (or already dead), -1 if not found or refused
 */
int capsule_vm_kill(const char *name);

/**
 * capsule_vm_registry_get - Get VM registry entry by ID
 *
 * @param vm_id  VM ID to look up
 * @param out    Output: registry entry copy
 * @return 0 on success, -1 if not found
 */
int capsule_vm_registry_get(uint32_t vm_id, VMRegistryEntry *out);

/**
 * capsule_vm_registry_count - Get number of registered VMs
 */
uint32_t capsule_vm_registry_count(void);

/**
 * capsule_vm_find_by_name - Find VM registry entry by symbolic name
 *
 * Case-sensitive. Returns the first match.
 *
 * @param name  Symbolic VM name, e.g. "Hermes"
 * @param out   Output: registry entry copy
 * @return 0 if found, -1 if not found
 */
int capsule_vm_find_by_name(const char *name, VMRegistryEntry *out);

/**
 * capsule_vm_find_by_name_nocase - Find VM registry entry by name (case-insensitive)
 *
 * Used by BIRTH for idempotency: prevents birthing a second VM with the
 * same name regardless of case differences.
 *
 * @param name  Symbolic VM name (compared case-insensitively)
 * @param out   Output: registry entry copy
 * @return 0 if found, -1 if not found
 */
int capsule_vm_find_by_name_nocase(const char *name, VMRegistryEntry *out);

/**
 * capsule_vm_set_state - Update a VM's state in the registry
 *
 * @param vm_id  VM ID to update
 * @param state  New VMState value
 */
void capsule_vm_set_state(uint32_t vm_id, uint32_t state);

/**
 * capsule_vm_registry_set_name - Assign a symbolic name to a registered VM
 *
 * Truncates to VM_NAME_MAX-1 characters. No-op if vm_id not found.
 *
 * @param vm_id  VM ID to name
 * @param name   Symbolic name string
 */
void capsule_vm_registry_set_name(uint32_t vm_id, const char *name);

/**
 * capsule_vm_kill_all_nonmama - Kill every non-Mama VM in the registry.
 *
 * Sets halted, calls vm_cleanup + sf_free, marks state DEAD. Used by
 * Hera's BYE immediately before arch_cold_reset() to reap all children.
 */
void capsule_vm_kill_all_nonmama(void);

#ifdef __cplusplus
}
#endif

#endif /* STARKERNEL_CAPSULE_BIRTH_H */