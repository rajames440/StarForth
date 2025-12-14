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

#include "include/editor_words.h"        /* register_editor_words prototype */
#include "include/block_words.h"         /* mark_buffer_dirty(), MAX_BLOCKS, BLOCK_SIZE */
#include "../../include/vm.h"            /* VM, vm_load_cell, vm_store_cell, vm_addr_ok, vm_ptr */
#include "../../include/word_registry.h" /* register_word */
#include "../../include/log.h"           /* log_message */
#include <stdio.h>                      /* fputs, fflush, puts, printf */
#include <string.h>                     /* memset, memcpy, strlen, isdigit, isspace */
#include <ctype.h>                      /* isdigit, isspace */

/* -------- helpers -------- */

/** @brief Get current screen number from VM
 *  @param vm Pointer to VM instance
 *  @return Current screen number
 */
static inline cell_t current_scr(VM *vm) {
    return vm_load_cell(vm, vm->scr_addr);
}

/** @brief Set current screen number in VM
 *  @param vm Pointer to VM instance
 *  @param blk Screen number to set
 */
static inline void set_scr(VM *vm, cell_t blk) {
    vm_store_cell(vm, vm->scr_addr, blk);
}

/** @brief Get pointer to specific line in a screen
 *  @param vm Pointer to VM instance
 *  @param scr Screen number (1-based)
 *  @param line Line number (0-15)
 *  @param out Output parameter for line pointer
 *  @return 1 if successful, 0 if parameters invalid
 */
static int line_ptr(VM *vm, cell_t scr, cell_t line, unsigned char **out) {
    if (!vm || !out) return 0;
    if (scr < 1 || scr >= (cell_t) MAX_BLOCKS) return 0;
    if (line < 0 || line > 15) return 0;

    /* Use block subsystem to get block buffer */
    extern uint8_t *blk_get_buffer(uint32_t block_num, int writable);
    extern int blk_is_valid(uint32_t block_num);

    if (!blk_is_valid((uint32_t) scr)) return 0;

    uint8_t *block = blk_get_buffer((uint32_t) scr, 1); /* writable for editing */
    if (!block) return 0;

    *out = block + (line * 64);
    return 1;
}

/** @brief Print 64-character line with character filtering
 *  @param p Pointer to 64-byte line buffer
 */
static void print_line_64(const unsigned char *p) {
#ifdef STARFORTH_ANSI
    /* No cursor moves unless asked; just render content cleanly. */
#endif
    for (int i = 0; i < 64; ++i) {
        unsigned char c = p[i];
        if (c == 0) { putchar(' '); } else if (c >= 32 && c <= 126) { putchar((int) c); } else {
            putchar('.');
        }
    }
    putchar('\n');
}

/* -------- words -------- */

/* L ( u -- ) : print line u of current SCR (0..15). Errors on empty/range/invalid SCR. */
static void editor_word_l(VM *vm) {
    if (!vm) return;

    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "L: stack underflow (needs line# 0..15)");
        return;
    }
    cell_t line = vm_pop(vm);

    if (line < 0 || line > 15) {
        vm->error = 1;
        log_message(LOG_ERROR, "L: line out of range (0..15)");
        return;
    }

    cell_t scr = current_scr(vm);
    if (scr < 1 || scr >= (cell_t) MAX_BLOCKS) {
        vm->error = 1;
        log_message(LOG_ERROR, "L: SCR out of range (SCR=%ld)", (long) scr);
        return;
    }

    unsigned char *lp = NULL;
    if (!line_ptr(vm, scr, line, &lp)) {
        vm->error = 1;
        log_message(LOG_ERROR, "L: address invalid for SCR=%ld line=%ld", (long) scr, (long) line);
        return;
    }

    print_line_64(lp);
}

/* S ( c-addr len u -- ) : set line u from buffer (pads with spaces to 64, truncates if longer). */
static void editor_word_s(VM *vm) {
    if (!vm) return;

    /* Pop in the order that lets us validate line range before address checks (useful in tests). */
    if (vm->dsp < 2) {
        vm->error = 1;
        log_message(LOG_ERROR, "S: stack underflow (needs c-addr len line#)");
        return;
    }
    cell_t line = vm_pop(vm);
    if (line < 0 || line > 15) {
        vm->error = 1;
        log_message(LOG_ERROR, "S: line out of range (0..15)");
        /* Drain remaining args to keep stack sane */
        (void) vm_pop(vm);
        (void) vm_pop(vm);
        return;
    }
    cell_t len = vm_pop(vm);
    cell_t addr = vm_pop(vm);

    if (len < 0) len = 0; /* defensive: negative length treated as 0 */

    cell_t scr = current_scr(vm);
    if (scr < 1 || scr >= (cell_t) MAX_BLOCKS) {
        vm->error = 1;
        log_message(LOG_ERROR, "S: SCR out of range (SCR=%ld)", (long) scr);
        return;
    }

    unsigned char *dst = NULL;
    if (!line_ptr(vm, scr, line, &dst)) {
        vm->error = 1;
        log_message(LOG_ERROR, "S: destination address invalid");
        return;
    }

    /* Source pointer */
    if (!vm_addr_ok(vm, (vaddr_t) addr, (size_t) len)) {
        vm->error = 1;
        log_message(LOG_ERROR, "S: source range invalid (addr=%ld len=%ld)",
                    (long) addr, (long) len);
        return;
    }
    const unsigned char *src = (const unsigned char *) vm_ptr(vm, (vaddr_t) addr);

    /* Write: pad spaces, then copy up to 64 */
    memset(dst, ' ', 64);
    size_t n = (len > 64) ? 64 : (size_t) len;
    if (n > 0) memcpy(dst, src, n);

    mark_buffer_dirty(vm);
}

