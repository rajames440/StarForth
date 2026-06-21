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
 * bootstrap.c - Kernel VM bootstrap + parity emit
 *
 * Gated by STARFORTH_ENABLE_VM to avoid touching stable hosted VM builds.
 * Initializes the VM using kernel host hooks, collects parity, and prints it.
 */

#ifdef __STARKERNEL__

#include "vm.h"
#include "log.h"
#include "block_subsystem.h"
#include "starkernel/vm/parity.h"
#include "starkernel/vm/bootstrap/sk_vm_bootstrap.h"
#include "starkernel/hal/hal.h"
#include "starkernel/console.h"
#include "starkernel/capsule_birth.h"
#include "starkernel/capsule_run.h"
#include "vm_internal.h"
#include "platform_time.h"
#include "test_runner/include/test_runner.h"
#include "word_source/include/mama_forth_words.h"

#if SK_PARITY_DEBUG
extern char __text_start[];
extern char __text_end[];
extern char __rodata_start[];
extern char __rodata_end[];
extern char __data_start[];
extern char __data_end[];
extern char __bss_start[];
extern char __bss_end[];

/**
 * @brief Print a 64-bit value as "0xNNNNNNNNNNNNNNNN" to the kernel console.
 *
 * Used by @c sk_bootstrap_debug_log_xt() when @c SK_PARITY_DEBUG is enabled
 * to dump raw pointer values during XT validation. Formats directly into a
 * 19-byte stack buffer; no libc dependency.
 *
 * Only compiled when @c SK_PARITY_DEBUG is non-zero.
 *
 * @param value 64-bit value to format as uppercase-nibble hex.
 */
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

/**
 * @brief Test whether a 64-bit address is a canonical x86-64 virtual address.
 *
 * Checks that bits [63:48] are all 0x0000 (user-space range) or all 0xFFFF
 * (kernel-space range). A non-canonical address indicates a corrupt function
 * pointer and triggers a @c sk_hal_panic() in the calling debug log function.
 *
 * Only compiled when @c SK_PARITY_DEBUG is non-zero.
 *
 * @param addr 64-bit address to test.
 * @return Non-zero if canonical, 0 if non-canonical.
 */
static int sk_bootstrap_xt_is_canonical(uint64_t addr) {
    uint64_t upper = addr >> 48;
    return (upper == 0x0000ULL) || (upper == 0xFFFFULL);
}

/**
 * @brief Return a short string naming the memory region containing @p addr.
 *
 * Classifies @p addr into:
 * - @c "null" — zero address
 * - @c "text" — within the kernel .text section [@c sk_hal_text_start(),
 *   @c sk_hal_text_end())
 * - @c "directmap" — ≥ @c 0xffff800000000000 (kernel direct-map range)
 * - @c "unknown" — anything else
 *
 * Used by @c sk_bootstrap_debug_log_xt() to label XTs for diagnostic output.
 * Rodata/data/bss are not checked because XTs should only reside in .text.
 * Uses @c sk_hal_text_start() / @c sk_hal_text_end() to avoid GOT-relative
 * accesses under @c -fPIC.
 *
 * Only compiled when @c SK_PARITY_DEBUG is non-zero.
 *
 * @param addr 64-bit address to classify.
 * @return Static string literal naming the region.
 */
static const char *sk_bootstrap_region_name(uint64_t addr) {
    if (addr == 0) return "null";
    /* Use HAL getters to avoid GOT indirection issues with -fPIC */
    uint64_t text_start = sk_hal_text_start();
    uint64_t text_end = sk_hal_text_end();
    if (addr >= text_start && addr < text_end) {
        return "text";
    }
    /* For rodata/data/bss, we rely on the HAL whitelist check instead,
     * since those symbols also have the same GOT issue. XTs should be
     * in text section anyway. */
    if (addr >= 0xffff800000000000ULL) {
        return "directmap";
    }
    return "unknown";
}

/**
 * @brief Log a dictionary entry's XT pointer and panic if it is invalid.
 *
 * Prints a @c "[SK_BOOTDBG] <label> entry=ADDR xt=ADDR region=NAME" line to
 * the kernel console. If the XT address is non-canonical or in the "unknown"
 * region, prints an additional error line and calls @c sk_hal_panic() —
 * never returns in that case.
 *
 * Marked @c __attribute__((unused)) because call sites are conditionally
 * compiled; the function exists for use during active debug sessions.
 * Only compiled when @c SK_PARITY_DEBUG is non-zero.
 *
 * @param label Short label string (e.g., the word name) for diagnostic output.
 * @param entry Dictionary entry whose XT pointer to validate.
 */
