/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/**
 * q48_words.c — Module 25: Q48.16 fixed-point FORTH words
 *
 * FORTH word set for Q48.16 fixed-point arithmetic.
 * All stack values are cell_t (int64_t) reinterpreted as uint64_t Q48.16.
 * 1.0 = 0x10000 = 65536.  Fractional resolution = 1/65536 ≈ 0.0000153.
 *
 * Words registered:
 *   Q.+     ( q1 q2 -- q_sum )
 *   Q.-     ( q1 q2 -- q_diff )
 *   Q.*     ( q1 q2 -- q_prod )
 *   Q./     ( q1 q2 -- q_quot )   division by zero → q=0
 *   Q.ABS   ( q -- |q| )
 *   Q.NEG   ( q -- -q )
 *   Q.LOG   ( q -- ln_q )         natural log, Newton-Raphson
 *   Q.EXP   ( q -- e^q )          Taylor series
 *   Q.SQRT  ( q -- sqrt_q )       Newton-Raphson
 *   Q.FROM-INT ( n -- q )         integer → Q48.16 (n << 16)
 *   Q.TO-INT   ( q -- n )         Q48.16 → integer (q >> 16, truncate)
 *   Q.1     ( -- 65536 )          1.0 in Q48.16
 *   Q.0     ( -- 0 )              0.0 in Q48.16
 *   Q.SCALE ( -- 65536 )          scale factor (alias of Q.1)
 *   Q.=     ( q1 q2 -- flag )     -1 if equal, 0 otherwise
 *   Q.<     ( q1 q2 -- flag )     -1 if q1 < q2
 *   Q.>     ( q1 q2 -- flag )     -1 if q1 > q2
 *   Q.0=    ( q -- flag )         -1 if q = 0
 *   Q.MAX   ( q1 q2 -- q_max )
 *   Q.MIN   ( q1 q2 -- q_min )
 *   Q.PRINT ( q -- )              print as "integer.frac" to console
 */

#include <stdint.h>
#include <stddef.h>

#include "q48_16.h"
#include "vm.h"
#include "word_registry.h"

#ifdef __STARKERNEL__
#include "starkernel/freestanding/stdio.h"
#include "starkernel/console.h"
#else
#include <stdio.h>
#endif

/* ── helpers ──────────────────────────────────────────────────────────── */

static inline q48_16_t q48_pop(VM *vm)  { return (q48_16_t)(uint64_t)VM_POP(vm); }
static inline void      q48_push(VM *vm, q48_16_t q) { VM_PUSH(vm, (cell_t)(int64_t)q); }

/* ── arithmetic ──────────────────────────────────────────────────────── */

static void q48_word_add(VM *vm)
{
    q48_16_t b = q48_pop(vm);
    q48_16_t a = q48_pop(vm);
    q48_push(vm, q48_add(a, b));
}

static void q48_word_sub(VM *vm)
{
    q48_16_t b = q48_pop(vm);
    q48_16_t a = q48_pop(vm);
    q48_push(vm, q48_sub(a, b));
}

static void q48_word_mul(VM *vm)
{
    q48_16_t b = q48_pop(vm);
    q48_16_t a = q48_pop(vm);
    q48_push(vm, q48_mul(a, b));
}

static void q48_word_div(VM *vm)
{
    q48_16_t b = q48_pop(vm);
    q48_16_t a = q48_pop(vm);
    q48_push(vm, q48_div(a, b));   /* q48_div returns 0 on b=0 */
}

static void q48_word_abs(VM *vm)
{
    q48_push(vm, q48_abs(q48_pop(vm)));
}

static void q48_word_neg(VM *vm)
{
    q48_push(vm, (q48_16_t)(0ULL - q48_pop(vm)));
}

/* ── approximations ──────────────────────────────────────────────────── */

static void q48_word_log(VM *vm)
{
    q48_16_t a = q48_pop(vm);
    q48_push(vm, q48_log_approx((uint64_t)a));
}

static void q48_word_exp(VM *vm)
{
    q48_push(vm, q48_exp_approx(q48_pop(vm)));
}

static void q48_word_sqrt(VM *vm)
{
    q48_push(vm, q48_sqrt_approx(q48_pop(vm)));
}

