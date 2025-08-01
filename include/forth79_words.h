#ifndef FORTH79_WORDS_H
#define FORTH79_WORDS_H

#include "vm.h"

/* Function pointer type for word implementations */
typedef void (*word_func_t)(VM *vm);

/* Register the built-in Forth-79 words into the VM dictionary */
void register_forth79_words(VM *vm);

/* Lookup a word function by its name; returns NULL if not found */
word_func_t lookup_word(const char *name);

#endif /* FORTH79_WORDS_H */
