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

#include "include/format_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"

#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>


/** @brief Picture number buffer for numeric conversion */
static unsigned char *pn_buf = NULL;
/** @brief Capacity of picture number buffer */
static size_t pn_cap = 0;
/** @brief Current position in picture number buffer */
static int conversion_pos = 0;

/** @brief Ensures picture number buffer is allocated */
static void ensure_pn(VM *vm) {
    if (!pn_buf) {
        pn_buf = (unsigned char *) vm_allot(vm, 64); /* same size as before */
        if (!pn_buf) {
            vm->error = 1;
            return;
        }
        pn_cap = 64;
        memset(pn_buf, 0, pn_cap);
    }
}

#define CELL_BITS   ((int)(sizeof(cell_t) * CHAR_BIT))
#define CHUNK_BITS  16
#define CHUNK_MASK  ((unsigned long)((1UL << CHUNK_BITS) - 1))

/** @brief Returns current numeric conversion base, defaulting to 10 if invalid */
static unsigned current_base(VM *vm) {
    unsigned b = (unsigned) vm->base;
    return (b < 2 || b > 36) ? 10u : b;
}

/** @brief Converts value 0-35 to character '0'-'9' or 'A'-'Z' */
static char digit_for(unsigned v) {
    return (v < 10) ? (char) ('0' + v) : (char) ('A' + (v - 10));
}

/* Divide unsigned double-cell (dhigh:dlow) by base (2..36), portable C99 */
/** @brief Divides unsigned double-cell number by base using portable C99 code */
static void div_ud_by_base(cell_t dhigh_in, cell_t dlow_in, unsigned base,
                           cell_t *qhigh_out, cell_t *qlow_out, unsigned *rem_out) {
    unsigned long base_ul = (unsigned long) base;
    unsigned long r = 0UL;
    unsigned long qh = 0UL, ql = 0UL;

    /* High word */
    {
        unsigned long hi = (unsigned long) dhigh_in;
        int parts = CELL_BITS / CHUNK_BITS;
        for (int i = parts - 1; i >= 0; --i) {
            unsigned long chunk = (hi >> (i * CHUNK_BITS)) & CHUNK_MASK;
            unsigned long val = (r << CHUNK_BITS) + chunk;
            unsigned long qchunk = val / base_ul;
            r = val % base_ul;
            qh = (qh << CHUNK_BITS) | (qchunk & CHUNK_MASK);
        }
    }
    /* Low word */
    {
        unsigned long lo = (unsigned long) dlow_in;
        int parts = CELL_BITS / CHUNK_BITS;
        for (int i = parts - 1; i >= 0; --i) {
            unsigned long chunk = (lo >> (i * CHUNK_BITS)) & CHUNK_MASK;
            unsigned long val = (r << CHUNK_BITS) + chunk;
            unsigned long qchunk = val / base_ul;
            r = val % base_ul;
            ql = (ql << CHUNK_BITS) | (qchunk & CHUNK_MASK);
        }
    }

    *qhigh_out = (cell_t) qh;
    *qlow_out = (cell_t) ql;
    *rem_out = (unsigned) r;
}

/* Render one cell in current base (no printf %x/%o so input==output base) */
/** @brief Prints number with optional width field and sign handling */
static void print_number_formatted(VM *vm, cell_t n, int width, int is_unsigned) {
    char buf[80];
    int i = 0;
    unsigned base = current_base(vm);

    if (!is_unsigned && n == 0) {
        buf[i++] = '0';
    } else if (is_unsigned) {
        unsigned long u = (unsigned long) n;
        if (u == 0UL) buf[i++] = '0';
        while (u != 0UL) {
            unsigned long rem = u % (unsigned long) base;
            u /= (unsigned long) base;
            buf[i++] = digit_for((unsigned) rem);
        }
    } else {
        int neg = (n < 0);
        unsigned long u = (unsigned long) n;
        if (neg) u = (unsigned long) 0 - u;
        if (u == 0UL) buf[i++] = '0';
        while (u != 0UL) {
            unsigned long rem = u % (unsigned long) base;
            u /= (unsigned long) base;
            buf[i++] = digit_for((unsigned) rem);
        }
        if (neg) buf[i++] = '-';
    }

    int len = i;
    if (width > 0 && len < width) for (int s = 0; s < width - len; ++s) putchar(' ');
    while (i--) putchar(buf[i]);
}

/* ===== Words ===== */

/* BASE ( -- addr )  — returns VM address (offset) of the BASE variable */
void format_word_base(VM *vm) {
    if (!vm) { return; }
    /* vm->base_addr is a VM address (offset into vm->memory), not a host pointer */
    vm_push(vm, CELL(vm->base_addr));
}

/* DECIMAL ( -- ) — set BASE=10 */
void format_word_decimal(VM *vm) {
    if (!vm) { return; }
    vm_store_cell(vm, vm->base_addr, (cell_t) 10);
}

