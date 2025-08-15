/*

                                 ***   StarForth   ***
  system_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/14/25, 7:38 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "include/system_words.h"
#include "../../include/vm.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ───────────────────────── Global system state ───────────────────────── */
static int system_running = 1;    /* System running flag */
static int forth_79_standard = 1; /* FORTH-79 standard compliance */

/* ─────────────────────────── Utilities/helpers ───────────────────────── */

/* Print a word’s name (no newline) */
static void print_word_name(const DictEntry *entry) {
    if (!entry) return;
    for (size_t i = 0; i < entry->name_len; i++) {
        putchar((unsigned char)entry->name[i]);
    }
}

/* Reset VM to a known state. If cold_start!=0, also rewinds HERE a bit. */
static void reset_vm_state(VM *vm, int cold_start) {
    if (!vm) return;

    /* Reset stacks */
    vm->dsp = -1;
    vm->rsp = -1;

    /* Clear error state & enter interpret mode */
    vm->error = 0;
    vm->mode  = MODE_INTERPRET;

    if (cold_start) {
        /* In a complete system, this would restore a boot image.
           For now we keep a minimal fence by not crushing the base dictionary. */
        if (vm->here > 1024) vm->here = 1024;
    }

    system_running = 1;
}

/* ───────────────────────────── Core words ───────────────────────────── */

/* EXIT ( -- )  — terminate the current colon definition immediately */
static void runtime_exit(VM *vm) {
    if (!vm) return;

    /* Discard the IP saved for the current step (if any) */
    if (vm->rsp >= 0) {
        (void)vm_rpop(vm);
    }

    /* Signal execute_colon_word() to unwind: empty the return stack */
    vm->rsp = -1;

    log_message(LOG_DEBUG, "EXIT: return from colon");
}

/* (  ( -- ) : Begin comment; skip to closing ) — IMMEDIATE */
static void forth_paren(VM *vm) {
    char word[64];
    int paren_depth = 1;

    while (paren_depth > 0 && vm_parse_word(vm, word, sizeof(word))) {
        if (strcmp(word, "(") == 0) {
            paren_depth++;
        } else if (strcmp(word, ")") == 0) {
            paren_depth--;
        }
        /* else: ignore */
    }

    if (paren_depth > 0) {
        log_message(LOG_WARN, "( Unterminated comment");
    }
}

/* COLD ( -- ) */
void system_word_cold(VM *vm) {
    puts("FORTH-79 Cold Start");
    reset_vm_state(vm, 1);
    puts("System initialized.");
}

/* WARM ( -- ) */
void system_word_warm(VM *vm) {
    puts("FORTH-79 Warm Start");
    reset_vm_state(vm, 0);
    puts("System restarted.");
}

/* BYE ( -- ) */
void system_word_bye(VM *vm) {
    puts("Goodbye!");
    vm_cleanup(vm);
    exit(0);
}

/* SAVE-SYSTEM ( -- ) : trivial snapshot of memory prefix */
void system_word_save_system(VM *vm) {
    FILE *save_file = fopen("forth_system.img", "wb");
    if (!save_file) {
        puts("Error: Cannot create system image file");
        vm->error = 1;
        return;
    }
    if (fwrite(&vm->here, sizeof(vm->here), 1, save_file) != 1) {
        puts("Error: Failed to save system state");
        fclose(save_file);
        vm->error = 1;
        return;
    }
    if (fwrite(vm->memory, 1, vm->here, save_file) != (size_t)vm->here) {
        puts("Error: Failed to save memory image");
        fclose(save_file);
        vm->error = 1;
        return;
    }
    fclose(save_file);
    puts("System image saved successfully");
}

/* WORDS ( -- ) : list words in current vocabulary */
void system_word_words(VM *vm) {
    DictEntry *e = vm->latest;
    int count = 0, per_line = 0;

    puts("Words in current vocabulary:");
    while (e) {
        print_word_name(e);
        putchar(' ');
        count++; per_line++;
        if (per_line >= 8) { putchar('\n'); per_line = 0; }
        e = e->link;
    }
    if (per_line) putchar('\n');
    printf("Total: %d words\n", count);
}

/* VLIST ( -- ) : detailed listing */
void system_word_vlist(VM *vm) {
    DictEntry *e = vm->latest;
    int count = 0;

    puts("Complete vocabulary listing:");
    puts("Name                 Address    Flags");
    puts("-------------------- ---------- -----");

    while (e) {
        /* Name padded to 20 chars */
        size_t pad = (e->name_len < 20) ? (20 - e->name_len) : 0;
        print_word_name(e);
        while (pad--) putchar(' ');
        printf(" %p %02X\n", (void*)e, e->flags);
        count++;
        e = e->link;
    }
    printf("Total: %d words\n", count);
}

/* PAGE ( -- ) */
void system_word_page(VM *vm) {
    (void)vm;
    printf("\033[2J\033[H");
    fflush(stdout);
}

/* NOP ( -- ) */
void system_word_nop(VM *vm) { (void)vm; }

