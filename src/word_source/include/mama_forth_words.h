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
 * @file mama_forth_words.h
 * @brief Mama FORTH vocabulary words for kernel capsule system (M7.1)
 *
 * This header defines the Mama FORTH vocabulary - kernel-only words
 * for the LithosAnanke capsule birth protocol.
 *
 * These words are ONLY available in __STARKERNEL__ builds.
 * Hosted StarForth builds do not include capsule functionality.
 *
 * Mama FORTH Vocabulary:
 *   CAPSULE-COUNT    ( -- n )        Number of capsules in directory
 *   CAPSULE@         ( idx -- desc ) Get descriptor address by index
 *   CAPSULE-HASH@    ( desc -- hash ) Get capsule content hash
 *   CAPSULE-FLAGS@   ( desc -- flags ) Get capsule flags
 *   CAPSULE-LEN@     ( desc -- len ) Get payload length
 *   CAPSULE-BIRTH    ( hash -- vm-id ) Birth baby VM from (p) capsule
 *   CAPSULE-RUN      ( hash -- ) Run (e) capsule on Mama
 *   MAMA-VM-ID       ( -- 0 ) Mama's VM ID (always 0)
 *   VM-COUNT         ( -- n ) Number of live VMs
 */

#ifndef MAMA_FORTH_WORDS_H
#define MAMA_FORTH_WORDS_H

#include "../../../include/vm.h"

#ifdef __STARKERNEL__

/**
 * @brief Register Mama FORTH vocabulary words with the VM
 *
 * Creates the MAMA vocabulary and registers all capsule-related words.
 * Only available in kernel builds.
 *
 * @param vm Pointer to the VM instance
 */
void register_mama_forth_words(VM *vm);

/* Individual word implementations (kernel-only) */

/**
 * @brief CAPSULE-COUNT ( -- n )
 * Push number of capsules in the capsule directory.
 */
void mama_word_capsule_count(VM *vm);

/**
 * @brief CAPSULE@ ( idx -- desc )
 * Get capsule descriptor address by index.
 */
void mama_word_capsule_fetch(VM *vm);

/**
 * @brief CAPSULE-HASH@ ( desc -- hash )
 * Get content hash from capsule descriptor.
 */
void mama_word_capsule_hash_fetch(VM *vm);

/**
 * @brief CAPSULE-FLAGS@ ( desc -- flags )
 * Get flags from capsule descriptor.
 */
void mama_word_capsule_flags_fetch(VM *vm);

/**
 * @brief CAPSULE-LEN@ ( desc -- len )
 * Get payload length from capsule descriptor.
 */
void mama_word_capsule_len_fetch(VM *vm);

/**
 * @brief CAPSULE-BIRTH ( capsule-id -- vm-id )
 * Birth a baby VM from a production (p) capsule.
 * Returns the new VM's ID, or -1 on failure.
 */
void mama_word_capsule_birth(VM *vm);

/**
 * @brief CAPSULE-RUN ( capsule-id -- )
 * Run an experiment (e) capsule on Mama.
 */
void mama_word_capsule_run(VM *vm);

/**
 * @brief MAMA-VM-ID ( -- 0 )
 * Push Mama's VM ID (always 0).
 */
void mama_word_mama_vm_id(VM *vm);

/**
 * @brief VM-COUNT ( -- n )
 * Push number of registered VMs.
 */
void mama_word_vm_count(VM *vm);

/**
 * @brief CAPSULE-TEST ( -- )
 * Print diagnostic message confirming capsule system is active.
 */
void mama_word_capsule_test(VM *vm);

#else /* !__STARKERNEL__ */

/**
 * @brief Stub for hosted builds - does nothing
 */
static inline void register_mama_forth_words(VM *vm) {
    (void)vm;
    /* Mama FORTH words are kernel-only */
}

#endif /* __STARKERNEL__ */

#endif /* MAMA_FORTH_WORDS_H */
