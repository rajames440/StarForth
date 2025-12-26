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

#include "../../parity.h"

/**
 * sk_vm_bootstrap_parity - Initialize VM and emit parity packet.
 *
 * @param out Optional parity packet to fill (may be NULL to use a local)
 * @return 0 on success, -1 on failure (see parity packet for details)
 */
int sk_vm_bootstrap_parity(ParityPacket *out);

#endif /* __STARKERNEL__ */

#endif /* STARKERNEL_VM_BOOTSTRAP_SK_VM_BOOTSTRAP_H */
