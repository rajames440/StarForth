/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.
*/

/**
 * capsule_vm_hooks.c - VM Hook Implementations for Capsule Birth (M7.1)
 *
 * Concrete implementations of the three function pointers required by
 * capsule_birth_set_hooks():
 *
 *   capsule_exec_hook      -- wraps vm_interpret()
 *   capsule_dict_hash_hook -- xxhash64 walk of the live dictionary
 *   capsule_vm_alloc_hook  -- allocates and initialises a fresh VM
 *
 * Call capsule_vm_hooks_register() once during VM bootstrap (after
 * register_forth79_words) to wire everything up.
 */

#include "starkernel/capsule_birth.h"
#include "starkernel/capsule_loader.h"
#include "starkernel/xxhash64.h"
#include "vm.h"
#include "word_registry.h"
#include "platform_alloc.h"
#include "word_source/include/mama_forth_words.h"
#ifdef __STARKERNEL__
#include "starkernel/hal/hal.h"  /* sk_hal_host_services() */
#endif

/*===========================================================================
 * exec hook  ( vm_ctx, code, code_len ) -> 0 on success
 *===========================================================================*/

static int capsule_exec_hook(void *vm_ctx, const char *code, uint64_t code_len) {
    return capsule_exec_payload(vm_ctx, (const uint8_t *)code, code_len);
}

/*===========================================================================
 * dict hash hook  ( vm_ctx ) -> uint64_t
 *
 * Walks the live dictionary linked list and hashes each word's name and
 * execution heat.  Provides a cheap, deterministic snapshot of dictionary
 * state for parity logging.
 *===========================================================================*/

static uint64_t capsule_dict_hash_hook(void *vm_ctx) {
    VM *vm = (VM *)vm_ctx;
    if (!vm) return 0;

    XXHash64State state;
    xxhash64_reset(&state, 0);

    DictEntry *entry = vm->latest;
    while (entry) {
        /* Hash: word name */
        xxhash64_update(&state, entry->name, entry->name_len);
        /* Hash: execution heat (captures runtime activity level) */
        xxhash64_update(&state, &entry->execution_heat,
                        sizeof(entry->execution_heat));
        entry = entry->link;
    }

    return xxhash64_digest(&state);
}

/*===========================================================================
 * vm alloc hook  () -> void* (VM*)
 *
 * Allocates a fresh VM on the heap, initialises it fully (including word
 * registration and physics subsystems), and returns it as an opaque pointer.
 * Returns NULL on allocation failure.
 *===========================================================================*/

static void *capsule_vm_alloc_hook(void) {
    VM *vm = (VM *)sf_malloc(sizeof(VM));
    if (!vm) return (void *)0;

#ifdef __STARKERNEL__
    vm_init_with_host(vm, sk_hal_host_services());
#else
    vm_init(vm);
#endif

    /* vm_init sets vm->error if memory allocation failed internally */
    if (vm->error) {
        vm_cleanup(vm);
        sf_free(vm);
        return (void *)0;
    }

    /* Enable interpreter — child VMs skip sk_vm_bootstrap_parity, must set manually */
    vm_enable_interpreter(vm);

    /* Give every child VM the STOP word so it can self-stop */
    register_word(vm, "STOP", mama_word_stop);

    /* Give every child VM EXEC so it can load capsules (e.g. doe.4th) */
    register_word(vm, "EXEC", mama_word_exec);

    return (void *)vm;
}

/*===========================================================================
 * Registration
 *===========================================================================*/

/**
 * capsule_vm_hooks_register - Wire the three hooks into the capsule subsystem.
 *
 * Must be called after vm_init() completes on Mama's VM, before any capsule
 * birth or experiment operations.
 */
void capsule_vm_hooks_register(void) {
    capsule_birth_set_hooks(
        capsule_exec_hook,
        capsule_dict_hash_hook,
        capsule_vm_alloc_hook
    );
}
