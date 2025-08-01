#include "forth79_words.h"
#include <stdio.h>
#include <string.h>

#define MAX_WORDS 16

typedef struct {
    const char *name;
    word_func_t func;
} Word;

static Word dictionary[MAX_WORDS];
static int word_count = 0;

/* Registers a word in the dictionary */
static void register_word(const char *name, word_func_t func) {
    if (word_count < MAX_WORDS) {
        dictionary[word_count].name = name;
        dictionary[word_count].func = func;
        word_count++;
    }
}

/* Word implementations */

static void word_plus(VM *vm) {
    cell_t b = vm_pop(vm);
    cell_t a = vm_pop(vm);
    vm_push(vm, a + b);
}

static void word_minus(VM *vm) {
    cell_t b = vm_pop(vm);
    cell_t a = vm_pop(vm);
    vm_push(vm, a - b);
}

static void word_dot(VM *vm) {
    cell_t val = vm_pop(vm);
    printf("%d\n", val);
}

/* Registers all the basic Forth-79 words */
void register_forth79_words(VM *vm) {
    register_word("+", word_plus);
    register_word("-", word_minus);
    register_word(".", word_dot);
    printf("Registered %d basic words.\n", word_count);
}

/* Lookup a word by name */
word_func_t lookup_word(const char *name) {
    int i;
    for (i = 0; i < word_count; i++) {
        if (strcmp(dictionary[i].name, name) == 0) {
            return dictionary[i].func;
        }
    }
    return (word_func_t)0;
}
