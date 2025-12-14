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

/* double_words.c - FORTH-79 Double Number Words */
#include "include/double_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/**
 * @brief Convert single number to double ( n -- d )
 *
 * Converts a single-precision number to double-precision format.
 * Result is pushed as ( dlow dhigh ) with high cell on top.
 *
 * @param vm Pointer to the virtual machine state
 */
void double_word_s_to_d(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }

    cell_t n = vm_pop(vm);
    cell_t dhigh = (n < 0) ? -1 : 0;
    vm_push(vm, n); // push low
    vm_push(vm, dhigh); // push high
}

/* D+ ( d1 d2 -- d3 ) */
void double_word_d_plus(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    // Stack layout: d1low d1high d2low d2high (top)
    // Pop from top: d2high, d2low, d1high, d1low
    cell_t d2high = vm_pop(vm);
    cell_t d2low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);

    // Use unsigned arithmetic for carry detection
    unsigned long ud1low = (unsigned long) d1low;
    unsigned long ud2low = (unsigned long) d2low;
    unsigned long uresult_low = ud1low + ud2low;

    cell_t result_low = (cell_t) uresult_low;
    cell_t carry = (uresult_low < ud1low) ? 1 : 0;
    cell_t result_high = d1high + d2high + carry;

    // Push result: low first, then high (so high is on top)
    vm_push(vm, result_low);
    vm_push(vm, result_high);
}

/* D- ( d1 d2 -- d3 ) */
void double_word_d_minus(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    // Stack layout: d1low d1high d2low d2high (top)
    // Pop from top: d2high, d2low, d1high, d1low
    cell_t d2high = vm_pop(vm);
    cell_t d2low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);

    // Use unsigned arithmetic for borrow detection
    unsigned long ud1low = (unsigned long) d1low;
    unsigned long ud2low = (unsigned long) d2low;
    unsigned long uresult_low = ud1low - ud2low;

    cell_t result_low = (cell_t) uresult_low;
    cell_t borrow = (ud1low < ud2low) ? 1 : 0;
    cell_t result_high = d1high - d2high - borrow;

    // Push result: low first, then high (so high is on top)
    vm_push(vm, result_low);
    vm_push(vm, result_high);
}

/* DNEGATE ( d1 -- d2 ) */
void double_word_dnegate(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    // Stack layout: dlow dhigh (top)
    cell_t dhigh = vm_pop(vm);
    cell_t dlow = vm_pop(vm);

    // Negate: ~d + 1
    unsigned long new_low = ~(unsigned long) dlow + 1;
    cell_t new_dhigh = ~dhigh + (new_low == 0 ? 1 : 0);

    vm_push(vm, (cell_t) new_low);
    vm_push(vm, new_dhigh);
}

/* DABS ( d1 -- d2 ) */
void double_word_dabs(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    // High cell is on top of stack (dsp points to top)
    cell_t dhigh = vm->data_stack[vm->dsp];
    if (dhigh < 0) {
        double_word_dnegate(vm);
    }
}

/**
 * @brief Compare two double numbers
 *
 * @param d1h High cell of first double number
 * @param d1l Low cell of first double number
 * @param d2h High cell of second double number
 * @param d2l Low cell of second double number
 * @return int -1 if d1<d2, 0 if d1=d2, 1 if d1>d2
 */
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

    cell_t d2high = vm_pop(vm);
    cell_t d2low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);

    if (d_compare(d1high, d1low, d2high, d2low) >= 0) {
        vm_push(vm, d1low);
        vm_push(vm, d1high);
    } else {
        vm_push(vm, d2low);
        vm_push(vm, d2high);
    }
}

/* DMIN ( d1 d2 -- d3 ) */
void double_word_dmin(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2high = vm_pop(vm);
    cell_t d2low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);

    if (d_compare(d1high, d1low, d2high, d2low) <= 0) {
        vm_push(vm, d1low);
        vm_push(vm, d1high);
    } else {
        vm_push(vm, d2low);
        vm_push(vm, d2high);
    }
}

/* D< ( d1 d2 -- flag ) */
void double_word_d_less(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2high = vm_pop(vm);
    cell_t d2low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);

    int cmp = d_compare(d1high, d1low, d2high, d2low);
    vm_push(vm, (cmp < 0) ? -1 : 0);
}

/* D= ( d1 d2 -- flag ) */
void double_word_d_equals(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2high = vm_pop(vm);
    cell_t d2low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);

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

    // Stack: dlow dhigh (top)
    cell_t dhigh = vm->data_stack[vm->dsp];
    cell_t dlow = vm->data_stack[vm->dsp - 1];
    vm_push(vm, dlow);
    vm_push(vm, dhigh);
}