/* HEX ( -- ) — set BASE=16 */
void format_word_hex(VM *vm) {
    if (!vm) { return; }
    vm_store_cell(vm, vm->base_addr, (cell_t) 16);
}

/* OCTAL ( -- ) — set BASE=8 */
void format_word_octal(VM *vm) {
    if (!vm) { return; }
    vm_store_cell(vm, vm->base_addr, (cell_t) 8);
}

/* <# ( -- ) */
void format_word_begin_conversion(VM *vm) {
    ensure_pn(vm);
    if (vm->error) return;
    conversion_pos = 0;
    memset(pn_buf, 0, pn_cap);
}

/* HOLD ( c -- ) */
void format_word_hold(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "HOLD: data stack underflow");
        return;
    }
    cell_t c = vm_pop(vm);

    /* Must be a single byte 0..255 */
    if (c < 0 || c > 255) {
        vm->error = 1;
        log_message(LOG_ERROR, "HOLD: byte out of range (%ld)", (long) c);
        return;
    }

    ensure_pn(vm);
    if (vm->error) return;

    /* keep 1 byte of headroom (null safety), like before */
    if ((size_t) conversion_pos >= pn_cap - 1) {
        vm->error = 1;
        log_message(LOG_ERROR, "HOLD: conversion buffer full (pos=%d cap=%zu)", conversion_pos, pn_cap);
        return;
    }

    /* Prepend the byte */
    memmove(&pn_buf[1], &pn_buf[0], (size_t) conversion_pos);
    pn_buf[0] = (unsigned char) c;
    conversion_pos++;
}


/* SIGN ( n -- ) */
void format_word_sign(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t n = vm_pop(vm);
    if (n < 0) {
        vm_push(vm, '-');
        format_word_hold(vm);
    }
}

/* # ( ud1 | n -- ud2 )
   Tolerant form:
   - If only one cell is present, treat it as a signed single and convert its magnitude.
   - SIGN (if used) is responsible for inserting '-' afterwards. */
void format_word_hash(VM *vm) {
    cell_t dlow, dhigh;

    /* If we don't have a full double, but have one cell, promote it. */
    if (vm->dsp < 1) {
        if (vm->dsp < 0) {
            vm->error = 1;
            return;
        }
        cell_t n = vm_pop(vm);
        unsigned long u = (n < 0) ? (unsigned long) (-(cell_t) n) : (unsigned long) n;
        dhigh = 0;
        dlow = (cell_t) u;
    } else {
        /* Normal path: consume unsigned double (dlow dhigh) */
        dlow = vm_pop(vm);
        dhigh = vm_pop(vm);
    }

    unsigned base = current_base(vm);
    cell_t qh, ql;
    unsigned rem;
    div_ud_by_base(dhigh, dlow, base, &qh, &ql, &rem);

    /* Emit rightmost digit via HOLD */
    vm_push(vm, (cell_t) digit_for(rem));
    format_word_hold(vm);
    if (vm->error) return;

    /* Push quotient back as (dhigh dlow) so #S can loop */
    vm_push(vm, qh);
    vm_push(vm, ql);
}

/* #S ( ud | n -- 0 0 )  — tolerant:
   If only one cell is present, treat it as a signed single and convert its magnitude.
   SIGN (if used) is responsible for inserting '-' later. */
void format_word_hash_s(VM *vm) {
    /* If we don't have a full double, but have one cell, promote it. */
    if (vm->dsp < 1) {
        if (vm->dsp < 0) {
            vm->error = 1;
            return;
        }
        /* Promote single to (dhigh=0, dlow=abs(n)) so # works on magnitude */
        cell_t n = vm_pop(vm);
        unsigned long u = (n < 0) ? (unsigned long) (-(cell_t) n) : (unsigned long) n;
        /* Stack order for format_word_hash: top must be dlow, under it dhigh */
        vm_push(vm, (cell_t) 0); /* dhigh */
        vm_push(vm, (cell_t) u); /* dlow  */
    }

    for (;;) {
        /* Always convert at least once so 0 produces "0". */
        format_word_hash(vm);
        if (vm->error) return;

        /* Peek the quotient (dhigh dlow) that '#' just pushed back */
        cell_t dlow = vm->data_stack[vm->dsp];
        cell_t dhigh = vm->data_stack[vm->dsp - 1];
        if (dhigh == 0 && dlow == 0) break;
    }
}

/* #> ( [ud] -- addr u ) — tolerant: pops ud iff present, else just returns buffer */
void format_word_end_conversion(VM *vm) {
    /* If there’s at least a double on the stack, drop it (dlow dhigh). */
    if (vm->dsp >= 1) {
        (void) vm_pop(vm);
        (void) vm_pop(vm);
    }

    ensure_pn(vm);
    if (vm->error) return;

    /* Push VM address (byte offset), not a host pointer */
    vaddr_t v = (vaddr_t)((uint8_t *) pn_buf - vm->memory);
    vm_push(vm, CELL(v));
    vm_push(vm, (cell_t) conversion_pos);
}

