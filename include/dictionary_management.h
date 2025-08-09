#ifndef DICTIONARY_MANAGEMENT_H
#define DICTIONARY_MANAGEMENT_H

#include "vm.h"  /* For VM type and word_func_t */

/* Initialize dictionary structures */
void dictionary_init(VM *vm);

/* Register a word into the dictionary */
void dictionary_register(VM *vm, const char *name, word_func_t func);

/* Look up a word by name */
word_func_t dictionary_lookup(VM *vm, const char *name);

/* Clear dictionary (optional) */
void dictionary_clear(VM *vm);

#endif