/* 2SWAP ( d1 d2 -- d2 d1 ) */
void double_word_2swap(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    cell_t d2high = vm_pop(vm);
    cell_t d2low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);

    vm_push(vm, d2low);
    vm_push(vm, d2high);
    vm_push(vm, d1low);
    vm_push(vm, d1high);
}

/* 2OVER ( d1 d2 -- d1 d2 d1 ) */
void double_word_2over(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }

    // Stack: d1low d1high d2low d2high (top)
    // Indices: dsp-3  dsp-2  dsp-1  dsp
    cell_t d1low = vm->data_stack[vm->dsp - 3];
    cell_t d1high = vm->data_stack[vm->dsp - 2];
    vm_push(vm, d1low);
    vm_push(vm, d1high);
}

/* 2ROT ( d1 d2 d3 -- d2 d3 d1 ) */
void double_word_2rot(VM *vm) {
    if (vm->dsp < 5) {
        vm->error = 1;
        return;
    }

    cell_t d3high = vm_pop(vm);
    cell_t d3low = vm_pop(vm);
    cell_t d2high = vm_pop(vm);
    cell_t d2low = vm_pop(vm);
    cell_t d1high = vm_pop(vm);
    cell_t d1low = vm_pop(vm);

    vm_push(vm, d2low);
    vm_push(vm, d2high);
    vm_push(vm, d3low);
    vm_push(vm, d3high);
    vm_push(vm, d1low);
    vm_push(vm, d1high);
}

/* 2>R ( d -- ) ( R: -- d ) */
void double_word_2to_r(VM *vm) {
    if (vm->dsp < 1 || vm->rsp >= STACK_SIZE - 2) {
        vm->error = 1;
        return;
    }

    cell_t dhigh = vm_pop(vm);
    cell_t dlow = vm_pop(vm);
    vm->return_stack[++vm->rsp] = dlow;
    vm->return_stack[++vm->rsp] = dhigh;
}

/* 2R> ( -- d ) ( R: d -- ) */
void double_word_2r_from(VM *vm) {
    if (vm->rsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dhigh = vm->return_stack[vm->rsp--];
    cell_t dlow = vm->return_stack[vm->rsp--];
    vm_push(vm, dlow);
    vm_push(vm, dhigh);
}

/* 2R@ ( -- d ) ( R: d -- d ) */
void double_word_2r_fetch(VM *vm) {
    if (vm->rsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dhigh = vm->return_stack[vm->rsp];
    cell_t dlow = vm->return_stack[vm->rsp - 1];
    vm_push(vm, dlow);
    vm_push(vm, dhigh);
}

/* D0= ( d -- flag ) */
void double_word_d_zero_equals(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dhigh = vm_pop(vm);
    cell_t dlow = vm_pop(vm);
    vm_push(vm, (dhigh == 0 && dlow == 0) ? -1 : 0);
}

/* D0< ( d -- flag ) */
void double_word_d_zero_less(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dhigh = vm_pop(vm);
    vm_pop(vm); // dlow (unused, but must pop)
    vm_push(vm, (dhigh < 0) ? -1 : 0);
}

/* D2* ( d1 -- d2 ) */
void double_word_d_two_star(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dhigh = vm_pop(vm);
    cell_t dlow = vm_pop(vm);

    // Left shift: new_low = dlow << 1, new_high = (dhigh << 1) | (carry from dlow)
    unsigned long udlow = (unsigned long) dlow;
    cell_t new_dlow = (cell_t)(udlow << 1);
    cell_t new_dhigh = (dhigh << 1) | ((udlow >> 63) & 1);

    vm_push(vm, new_dlow);
    vm_push(vm, new_dhigh);
}

/* D2/ ( d1 -- d2 ) */
void double_word_d_two_slash(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }

    cell_t dhigh = vm_pop(vm);
    cell_t dlow = vm_pop(vm);

    // Arithmetic right shift: preserve sign in high cell
    cell_t new_dhigh = dhigh >> 1;
    unsigned long udlow = (unsigned long) dlow;
    cell_t new_dlow = (cell_t)((udlow >> 1) | ((dhigh & 1) ? (1UL << 63) : 0));

    vm_push(vm, new_dlow);
    vm_push(vm, new_dhigh);
}

/**
 * @brief Register all double number words
 *
 * Registers all double-precision number operations with the virtual machine.
 *
 * @param vm Pointer to the virtual machine state
 *
 * @note This function must be called during VM initialization.
 */
void register_double_words(VM *vm) {
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
}