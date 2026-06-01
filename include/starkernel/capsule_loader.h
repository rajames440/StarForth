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
 * capsule_loader.h - Block Capsule Loader and Executor (M7.1)
 *
 * Parses a capsule payload for "Block <num>" headers and writes each
 * block's content into the correct ramdrive slot, then executes the
 * entry block and zeros the ramdrive slots afterward.
 *
 * Format:
 *   Block <decimal>\n
 *   <content: up to 1024 bytes of FORTH source>
 *   Block <decimal>\n
 *   ...
 *
 * Blocks may appear in any order; block numbers are arbitrary.
 * Content exceeding 1024 bytes is truncated with a warning.
 */

#ifndef STARKERNEL_CAPSULE_LOADER_H
#define STARKERNEL_CAPSULE_LOADER_H

#include <stdint.h>
#include "starkernel/capsule.h"
#include "starkernel/capsule_run.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * capsule_load_blocks - Parse a capsule payload and write blocks to the ramdrive
 *
 * For each "Block <num>" header found in the payload:
 *   1. Zero the 1024-byte ramdrive slot for block <num>
 *   2. Copy content bytes into the slot (excess beyond 1024 silently dropped)
 *   3. Mark the slot dirty via blk_update()
 *
 * Content between headers may be text or binary.
 * Anything before the first "Block <num>" header is ignored.
 *
 * @param payload          Raw capsule payload bytes (not null-terminated)
 * @param length           Payload length in bytes
 * @param out_entry_block  If non-NULL, receives the first block number found
 *                         (the execution entry point); set to 0 if no blocks found
 * @return                 Number of blocks written, or -1 on invalid input
 */
int capsule_load_blocks(const uint8_t *payload, uint64_t length,
                        uint32_t *out_entry_block);

/**
 * capsule_clear_blocks - Zero all ramdrive slots referenced in a capsule payload
 *
 * Re-parses the payload for "Block <num>" headers and zeros each
 * corresponding 1024-byte ramdrive slot.  Called after execution to
 * reclaim ramdrive space.
 *
 * @param payload  Raw capsule payload bytes
 * @param length   Payload length in bytes
 */
void capsule_clear_blocks(const uint8_t *payload, uint64_t length);

/**
 * capsule_exec_payload - Parse, populate block device, and execute a payload
 *
 * For each "Block <num>" section: write content to block device (warn+truncate
 * if > 1KB), then execute line-by-line via vm_interpret.  No LOAD is injected.
 *
 * @param vm_opaque  VM to execute on
 * @param payload    Raw payload bytes
 * @param length     Payload length in bytes
 * @return 0 on success, -1 on execution error
 */
int capsule_exec_payload(void *vm_opaque, const uint8_t *payload, uint64_t length);

/**
 * capsule_exec_init - Load, execute, and clean up an init capsule
 *
 * Full init sequence:
 *   1. Locate capsule by colon-separated name in the capsule directory
 *   2. Validate content hash
 *   3. capsule_exec_payload: populate block device + execute content
 *   4. Zero block slots for userspace (capsule_clear_blocks)
 *
 * @param vm            VM to execute on
 * @param capsule_name  Colon-separated capsule name, e.g. "init.4th"
 * @param dir           Capsule directory header
 * @param descs         Capsule descriptor array
 * @param names         Capsule name entry array (parallel to descs)
 * @param arena         Capsule payload arena
 * @return              CAPSULE_RUN_OK on success, error code otherwise
 */
CapsuleRunResult capsule_exec_init(
    void                   *vm,
    const char             *capsule_name,
    const CapsuleDirHeader *dir,
    const CapsuleDesc      *descs,
    const CapsuleNameEntry *names,
    const uint8_t          *arena
);

#ifdef __STARKERNEL__
/**
 * capsule_loader_set_ramdrive - Set the kernel ramdrive buffer.
 *
 * Must be a pre-zeroed buffer of at least 1024 * 1024 bytes covering
 * block numbers CAPSULE_RAM_OFFSET (2048) through 2048+1023.
 */
void capsule_loader_set_ramdrive(uint8_t *buf);

/**
 * capsule_blk_init - Initialize block subsystem and kernel ramdrive.
 *
 * @param vm       Opaque VM pointer (mama VM)
 * @param ram_buf  1MB buffer for dedicated RAM blocks (LBN 0-991)
 * @param ram_size Size of ram_buf (must be >= 1MB)
 * @param krd_buf  Pre-zeroed 1MB buffer for ramdrive blocks (2048-3071)
 * @return         0 on success, non-zero on failure
 */
int capsule_blk_init(void *vm, uint8_t *ram_buf, size_t ram_size, uint8_t *krd_buf);
#endif /* __STARKERNEL__ */

#ifdef __cplusplus
}
#endif

#endif /* STARKERNEL_CAPSULE_LOADER_H */
