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
 * Finds the MAMA_INIT capsule, validates it, and executes it on Mama's VM.
 * This establishes Mama's PERSONALITY.
 *
 * @param mama_vm    Mama's VM context
 * @param dir        Capsule directory header
 * @param descs      Capsule descriptor array
 * @param arena      Capsule payload arena
 * @return CAPSULE_RUN_OK on success, error code otherwise
 */
CapsuleRunResult capsule_birth_mama(
    void *mama_vm,
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    const uint8_t *arena
);

/*===========================================================================
 * Baby Birth
 *===========================================================================*/

/**
 * capsule_birth_baby - Birth a new VM from a (p) capsule
 *
 * Allocates a new VM, finds the capsule by hash, validates it,
 * executes it to establish the baby's PERSONALITY.
 *
 * @param capsule_id  Content hash of the (p) capsule
 * @param dir         Capsule directory header
 * @param descs       Capsule descriptor array
 * @param arena       Capsule payload arena
 * @param out_vm_id   Output: assigned VM ID
 * @param out_vm_ctx  Output: new VM context
 * @return CAPSULE_RUN_OK on success, error code otherwise
 */
CapsuleRunResult capsule_birth_baby(
    uint64_t capsule_id,
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    const uint8_t *arena,
    uint32_t *out_vm_id,
    void **out_vm_ctx
);

/*===========================================================================
 * Experiment Execution
 *===========================================================================*/

/**
 * capsule_run_experiment - Execute an (e) capsule on Mama
 *
 * Finds the experiment capsule by hash, validates it,
 * executes it on Mama's VM without creating a new VM.
 *
 * @param mama_vm     Mama's VM context
 * @param capsule_id  Content hash of the (e) capsule
 * @param dir         Capsule directory header
 * @param descs       Capsule descriptor array
 * @param arena       Capsule payload arena
 * @param out_run_id  Output: assigned run ID
 * @return CAPSULE_RUN_OK on success, error code otherwise
 */
CapsuleRunResult capsule_run_experiment(
    void *mama_vm,
    uint64_t capsule_id,
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    const uint8_t *arena,
    uint64_t *out_run_id
);

/*===========================================================================
 * VM Registry
 *===========================================================================*/

/**
 * capsule_vm_registry_init - Initialize VM registry
 */
void capsule_vm_registry_init(void);

/**
 * capsule_vm_registry_get - Get VM registry entry by ID
 *
 * @param vm_id  VM ID to look up
 * @param out    Output: registry entry
 * @return 0 on success, -1 if not found
 */
int capsule_vm_registry_get(uint32_t vm_id, VMRegistryEntry *out);

/**
 * capsule_vm_registry_count - Get number of registered VMs
 */
uint32_t capsule_vm_registry_count(void);

#ifdef __cplusplus
}
#endif

#endif /* STARKERNEL_CAPSULE_BIRTH_H */