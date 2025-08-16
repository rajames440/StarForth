/*

                                 ***   StarForth   ***
  double_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/* double_words.c - FORTH-79 Double Number Words */
#include "include/double_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* S>D ( n -- d ) */
void double_word_s_to_d(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    cell_t dhigh = (n < 0) ? -1 : 0;
    vm_push(vm, dhigh);
    vm_push(vm, n);
}

/* D+ ( d1 d2 -- d3 ) */
void double_word_d_plus(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2low = vm_pop(vm);
    cell_t d2high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);

    cell_t result_low = d1low + d2low;
    cell_t carry = (result_low < d1low) ? 1 : 0;
    cell_t result_high = d1high + d2high + carry;

    vm_push(vm, result_high);
    vm_push(vm, result_low);
}

/* D- ( d1 d2 -- d3 ) */
void double_word_d_minus(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2low = vm_pop(vm);
    cell_t d2high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);

    cell_t result_low = d1low - d2low;
    cell_t borrow = (d1low < d2low) ? 1 : 0;
    cell_t result_high = d1high - d2high - borrow;

    vm_push(vm, result_high);
    vm_push(vm, result_low);
}

/* DNEGATE ( d1 -- d2 ) */
void double_word_dnegate(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);

    dlow = ~dlow + 1;
    dhigh = ~dhigh + (dlow == 0 ? 1 : 0);

    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/* DABS ( d1 -- d2 ) */
void double_word_dabs(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dhigh = vm->data_stack[vm->dsp - 1];
    if (dhigh < 0) {
        double_word_dnegate(vm);
    }
}

/* Internal comparison helper */
static int d_compare(cell_t d1h, cell_t d1l, cell_t d2h, cell_t d2l) {
    if (d1h < d2h) return -1;
    if (d1h > d2h) return 1;

    unsigned long ul1 = (unsigned long) d1l;
    unsigned long ul2 = (unsigned long) d2l;
    return (ul1 > ul2) - (ul1 < ul2);
}

/* DMAX ( d1 d2 -- d3 ) */
void double_word_dmax(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2low = vm_pop(vm);
    cell_t d2high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);

    if (d_compare(d1high, d1low, d2high, d2low) >= 0) {
        vm_push(vm, d1high);
        vm_push(vm, d1low);
    } else {
        vm_push(vm, d2high);
        vm_push(vm, d2low);
    }
}

/* DMIN ( d1 d2 -- d3 ) */
void double_word_dmin(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2low = vm_pop(vm);
    cell_t d2high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);

    if (d_compare(d1high, d1low, d2high, d2low) <= 0) {
        vm_push(vm, d1high);
        vm_push(vm, d1low);
    } else {
        vm_push(vm, d2high);
        vm_push(vm, d2low);
    }
}

/* D< ( d1 d2 -- flag ) */
void double_word_d_less(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2low = vm_pop(vm);
    cell_t d2high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);

    int cmp = d_compare(d1high, d1low, d2high, d2low);
    vm_push(vm, (cmp < 0) ? -1 : 0);
}

/* D= ( d1 d2 -- flag ) */
void double_word_d_equals(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2low = vm_pop(vm);
    cell_t d2high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);

    int cmp = d_compare(d1high, d1low, d2high, d2low);
    vm_push(vm, (cmp == 0) ? -1 : 0);
}

/* 2DROP ( d -- ) */
void double_word_2drop(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    vm_pop(vm);
    vm_pop(vm);
}

/* 2DUP ( d -- d d ) */
void double_word_2dup(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dhigh = vm->data_stack[vm->dsp - 1];
    cell_t dlow = vm->data_stack[vm->dsp];
    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/* 2SWAP ( d1 d2 -- d2 d1 ) */
void double_word_2swap(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2low = vm_pop(vm);
    cell_t d2high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);

    vm_push(vm, d2high);
    vm_push(vm, d2low);
    vm_push(vm, d1high);
    vm_push(vm, d1low);
}

/* 2OVER ( d1 d2 -- d1 d2 d1 ) */
void double_word_2over(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d1high = vm->data_stack[vm->dsp - 3];
    cell_t d1low = vm->data_stack[vm->dsp - 2];
    vm_push(vm, d1high);
    vm_push(vm, d1low);
}

