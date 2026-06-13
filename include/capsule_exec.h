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
 * capsule_exec.h - Hosted capsule file loader
 *
 * Reads a .4th file from CAPSULES_DIR, parses "Block N" headers,
 * and interprets each block's content sequentially via vm_interpret().
 *
 * This is the hosted-VM counterpart to the kernel's capsule_loader.c.
 * It does NOT use the block storage subsystem — it drives vm_interpret()
 * directly, which avoids the block-number range mismatch (doe.4th uses
 * blocks 2049+, hosted RAM only covers 0..1023).
 */

#ifndef CAPSULE_EXEC_H
#define CAPSULE_EXEC_H

#include "vm.h"

/**
 * @brief Load and execute a capsule .4th file from CAPSULES_DIR.
 *
 * Parses "Block N" headers and calls vm_interpret() on each block's
 * content in order. Block content is processed line by line so that
 * each line fits within INPUT_BUFFER_SIZE.
 *
 * @param vm   Active VM instance
 * @param name Capsule filename (no path, e.g. "doe.4th")
 * @param name_len Length of name (bytes)
 * @return 0 on success, -1 on I/O error or missing file
 */
int capsule_exec_file(VM *vm, const char *name, size_t name_len);

#endif /* CAPSULE_EXEC_H */
