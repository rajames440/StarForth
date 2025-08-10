/*
                                 ***   StarForth   ***
  format_words.c - FORTH-79 Standard and ANSI C99 ONLY
  Last modified - 2025-08-09
  (c) 2025 Robert A. James - StarshipOS Forth Project. CC0-1.0
*/

#include "include/format_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"
#include "../../include/vm_api.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

/* Conversion buffer for <# ... #> */
static unsigned char *pn_buf = NULL;
static size_t         pn_cap = 0;
static int            conversion_pos = 0;

static void ensure_pn(VM *vm) {
    if (!pn_buf) {
        pn_buf = (unsigned char*)vm_allot(vm, 64);  /* same size as before */
        if (!pn_buf) { vm->error = 1; return; }
        pn_cap = 64;
        memset(pn_buf, 0, pn_cap);
    }
}

#define CELL_BITS   ((int)(sizeof(cell_t) * CHAR_BIT))
#define CHUNK_BITS  16
#define CHUNK_MASK  ((unsigned long)((1UL << CHUNK_BITS) - 1))

static unsigned current_base(VM *vm) {
    unsigned b = (unsigned)vm->base;
    return (b < 2 || b > 36) ? 10u : b;
}

static char digit_for(unsigned v) {
    return (v < 10) ? (char)('0' + v) : (char)('A' + (v - 10));
}

/* Divide unsigned double-cell (dhigh:dlow) by base (2..36), portable C99 */
static void div_ud_by_base(cell_t dhigh_in, cell_t dlow_in, unsigned base,
                           cell_t *qhigh_out, cell_t *qlow_out, unsigned *rem_out)
{
    unsigned long base_ul = (unsigned long)base;
    unsigned long r = 0UL;
    unsigned long qh = 0UL, ql = 0UL;

    /* High word */
    {
        unsigned long hi = (unsigned long)dhigh_in;
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
        unsigned long lo = (unsigned long)dlow_in;
        int parts = CELL_BITS / CHUNK_BITS;
        for (int i = parts - 1; i >= 0; --i) {
            unsigned long chunk = (lo >> (i * CHUNK_BITS)) & CHUNK_MASK;
            unsigned long val = (r << CHUNK_BITS) + chunk;
            unsigned long qchunk = val / base_ul;
            r = val % base_ul;
            ql = (ql << CHUNK_BITS) | (qchunk & CHUNK_MASK);
        }
    }

    *qhigh_out = (cell_t)qh;
    *qlow_out  = (cell_t)ql;
    *rem_out   = (unsigned)r;
}

/* Render one cell in current base (no printf %x/%o so input==output base) */
static void print_number_formatted(VM *vm, cell_t n, int width, int is_unsigned) {
    char buf[80];
    int i = 0;
    unsigned base = current_base(vm);

    if (!is_unsigned && n == 0) {
        buf[i++] = '0';
    } else if (is_unsigned) {
        unsigned long u = (unsigned long)n;
        if (u == 0UL) buf[i++] = '0';
        while (u != 0UL) { unsigned long rem = u % (unsigned long)base; u /= (unsigned long)base; buf[i++] = digit_for((unsigned)rem); }
    } else {
        int neg = (n < 0);
        unsigned long u = (unsigned long)n;
        if (neg) u = (unsigned long)0 - u;
        if (u == 0UL) buf[i++] = '0';
        while (u != 0UL) { unsigned long rem = u % (unsigned long)base; u /= (unsigned long)base; buf[i++] = digit_for((unsigned)rem); }
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
    vm_store_cell(vm, vm->base_addr, (cell_t)10);
}

/* HEX ( -- ) — set BASE=16 */
void format_word_hex(VM *vm) {
    if (!vm) { return; }
    vm_store_cell(vm, vm->base_addr, (cell_t)16);
}

/* OCTAL ( -- ) — set BASE=8 */
void format_word_octal(VM *vm) {
    if (!vm) { return; }
    vm_store_cell(vm, vm->base_addr, (cell_t)8);
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
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t c = vm_pop(vm);

    ensure_pn(vm);
    if (vm->error) return;

    if ((size_t)conversion_pos >= pn_cap - 1) { vm->error = 1; return; }
    memmove(&pn_buf[1], &pn_buf[0], (size_t)conversion_pos);
    pn_buf[0] = (char)(c & 0xFF);
    conversion_pos++;
}


/* SIGN ( n -- ) */
void format_word_sign(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t n = vm_pop(vm);
    if (n < 0) { vm_push(vm, '-'); format_word_hold(vm); }
}

/* # ( ud1 -- ud2 ) */
void format_word_hash(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    cell_t dlow  = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);

    unsigned base = current_base(vm);
    cell_t qh, ql; unsigned rem;
    div_ud_by_base(dhigh, dlow, base, &qh, &ql, &rem);

    vm_push(vm, (cell_t)digit_for(rem));
    format_word_hold(vm); if (vm->error) return;

    vm_push(vm, qh);
    vm_push(vm, ql);
}

