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

/* double_words.c - FORTH-79 Double Number Words */
#include "include/double_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* Implementation of FORTH-79 Double Number Words */

/* S>D ( n -- d )  Convert single to double */
void word_s_to_d(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    cell_t n = vm_pop(vm);
    
    /* Sign extend to create double */
    cell_t dhigh = (n < 0) ? -1 : 0;
    
    vm_push(vm, dhigh);  /* dhigh */
    vm_push(vm, n);      /* dlow */
}

/* D+ ( d1 d2 -- d3 )  Add double numbers */
void word_d_plus(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }
    
    cell_t d2_low = vm_pop(vm);
    cell_t d2_high = vm_pop(vm);
    cell_t d1_low = vm_pop(vm);
    cell_t d1_high = vm_pop(vm);
    
    /* Add low parts */
    cell_t old_d1_low = d1_low;
    cell_t result_low = d1_low + d2_low;
    
    /* Add high parts with carry */
    cell_t result_high = d1_high + d2_high;
    
    /* Check for carry from low to high */
    if ((d2_low > 0 && result_low < old_d1_low) || 
        (d2_low < 0 && result_low > old_d1_low)) {
        if (d2_low > 0) {
            result_high++;  /* Carry */
        } else {
            result_high--;  /* Borrow */
        }
    }
    
    vm_push(vm, result_high);
    vm_push(vm, result_low);
}

/* D- ( d1 d2 -- d3 )  Subtract double numbers */
void word_d_minus(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }
    
    cell_t d2_low = vm_pop(vm);
    cell_t d2_high = vm_pop(vm);
    cell_t d1_low = vm_pop(vm);
    cell_t d1_high = vm_pop(vm);
    
    /* Subtract low parts */
    cell_t old_d1_low = d1_low;
    cell_t result_low = d1_low - d2_low;
    
    /* Subtract high parts with borrow */
    cell_t result_high = d1_high - d2_high;
    
    /* Check for borrow from high to low */
    if ((d2_low > 0 && result_low > old_d1_low) || 
        (d2_low < 0 && result_low < old_d1_low)) {
        if (d2_low > 0) {
            result_high--;  /* Borrow */
        } else {
            result_high++;  /* Carry */
        }
    }
    
    vm_push(vm, result_high);
    vm_push(vm, result_low);
}

/* DNEGATE ( d1 -- d2 )  Negate double number */
void word_dnegate(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    
    /* Two's complement negation */
    dlow = ~dlow + 1;
    dhigh = ~dhigh;
    
    /* Handle carry from low part */
    if (dlow == 0) {
        dhigh++;
    }
    
    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/* DABS ( d1 -- d2 )  Absolute value of double */
void word_dabs(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    cell_t dhigh = vm->data_stack[vm->dsp - 1];
    
    /* If negative, negate */
    if (dhigh < 0) {
        word_dnegate(vm);
    }
}

/* Helper function to compare two doubles: returns -1, 0, or 1 */
static int d_compare(cell_t d1_high, cell_t d1_low, cell_t d2_high, cell_t d2_low) {
    if (d1_high < d2_high) return -1;
    if (d1_high > d2_high) return 1;
    
    /* High parts are equal, compare low parts as unsigned */
    /* Cast to unsigned for proper comparison */
    unsigned long ul1 = (unsigned long)d1_low;
    unsigned long ul2 = (unsigned long)d2_low;
    
    if (ul1 < ul2) return -1;
    if (ul1 > ul2) return 1;
    
    return 0;
}

/* DMAX ( d1 d2 -- d3 )  Maximum of two doubles */
void word_dmax(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }
    
    cell_t d2_low = vm->data_stack[vm->dsp];
    cell_t d2_high = vm->data_stack[vm->dsp - 1];
    cell_t d1_low = vm->data_stack[vm->dsp - 2];
    cell_t d1_high = vm->data_stack[vm->dsp - 3];
    
    if (d_compare(d1_high, d1_low, d2_high, d2_low) >= 0) {
        /* d1 >= d2, keep d1 */
        vm_pop(vm);  /* Remove d2_low */
        vm_pop(vm);  /* Remove d2_high */
    } else {
        /* d2 > d1, replace d1 with d2 */
        vm->data_stack[vm->dsp - 2] = d2_low;
        vm->data_stack[vm->dsp - 3] = d2_high;
        vm_pop(vm);  /* Remove d2_low */
        vm_pop(vm);  /* Remove d2_high */
    }
}

