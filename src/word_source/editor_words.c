/*

                                 ***   StarForth   ***
  editor_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
 Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


*/

#include "include/editor_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------- Module state (must come before helpers) ---------- */
static int current_line   = 0;   /* Current line position (0-15) */
static int current_screen = 0;   /* Current screen number */
static char search_char   = 0;   /* Character being searched for */
static int  search_pos    = 0;   /* Position in search */
static char input_buffer[256];   /* Buffer for text input */

/* C99-safe bounded strlen (strnlen is not C99) */
static size_t bounded_strlen(const char *s, size_t maxlen) {
    size_t n = 0;
    while (n < maxlen && s[n] != '\0') n++;
    return n;
}

/* ---- Local block buffer helpers (self-contained; no cross-module deps) ---- */
static unsigned char *get_block_buffer(VM *vm, int screen) {
    if (screen < 0 || screen >= MAX_BLOCKS) { vm->error = 1; return NULL; }
    vaddr_t addr = (vaddr_t)(screen * BLOCK_SIZE);
    if (!vm_addr_ok(vm, addr, BLOCK_SIZE)) { vm->error = 1; return NULL; }
    return (unsigned char *)vm_ptr(vm, addr);
}

/* Mark buffer dirty by setting a bit in the last byte of the block.
   (Self-contained dirty marker; does not depend on other modules.) */
static void mark_buffer_dirty(VM *vm) {
    int blk = current_screen;
    if (vm_addr_ok(vm, vm->scr_addr, sizeof(cell_t))) {
        cell_t s = vm_load_cell(vm, vm->scr_addr);
        if (s >= 0 && s < MAX_BLOCKS) blk = (int)s;
    }
    vaddr_t base = (vaddr_t)(blk * BLOCK_SIZE);
    if (!vm_addr_ok(vm, base, BLOCK_SIZE)) { vm->error = 1; return; }
    {
        uint8_t *p = vm_ptr(vm, base);
        p[BLOCK_SIZE - 1] |= 0x01; /* set dirty bit */
    }
}

/* Helper: Get line pointer within a screen */
static char* get_screen_line(VM *vm, int screen, int line) {
    unsigned char *block_data = get_block_buffer(vm, screen);
    if (block_data == NULL) return NULL;
    return (char*)(block_data + (line * 64));  /* 64 chars per line */
}

/* Helper: Print a line from a screen */
static void print_line(VM *vm, int screen, int line) {
    char *line_data = get_screen_line(vm, screen, line);
    if (line_data == NULL) return;

    for (int i = 0; i < 64; i++) {
        putchar(line_data[i]);
    }
    putchar('\n');
}

/* Helper: Input a line with prompt */
static int get_text_input(const char *prompt, char *buf, size_t buflen) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)buflen, stdin)) return 0;

    /* Strip newline */
    {
        size_t len = strlen(buf);
        if (len && buf[len - 1] == '\n') buf[len - 1] = '\0';
    }
    return (int)strlen(buf);
}

/* Helper: Copy line data safely (pad with spaces to 64) */
static void copy_line_data(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < 64 && i < n; i++) {
        dest[i] = src[i];
    }
    for (; i < 64; i++) {
        dest[i] = ' ';
    }
}

/* ===================== EDITOR COMMANDS ===================== */

/* L ( -- ) List current screen */
void editor_word_l(VM *vm) {
    printf("Screen %d:\n", current_screen);
    for (int i = 0; i < 16; i++) {
        printf("%2d: ", i);
        print_line(vm, current_screen, i);
    }
}

/* N ( -- ) Next screen */
void editor_word_n(VM *vm) {
    if (current_screen + 1 >= MAX_BLOCKS) {
        log_message(LOG_WARN, "N: Already at last screen");
        return;
    }
    current_screen++;
    vm_store_cell(vm, vm->scr_addr, current_screen);
    editor_word_l(vm);
}

