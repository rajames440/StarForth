// src/word_source/core_words.h
#ifndef CORE_WORDS_H
#define CORE_WORDS_H

#include "vm.h"

/* Core Forth-79 word function prototypes */
void word_dot(VM *vm);
void word_drop(VM *vm);
void word_dup(VM *vm);
void word_swap(VM *vm);
void word_over(VM *vm);
void word_rot(VM *vm);
void word_minus_rot(VM *vm);
void word_depth(VM *vm);
void word_emit(VM *vm);
void word_cr(VM *vm);
void word_key(VM *vm);
void word_tuck(VM *vm);
void word_store(VM *vm);     /* '!' */
void word_fetch(VM *vm);     /* '@' */
void word_plus_store(VM *vm);/* '+!' */
void word_lit(VM *vm);
void word_branch(VM *vm);
void word_zero_branch(VM *vm);
void word_semicolon(VM *vm); /* ';' */
void word_colon(VM *vm);     /* ':' */
void word_immediate(VM *vm);
void word_exit(VM *vm);
void word_nop(VM *vm);

/* Register all core words */
void register_core_words(VM *vm);

#endif /* CORE_WORDS_H */
