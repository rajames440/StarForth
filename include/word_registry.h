/* word_registry.h */
#ifndef WORD_REGISTRY_H
#define WORD_REGISTRY_H

#include "vm.h"

/* Register core words */
void register_core_words(VM *vm);

/* Register math words */
void register_math_words(VM *vm);

/* Register all Forth-79 words */
void register_forth79_words(VM *vm);

/* Lookup a word function pointer by name */
word_func_t lookup_word(VM *vm, const char *name);

#endif /* WORD_REGISTRY_H */