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

/* format_words.c - FORTH-79 Formatting & Conversion Words */
#include "include/format_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <stdio.h>
#include <string.h>

/* Global formatting state */
static cell_t base_value = 10;          /* Number base (10, 16, 8, etc.) */
static char conversion_buffer[64];      /* Buffer for numeric conversion */
static int conversion_pos = 0;          /* Current position in conversion buffer */

/* BASE ( -- addr )  Variable containing number base */
void word_base(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)&base_value);
}

/* DECIMAL ( -- )  Set base to 10 */
void word_decimal(VM *vm) {
    base_value = 10;
}

/* HEX ( -- )  Set base to 16 */
void word_hex(VM *vm) {
    base_value = 16;
}

/* OCTAL ( -- )  Set base to 8 */
void word_octal(VM *vm) {
    base_value = 8;
}

/* <# ( -- )  Begin numeric conversion */
void word_begin_conversion(VM *vm) {
    conversion_pos = 0;
    memset(conversion_buffer, 0, sizeof(conversion_buffer));
}

/* HOLD ( c -- )  Insert character in conversion */
void word_hold(VM *vm) {
    cell_t c;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    c = vm_pop(vm);
    
    if (conversion_pos >= (int)sizeof(conversion_buffer) - 1) {
        vm->error = 1;  /* Buffer overflow */
        return;
    }
    
    /* Insert character at beginning (FORTH builds strings backwards) */
    memmove(&conversion_buffer[1], &conversion_buffer[0], (size_t)conversion_pos);
    conversion_buffer[0] = (char)(c & 0xFF);
    conversion_pos++;
}

/* SIGN ( n -- )  Insert sign if negative */
void word_sign(VM *vm) {
    cell_t n;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    n = vm_pop(vm);
    
    if (n < 0) {
        vm_push(vm, '-');
        word_hold(vm);
    }
}

/* # ( ud1 -- ud2 )  Convert one digit */
void word_hash(VM *vm) {
    cell_t dlow, dhigh;
    unsigned long ud;
    unsigned long remainder;
    char digit;
    
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    dlow = vm_pop(vm);
    dhigh = vm_pop(vm);
    
    /* Convert double to unsigned for division */
    ud = ((unsigned long)dhigh << (sizeof(cell_t) * 4)) | (unsigned long)dlow;
    
    /* Get remainder and quotient */
    remainder = ud % (unsigned long)base_value;
    ud = ud / (unsigned long)base_value;
    
    /* Convert remainder to digit */
    if (remainder < 10) {
        digit = '0' + (char)remainder;
    } else {
        digit = 'A' + (char)(remainder - 10);
    }
    
    /* Add digit to conversion buffer */
    vm_push(vm, digit);
    word_hold(vm);
    
    /* Push quotient back as double */
    vm_push(vm, (cell_t)(ud >> (sizeof(cell_t) * 4)));  /* dhigh */
    vm_push(vm, (cell_t)(ud & 0xFFFFFFFF));             /* dlow */
}

/* #S ( ud -- 0 0 )  Convert all remaining digits */
void word_hash_s(VM *vm) {
    cell_t dlow, dhigh;
    
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    /* Keep converting digits until double is zero */
    do {
        word_hash(vm);
        if (vm->error) return;
        
        dlow = vm->data_stack[vm->dsp];
        dhigh = vm->data_stack[vm->dsp - 1];
    } while (dhigh != 0 || dlow != 0);
}

/* #> ( ud -- addr u )  End numeric conversion */
void word_end_conversion(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    /* Drop the double from stack */
    vm_pop(vm);  /* dlow */
    vm_pop(vm);  /* dhigh */
    
    /* Push address and length of conversion buffer */
    vm_push(vm, (cell_t)(uintptr_t)conversion_buffer);
    vm_push(vm, conversion_pos);
}

/* Helper function to print a number with specific formatting */
static void print_number_formatted(cell_t n, int width, int is_unsigned) {
    char temp_buffer[32];
    int len;
    int i;
    
    if (is_unsigned) {
        if (base_value == 16) {
            len = snprintf(temp_buffer, sizeof(temp_buffer), "%lX", (unsigned long)n);
        } else if (base_value == 8) {
            len = snprintf(temp_buffer, sizeof(temp_buffer), "%lo", (unsigned long)n);
        } else {
            len = snprintf(temp_buffer, sizeof(temp_buffer), "%lu", (unsigned long)n);
        }
    } else {
        if (base_value == 16) {
            len = snprintf(temp_buffer, sizeof(temp_buffer), "%lX", (long)n);
        } else if (base_value == 8) {
            len = snprintf(temp_buffer, sizeof(temp_buffer), "%lo", (long)n);
        } else {
            len = snprintf(temp_buffer, sizeof(temp_buffer), "%ld", (long)n);
        }
    }
    
    /* Print with right justification if width specified */
    if (width > 0 && len < width) {
        for (i = 0; i < width - len; i++) {
            printf(" ");
        }
    }
    
    printf("%s", temp_buffer);
}

/* . ( n -- )  Print signed number */
void word_dot(VM *vm) {
    cell_t n;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    n = vm_pop(vm);
    print_number_formatted(n, 0, 0);
    printf(" ");
}

