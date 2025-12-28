/**
 * bootstrap.c - Kernel VM bootstrap + parity emit
 *
 * Gated by STARFORTH_ENABLE_VM to avoid touching stable hosted VM builds.
 * Initializes the VM using kernel host hooks, collects parity, and prints it.
 */

#ifdef __STARKERNEL__

#include "vm.h"
#include "log.h"
#include "starkernel/vm/parity.h"
#include "starkernel/vm/bootstrap/sk_vm_bootstrap.h"
#include "starkernel/hal/hal.h"
#include "starkernel/console.h"
#include "vm_internal.h"
#include "platform_time.h"

#if SK_PARITY_DEBUG
extern char __text_start[];
extern char __text_end[];
extern char __rodata_start[];
extern char __rodata_end[];
extern char __data_start[];
extern char __data_end[];
extern char __bss_start[];
extern char __bss_end[];

static void sk_bootstrap_debug_print_hex(uint64_t value) {
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    buf[18] = '\0';
    for (int i = 0; i < 16; ++i) {
        uint8_t nibble = (uint8_t)((value >> ((15 - i) * 4)) & 0xF);
        buf[i + 2] = (nibble < 10) ? (char)('0' + nibble) : (char)('a' + nibble - 10);
    }
    console_puts(buf);
}

static int sk_bootstrap_xt_is_canonical(uint64_t addr) {
    uint64_t upper = addr >> 48;
    return (upper == 0x0000ULL) || (upper == 0xFFFFULL);
}

static const char *sk_bootstrap_region_name(uint64_t addr) {
    if (addr == 0) return "null";
    if (addr >= (uint64_t)(uintptr_t)__text_start && addr < (uint64_t)(uintptr_t)__text_end) {
        return "text";
    }
    if (addr >= (uint64_t)(uintptr_t)__rodata_start && addr < (uint64_t)(uintptr_t)__rodata_end) {
        return "rodata";
    }
    if (addr >= (uint64_t)(uintptr_t)__data_start && addr < (uint64_t)(uintptr_t)__data_end) {
        return "data";
    }
    if (addr >= (uint64_t)(uintptr_t)__bss_start && addr < (uint64_t)(uintptr_t)__bss_end) {
        return "bss";
    }
    if (addr >= 0xffff800000000000ULL) {
        return "directmap";
    }
    return "unknown";
}

static void sk_bootstrap_debug_log_xt(const char *label, const DictEntry *entry) {
    console_puts("[SK_BOOTDBG] ");
    console_puts(label ? label : "(word)");
    console_puts(" entry=");
    sk_bootstrap_debug_print_hex((uint64_t)(uintptr_t)entry);
    const void *xt = entry ? (const void *)entry->func : NULL;
    console_puts(" xt=");
    sk_bootstrap_debug_print_hex((uint64_t)(uintptr_t)xt);
    console_puts(" region=");
    const char *region = sk_bootstrap_region_name((uint64_t)(uintptr_t)xt);
    console_puts(region);
    console_println("");

    uint64_t addr = (uint64_t)(uintptr_t)xt;
    if (!sk_bootstrap_xt_is_canonical(addr) || (region[0] == 'u' && region[1] == 'n')) {
        console_puts("[SK_BOOTDBG] invalid XT for ");
        console_puts(label ? label : "(word)");
        console_puts(" region=");
        console_puts(region);
        console_println("");
        sk_hal_panic("bootstrap XT pointer invalid");
    }
}
#endif

#ifndef STARFORTH_ENABLE_VM
#define STARFORTH_ENABLE_VM 0
#endif

#if STARFORTH_ENABLE_VM

static int sk_vm_run_minimal_script(VM *vm) {
    if (!vm) {
        return -1;
    }
#if SK_PARITY_DEBUG
    console_puts("[VM_BOOTSTRAP] root vocabulary entry: 'FORTH'");
    console_println("");
#endif
    DictEntry *forth_vocab = vm_find_word(vm, "FORTH", 5);
#if SK_PARITY_DEBUG
    if (forth_vocab) {
        sk_bootstrap_debug_log_xt("FORTH", forth_vocab);
    } else {
        console_println("[VM_BOOTSTRAP] FORTH vocabulary NOT FOUND");
    }
#endif
    vm_push(vm, 1);
    vm_push(vm, 2);
    DictEntry *plus = vm_find_word(vm, "+", 1);
#if SK_PARITY_DEBUG
    if (plus) {
        sk_bootstrap_debug_log_xt("+", plus);
    }
#endif
    if (!plus || !plus->func) {
        return -1;
    }
    plus->func(vm);
    if (vm->error || vm->dsp < 0 || vm->data_stack[vm->dsp] != 3) {
        return -1;
    }
    DictEntry *dot = vm_find_word(vm, ".", 1);
#if SK_PARITY_DEBUG
    if (dot) {
        sk_bootstrap_debug_log_xt(".", dot);
    }
#endif
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
    sf_time_init();

    vm_init_with_host(&vm, sk_hal_host_services());
    if (vm.error) {
        console_println("VM: init failed");
        sk_parity_collect(&vm, pkt);
        sk_parity_print(pkt);
        return -1;
    }
#if SK_PARITY_DEBUG
    log_set_level(LOG_DEBUG);
#endif

    if (sk_vm_run_minimal_script(&vm) != 0) {
        console_println("VM: minimal script failed");
        sk_parity_collect(&vm, pkt);
        sk_parity_print(pkt);
        sk_hal_freeze_exec_range();
        return -1;
    }

    /* Collect and print parity */
    sk_parity_collect(&vm, pkt);
    sk_parity_print(pkt);
    if (pkt->bootstrap_result == SK_BOOTSTRAP_OK &&
        pkt->tests_failed == 0 &&
        pkt->tests_errors == 0) {
        vm_enable_interpreter(&vm);
    }
    sk_hal_freeze_exec_range();

    return (pkt->bootstrap_result == SK_BOOTSTRAP_OK) ? 0 : -1;
}

#endif /* STARFORTH_ENABLE_VM */

#endif /* __STARKERNEL__ */
