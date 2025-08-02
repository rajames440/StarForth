#include "math_words.h"
#include "log.h"
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

/* Helper stack operations */
static int pop1(VM *vm, cell_t *val) {
    if (vm->data_sp <= 0) {
        log_message(LOG_ERROR, "Stack underflow");
        vm->error = 1;
        return 0;
    }
    *val = vm->data_stack[--vm->data_sp];
    return 1;
}

static int pop2(VM *vm, cell_t *a, cell_t *b) {
    if (vm->data_sp < 2) {
        log_message(LOG_ERROR, "Stack underflow");
        vm->error = 1;
        return 0;
    }
    *b = vm->data_stack[--vm->data_sp];
    *a = vm->data_stack[--vm->data_sp];
    return 1;
}

static int pop3(VM *vm, cell_t *a, cell_t *b, cell_t *c) {
    if (vm->data_sp < 3) {
        log_message(LOG_ERROR, "Stack underflow");
        vm->error = 1;
        return 0;
    }
    *c = vm->data_stack[--vm->data_sp];
    *b = vm->data_stack[--vm->data_sp];
    *a = vm->data_stack[--vm->data_sp];
    return 1;
}

static int pop_double(VM *vm, int64_t *val) {
    if (vm->data_sp < 2) {
        log_message(LOG_ERROR, "Stack underflow for double");
        vm->error = 1;
        return 0;
    }
    cell_t high = vm->data_stack[--vm->data_sp];
    cell_t low = vm->data_stack[--vm->data_sp];
    *val = ((int64_t)high << 32) | (uint32_t)low;
    return 1;
}

static void push1(VM *vm, cell_t val) {
    if (vm->data_sp >= STACK_SIZE) {
        log_message(LOG_ERROR, "Stack overflow");
        vm->error = 1;
        return;
    }
    vm->data_stack[vm->data_sp++] = val;
    vm->error = 0;
}

static void push_double(VM *vm, int64_t val) {
    if (vm->data_sp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "Stack overflow for double");
        vm->error = 1;
        return;
    }
    vm->data_stack[vm->data_sp++] = (cell_t)(val & 0xFFFFFFFF);
    vm->data_stack[vm->data_sp++] = (cell_t)((val >> 32) & 0xFFFFFFFF);
    vm->error = 0;
}

/* Single precision arithmetic */
void word_plus(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, a + b);
}

void word_minus(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, a - b);
}

void word_multiply(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, a * b);
}

void word_divide(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    if (b == 0) {
        log_message(LOG_ERROR, "Division by zero");
        vm->error = 1;
        return;
    }
    push1(vm, a / b);
}

void word_mod(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    if (b == 0) {
        log_message(LOG_ERROR, "Division by zero in MOD");
        vm->error = 1;
        return;
    }
    push1(vm, a % b);
}

void word_div_mod(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    if (b == 0) {
        log_message(LOG_ERROR, "Division by zero in /MOD");
        vm->error = 1;
        return;
    }
    push1(vm, a % b);
    push1(vm, a / b);
}

void word_multiply_divide(VM *vm) {
    cell_t a, b, c;
    if (!pop3(vm, &a, &b, &c)) return;
    if (c == 0) {
        log_message(LOG_ERROR, "Division by zero in */");
        vm->error = 1;
        return;
    }
    int64_t intermediate = (int64_t)a * b;
    push1(vm, (cell_t)(intermediate / c));
}

void word_multiply_divide_mod(VM *vm) {
    cell_t a, b, c;
    if (!pop3(vm, &a, &b, &c)) return;
    if (c == 0) {
        log_message(LOG_ERROR, "Division by zero in */MOD");
        vm->error = 1;
        return;
    }
    int64_t intermediate = (int64_t)a * b;
    push1(vm, (cell_t)(intermediate % c));
    push1(vm, (cell_t)(intermediate / c));
}

void word_abs(VM *vm) {
    cell_t val;
    if (!pop1(vm, &val)) return;
    push1(vm, val < 0 ? -val : val);
}

void word_negate(VM *vm) {
    cell_t val;
    if (!pop1(vm, &val)) return;
    push1(vm, -val);
}

void word_max(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, a > b ? a : b);
}

void word_min(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, a < b ? a : b);
}

/* Double precision arithmetic */
void word_d_plus(VM *vm) {
    int64_t a, b;
    if (!pop_double(vm, &b)) return;
    if (!pop_double(vm, &a)) return;
    push_double(vm, a + b);
}

void word_d_minus(VM *vm) {
    int64_t a, b;
    if (!pop_double(vm, &b)) return;
    if (!pop_double(vm, &a)) return;
    push_double(vm, a - b);
}