/* .R ( n width -- )  Print signed number right-justified */
void word_dot_r(VM *vm) {
    cell_t width, n;
    
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    width = vm_pop(vm);
    n = vm_pop(vm);
    
    print_number_formatted(n, (int)width, 0);
    printf(" ");
}

/* U. ( u -- )  Print unsigned number */
void word_u_dot(VM *vm) {
    cell_t u;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    u = vm_pop(vm);
    print_number_formatted(u, 0, 1);
    printf(" ");
}

/* U.R ( u width -- )  Print unsigned number right-justified */
void word_u_dot_r(VM *vm) {
    cell_t width, u;
    
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    width = vm_pop(vm);
    u = vm_pop(vm);
    
    print_number_formatted(u, (int)width, 1);
    printf(" ");
}

/* D. ( d -- )  Print double number */
void word_d_dot(VM *vm) {
    cell_t dlow, dhigh;
    
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    dlow = vm_pop(vm);
    dhigh = vm_pop(vm);
    
    /* For simplicity, convert to single and print */
    /* In a full implementation, this would handle full double precision */
    if (dhigh == 0) {
        print_number_formatted(dlow, 0, 0);
    } else if (dhigh == -1 && dlow < 0) {
        print_number_formatted(dlow, 0, 0);
    } else {
        printf("DOUBLE-OVERFLOW");
    }
    printf(" ");
}

/* D.R ( d width -- )  Print double number right-justified */
void word_d_dot_r(VM *vm) {
    cell_t width, dlow, dhigh;
    
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    
    width = vm_pop(vm);
    dlow = vm_pop(vm);
    dhigh = vm_pop(vm);
    
    /* For simplicity, convert to single and print */
    if (dhigh == 0) {
        print_number_formatted(dlow, (int)width, 0);
    } else if (dhigh == -1 && dlow < 0) {
        print_number_formatted(dlow, (int)width, 0);
    } else {
        printf("DOUBLE-OVERFLOW");
    }
    printf(" ");
}

/* .S ( -- )  Print stack contents */
void word_dot_s(VM *vm) {
    int i;
    
    printf("<");
    printf("%d", vm->dsp + 1);
    printf("> ");
    
    for (i = 0; i <= vm->dsp; i++) {
        print_number_formatted(vm->data_stack[i], 0, 0);
        printf(" ");
    }
    
    printf("\n");
}

/* ? ( addr -- )  Print value at address */
void word_question(VM *vm) {
    cell_t addr;
    cell_t *ptr;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    addr = vm_pop(vm);
    ptr = (cell_t*)(uintptr_t)addr;
    
    /* Basic address validation */
    if (ptr == NULL) {
        vm->error = 1;
        return;
    }
    
    print_number_formatted(*ptr, 0, 0);
    printf(" ");
}

/* DUMP ( addr u -- )  Hex dump u bytes from addr */
void word_dump(VM *vm) {
    cell_t u, addr;
    uint8_t *ptr;
    size_t i, j;
    size_t bytes_to_dump;
    
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    u = vm_pop(vm);
    addr = vm_pop(vm);
    
    ptr = (uint8_t*)(uintptr_t)addr;
    bytes_to_dump = (size_t)u;
    
    if (ptr == NULL || u < 0) {
        vm->error = 1;
        return;
    }
    
    /* Print hex dump in 16-byte lines */
    for (i = 0; i < bytes_to_dump; i += 16) {
        /* Print address */
        printf("%08lX: ", (unsigned long)(addr + (cell_t)i));
        
        /* Print hex bytes */
        for (j = 0; j < 16 && i + j < bytes_to_dump; j++) {
            printf("%02X ", ptr[i + j]);
        }
        
        /* Pad with spaces if less than 16 bytes */
        for (; j < 16; j++) {
            printf("   ");
        }
        
        /* Print ASCII representation */
        printf(" |");
        for (j = 0; j < 16 && i + j < bytes_to_dump; j++) {
            char c = (char)ptr[i + j];
            if (c >= 32 && c <= 126) {
                printf("%c", c);
            } else {
                printf(".");
            }
        }
        printf("|\n");
    }
}

/* FORTH-79 Formatting Word Registration and Testing */
void register_format_words(VM *vm) {
    log_message(LOG_INFO, "Registering formatting & conversion words...");
    
    /* Register all formatting & conversion words */
    register_word(vm, ".", word_dot);
    register_word(vm, ".R", word_dot_r);
    register_word(vm, "U.", word_u_dot);
    register_word(vm, "U.R", word_u_dot_r);
    register_word(vm, "D.", word_d_dot);
    register_word(vm, "D.R", word_d_dot_r);
    register_word(vm, ".S", word_dot_s);
    register_word(vm, "?", word_question);
    register_word(vm, "DUMP", word_dump);
    register_word(vm, "<#", word_begin_conversion);
    register_word(vm, "#", word_hash);
    register_word(vm, "#>", word_end_conversion);
    register_word(vm, "#S", word_hash_s);
    register_word(vm, "HOLD", word_hold);
    register_word(vm, "SIGN", word_sign);
    register_word(vm, "BASE", word_base);
    register_word(vm, "DECIMAL", word_decimal);
    register_word(vm, "HEX", word_hex);
    register_word(vm, "OCTAL", word_octal);

    log_message(LOG_INFO, "Note: Output formatting words (., .R, U., etc.) require manual verification");
}