/* . ( n -- ) */
void format_word_dot(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t n = vm_pop(vm);
    print_number_formatted(vm, n, 0, 0);
    putchar(' ');
}

/* .R ( n width -- ) */
void format_word_dot_r(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    cell_t width = vm_pop(vm);
    cell_t n = vm_pop(vm);
    print_number_formatted(vm, n, (int) width, 0);
    putchar(' ');
}

/* U. ( u -- ) */
void format_word_u_dot(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t u = vm_pop(vm);
    print_number_formatted(vm, u, 0, 1);
    putchar(' ');
}

/* U.R ( u width -- ) */
void format_word_u_dot_r(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    cell_t width = vm_pop(vm);
    cell_t u = vm_pop(vm);
    print_number_formatted(vm, u, (int) width, 1);
    putchar(' ');
}

/* D. ( d -- ) */
void format_word_d_dot(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    if ((dhigh == 0) || (dhigh == -1 && dlow < 0)) {
        print_number_formatted(vm, dlow, 0, 0);
    } else {
        fputs("DOUBLE-OVERFLOW", stdout);
    }
    putchar(' ');
}

/* D.R ( d width -- ) */
void format_word_d_dot_r(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    cell_t width = vm_pop(vm);
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    if ((dhigh == 0) || (dhigh == -1 && dlow < 0)) {
        print_number_formatted(vm, dlow, (int) width, 0);
    } else {
        int len = (int) strlen("DOUBLE-OVERFLOW");
        for (int i = 0; i < ((int) width > len ? (int) width - len : 0); ++i) putchar(' ');
        fputs("DOUBLE-OVERFLOW", stdout);
    }
    putchar(' ');
}

/* .S ( -- ) */
void format_word_dot_s(VM *vm) {
    printf("<%d> ", vm->dsp + 1);
    for (int i = 0; i <= vm->dsp; i++) {
        print_number_formatted(vm, vm->data_stack[i], 0, 0);
        putchar(' ');
    }
    putchar('\n');
}

/* ? ( addr -- ) */
void format_word_question(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t addr = vm_pop(vm);
    cell_t *ptr = (cell_t *) (uintptr_t) addr;
    if (!ptr) {
        vm->error = 1;
        return;
    }
    print_number_formatted(vm, *ptr, 0, 0);
    putchar(' ');
}

/* DUMP ( addr u -- ) */
void format_word_dump(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    cell_t u = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    if (u < 0) {
        vm->error = 1;
        return;
    }
    uint8_t *ptr = (uint8_t *) (uintptr_t) addr;
    if (!ptr) {
        vm->error = 1;
        return;
    }
    size_t n = (size_t) u;
    int addr_width = (int) (sizeof(uintptr_t) * 2);

    for (size_t i = 0; i < n; i += 16) {
        unsigned long long a = (unsigned long long) ((uintptr_t) addr + (uintptr_t) i);
        printf("%0*llX: ", addr_width, a);
        size_t j = 0;
        for (; j < 16 && i + j < n; j++) printf("%02X ", ptr[i + j]);
        for (; j < 16; j++) printf("   ");
        fputs(" |", stdout);
        for (j = 0; j < 16 && i + j < n; j++) {
            char c = (char) ptr[i + j];
            putchar((c >= 32 && c <= 126) ? c : '.');
        }
        fputs("|\n", stdout);
    }
}

/* Registration */
/**
 * @brief Registers all formatting and numeric conversion words with the VM.
 *
 * This function registers the standard FORTH formatting words including:
 * - Number output words (., .R, U., U.R, D., D.R, .S, ?)
 * - Picture number formatting (<#, #, #S, #>, HOLD, SIGN)
 * - Base conversion (BASE, DECIMAL, HEX, OCTAL)
 * - Memory dump (DUMP)
 *
 * @param vm Pointer to the virtual machine instance
 */
void register_format_words(VM *vm) {
    register_word(vm, ".", format_word_dot);
    register_word(vm, ".R", format_word_dot_r);
    register_word(vm, "U.", format_word_u_dot);
    register_word(vm, "U.R", format_word_u_dot_r);
    register_word(vm, "D.", format_word_d_dot);
    register_word(vm, "D.R", format_word_d_dot_r);
    register_word(vm, ".S", format_word_dot_s);
    register_word(vm, "?", format_word_question);
    register_word(vm, "DUMP", format_word_dump);

    register_word(vm, "<#", format_word_begin_conversion);
    register_word(vm, "#", format_word_hash);
    register_word(vm, "#S", format_word_hash_s);
    register_word(vm, "#>", format_word_end_conversion);
    register_word(vm, "HOLD", format_word_hold);
    register_word(vm, "SIGN", format_word_sign);

    register_word(vm, "BASE", format_word_base);
    register_word(vm, "DECIMAL", format_word_decimal);
    register_word(vm, "HEX", format_word_hex);
    register_word(vm, "OCTAL", format_word_octal);
}