/* P ( -- ) Previous screen */
void editor_word_p(VM *vm) {
    if (current_screen - 1 < 0) {
        log_message(LOG_WARN, "P: Already at first screen");
        return;
    }
    current_screen--;
    vm_store_cell(vm, vm->scr_addr, current_screen);
    editor_word_l(vm);
}

/* R ( n -- ) Replace line n by prompting input */
void editor_word_r(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    {
        int line = (int)vm_pop(vm);
        if (line < 0 || line > 15) {
            log_message(LOG_ERROR, "E: Invalid line number %d", line);
            vm->error = 1;
            return;
        }
        current_line = line;

        /* Show current line content */
        printf("Line %d: ", line);
        print_line(vm, current_screen, line);

        /* Get replacement text */
        if (get_text_input("New text: ", input_buffer, sizeof(input_buffer))) {
            char *line_data = get_screen_line(vm, current_screen, line);
            if (line_data != NULL) {
                copy_line_data(line_data, input_buffer, strlen(input_buffer));
                mark_buffer_dirty(vm);
                printf("Line %d updated\n", line);
            }
        }
    }
}

/* S ( n addr -- ) Replace line n with string at addr */
void editor_word_s(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    {
        vaddr_t addr = VM_ADDR(vm_pop(vm));
        int line = (int)vm_pop(vm);
        if (line < 0 || line > 15) {
            log_message(LOG_ERROR, "S: Invalid line number %d", line);
            vm->error = 1;
            return;
        }

        if (!vm_addr_ok(vm, addr, 64)) { vm->error = 1; return; }
        {
            char *src  = (char*)vm_ptr(vm, addr);
            char *dest = get_screen_line(vm, current_screen, line);
            if (dest == NULL) return;

            size_t n = bounded_strlen(src, 64);
            copy_line_data(dest, src, n);
            mark_buffer_dirty(vm);
        }
    }
}

/* C ( n -- ) Clear line n */
void editor_word_c(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    {
        int line = (int)vm_pop(vm);
        if (line < 0 || line > 15) {
            log_message(LOG_ERROR, "C: Invalid line number %d", line);
            vm->error = 1;
            return;
        }

        {
            char *line_data = get_screen_line(vm, current_screen, line);
            if (line_data != NULL) {
                memset(line_data, ' ', 64);
                mark_buffer_dirty(vm);
            }
        }
    }
}

/* E ( n -- ) Edit line n (prompt) */
void editor_word_e(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    {
        int line = (int)vm_pop(vm);
        if (line < 0 || line > 15) {
            log_message(LOG_ERROR, "E: Invalid line number %d", line);
            vm->error = 1;
            return;
        }

        current_line = line;
        printf("Line %d: ", line);
        print_line(vm, current_screen, line);

        if (get_text_input("Edit: ", input_buffer, sizeof(input_buffer))) {
            char *line_data = get_screen_line(vm, current_screen, line);
            if (line_data != NULL) {
                copy_line_data(line_data, input_buffer, strlen(input_buffer));
                mark_buffer_dirty(vm);
            }
        }
    }
}

/* POS ( n -- ) Position to line n */
void editor_word_pos(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    {
        int line = (int)vm_pop(vm);
        if (line < 0 || line > 15) {
            log_message(LOG_ERROR, "P: Invalid line number %d", line);
            vm->error = 1;
            return;
        }

        current_line = line;
        printf("Positioned to line %d\n", line);
    }
}

/* COPY ( n1 n2 -- ) Copy line n1 to line n2 */
void editor_word_copy(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    {
        int n2 = (int)vm_pop(vm);
        int n1 = (int)vm_pop(vm);

        if (n1 < 0 || n1 > 15 || n2 < 0 || n2 > 15) {
            vm->error = 1;
            return;
        }

        {
            char *src = get_screen_line(vm, current_screen, n1);
            char *dst = get_screen_line(vm, current_screen, n2);
            if (src && dst) {
                memmove(dst, src, 64);
                mark_buffer_dirty(vm);
            }
        }
    }
}

