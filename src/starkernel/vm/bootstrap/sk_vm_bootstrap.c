/**
 * bootstrap.c - Kernel VM bootstrap + parity emit
 *
 * Gated by STARFORTH_ENABLE_VM to avoid touching stable hosted VM builds.
 * Initializes the VM using kernel host hooks, collects parity, and prints it.
 */

#ifdef __STARKERNEL__

#include "vm.h"
#include "starkernel/vm/parity.h"
#include "starkernel/vm/bootstrap/sk_vm_bootstrap.h"
#include "starkernel/hal/hal.h"
#include "console.h"

#ifndef STARFORTH_ENABLE_VM
#define STARFORTH_ENABLE_VM 0
#endif

#if STARFORTH_ENABLE_VM

static int sk_vm_run_minimal_script(VM *vm) {
    if (!vm) {
        return -1;
    }
    vm_push(vm, 1);
    vm_push(vm, 2);
    DictEntry *plus = vm_find_word(vm, "+", 1);
    if (!plus || !plus->func) {
        return -1;
    }
    plus->func(vm);
    if (vm->error || vm->dsp < 0 || vm->data_stack[vm->dsp] != 3) {
        return -1;
    }
    DictEntry *dot = vm_find_word(vm, ".", 1);
    if (dot && dot->func) {
        dot->func(vm);
    }
    return vm->error ? -1 : 0;
}

int sk_vm_bootstrap_parity(ParityPacket *out) {
    static VM vm;            /* Static to avoid large stack frame */
    ParityPacket local_pkt;
    ParityPacket *pkt = out ? out : &local_pkt;

    /* Reset parity packet */
    pkt->bootstrap_result = SK_BOOTSTRAP_INIT_FAIL;

    /* Initialize host services (allocator/time/console) */
    sk_hal_init();

    vm_init_with_host(&vm, sk_hal_host_services());
    if (vm.error) {
        console_println("VM: init failed");
        sk_parity_collect(&vm, pkt);
        sk_parity_print(pkt);
        return -1;
    }

    if (sk_vm_run_minimal_script(&vm) != 0) {
        console_println("VM: minimal script failed");
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
