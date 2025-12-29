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

/**
 * sk_vm_bootstrap.h - Kernel VM bootstrap/parity entrypoints
 *
 * Gated by STARFORTH_ENABLE_VM so kernel images stay lean by default.
 * Provides a single entry point that initializes the VM using kernel
 * HAL services and emits the M7 parity packet.
 */

#ifndef STARKERNEL_VM_BOOTSTRAP_SK_VM_BOOTSTRAP_H
#define STARKERNEL_VM_BOOTSTRAP_SK_VM_BOOTSTRAP_H

#ifdef __STARKERNEL__

#include "starkernel/vm/parity.h"

/**
 * sk_vm_bootstrap_parity - Initialize VM and emit parity packet.
 *
 * @param out Optional parity packet to fill (may be NULL to use a local)
 * @return 0 on success, -1 on failure (see parity packet for details)
 */
int sk_vm_bootstrap_parity(ParityPacket *out);

#endif /* __STARKERNEL__ */

#endif /* STARKERNEL_VM_BOOTSTRAP_SK_VM_BOOTSTRAP_H */