/* ── conversions ─────────────────────────────────────────────────────── */

static void q48_word_from_int(VM *vm)
{
    cell_t n = VM_POP(vm);
    q48_push(vm, q48_from_u64((uint64_t)(n < 0 ? 0 : n)));
}

static void q48_word_to_int(VM *vm)
{
    VM_PUSH(vm, (cell_t)q48_to_u64(q48_pop(vm)));
}

/* ── constants ───────────────────────────────────────────────────────── */

static void q48_word_one(VM *vm)   { q48_push(vm, (q48_16_t)65536ULL); }
static void q48_word_zero(VM *vm)  { q48_push(vm, (q48_16_t)0ULL); }
static void q48_word_scale(VM *vm) { q48_push(vm, (q48_16_t)65536ULL); }

/* ── comparisons ─────────────────────────────────────────────────────── */

static void q48_word_eq(VM *vm)
{
    q48_16_t b = q48_pop(vm);
    q48_16_t a = q48_pop(vm);
    VM_PUSH(vm, (a == b) ? (cell_t)-1 : (cell_t)0);
}

static void q48_word_lt(VM *vm)
{
    q48_16_t b = q48_pop(vm);
    q48_16_t a = q48_pop(vm);
    VM_PUSH(vm, (a < b) ? (cell_t)-1 : (cell_t)0);
}

static void q48_word_gt(VM *vm)
{
    q48_16_t b = q48_pop(vm);
    q48_16_t a = q48_pop(vm);
    VM_PUSH(vm, (a > b) ? (cell_t)-1 : (cell_t)0);
}

static void q48_word_zero_eq(VM *vm)
{
    q48_16_t a = q48_pop(vm);
    VM_PUSH(vm, (a == 0ULL) ? (cell_t)-1 : (cell_t)0);
}

static void q48_word_max(VM *vm)
{
    q48_16_t b = q48_pop(vm);
    q48_16_t a = q48_pop(vm);
    q48_push(vm, (a >= b) ? a : b);
}

static void q48_word_min(VM *vm)
{
    q48_16_t b = q48_pop(vm);
    q48_16_t a = q48_pop(vm);
    q48_push(vm, (a <= b) ? a : b);
}

/* ── output ──────────────────────────────────────────────────────────── */

static void q48_word_print(VM *vm)
{
    q48_16_t q = q48_pop(vm);
    uint64_t int_part   = q >> 16;
    uint64_t frac_raw   = q & 0xFFFFULL;
    uint32_t frac_digits = (uint32_t)((frac_raw * 100000ULL) / 65536ULL);
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu.%05u ", (unsigned long)int_part, frac_digits);
#ifdef __STARKERNEL__
    console_puts(buf);
#else
    fputs(buf, stdout);
    fflush(stdout);
#endif
}

/* ── registration ────────────────────────────────────────────────────── */

void register_q48_words(VM *vm)
{
    register_word(vm, "Q.+",        q48_word_add);
    register_word(vm, "Q.-",        q48_word_sub);
    register_word(vm, "Q.*",        q48_word_mul);
    register_word(vm, "Q./",        q48_word_div);
    register_word(vm, "Q.ABS",      q48_word_abs);
    register_word(vm, "Q.NEG",      q48_word_neg);
    register_word(vm, "Q.LOG",      q48_word_log);
    register_word(vm, "Q.EXP",      q48_word_exp);
    register_word(vm, "Q.SQRT",     q48_word_sqrt);
    register_word(vm, "Q.FROM-INT", q48_word_from_int);
    register_word(vm, "Q.TO-INT",   q48_word_to_int);
    register_word(vm, "Q.1",        q48_word_one);
    register_word(vm, "Q.0",        q48_word_zero);
    register_word(vm, "Q.SCALE",    q48_word_scale);
    register_word(vm, "Q.=",        q48_word_eq);
    register_word(vm, "Q.<",        q48_word_lt);
    register_word(vm, "Q.>",        q48_word_gt);
    register_word(vm, "Q.0=",       q48_word_zero_eq);
    register_word(vm, "Q.MAX",      q48_word_max);
    register_word(vm, "Q.MIN",      q48_word_min);
    register_word(vm, "Q.PRINT",    q48_word_print);
}
