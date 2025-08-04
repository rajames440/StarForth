/*
 * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
 *
 * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 * To the extent possible under law, the author(s) have dedicated all copyright and related
 * and neighboring rights to this software to the public domain worldwide.
 * This software is distributed without any warranty.
 *
 * See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
 */

/* editor_words.c - FORTH-79 Line Editor Words */
#include "include/editor_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../word_source/include/block_words.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Editor state */
static int current_line = 0;        /* Current line position (0-15) */
static int current_screen = 0;      /* Current screen number */
static char search_char = 0;        /* Character being searched for */
static int search_pos = 0;          /* Position in search */
static char input_buffer[256];      /* Buffer for text input */

/* Helper: Get line from current screen */
static char* get_screen_line(VM *vm, int screen, int line) {
    unsigned char *block_data = get_block_buffer(vm, screen);
    if (block_data == NULL) return NULL;

    return (char*)(block_data + (line * 64));  /* 64 chars per line */
}

/* Helper: Print a single line with line number */
static void print_line(VM *vm, int screen, int line) {
    char *line_data = get_screen_line(vm, screen, line);
    if (line_data == NULL) return;

    printf("%2d: ", line);
    for (int i = 0; i < 64; i++) {
        char c = line_data[i];
        if (c >= 32 && c <= 126) {
            printf("%c", c);
        } else if (c == 0) {
            printf(" ");
        } else {
            printf(".");
        }
    }
    printf("\n");
}

/* Helper: Get text input from user */
static int get_text_input(const char *prompt, char *buffer, int max_len) {
    printf("%s", prompt);
    fflush(stdout);

    if (fgets(buffer, max_len, stdin) == NULL) {
        return 0;
    }

    /* Remove trailing newline */
    int len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
        len--;
    }

    return len;
}

/* Helper: Copy line data safely */
static void copy_line_data(char *dest, const char *src, int max_len) {
    int i;
    for (i = 0; i < max_len && i < 64; i++) {
        dest[i] = src[i];
    }
    /* Pad with spaces to 64 characters */
    for (; i < 64; i++) {
        dest[i] = ' ';
    }
}

/* Helper: Shift lines up (for insert) */
static void shift_lines_up(VM *vm, int screen, int start_line) {
    unsigned char *block_data = get_block_buffer(vm, screen);
    if (block_data == NULL) return;

    /* Move lines 14 down to start_line+1 up by one position */
    for (int line = 14; line >= start_line; line--) {
        memcpy(block_data + ((line + 1) * 64),
               block_data + (line * 64), 64);
    }

    /* Clear the inserted line */
    memset(block_data + (start_line * 64), ' ', 64);
}

/* Helper: Shift lines down (for delete) */
static void shift_lines_down(VM *vm, int screen, int start_line) {
    unsigned char *block_data = get_block_buffer(vm, screen);
    if (block_data == NULL) return;

    /* Move lines start_line+1 to 15 down by one position */
    for (int line = start_line; line < 15; line++) {
        memcpy(block_data + (line * 64),
               block_data + ((line + 1) * 64), 64);
    }

    /* Clear the last line */
    memset(block_data + (15 * 64), ' ', 64);
}

/* L ( -- ) List current screen */
void word_l(VM *vm) {
    printf("Screen %d:\n", current_screen);
    printf("================\n");

    for (int line = 0; line < 16; line++) {
        if (line == current_line) {
            printf(">");  /* Mark current line */
        } else {
            printf(" ");
        }
        print_line(vm, current_screen, line);
    }
}

/* T ( n -- ) Type line n of current screen */
void word_t(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    int line = (int)vm_pop(vm);
    if (line < 0 || line > 15) {
        log_message(LOG_ERROR, "T: Invalid line number %d", line);
        vm->error = 1;
        return;
    }

    print_line(vm, current_screen, line);
}

