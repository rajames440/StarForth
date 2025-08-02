/* word_registry.c */
#include "word_registry.h"
#include "word_source/core_words.h"  /* Correct path */
#include "math_words.h"              /* Add this include */
#include <string.h>
#include <stddef.h>  /* for NULL */
#include <stdio.h>

#define MAX_DICTIONARY_SIZE 65535

typedef struct {
    const char *name;
    word_func_t func;
} WordEntry;

static WordEntry dictionary[MAX_DICTIONARY_SIZE];
static int dictionary_count = 0;

/* Helper to register a single word */
void register_word(VM *vm, const char *name, word_func_t func) {
    (void)vm;  /* unused param for now */
    if (dictionary_count < MAX_DICTIONARY_SIZE) {
        dictionary[dictionary_count].name = name;
        dictionary[dictionary_count].func = func;
        dictionary_count++;
    } else {
        fprintf(stderr, "Dictionary full, cannot register word: %s\n", name);
    }
}

/* Lookup word by name */
word_func_t lookup_word(VM *vm, const char *name) {
    (void)vm;  /* unused param for now */
    int i;
    for (i = 0; i < dictionary_count; i++) {
        if (strcmp(dictionary[i].name, name) == 0) {
            return dictionary[i].func;
        }
    }
    return NULL;
}

/* Register all Forth-79 words */
void register_forth79_words(VM *vm) {
    register_core_words(vm);
    register_math_words(vm);  /* Add this line */
    /* Future: register other word sets here */
}