/* SHOW ( -- ) : print the whole 16x64 screen with simple line numbers. */
static void editor_word_show(VM *vm) {
    if (!vm) return;

    cell_t scr = current_scr(vm);
    if (scr < 1 || scr >= (cell_t) MAX_BLOCKS) {
        vm->error = 1;
        log_message(LOG_ERROR, "SHOW: SCR out of range (SCR=%ld)", (long) scr);
        return;
    }

#ifdef STARFORTH_ANSI
    /* Clear + home (optional; compile-time opt-in) */
    fputs("\x1b[2J\x1b[H", stdout);
    printf("\x1b[1mScreen %ld\x1b[0m:\n", (long) scr);
#else
    printf("Screen %ld:\n", (long) scr);
#endif

    for (cell_t line = 0; line < 16; ++line) {
        unsigned char *lp = NULL;
        if (!line_ptr(vm, scr, line, &lp)) {
            vm->error = 1;
            return;
        }
#ifdef STARFORTH_ANSI
        printf("\x1b[90m%2ld:\x1b[0m ", (long) line);
#else
        printf("%2ld: ", (long) line);
#endif
        print_line_64(lp);
    }
}

/* EDIT ( u -- ) : tiny line-editor shell (stdin/stdout). No raw mode, no curses. */
/* EDIT ( u -- ) : tiny line-editor shell (stdin/stdout). No raw mode, no curses. */
static void editor_word_edit(VM *vm) {
    if (!vm) return;

    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "EDIT: stack underflow (needs block#)");
        return;
    }
    cell_t blk = vm_pop(vm);
    if (blk < 1 || blk >= (cell_t) MAX_BLOCKS) {
        vm->error = 1;
        log_message(LOG_ERROR, "EDIT: block out of range");
        return;
    }
    vm_store_cell(vm, vm->scr_addr, blk);
    editor_word_show(vm);
    if (vm->error) return;

    char buf[1024];
    for (;;) {
        printf("SCR %ld> ", (long) vm_load_cell(vm, vm->scr_addr));
        fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) break;

        /* Trim trailing newline */
        size_t L = strlen(buf);
        if (L && buf[L - 1] == '\n') buf[L - 1] = '\0';

        if (buf[0] == 'q' || buf[0] == 'Q') break;
        else if (buf[0] == 'h' || buf[0] == 'H') {
            puts("Commands: p=print, l <n>=line, s <n> <text>=set, n=next, b=prev, w=write, q=quit");
        } else if (buf[0] == 'p' || buf[0] == 'P') {
            editor_word_show(vm);
        } else if (buf[0] == 'l' || buf[0] == 'L') {
            long ln = -1;
            /* Simple number parsing to avoid sscanf */
            const char *p = buf + 1;
            while (*p == ' ' || *p == '\t') p++;
            if (isdigit(*p)) {
                ln = 0;
                while (isdigit(*p)) {
                    ln = ln * 10 + (*p - '0');
                    p++;
                }
                vm_push(vm, (cell_t) ln);
                editor_word_l(vm);
            } else {
                puts("usage: l <0..15>");
            }
        } else if (buf[0] == 's' || buf[0] == 'S') {
            long ln = -1;
            const char *p = buf + 1;
            while (*p && isspace((unsigned char) *p)) ++p;
            /* Parse number manually */
            if (isdigit(*p)) {
                ln = 0;
                while (isdigit(*p)) {
                    ln = ln * 10 + (*p - '0');
                    p++;
                }
                /* Move past any remaining spaces */
                while (*p && isspace((unsigned char) *p)) ++p;
                /* p is now the text to write */
                size_t tlen = strlen(p);

                cell_t scr = vm_load_cell(vm, vm->scr_addr);
                unsigned char *dst = NULL;
                if (ln < 0 || ln > 15 || scr < 1 || scr >= (cell_t) MAX_BLOCKS ||
                    !line_ptr(vm, scr, (cell_t) ln, &dst)) {
                    puts("error: bad SCR/line");
                } else {
                    memset(dst, ' ', 64);
                    size_t n = (tlen > 64) ? 64 : tlen;
                    if (n) memcpy(dst, p, n);
                    /* ✅ Use helpers declared in block_words.h (no implicit decl) */
                    mark_buffer_dirty(vm);
                    puts("ok");
                }
            } else {
                puts("usage: s <0..15> <text>");
            }
        } else if (buf[0] == 'n' || buf[0] == 'N') {
            cell_t s = vm_load_cell(vm, vm->scr_addr);
            if (s + 1 < (cell_t) MAX_BLOCKS) vm_store_cell(vm, vm->scr_addr, s + 1);
            editor_word_show(vm);
        } else if (buf[0] == 'b' || buf[0] == 'B') {
            cell_t s = vm_load_cell(vm, vm->scr_addr);
            if (s - 1 >= 1) vm_store_cell(vm, vm->scr_addr, s - 1);
            editor_word_show(vm);
        } else if (buf[0] == 'w' || buf[0] == 'W') {
            /* ✅ No more implicit decls: use helpers */
            mark_buffer_dirty(vm);
            save_all_buffers(vm);
            puts("saved");
        } else {
            puts("h for help");
        }

        if (vm->error) {
            puts("error");
            vm->error = 0; /* keep shell alive */
        }
    }
}


/* -------- registration -------- */

void register_editor_words(VM *vm) {
    if (!vm) return;
    register_word(vm, "L", editor_word_l); /* non-79 */
    register_word(vm, "S", editor_word_s); /* non-79 */
    register_word(vm, "SHOW", editor_word_show); /* non-79 */
    register_word(vm, "EDIT", editor_word_edit); /* non-79 */
}