/* E ( n -- ) Edit line n of current screen */
void word_e(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

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

/* S ( n addr -- ) Replace line n with string at addr */
void word_s(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t addr = vm_pop(vm);
    int line = (int)vm_pop(vm);

    if (line < 0 || line > 15) {
        log_message(LOG_ERROR, "S: Invalid line number %d", line);
        vm->error = 1;
        return;
    }

    char *src_str = (char*)(uintptr_t)addr;
    if (src_str == NULL) {
        vm->error = 1;
        return;
    }

    /* Get string length (assume counted string) */
    int str_len = (unsigned char)src_str[0];
    char *str_data = src_str + 1;

    char *line_data = get_screen_line(vm, current_screen, line);
    if (line_data != NULL) {
        copy_line_data(line_data, str_data, str_len);
        mark_buffer_dirty(vm);
        printf("Line %d replaced\n", line);
    }
}

/* I ( n -- ) Insert before line n */
void word_i(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    int line = (int)vm_pop(vm);
    if (line < 0 || line > 15) {
        log_message(LOG_ERROR, "I: Invalid line number %d", line);
        vm->error = 1;
        return;
    }

    if (line < 15) {  /* Can't insert after last line */
        shift_lines_up(vm, current_screen, line);
    }

    current_line = line;

    /* Get text for new line */
    if (get_text_input("Insert text: ", input_buffer, sizeof(input_buffer))) {
        char *line_data = get_screen_line(vm, current_screen, line);
        if (line_data != NULL) {
            copy_line_data(line_data, input_buffer, strlen(input_buffer));
            mark_buffer_dirty(vm);
            printf("Line inserted at %d\n", line);
        }
    }
}

/* R ( n -- ) Replace line n */
void word_r(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    int line = (int)vm_pop(vm);
    if (line < 0 || line > 15) {
        log_message(LOG_ERROR, "R: Invalid line number %d", line);
        vm->error = 1;
        return;
    }

    current_line = line;

    /* Get replacement text */
    if (get_text_input("Replace with: ", input_buffer, sizeof(input_buffer))) {
        char *line_data = get_screen_line(vm, current_screen, line);
        if (line_data != NULL) {
            copy_line_data(line_data, input_buffer, strlen(input_buffer));
            mark_buffer_dirty(vm);
            printf("Line %d replaced\n", line);
        }
    }
}

/* D ( n -- ) Delete line n */
void word_d(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    int line = (int)vm_pop(vm);
    if (line < 0 || line > 15) {
        log_message(LOG_ERROR, "D: Invalid line number %d", line);
        vm->error = 1;
        return;
    }

    shift_lines_down(vm, current_screen, line);
    mark_buffer_dirty(vm);

    printf("Line %d deleted\n", line);
}

/* P ( n -- ) Position to line n */
void word_p(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    int line = (int)vm_pop(vm);
    if (line < 0 || line > 15) {
        log_message(LOG_ERROR, "P: Invalid line number %d", line);
        vm->error = 1;
        return;
    }

    current_line = line;
    printf("Positioned to line %d\n", line);
}

/* COPY ( n1 n2 -- ) Copy line n1 to line n2 */
void word_copy(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    int dest_line = (int)vm_pop(vm);
    int src_line = (int)vm_pop(vm);

    if (src_line < 0 || src_line > 15 || dest_line < 0 || dest_line > 15) {
        log_message(LOG_ERROR, "COPY: Invalid line numbers %d %d", src_line, dest_line);
        vm->error = 1;
        return;
    }

    char *src_data = get_screen_line(vm, current_screen, src_line);
    char *dest_data = get_screen_line(vm, current_screen, dest_line);

    if (src_data != NULL && dest_data != NULL) {
        memcpy(dest_data, src_data, 64);
        mark_buffer_dirty(vm);
        printf("Line %d copied to line %d\n", src_line, dest_line);
    }
}

/* M ( n1 n2 -- ) Move line n1 to line n2 */
void word_m(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    int dest_line = (int)vm_pop(vm);
    int src_line = (int)vm_pop(vm);

    if (src_line < 0 || src_line > 15 || dest_line < 0 || dest_line > 15) {
        log_message(LOG_ERROR, "M: Invalid line numbers %d %d", src_line, dest_line);
        vm->error = 1;
        return;
    }

    if (src_line == dest_line) {
        return;  /* No-op */
    }

    /* Save source line */
    char temp_line[64];
    char *src_data = get_screen_line(vm, current_screen, src_line);
    if (src_data == NULL) return;

    memcpy(temp_line, src_data, 64);

    /* Delete source line */
    shift_lines_down(vm, current_screen, src_line);

    /* Adjust destination if it's after the deleted line */
    if (dest_line > src_line) {
        dest_line--;
    }

    /* Insert at destination */
    if (dest_line < 15) {
        shift_lines_up(vm, current_screen, dest_line);
    }

    char *dest_data = get_screen_line(vm, current_screen, dest_line);
    if (dest_data != NULL) {
        memcpy(dest_data, temp_line, 64);
        mark_buffer_dirty(vm);
        printf("Line %d moved to line %d\n", src_line, dest_line);
    }
}

/* TILL ( c -- ) Search for character c */
void word_till(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    search_char = (char)(vm_pop(vm) & 0xFF);
    search_pos = 0;  /* Reset search position */

    unsigned char *block_data = get_block_buffer(vm, current_screen);
    if (block_data == NULL) return;

    /* Search from current position */
    for (int line = current_line; line < 16; line++) {
        for (int col = (line == current_line ? search_pos : 0); col < 64; col++) {
            if (block_data[line * 64 + col] == search_char) {
                current_line = line;
                search_pos = col + 1;  /* Next search starts after this */
                printf("Found '%c' at line %d, column %d\n", search_char, line, col);
                return;
            }
        }
    }

    printf("Character '%c' not found\n", search_char);
}

/* N ( -- ) Search for next occurrence */
void word_n(VM *vm) {
    if (search_char == 0) {
        printf("No search character set - use TILL first\n");
        return;
    }

    unsigned char *block_data = get_block_buffer(vm, current_screen);
    if (block_data == NULL) return;

    /* Continue search from current position */
    for (int line = current_line; line < 16; line++) {
        for (int col = (line == current_line ? search_pos : 0); col < 64; col++) {
            if (block_data[line * 64 + col] == search_char) {
                current_line = line;
                search_pos = col + 1;
                printf("Found '%c' at line %d, column %d\n", search_char, line, col);
                return;
            }
        }
    }

    /* Wrap to beginning of screen */
    for (int line = 0; line <= current_line; line++) {
        int max_col = (line == current_line) ? search_pos - 1 : 64;
        for (int col = 0; col < max_col; col++) {
            if (block_data[line * 64 + col] == search_char) {
                current_line = line;
                search_pos = col + 1;
                printf("Found '%c' at line %d, column %d (wrapped)\n", search_char, line, col);
                return;
            }
        }
    }

    printf("No more occurrences of '%c' found\n", search_char);
}

/* WHERE ( -- ) Show current position */
void word_where(VM *vm) {
    printf("Screen %d, Line %d\n", current_screen, current_line);
}

/* TOP ( -- ) Go to top of screen */
void word_top(VM *vm) {
    current_line = 0;
    printf("At top of screen\n");
}

/* BOTTOM ( -- ) Go to bottom of screen */
void word_bottom(VM *vm) {
    current_line = 15;
    printf("At bottom of screen\n");
}

/* CLEAR ( -- ) Clear current screen */
void word_clear(VM *vm) {
    unsigned char *block_data = get_block_buffer(vm, current_screen);
    if (block_data == NULL) return;

    memset(block_data, ' ', 1024);  /* Fill with spaces */
    mark_buffer_dirty(vm);

    printf("Screen %d cleared\n", current_screen);
}

void register_editor_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 editor words...");

    /* Complete editor implementation */
    register_word(vm, "L", word_l);
    register_word(vm, "T", word_t);
    register_word(vm, "E", word_e);
    register_word(vm, "S", word_s);
    register_word(vm, "I", word_i);
    register_word(vm, "R", word_r);
    register_word(vm, "D", word_d);
    register_word(vm, "P", word_p);
    register_word(vm, "COPY", word_copy);
    register_word(vm, "M", word_m);
    register_word(vm, "TILL", word_till);
    register_word(vm, "N", word_n);
    register_word(vm, "WHERE", word_where);
    register_word(vm, "TOP", word_top);
    register_word(vm, "BOTTOM", word_bottom);
    register_word(vm, "CLEAR", word_clear);

    log_message(LOG_INFO, "Complete FORTH-79 editor registered - 16 words");
}