/* #S ( ud -- 0 0 ) */
void format_word_hash_s(VM *vm) {
    /* FORTH-79: requires an unsigned double on the stack (dlow dhigh). */
    if (vm->dsp < 1) { vm->error = 1; return; }

    for (;;) {
        /* Always convert at least once so 0 0 produces "0". */
        format_word_hash(vm);
        if (vm->error) return;

        /* Peek the quotient (dhigh dlow) that '#' just pushed back */
        cell_t dlow  = vm->data_stack[vm->dsp];
        cell_t dhigh = vm->data_stack[vm->dsp - 1];
        if (dhigh == 0 && dlow == 0) break;
    }
}

/* #> ( ud -- addr u ) */
void format_word_end_conversion(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    (void)vm_pop(vm); /* dlow */
    (void)vm_pop(vm); /* dhigh */

    ensure_pn(vm);
    if (vm->error) return;

    vm_push(vm, (cell_t)(uintptr_t)pn_buf);   /* VM address */
    vm_push(vm, (cell_t)conversion_pos);
}

/* . ( n -- ) */
void format_word_dot(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t n = vm_pop(vm);
    print_number_formatted(vm, n, 0, 0);
    putchar(' ');
}

/* .R ( n width -- ) */
void format_word_dot_r(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    cell_t width = vm_pop(vm);
    cell_t n     = vm_pop(vm);
    print_number_formatted(vm, n, (int)width, 0);
    putchar(' ');
}

/* U. ( u -- ) */
void format_word_u_dot(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t u = vm_pop(vm);
    print_number_formatted(vm, u, 0, 1);
    putchar(' ');
}

/* U.R ( u width -- ) */
void format_word_u_dot_r(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    cell_t width = vm_pop(vm);
    cell_t u     = vm_pop(vm);
    print_number_formatted(vm, u, (int)width, 1);
    putchar(' ');
}

/* D. ( d -- ) */
void format_word_d_dot(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    cell_t dlow  = vm_pop(vm);
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
    if (vm->dsp < 2) { vm->error = 1; return; }
    cell_t width = vm_pop(vm);
    cell_t dlow  = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    if ((dhigh == 0) || (dhigh == -1 && dlow < 0)) {
        print_number_formatted(vm, dlow, (int)width, 0);
    } else {
        int len = (int)strlen("DOUBLE-OVERFLOW");
        for (int i = 0; i < ((int)width > len ? (int)width - len : 0); ++i) putchar(' ');
        fputs("DOUBLE-OVERFLOW", stdout);
    }
    putchar(' ');
}

/* .S ( -- ) */
void format_word_dot_s(VM *vm) {
    printf("<%d> ", vm->dsp + 1);
    for (int i = 0; i <= vm->dsp; i++) { print_number_formatted(vm, vm->data_stack[i], 0, 0); putchar(' '); }
    putchar('\n');
}

/* ? ( addr -- ) */
void format_word_question(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t addr = vm_pop(vm);
    cell_t *ptr = (cell_t*)(uintptr_t)addr;
    if (!ptr) { vm->error = 1; return; }
    print_number_formatted(vm, *ptr, 0, 0);
    putchar(' ');
}

/* DUMP ( addr u -- ) */
void format_word_dump(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    cell_t u    = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    if (u < 0) { vm->error = 1; return; }
    uint8_t *ptr = (uint8_t*)(uintptr_t)addr;
    if (!ptr) { vm->error = 1; return; }
    size_t n = (size_t)u;
    int addr_width = (int)(sizeof(uintptr_t) * 2);

    for (size_t i = 0; i < n; i += 16) {
        unsigned long long a = (unsigned long long)((uintptr_t)addr + (uintptr_t)i);
        printf("%0*llX: ", addr_width, a);
        size_t j = 0;
        for (; j < 16 && i + j < n; j++) printf("%02X ", ptr[i + j]);
        for (; j < 16; j++) printf("   ");
        fputs(" |", stdout);
        for (j = 0; j < 16 && i + j < n; j++) {
            char c = (char)ptr[i + j];
            putchar((c >= 32 && c <= 126) ? c : '.');
        }
        fputs("|\n", stdout);
    }
}

/* Registration */
void register_format_words(VM *vm) {
    log_message(LOG_INFO, "Registering formatting & conversion words...");

    register_word(vm, ".",   format_word_dot);
    register_word(vm, ".R",  format_word_dot_r);
    register_word(vm, "U.",  format_word_u_dot);
    register_word(vm, "U.R", format_word_u_dot_r);
    register_word(vm, "D.",  format_word_d_dot);
    register_word(vm, "D.R", format_word_d_dot_r);
    register_word(vm, ".S",  format_word_dot_s);
    register_word(vm, "?",   format_word_question);
    register_word(vm, "DUMP",format_word_dump);

    register_word(vm, "<#",  format_word_begin_conversion);
    register_word(vm, "#",   format_word_hash);
    register_word(vm, "#S",  format_word_hash_s);
    register_word(vm, "#>",  format_word_end_conversion);
    register_word(vm, "HOLD",format_word_hold);
    register_word(vm, "SIGN",format_word_sign);

    register_word(vm, "BASE",   format_word_base);
    register_word(vm, "DECIMAL",format_word_decimal);
    register_word(vm, "HEX",    format_word_hex);
    register_word(vm, "OCTAL",  format_word_octal);

    log_message(LOG_INFO, "Formatting & conversion words registered");
}