/* DMIN ( d1 d2 -- d3 )  Minimum of two doubles */
void word_dmin(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }
    
    cell_t d2_low = vm->data_stack[vm->dsp];
    cell_t d2_high = vm->data_stack[vm->dsp - 1];
    cell_t d1_low = vm->data_stack[vm->dsp - 2];
    cell_t d1_high = vm->data_stack[vm->dsp - 3];
    
    if (d_compare(d1_high, d1_low, d2_high, d2_low) <= 0) {
        /* d1 <= d2, keep d1 */
        vm_pop(vm);  /* Remove d2_low */
        vm_pop(vm);  /* Remove d2_high */
    } else {
        /* d2 < d1, replace d1 with d2 */
        vm->data_stack[vm->dsp - 2] = d2_low;
        vm->data_stack[vm->dsp - 3] = d2_high;
        vm_pop(vm);  /* Remove d2_low */
        vm_pop(vm);  /* Remove d2_high */
    }
}

/* D< ( d1 d2 -- flag )  Compare doubles: d1 < d2 */
void word_d_less(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }
    
    cell_t d2_low = vm_pop(vm);
    cell_t d2_high = vm_pop(vm);
    cell_t d1_low = vm_pop(vm);
    cell_t d1_high = vm_pop(vm);
    
    int result = d_compare(d1_high, d1_low, d2_high, d2_low);
    vm_push(vm, (result < 0) ? -1 : 0);
}

/* D= ( d1 d2 -- flag )  Compare doubles: d1 = d2 */
void word_d_equals(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }
    
    cell_t d2_low = vm_pop(vm);
    cell_t d2_high = vm_pop(vm);
    cell_t d1_low = vm_pop(vm);
    cell_t d1_high = vm_pop(vm);
    
    int result = d_compare(d1_high, d1_low, d2_high, d2_low);
    vm_push(vm, (result == 0) ? -1 : 0);
}

/* 2DROP ( d -- )  Drop double from stack */
void word_2drop(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    vm_pop(vm);  /* Drop low */
    vm_pop(vm);  /* Drop high */
}

/* 2DUP ( d -- d d )  Duplicate double */
void word_2dup(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    cell_t dlow = vm->data_stack[vm->dsp];
    cell_t dhigh = vm->data_stack[vm->dsp - 1];
    
    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/* 2SWAP ( d1 d2 -- d2 d1 )  Swap two doubles */
void word_2swap(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }
    
    cell_t d2_low = vm->data_stack[vm->dsp];
    cell_t d2_high = vm->data_stack[vm->dsp - 1];
    cell_t d1_low = vm->data_stack[vm->dsp - 2];
    cell_t d1_high = vm->data_stack[vm->dsp - 3];
    
    vm->data_stack[vm->dsp] = d1_low;
    vm->data_stack[vm->dsp - 1] = d1_high;
    vm->data_stack[vm->dsp - 2] = d2_low;
    vm->data_stack[vm->dsp - 3] = d2_high;
}

/* 2OVER ( d1 d2 -- d1 d2 d1 )  Copy second double to top */
void word_2over(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        return;
    }
    
    cell_t d1_low = vm->data_stack[vm->dsp - 2];
    cell_t d1_high = vm->data_stack[vm->dsp - 3];
    
    vm_push(vm, d1_high);
    vm_push(vm, d1_low);
}

/* 2ROT ( d1 d2 d3 -- d2 d3 d1 )  Rotate three doubles */
void word_2rot(VM *vm) {
    if (vm->dsp < 5) {
        vm->error = 1;
        return;
    }
    
    cell_t d3_low = vm->data_stack[vm->dsp];
    cell_t d3_high = vm->data_stack[vm->dsp - 1];
    cell_t d2_low = vm->data_stack[vm->dsp - 2];
    cell_t d2_high = vm->data_stack[vm->dsp - 3];
    cell_t d1_low = vm->data_stack[vm->dsp - 4];
    cell_t d1_high = vm->data_stack[vm->dsp - 5];
    
    /* Rotate: d1 d2 d3 -> d2 d3 d1 */
    vm->data_stack[vm->dsp] = d1_low;
    vm->data_stack[vm->dsp - 1] = d1_high;
    vm->data_stack[vm->dsp - 2] = d3_low;
    vm->data_stack[vm->dsp - 3] = d3_high;
    vm->data_stack[vm->dsp - 4] = d2_low;
    vm->data_stack[vm->dsp - 5] = d2_high;
}

