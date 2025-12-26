/**
 * vm_bootstrap.h - Kernel VM bootstrap/parity entrypoints
 *
 * Minimal helper to initialize the StarForth VM inside StarKernel
 * and emit the M7 parity packet. Gated by STARFORTH_ENABLE_VM.
 */

#ifndef STARKERNEL_VM_BOOTSTRAP_H
#define STARKERNEL_VM_BOOTSTRAP_H

#ifdef __STARKERNEL__

#include "../parity.h"

/**
 * sk_vm_bootstrap_parity - Initialize VM and emit parity packet
 *
 * @param out Optional parity packet to fill; may be NULL
 * @return 0 on success, -1 on failure
 */
int sk_vm_bootstrap_parity(ParityPacket *out);

#endif /* __STARKERNEL__ */

#endif /* STARKERNEL_VM_BOOTSTRAP_H */
