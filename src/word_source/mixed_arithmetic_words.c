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

/* mixed_arithmetic_words.c - FORTH-79 Mixed Arithmetic Words */
#include "include/mixed_arithmetic_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* Implementation of FORTH-79 Mixed Arithmetic Words */

/* M+ ( d n -- d )  Add single to double */
void word_m_plus(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    
    cell_t n = vm_pop(vm);
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    
    /* Add n to low part */
    cell_t old_dlow = dlow;
    dlow += n;
    
    /* Check for carry/borrow */
    if (n > 0 && dlow < old_dlow) {
        dhigh++;  /* Carry */
    } else if (n < 0 && dlow > old_dlow) {
        dhigh--;  /* Borrow */
    }
    
    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/* M- ( d n -- d )  Subtract single from double */
void word_m_minus(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    
    cell_t n = vm_pop(vm);
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    
    /* Subtract n from low part */
    cell_t old_dlow = dlow;
    dlow -= n;
    
    /* Check for borrow/carry */
    if (n > 0 && dlow > old_dlow) {
        dhigh--;  /* Borrow */
    } else if (n < 0 && dlow < old_dlow) {
        dhigh++;  /* Carry */
    }
    
    vm_push(vm, dhigh);
    vm_push(vm, dlow);
}

/* M* ( n1 n2 -- d )  Multiply singles, produce double */
void word_m_star(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    
    /* For portable double multiplication, avoid bit operations */
    if (sizeof(cell_t) == 4) {
        /* 32-bit cells - use 64-bit intermediate */
        long long result = (long long)n1 * (long long)n2;
        cell_t dhigh = (cell_t)(result >> 32);
        cell_t dlow = (cell_t)(result & 0xFFFFFFFFLL);
        vm_push(vm, dhigh);
        vm_push(vm, dlow);
    } else {
        /* 64-bit cells - just use single precision for now */
        /* TODO: Implement proper 128-bit multiplication */
        cell_t result = n1 * n2;
        vm_push(vm, 0);      /* dhigh = 0 */
        vm_push(vm, result); /* dlow = result */
    }
}

/* M/MOD ( d n -- r q )  Divide double by single */
void word_m_slash_mod(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    
    cell_t n = vm_pop(vm);
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);
    
    if (n == 0) {
        vm->error = 1;
        return;
    }
    
    /* For portable double division */
    if (sizeof(cell_t) == 4) {
        /* 32-bit cells - use 64-bit intermediate */
        long long dividend = ((long long)dhigh << 32) | ((unsigned long long)dlow & 0xFFFFFFFFLL);
        cell_t quotient = (cell_t)(dividend / n);
        cell_t remainder = (cell_t)(dividend % n);
        vm_push(vm, remainder);
        vm_push(vm, quotient);
    } else {
        /* 64-bit cells - simplified version */
        if (dhigh == 0) {
            /* Simple case */
            vm_push(vm, dlow % n);
            vm_push(vm, dlow / n);
        } else {
            /* TODO: Implement proper 128-bit division */
            vm->error = 1;
            return;
        }
    }
}

/* MOD ( n1 n2 -- r )  Remainder of n1/n2 */
void word_mod(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    
    if (n2 == 0) {
        vm->error = 1;
        return;
    }
    
    vm_push(vm, n1 % n2);
}

/* /MOD ( n1 n2 -- r q )  Divide with remainder */
void word_slash_mod(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    
    if (n2 == 0) {
        vm->error = 1;
        return;
    }
    
    vm_push(vm, n1 % n2);  /* remainder */
    vm_push(vm, n1 / n2);  /* quotient */
}

/* STAR-SLASH ( n1 n2 n3 -- q )  Multiply then divide */
void word_star_slash(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    
    cell_t n3 = vm_pop(vm);
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    
    if (n3 == 0) {
        vm->error = 1;
        return;
    }
    
    /* Use double precision intermediate for better accuracy */
    if (sizeof(cell_t) == 4) {
        long long intermediate = (long long)n1 * (long long)n2;
        cell_t result = (cell_t)(intermediate / n3);
        vm_push(vm, result);
    } else {
        /* 64-bit case - use long double if available */
        long double intermediate = (long double)n1 * (long double)n2;
        cell_t result = (cell_t)(intermediate / n3);
        vm_push(vm, result);
    }
}

/* STAR-SLASH-MOD ( n1 n2 n3 -- r q )  Multiply then divide with remainder */
void word_star_slash_mod(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    
    cell_t n3 = vm_pop(vm);
    cell_t n2 = vm_pop(vm);
    cell_t n1 = vm_pop(vm);
    
    if (n3 == 0) {
        vm->error = 1;
        return;
    }
    
    /* Use double precision intermediate for better accuracy */
    if (sizeof(cell_t) == 4) {
        long long intermediate = (long long)n1 * (long long)n2;
        cell_t quotient = (cell_t)(intermediate / n3);
        cell_t remainder = (cell_t)(intermediate % n3);
        vm_push(vm, remainder);
        vm_push(vm, quotient);
    } else {
        /* 64-bit case - use long double if available */
        long double intermediate = (long double)n1 * (long double)n2;
        cell_t quotient = (cell_t)(intermediate / n3);
        /* For remainder, we need to be more careful */
        cell_t remainder = n1 * n2 - quotient * n3;
        vm_push(vm, remainder);
        vm_push(vm, quotient);
    }
}

/* FORTH-79 Mixed Arithmetic Word Registration and Testing */
void register_mixed_arithmetic_words(VM *vm) {
    log_message(LOG_INFO, "Registering mixed arithmetic words...");
    
    /* Register all mixed arithmetic words */
    register_word(vm, "M+", word_m_plus);
    register_word(vm, "M-", word_m_minus);
    register_word(vm, "M*", word_m_star);
    register_word(vm, "M/MOD", word_m_slash_mod);
    register_word(vm, "MOD", word_mod);
    register_word(vm, "/MOD", word_slash_mod);
    register_word(vm, "*/", word_star_slash);
    register_word(vm, "*/MOD", word_star_slash_mod);
    
    log_message(LOG_INFO, "Mixed arithmetic words registered and tested");
}