/* 2>R ( d -- ) ( R: -- d )  Move double to return stack */
void word_2to_r(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    if (vm->rsp >= STACK_SIZE - 2) {
        vm->error = 1;  /* Return stack overflow */
        return;
    }
    
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    
    vm->return_stack[++vm->rsp] = dhigh;
    vm->return_stack[++vm->rsp] = dlow;
}

/* 2R> ( -- d ) ( R: d -- )  Move double from return stack */
void word_2r_from(VM *vm) {
    if (vm->rsp < 1) {
        vm->error = 1;  /* Return stack underflow */
        return;
    }
    
    cell_t dlow = vm->return_stack[vm->rsp--];
    cell_t dhigh = vm->return_stack[vm->rsp--];
    
    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/* 2R@ ( -- d ) ( R: d -- d )  Copy double from return stack */
void word_2r_fetch(VM *vm) {
    if (vm->rsp < 1) {
        vm->error = 1;  /* Return stack underflow */
        return;
    }
    
    cell_t dlow = vm->return_stack[vm->rsp];
    cell_t dhigh = vm->return_stack[vm->rsp - 1];
    
    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/* D0= ( d -- flag )  Test if double is zero */
void word_d_zero_equals(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    
    vm_push(vm, (dhigh == 0 && dlow == 0) ? -1 : 0);
}

/* D0< ( d -- flag )  Test if double is negative */
void word_d_zero_less(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    vm_pop(vm);  /* Pop dlow but don't need to use it */
    cell_t dhigh = vm_pop(vm);
    
    vm_push(vm, (dhigh < 0) ? -1 : 0);
}

/* D2* ( d1 -- d2 )  Multiply double by 2 */
void word_d_two_star(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    
    /* Left shift with carry */
    cell_t new_dhigh = (dhigh << 1) | ((dlow < 0) ? 1 : 0);
    cell_t new_dlow = dlow << 1;
    
    vm_push(vm, new_dhigh);
    vm_push(vm, new_dlow);
}

/* D2/ ( d1 -- d2 )  Divide double by 2 */
void word_d_two_slash(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    
    /* Arithmetic right shift with sign extension and carry propagation */
    cell_t new_dhigh = dhigh >> 1;
    cell_t new_dlow = (dlow >> 1) | ((dhigh & 1) ? (1L << (sizeof(cell_t) * 8 - 1)) : 0);
    
    vm_push(vm, new_dhigh);
    vm_push(vm, new_dlow);
}

/* FORTH-79 Double Number Word Registration and Testing */
void register_double_words(VM *vm) {
    log_message(LOG_INFO, "Registering double number words...");
    
    /* Register all double number words */
    register_word(vm, "S>D", word_s_to_d);
    register_word(vm, "D+", word_d_plus);
    register_word(vm, "D-", word_d_minus);
    register_word(vm, "DNEGATE", word_dnegate);
    register_word(vm, "DABS", word_dabs);
    register_word(vm, "DMAX", word_dmax);
    register_word(vm, "DMIN", word_dmin);
    register_word(vm, "D<", word_d_less);
    register_word(vm, "D=", word_d_equals);
    register_word(vm, "2DROP", word_2drop);
    register_word(vm, "2DUP", word_2dup);
    register_word(vm, "2SWAP", word_2swap);
    register_word(vm, "2OVER", word_2over);
    register_word(vm, "2ROT", word_2rot);
    register_word(vm, "2>R", word_2to_r);
    register_word(vm, "2R>", word_2r_from);
    register_word(vm, "2R@", word_2r_fetch);
    register_word(vm, "D0=", word_d_zero_equals);
    register_word(vm, "D0<", word_d_zero_less);
    register_word(vm, "D2*", word_d_two_star);
    register_word(vm, "D2/", word_d_two_slash);

    log_message(LOG_INFO, "Double number words registered and tested");
}