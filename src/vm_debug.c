/*

                                 ***   StarForth   ***
  vm_debug.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/14/25, 9:02 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include <signal.h>
#include "../include/vm.h"
#include "../include/vm_debug.h"
#include <stdio.h>
#include <string.h>

static VM *g_vm = NULL;

void vm_debug_set_current_vm(VM *vm) { g_vm = vm; }

/* Safe BASE read (never touches vm->error). Adjust if you have a helper. */
static unsigned dbg_get_base(const VM *vm) {
    if (!vm) return 10u;
    size_t idx = (size_t) vm->base_addr;
    if (idx < (size_t) VM_MEMORY_SIZE) {
        cell_t v = vm->memory[idx];
        if (v >= 2 && v <= 36) return (unsigned) v;
    }
    if (vm->base >= 2 && vm->base <= 36) return (unsigned) vm->base;
    return 10u;
}

static const char *mode_str(int m) {
    switch (m) {
        case MODE_INTERPRET: return "INTERPRET";
        case MODE_COMPILE: return "COMPILE";
        default: return "UNKNOWN";
    }
}

static void dump_block_preview(const VM *vm, cell_t scr) {
    if (!vm) return;
    if (scr < 1 || scr >= (cell_t) MAX_BLOCKS) return;
    vaddr_t addr = (vaddr_t)((unsigned long long) scr * (unsigned long long) BLOCK_SIZE);
    if (!vm_addr_ok((VM *) vm, addr, BLOCK_SIZE)) return;
    const unsigned char *p = vm_ptr((VM *) vm, addr);
    char line[81];
    size_t n = 0;
    while (n < 80 && n < BLOCK_SIZE) {
        unsigned char ch = p[n];
        if (ch == 0 || ch == '\n' || ch == '\r') break;
        line[n++] = (ch < 32 || ch > 126) ? ' ' : (char) ch;
    }
    line[n] = '\0';
    fprintf(stderr, "  block[%ld]: \"%s\"%s\n", (long) scr, line, (n == 80 ? "..." : ""));
}

void vm_debug_dump_state(const VM *vm, const char *reason) {
    if (!vm) {
        fprintf(stderr, "===== VM STATE DUMP (no VM) [%s] =====\n", reason ? reason : "");
        return;
    }

    unsigned base = dbg_get_base(vm);
    cell_t scr = 0;
    if ((size_t) vm->scr_addr < (size_t) VM_MEMORY_SIZE) scr = vm->memory[(size_t) vm->scr_addr];

    fprintf(stderr, "\n===== VM STATE DUMP ===== [%s]\n", reason ? reason : "");
    fprintf(stderr, "  mode=%s  error=%d\n", mode_str(vm->mode), vm->error);
    fprintf(stderr, "  HERE=%ld  BASE=%u  SCR=%ld\n", (long) vm->here, base, (long) scr);
    fprintf(stderr, "  dsp=%d  rsp=%d\n", vm->dsp, vm->rsp);

    /* If your VM struct exposes stacks, show top few items (guarded). */
#ifdef DATA_STACK_SIZE
    if (vm->stack) {
        fprintf(stderr, "  DATA(top=%d):", vm->dsp);
        int shown = 0;
        for (int i = vm->dsp; i >= 0 && shown < 16; --i, ++shown)
            fprintf(
                stderr, " [%d]=%ld", i, (long) vm->stack[i]);
        if (vm->dsp >= 16) fprintf(stderr, " ...");
        fprintf(stderr, "\n");
    }
#endif
#ifdef RETURN_STACK_SIZE
    if (vm->rstack) {
        fprintf(stderr, "  RETURN(top=%d):", vm->rsp);
        int shown = 0;
        for (int i = vm->rsp; i >= 0 && shown < 16; --i, ++shown)
            fprintf(
                stderr, " [%d]=%ld", i, (long) vm->rstack[i]);
        if (vm->rsp >= 16) fprintf(stderr, " ...");
        fprintf(stderr, "\n");
    }
#endif

    /* If you track current word/ip, print them (guard with your flags/macros). */
#ifdef VM_HAS_CURRENT_ENTRY
    if (vm->current_executing_entry) fprintf(stderr, "  current=%s\n", vm->current_executing_entry->name);
#endif
#ifdef VM_HAS_IP
    fprintf(stderr, "  ip=%ld\n", (long) vm->ip);
#endif

    dump_block_preview(vm, scr);
    fprintf(stderr, "===== END VM STATE DUMP =====\n");
}

static void sig_handler(int sig) {
    const char *name = (sig == SIGSEGV) ? "SIGSEGV" : (sig == SIGABRT) ? "SIGABRT" : "SIGNAL";
    fprintf(stderr, "\n*** Caught %s ***\n", name);
    if (g_vm) vm_debug_dump_state(g_vm, name);
    signal(sig, SIG_DFL);
    raise(sig);
}

void vm_debug_install_signal_handlers(void) {
#if defined(__unix__) || defined(__APPLE__)
    /* Use sigaction when available */
#ifdef SA_RESETHAND
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND; /* one-shot: reset to default after firing */
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
#else
    /* Fallback: plain signal() if SA_RESETHAND isn’t defined */
    signal(SIGSEGV, sig_handler);
    signal(SIGABRT, sig_handler);
#endif
#else
    /* Non-POSIX platforms: best-effort */
    signal(SIGSEGV, sig_handler);
    signal(SIGABRT, sig_handler);
#endif
}