void word_d_negate(VM *vm) {
    int64_t val;
    if (!pop_double(vm, &val)) return;
    push_double(vm, -val);
}

void word_d_abs(VM *vm) {
    int64_t val;
    if (!pop_double(vm, &val)) return;
    push_double(vm, val < 0 ? -val : val);
}

void word_d_max(VM *vm) {
    int64_t a, b;
    if (!pop_double(vm, &b)) return;
    if (!pop_double(vm, &a)) return;
    push_double(vm, a > b ? a : b);
}

void word_d_min(VM *vm) {
    int64_t a, b;
    if (!pop_double(vm, &b)) return;
    if (!pop_double(vm, &a)) return;
    push_double(vm, a < b ? a : b);
}

/* Conversion words */
void word_s_to_d(VM *vm) {
    cell_t val;
    if (!pop1(vm, &val)) return;
    push_double(vm, (int64_t)val);
}

void word_d_to_s(VM *vm) {
    int64_t val;
    if (!pop_double(vm, &val)) return;
    if (val > INT32_MAX || val < INT32_MIN) {
        log_message(LOG_WARN, "Double to single conversion may overflow");
    }
    push1(vm, (cell_t)val);
}

/* Comparison operators */
void word_equal(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, (a == b) ? -1 : 0);
}

void word_not_equal(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, (a != b) ? -1 : 0);
}

void word_less_than(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, (a < b) ? -1 : 0);
}

void word_greater_than(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, (a > b) ? -1 : 0);
}

void word_less_equal(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, (a <= b) ? -1 : 0);
}

void word_greater_equal(VM *vm) {
    cell_t a, b;
    if (!pop2(vm, &a, &b)) return;
    push1(vm, (a >= b) ? -1 : 0);
}

void word_zero_equal(VM *vm) {
    cell_t val;
    if (!pop1(vm, &val)) return;
    push1(vm, (val == 0) ? -1 : 0);
}

void word_zero_less(VM *vm) {
    cell_t val;
    if (!pop1(vm, &val)) return;
    push1(vm, (val < 0) ? -1 : 0);
}

void word_zero_greater(VM *vm) {
    cell_t val;
    if (!pop1(vm, &val)) return;
    push1(vm, (val > 0) ? -1 : 0);
}

/* Double comparisons */
void word_d_equal(VM *vm) {
    int64_t a, b;
    if (!pop_double(vm, &b)) return;
    if (!pop_double(vm, &a)) return;
    push1(vm, (a == b) ? -1 : 0);
}

void word_d_less_than(VM *vm) {
    int64_t a, b;
    if (!pop_double(vm, &b)) return;
    if (!pop_double(vm, &a)) return;
    push1(vm, (a < b) ? -1 : 0);
}

void word_d_zero_equal(VM *vm) {
    int64_t val;
    if (!pop_double(vm, &val)) return;
    push1(vm, (val == 0) ? -1 : 0);
}

/* Forward declaration for register_word function */
extern void register_word(VM *vm, const char *name, word_func_t func);

/* Registration */
void register_math_words(VM *vm) {
    register_word(vm, "+", word_plus);
    register_word(vm, "-", word_minus);
    register_word(vm, "*", word_multiply);
    register_word(vm, "/", word_divide);
    register_word(vm, "MOD", word_mod);
    register_word(vm, "/MOD", word_div_mod);
    register_word(vm, "*/", word_multiply_divide);
    register_word(vm, "*/MOD", word_multiply_divide_mod);
    register_word(vm, "ABS", word_abs);
    register_word(vm, "NEGATE", word_negate);
    register_word(vm, "MAX", word_max);
    register_word(vm, "MIN", word_min);

    register_word(vm, "D+", word_d_plus);
    register_word(vm, "D-", word_d_minus);
    register_word(vm, "DNEGATE", word_d_negate);
    register_word(vm, "DABS", word_d_abs);
    register_word(vm, "DMAX", word_d_max);
    register_word(vm, "DMIN", word_d_min);

    register_word(vm, "S>D", word_s_to_d);
    register_word(vm, "D>S", word_d_to_s);

    register_word(vm, "=", word_equal);
    register_word(vm, "<>", word_not_equal);
    register_word(vm, "<", word_less_than);
    register_word(vm, ">", word_greater_than);
    register_word(vm, "<=", word_less_equal);
    register_word(vm, ">=", word_greater_equal);
    register_word(vm, "0=", word_zero_equal);
    register_word(vm, "0<", word_zero_less);
    register_word(vm, "0>", word_zero_greater);

    register_word(vm, "D=", word_d_equal);
    register_word(vm, "D<", word_d_less_than);
    register_word(vm, "D0=", word_d_zero_equal);
}