static void __attribute__((unused)) sk_bootstrap_debug_log_xt(const char *label, const DictEntry *entry) {
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

static VM sk_mama_vm;  /* Mama's VM — persists after bootstrap */

/* 1 MB POST block RAM — mirrors hosted blk_ram in main.c (BSS, no PE file bloat) */
static uint8_t sk_post_blk_ram[BLK_RAM_BLOCKS * BLK_FORTH_SIZE];

/**
 * @brief Return a pointer to the Mama VM instance.
 *
 * The Mama VM (@c sk_mama_vm) is a file-scope static that persists for the
 * lifetime of the kernel. This accessor is used by the capsule subsystem and
 * other kernel components that need to call back into the VM after bootstrap
 * without holding a direct reference to the static variable.
 *
 * Only compiled when @c STARFORTH_ENABLE_VM is non-zero.
 *
 * @return Pointer to @c sk_mama_vm cast to @c void*.
 */
void *sk_get_mama_vm(void) {
    return (void *)&sk_mama_vm;
}

/**
 * @brief Bootstrap the Mama VM, run POST, collect parity, and print it.
 *
 * This is the top-level M7 entry point called from @c kernel_main() after all
 * hardware milestones (M0–M6) are complete. Execution order:
 *
 * 1. @c sk_hal_init() + @c sf_time_init() — initialise HAL and time shim.
 * 2. @c vm_init_with_host() — initialise Mama VM using HAL host services.
 *    On failure, collects and prints the parity packet and returns -1.
 * 3. Capsule subsystem setup: @c capsule_vm_hooks_register(),
 *    @c capsule_vm_registry_init(), @c capsule_run_log_init().
 * 4. @c register_mama_forth_words() — BIRTH, KILL, START, STOP, USE + capsule words.
 * 5. @c vm_enable_interpreter() — required for POST @c vm_interpret() calls.
 * 6. @c blk_subsys_init() — POST block RAM (mirrors hosted main.c init order).
 * 7. @c run_all_tests() — full POST (936+ test cases).
 * 8. @c sk_parity_collect() + @c sk_parity_print() — emit M7.1a/M7.1b lines.
 * 9. @c sk_hal_freeze_exec_range() — lock the XT whitelist.
 *
 * The PARITY:OK / PARITY:FAIL verdict and the POST: PASSED / FAILED lines are
 * the normative acceptance signals visible in the QEMU serial log.
 *
 * Only compiled when @c STARFORTH_ENABLE_VM is non-zero.
 *
 * @param out Optional caller-supplied @c ParityPacket to receive the parity
 *            data. If @c NULL, a local @c ParityPacket is used (data is
 *            printed but not returned to the caller).
 * @return 0 if bootstrap and POST succeeded, -1 on any failure.
 */
int sk_vm_bootstrap_parity(ParityPacket *out) {
    VM *vm = &sk_mama_vm;
    ParityPacket local_pkt;
    ParityPacket *pkt = out ? out : &local_pkt;

    /* Reset parity packet */
    pkt->bootstrap_result = SK_BOOTSTRAP_INIT_FAIL;

    /* Initialize host services (allocator/time/console) */
    sk_hal_init();
    sf_time_init();

    vm_init_with_host(vm, sk_hal_host_services());
    if (vm->error) {
        console_println("VM: init failed");
        sk_parity_collect(vm, pkt);
        sk_parity_print(pkt);
        return -1;
    }

    /* Capsule subsystem: hooks, registry (Hera), run log.
     * Called exactly once here for Mama — child VMs go through
     * capsule_birth_baby() which never calls vm_init_with_host(). */
    capsule_vm_hooks_register();
    capsule_vm_registry_init(vm);  /* establishes [Hera] console prefix */
    capsule_run_log_init();
    register_mama_forth_words(vm); /* BIRTH KILL START STOP USE + capsule words */
#if SK_PARITY_DEBUG
    log_set_level(LOG_DEBUG);
#else
    /* Enable LOG_TEST level to see test output and count statistics */
    log_set_level(LOG_TEST);
#endif

    /* Enable interpreter - required for POST to execute vm_interpret() calls */
    vm_enable_interpreter(vm);

    /* Initialize block subsystem for POST — mirrors hosted main.c init order.
     * Without this, all block word tests fail with vm->error=1 (blk_is_valid→0). */
    blk_subsys_init(vm, sk_post_blk_ram, sizeof(sk_post_blk_ram));

    /* Run full POST (Power-On Self Test) */
    console_println("POST: Running comprehensive test suite...");
    run_all_tests(vm);

    /* Collect and print parity (includes test results) */
    sk_parity_collect(vm, pkt);
    sk_parity_print(pkt);

    /* Verify POST success: parity OK, no test failures, no test errors */
    if (pkt->bootstrap_result == SK_BOOTSTRAP_OK &&
        pkt->tests_failed == 0 &&
        pkt->tests_errors == 0) {
        console_println("POST: PASSED");
    } else {
        console_println("POST: FAILED");
    }
    sk_hal_freeze_exec_range();

    return (pkt->bootstrap_result == SK_BOOTSTRAP_OK) ? 0 : -1;
}

#endif /* STARFORTH_ENABLE_VM */

#endif /* __STARKERNEL__ */