/* 79-STANDARD ( -- flag ) : push -1 if compliance active else 0 */
void system_word_79_standard(VM *vm) {
    if (forth_79_standard) {
        puts("FORTH-79 Standard compliance: ACTIVE");
        puts("System conforms to FORTH-79 specification");
    } else {
        puts("FORTH-79 Standard compliance: INACTIVE");
        puts("System may have extensions or modifications");
    }
    vm_push(vm, forth_79_standard ? -1 : 0);
}

/* QUIT ( -- ) : FORTH-79 — clear return stack, return to interpreter. IMMEDIATE */
static void system_word_quit(VM *vm) {
    if (vm->mode == MODE_COMPILE) { vm->error = 1; return; }
    vm->rsp  = -1;
    vm->mode = MODE_INTERPRET;
    vm->error = 0;
}

/* ABORT ( -- ) : Clear both stacks & return to interpreter. NOT an error. */
static void system_word_abort(VM *vm) {
    if (!vm) return;
    reset_vm_state(vm, 0);
    vm->error = 0;
}

/* (ABORT") runtime
   Run-time stack:  flag addr len  --
   If flag ≠ 0: print message and perform ABORT semantics (no error). */
static void runtime_abortq(VM *vm) {
    if (!vm) return;
    if (vm->dsp < 2) { vm->error = 1; return; }

    cell_t len  = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    cell_t flag = vm_pop(vm);

    if (!flag) return; /* false => no-op */

    if (!vm_addr_ok(vm, (vaddr_t)addr, (size_t)len)) { vm->error = 1; return; }
    fwrite(vm->memory + (size_t)addr, 1, (size_t)len, stdout);
    fputc('\n', stdout);

    reset_vm_state(vm, 0);   /* control transfer, not a fault */
    vm->error = 0;
}

/* ABORT"  ( flag -- )   IMMEDIATE
   Interpret: parse up to next " ; if flag≠0 print string and ABORT (no error).
   Compile:   parse string; compile: LIT addr  LIT len  (ABORT") so at run-time
              the stack is: flag addr len and runtime_abortq handles it. */
static void system_word_abort_quote(VM *vm) {
    if (!vm) return;

    /* Parse message up to closing quote from current input position. */
    size_t pos = vm->input_pos, n = vm->input_length;
    while (pos < n && (vm->input_buffer[pos] == ' ' || vm->input_buffer[pos] == '\t')) pos++;
    size_t start = pos;
    while (pos < n && vm->input_buffer[pos] != '"') pos++;
    if (pos >= n) { vm->error = 1; return; }  /* unmatched quote */
    size_t end = pos;
    vm->input_pos = pos + 1;  /* consume the closing quote */

    const size_t msg_len = end - start;

    if (vm->mode == MODE_INTERPRET) {
        if (vm->dsp < 0) { vm->error = 1; return; }
        cell_t flag = vm_pop(vm);
        if (!flag) return;

        if (msg_len) fwrite(&vm->input_buffer[start], 1, msg_len, stdout);
        fputc('\n', stdout);

        reset_vm_state(vm, 0);
        vm->error = 0;
        return;
    }

    /* MODE_COMPILE */
    void *dst = NULL;
    if (msg_len) {
        dst = vm_allot(vm, msg_len);
        if (!dst) { vm->error = 1; return; }
        memcpy(dst, &vm->input_buffer[start], msg_len);
    }
    vaddr_t msg_addr = (vaddr_t)0;
    if (msg_len) msg_addr = (vaddr_t)((uint8_t*)dst - vm->memory);

    vm_compile_literal(vm, (cell_t)msg_addr);
    vm_compile_literal(vm, (cell_t)msg_len);

    /* Compile call to the runtime helper (must be registered) */
    vm_compile_call(vm, (word_func_t)runtime_abortq);
}

/* ───────────────────────────── API helpers ─────────────────────────── */
int system_is_running(void) { return system_running; }
void set_forth_79_compliance(int enabled) { forth_79_standard = enabled; }

/* ──────────────────── System Word Registration ─────────────────────── */
void register_system_words(VM *vm) {
    log_message(LOG_INFO, "Registering system & environment words...");

    /* EXIT first — needed by colon definitions */
    register_word(vm, "EXIT", runtime_exit);

    /* Comments */
    register_word(vm, "(", forth_paren);
    vm_make_immediate(vm);  /* '(' is IMMEDIATE */

    /* Environment/system */
    register_word(vm, "COLD", system_word_cold);
    register_word(vm, "WARM", system_word_warm);
    register_word(vm, "BYE", system_word_bye);
    register_word(vm, "SAVE-SYSTEM", system_word_save_system);
    register_word(vm, "WORDS", system_word_words);
    register_word(vm, "VLIST", system_word_vlist);
    register_word(vm, "PAGE", system_word_page);
    register_word(vm, "NOP", system_word_nop);
    register_word(vm, "79-STANDARD", system_word_79_standard);

    /* Control transfer */
    register_word(vm, "QUIT",   system_word_quit);
    vm_make_immediate(vm);  /* forbid inside defs */
    register_word(vm, "ABORT",  system_word_abort);

    /* Internal helper used by ABORT" compiled form */
    register_word(vm, "(ABORT\")", runtime_abortq);

    /* Immediate */
    register_word(vm, "ABORT\"", system_word_abort_quote);
    vm_make_immediate(vm);  /* ABORT" is IMMEDIATE */

    log_message(LOG_INFO, "System words registered.");
}