/* FIND ( c -- ) Find character c starting from search_pos */
void editor_word_find(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    search_char = (char)(vm_pop(vm) & 0xFF);
    {
        int found = -1;
        unsigned char *block_data = get_block_buffer(vm, current_screen);
        if (vm->error || block_data == NULL) return;

        for (int i = search_pos; i < 16 * 64; i++) {
            if (block_data[i] == (unsigned char)search_char) {
                found = i;
                break;
            }
        }

        if (found >= 0) {
            search_pos = found + 1;
            {
                int line = found / 64;
                int col  = found % 64;
                printf("Found '%c' at line %d, col %d\n", search_char, line, col);
            }
        } else {
            printf("Character '%c' not found\n", search_char);
        }
    }
}

/* REP ( c -- ) Replace next occurrence of char with c */
void editor_word_rep(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    {
        char newc = (char)(vm_pop(vm) & 0xFF);
        unsigned char *block_data = get_block_buffer(vm, current_screen);
        if (vm->error || block_data == NULL) return;

        for (int i = search_pos; i < 16 * 64; i++) {
            if (block_data[i] == (unsigned char)search_char) {
                block_data[i] = (unsigned char)newc;
                mark_buffer_dirty(vm);
                search_pos = i + 1;
                printf("Replaced at pos %d\n", i);
                return;
            }
        }
        printf("No more '%c' found\n", search_char);
    }
}

/* INS ( n addr -- ) Insert string at line n (overwrite within line) */
void editor_word_ins(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    {
        vaddr_t addr = VM_ADDR(vm_pop(vm));
        int line = (int)vm_pop(vm);
        if (line < 0 || line > 15) { vm->error = 1; return; }

        if (!vm_addr_ok(vm, addr, 64)) { vm->error = 1; return; }
        {
            char *src  = (char*)vm_ptr(vm, addr);
            char *dest = get_screen_line(vm, current_screen, line);
            if (!dest) return;

            size_t n = bounded_strlen(src, 64);
            for (size_t i = 0; i < n; i++) {
                dest[i] = src[i];
            }
            mark_buffer_dirty(vm);
        }
    }
}

/* CLR ( -- ) Clear the entire screen buffer */
void editor_word_clr(VM *vm) {
    unsigned char *block_data = get_block_buffer(vm, current_screen);
    if (vm->error || block_data == NULL) return;

    memset(block_data, ' ', 16 * 64);
    mark_buffer_dirty(vm);
}

/* SETSCR ( n -- ) Set current screen number */
void editor_word_setscr(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    {
        int n = (int)vm_pop(vm);
        if (n < 0 || n >= MAX_BLOCKS) { vm->error = 1; return; }

        current_screen = n;
        vm_store_cell(vm, vm->scr_addr, current_screen);
    }
}

/* SHOW ( -- ) Show current line and screen */
void editor_word_show(VM *vm) {
    printf("Screen=%d Line=%d SearchPos=%d\n",
           current_screen, current_line, search_pos);
}

/* Register editor words */
void register_editor_words(VM *vm) {
    register_word(vm, "L",      editor_word_l);
    register_word(vm, "N",      editor_word_n);
    register_word(vm, "P",      editor_word_p);
    register_word(vm, "R",      editor_word_r);
    register_word(vm, "S",      editor_word_s);
    register_word(vm, "C",      editor_word_c);
    register_word(vm, "E",      editor_word_e);
    register_word(vm, "POS",    editor_word_pos);
    register_word(vm, "COPY",   editor_word_copy);
    register_word(vm, "FIND",   editor_word_find);
    register_word(vm, "REP",    editor_word_rep);
    register_word(vm, "INS",    editor_word_ins);
    register_word(vm, "CLR",    editor_word_clr);
    register_word(vm, "SETSCR", editor_word_setscr);
    register_word(vm, "SHOW",   editor_word_show);
}
