/**
 * bootstrap.c - Kernel VM bootstrap + parity emit
 *
 * Gated by STARFORTH_ENABLE_VM to avoid touching stable hosted VM builds.
 * Initializes the VM using kernel host hooks, collects parity, and prints it.
 */

#ifdef __STARKERNEL__

#include "../../include/vm.h"
#include "../../include/starkernel/parity.h"
#include "../../include/starkernel/vm_host.h"
#include "console.h"

#ifndef STARFORTH_ENABLE_VM
#define STARFORTH_ENABLE_VM 0
#endif

#if STARFORTH_ENABLE_VM

int sk_vm_bootstrap_parity(ParityPacket *out) {
    static VM vm;            /* Static to avoid large stack frame */
    ParityPacket local_pkt;
    ParityPacket *pkt = out ? out : &local_pkt;

    /* Reset parity packet */
    pkt->bootstrap_result = SK_BOOTSTRAP_INIT_FAIL;

    /* Initialize host services (allocator/time/console) */
    sk_host_init();

    vm_init_with_host(&vm, sk_host_services());
    if (vm.error) {
        console_println("VM: init failed");
        sk_parity_collect(&vm, pkt);
        sk_parity_print(pkt);
        return -1;
    }

    /* Collect and print parity */
    sk_parity_collect(&vm, pkt);
    sk_parity_print(pkt);

    return (pkt->bootstrap_result == SK_BOOTSTRAP_OK) ? 0 : -1;
}

#endif /* STARFORTH_ENABLE_VM */

#endif /* __STARKERNEL__ */