/* 2ROT ( d1 d2 d3 -- d2 d3 d1 ) */
void double_word_2rot(VM *vm) {
    if (vm->dsp < 5) {
        vm->error = 1;
        return;
    }

    cell_t d3low = vm_pop(vm);
    cell_t d3high = vm_pop(vm);
    cell_t d2low = vm_pop(vm);
    cell_t d2high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);

    vm_push(vm, d2high);
    vm_push(vm, d2low);
    vm_push(vm, d3high);
    vm_push(vm, d3low);
    vm_push(vm, d1high);
    vm_push(vm, d1low);
}

/* 2>R ( d -- ) ( R: -- d ) */
void double_word_2to_r(VM *vm) {
    if (vm->dsp < 1 || vm->rsp >= STACK_SIZE - 2) {
        vm->error = 1;
        return;
    }

    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    vm->return_stack[++vm->rsp] = dhigh;
    vm->return_stack[++vm->rsp] = dlow;
}

/* 2R> ( -- d ) ( R: d -- ) */
void double_word_2r_from(VM *vm) {
    if (vm->rsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dlow = vm->return_stack[vm->rsp--];
    cell_t dhigh = vm->return_stack[vm->rsp--];
    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/* 2R@ ( -- d ) ( R: d -- d ) */
void double_word_2r_fetch(VM *vm) {
    if (vm->rsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dlow = vm->return_stack[vm->rsp];
    cell_t dhigh = vm->return_stack[vm->rsp - 1];
    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/* D0= ( d -- flag ) */
void double_word_d_zero_equals(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    vm_push(vm, (dhigh == 0 && dlow == 0) ? -1 : 0);
}

/* D0< ( d -- flag ) */
void double_word_d_zero_less(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    vm_pop(vm); // dlow
    cell_t dhigh = vm_pop(vm);
    vm_push(vm, (dhigh < 0) ? -1 : 0);
}

/* D2* ( d1 -- d2 ) */
void double_word_d_two_star(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);

    cell_t new_dhigh = (dhigh << 1) | ((dlow < 0) ? 1 : 0);
    cell_t new_dlow = dlow << 1;

    vm_push(vm, new_dhigh);
    vm_push(vm, new_dlow);
}

/* D2/ ( d1 -- d2 ) */
void double_word_d_two_slash(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);

    cell_t new_dlow = (dlow >> 1) | ((dhigh & 1) ? (1UL << (sizeof(cell_t) * 8 - 1)) : 0);
    cell_t new_dhigh = dhigh >> 1;

    vm_push(vm, new_dhigh);
    vm_push(vm, new_dlow);
}

/* Registration */
void register_double_words(VM *vm) {
    log_message(LOG_INFO, "Registering double number words...");

    register_word(vm, "S>D", double_word_s_to_d);
    register_word(vm, "D+", double_word_d_plus);
    register_word(vm, "D-", double_word_d_minus);
    register_word(vm, "DNEGATE", double_word_dnegate);
    register_word(vm, "DABS", double_word_dabs);
    register_word(vm, "DMAX", double_word_dmax);
    register_word(vm, "DMIN", double_word_dmin);
    register_word(vm, "D<", double_word_d_less);
    register_word(vm, "D=", double_word_d_equals);
    register_word(vm, "2DROP", double_word_2drop);
    register_word(vm, "2DUP", double_word_2dup);
    register_word(vm, "2SWAP", double_word_2swap);
    register_word(vm, "2OVER", double_word_2over);
    register_word(vm, "2ROT", double_word_2rot);
    register_word(vm, "2>R", double_word_2to_r);
    register_word(vm, "2R>", double_word_2r_from);
    register_word(vm, "2R@", double_word_2r_fetch);
    register_word(vm, "D0=", double_word_d_zero_equals);
    register_word(vm, "D0<", double_word_d_zero_less);
    register_word(vm, "D2*", double_word_d_two_star);
    register_word(vm, "D2/", double_word_d_two_slash);

    log_message(LOG_INFO, "Double number words registered and tested");
}