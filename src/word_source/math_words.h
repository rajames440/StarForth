#ifndef MATH_WORDS_H
#define MATH_WORDS_H

#include "vm.h"

/* Forth-79 Standard Math Words - Singles */
void word_plus(VM *vm);
void word_minus(VM *vm);
void word_multiply(VM *vm);
void word_divide(VM *vm);
void word_mod(VM *vm);
void word_div_mod(VM *vm);
void word_multiply_divide(VM *vm);
void word_multiply_divide_mod(VM *vm);
void word_abs(VM *vm);
void word_negate(VM *vm);
void word_max(VM *vm);
void word_min(VM *vm);

/* Forth-79 Standard Math Words - Doubles */
void word_d_plus(VM *vm);
void word_d_minus(VM *vm);
void word_d_negate(VM *vm);
void word_d_abs(VM *vm);
void word_d_max(VM *vm);
void word_d_min(VM *vm);

/* Conversion between singles and doubles */
void word_s_to_d(VM *vm);
void word_d_to_s(VM *vm);

/* Comparison operators */
void word_equal(VM *vm);
void word_not_equal(VM *vm);
void word_less_than(VM *vm);
void word_greater_than(VM *vm);
void word_less_equal(VM *vm);
void word_greater_equal(VM *vm);
void word_zero_equal(VM *vm);
void word_zero_less(VM *vm);
void word_zero_greater(VM *vm);

/* Double comparisons */
void word_d_equal(VM *vm);
void word_d_less_than(VM *vm);
void word_d_zero_equal(VM *vm);

/* Registration function */
void register_math_words(VM *vm);

#endif /* MATH_WORDS_H */
