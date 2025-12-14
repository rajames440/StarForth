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

#ifndef STARFORTH_STARTUP_H
#define STARFORTH_STARTUP_H

#include "vm.h"
#include "cli.h"

/**
 * @brief Initialize block I/O device from CLI configuration
 *
 * Opens disk image or allocates RAM disk based on CLIConfig.
 * Must be called before vm_init().
 *
 * @param config CLI configuration
 * @return 0 on success, 1 on error (logs messages internally)
 */
int startup_blkio_init(const CLIConfig* config);

/**
 * @brief Clean up block I/O device
 *
 * Flushes and closes block device, frees RAM disk if allocated.
 * Called automatically via atexit().
 */
void startup_blkio_cleanup(void);

/**
 * @brief Initialize VM with defaults
 *
 * Initializes the VM state after block device is ready.
 * @param vm Pointer to VM to initialize
 * @return 0 on success, 1 on error
 */
int startup_vm_init(VM * vm);

/**
 * @brief Initialize block subsystem
 *
 * Sets up block buffering and attaches blkio device.
 * @param vm Pointer to initialized VM
 * @return 0 on success, 1 on error
 */
int startup_block_subsys_init(VM * vm);

#endif /* STARFORTH_STARTUP_H */