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

/* string_words.c - FORTH-79 String & Text Processing Words */
#include "include/string_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* Global string processing state */
static char terminal_input_buffer[256];    /* TIB - Terminal Input Buffer */
static cell_t input_pointer = 0;           /* >IN - Input stream pointer */
static cell_t span_count = 0;              /* SPAN - Characters input */
static char word_buffer[64];               /* Buffer for WORD parsing */

/* Helper function to find word in dictionary */
static DictEntry* find_word_in_dict(VM *vm, const char *name, size_t name_len) {
    DictEntry *entry;
    
    entry = vm->latest;
    
    while (entry != NULL) {
        if (entry->name_len == name_len && 
            memcmp(entry->name, name, name_len) == 0) {
            return entry;
        }
        entry = entry->link;
    }
    return NULL;
}

/* Helper function to convert string to number */
static int convert_string_to_number(const char *str, size_t len, cell_t *result) {
    char temp_str[32];
    char *endptr;
    long value;
    
    if (len >= sizeof(temp_str)) {
        return 0;  /* String too long */
    }
    
    memcpy(temp_str, str, len);
    temp_str[len] = '\0';
    
    value = strtol(temp_str, &endptr, 10);
    
    if (endptr != temp_str + len) {
        return 0;  /* Invalid number */
    }
    
    *result = (cell_t)value;
    return 1;  /* Success */
}

/* COUNT ( addr1 -- addr2 u )  Get string length and address */
void word_count(VM *vm) {
    cell_t addr1;
    uint8_t *ptr;
    uint8_t count;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    addr1 = vm_pop(vm);
    ptr = (uint8_t*)(uintptr_t)addr1;
    
    if (ptr == NULL) {
        vm->error = 1;
        return;
    }
    
    /* First byte is the count */
    count = *ptr;
    
    /* Push address of string data and count */
    vm_push(vm, addr1 + 1);          /* addr2 - skip count byte */
    vm_push(vm, (cell_t)count);      /* u - length */
}

/* EXPECT ( addr u -- )  Accept input line */
void word_expect(VM *vm) {
    cell_t u, addr;
    char *buffer;
    char *input;
    size_t max_len;
    size_t actual_len;
    
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    u = vm_pop(vm);
    addr = vm_pop(vm);
    
    buffer = (char*)(uintptr_t)addr;
    max_len = (size_t)u;
    
    if (buffer == NULL || u < 0) {
        vm->error = 1;
        return;
    }
    
    /* Read input from stdin */
    input = fgets(buffer, (int)max_len, stdin);
    
    if (input == NULL) {
        span_count = 0;
        return;
    }
    
    actual_len = strlen(buffer);
    
    /* Remove trailing newline if present */
    if (actual_len > 0 && buffer[actual_len - 1] == '\n') {
        buffer[actual_len - 1] = '\0';
        actual_len--;
    }
    
    span_count = (cell_t)actual_len;
}

/* SPAN ( -- addr )  Variable: number of characters input */
void word_span(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)&span_count);
}

/* QUERY ( -- )  Accept input into TIB */
void word_query(VM *vm) {
    char *input;
    size_t len;
    
    /* Read input into terminal input buffer */
    input = fgets(terminal_input_buffer, sizeof(terminal_input_buffer), stdin);
    
    if (input == NULL) {
        terminal_input_buffer[0] = '\0';
        span_count = 0;
        input_pointer = 0;
        return;
    }
    
    len = strlen(terminal_input_buffer);
    
    /* Remove trailing newline if present */
    if (len > 0 && terminal_input_buffer[len - 1] == '\n') {
        terminal_input_buffer[len - 1] = '\0';
        len--;
    }
    
    span_count = (cell_t)len;
    input_pointer = 0;  /* Reset input pointer */
}

/* TIB ( -- addr )  Terminal input buffer address */
void word_tib(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)terminal_input_buffer);
}

/* WORD ( c -- addr )  Parse next word, delimited by c */
void word_word(VM *vm) {
    cell_t c;
    char delimiter;
    size_t tib_len;
    size_t start, end;
    size_t word_len;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    c = vm_pop(vm);
    delimiter = (char)(c & 0xFF);
    
    tib_len = strlen(terminal_input_buffer);
    
    /* Skip leading delimiters */
    while (input_pointer < (cell_t)tib_len && 
           terminal_input_buffer[input_pointer] == delimiter) {
        input_pointer++;
    }
    
    start = (size_t)input_pointer;
    
    /* Find end of word */
    while (input_pointer < (cell_t)tib_len && 
           terminal_input_buffer[input_pointer] != delimiter) {
        input_pointer++;
    }
    
    end = (size_t)input_pointer;
    word_len = end - start;
    
    /* Build counted string in word buffer */
    if (word_len >= sizeof(word_buffer) - 1) {
        word_len = sizeof(word_buffer) - 2;  /* Leave room for count and null */
    }
    
    word_buffer[0] = (char)word_len;  /* Count byte */
    memcpy(&word_buffer[1], &terminal_input_buffer[start], word_len);
    word_buffer[word_len + 1] = '\0';  /* Null terminate for safety */
    
    vm_push(vm, (cell_t)(uintptr_t)word_buffer);
}

/* >IN ( -- addr )  Input stream pointer */
void word_to_in(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)&input_pointer);
}

/* SOURCE ( -- addr u )  Input source specification */
void word_source(VM *vm) {
    vm_push(vm, (cell_t)(uintptr_t)terminal_input_buffer);
    vm_push(vm, (cell_t)strlen(terminal_input_buffer));
}

/* BL ( -- c )  ASCII space character (32) */
void word_bl(VM *vm) {
    vm_push(vm, 32);  /* ASCII space */
}

/* FIND ( addr -- addr flag )  Find word in dictionary */
void word_find(VM *vm) {
    cell_t addr;
    uint8_t *counted_str;
    uint8_t name_len;
    const char *name;
    DictEntry *entry;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    addr = vm->data_stack[vm->dsp];  /* Keep addr on stack */
    counted_str = (uint8_t*)(uintptr_t)addr;
    
    if (counted_str == NULL) {
        vm_push(vm, 0);  /* Not found */
        return;
    }
    
    name_len = counted_str[0];
    name = (const char*)&counted_str[1];
    
    entry = find_word_in_dict(vm, name, (size_t)name_len);
    
    if (entry != NULL) {
        /* Found - replace addr with execution token and push 1 */
        vm->data_stack[vm->dsp] = (cell_t)(uintptr_t)entry;
        vm_push(vm, 1);
    } else {
        /* Not found - keep addr and push 0 */
        vm_push(vm, 0);
    }
}

/* ' ( -- xt )  Get execution token of next word */
void word_tick(VM *vm) {
    vm_push(vm, 32);        /* BL - space delimiter */
    word_word(vm);          /* Parse next word */
    word_find(vm);          /* Find in dictionary */
    
    if (vm->error) return;
    
    if (vm->data_stack[vm->dsp] == 0) {
        /* Word not found */
        vm->error = 1;
        return;
    }
    
    vm_pop(vm);  /* Remove flag, leave xt */
}

/* ['] ( -- xt )  Compile execution token (immediate) */
void word_bracket_tick(VM *vm) {
    word_tick(vm);  /* Get execution token */
    if (vm->error) return;
    
    /* In a full implementation, this would compile LIT followed by the xt */
    /* For now, just leave the xt on the stack */
}

/* LITERAL ( n -- )  Compile literal number */
void word_literal(VM *vm) {
    /* In a full implementation, this would compile LIT followed by the number */
    /* For now, just leave the number on the stack */
    /* This is a compile-time word that should be used in definitions */
}

/* [LITERAL] ( n -- )  Compile literal at compile time (immediate) */
void word_bracket_literal(VM *vm) {
    word_literal(vm);
}

/* CONVERT ( d1 addr1 -- d2 addr2 )  Convert string to number */
void word_convert(VM *vm) {
    cell_t addr1, dlow, dhigh;
    char *str_ptr;
    char *end_ptr;
    cell_t digit_value;
    size_t i;
    
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    
    addr1 = vm_pop(vm);
    dlow = vm_pop(vm);
    dhigh = vm_pop(vm);
    
    str_ptr = (char*)(uintptr_t)addr1;
    
    if (str_ptr == NULL) {
        vm->error = 1;
        return;
    }
    
    /* Convert characters to digits and accumulate */
    end_ptr = str_ptr;
    for (i = 0; str_ptr[i] != '\0'; i++) {
        char c;
        
        c = str_ptr[i];
        
        if (c >= '0' && c <= '9') {
            digit_value = c - '0';
        } else if (c >= 'A' && c <= 'Z') {
            digit_value = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'z') {
            digit_value = c - 'a' + 10;
        } else {
            break;  /* Invalid character - stop conversion */
        }
        
        if (digit_value >= 10) {  /* Assume base 10 for simplicity */
            break;
        }
        
        /* Multiply double by 10 and add digit */
        /* This is a simplified implementation */
        dlow = dlow * 10 + digit_value;
        if (dlow < 0) {  /* Overflow check (simplified) */
            dhigh++;
        }
        
        end_ptr++;
    }
    
    /* Push result */
    vm_push(vm, dhigh);                           /* d2 high */
    vm_push(vm, dlow);                           /* d2 low */
    vm_push(vm, (cell_t)(uintptr_t)end_ptr);     /* addr2 */
}

/* NUMBER ( addr -- n flag )  Convert string to single number */
void word_number(VM *vm) {
    cell_t addr;
    uint8_t *counted_str;
    uint8_t str_len;
    const char *str_data;
    cell_t result;
    
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    
    addr = vm_pop(vm);
    counted_str = (uint8_t*)(uintptr_t)addr;
    
    if (counted_str == NULL) {
        vm_push(vm, 0);  /* n (dummy) */
        vm_push(vm, 0);  /* flag - failed */
        return;
    }
    
    str_len = counted_str[0];
    str_data = (const char*)&counted_str[1];
    
    if (convert_string_to_number(str_data, (size_t)str_len, &result)) {
        vm_push(vm, result);  /* n */
        vm_push(vm, 1);       /* flag - success */
    } else {
        vm_push(vm, 0);       /* n (dummy) */
        vm_push(vm, 0);       /* flag - failed */
    }
}

/* ENCLOSE ( addr c -- addr1 n1 n2 n3 )  Parse for delimiter */
void word_enclose(VM *vm) {
    cell_t c, addr;
    char delimiter;
    char *str;
    size_t len;
    size_t i;
    cell_t n1, n2, n3;  /* Start of word, end of word, end of string */
    
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    
    c = vm_pop(vm);
    addr = vm_pop(vm);
    
    delimiter = (char)(c & 0xFF);
    str = (char*)(uintptr_t)addr;
    
    if (str == NULL) {
        vm->error = 1;
        return;
    }
    
    len = strlen(str);
    
    /* Find start of word (skip leading delimiters) */
    for (i = 0; i < len && str[i] == delimiter; i++) {
        /* Skip delimiters */
    }
    n1 = (cell_t)i;
    
    /* Find end of word */
    for (; i < len && str[i] != delimiter; i++) {
        /* Skip non-delimiters */
    }
    n2 = (cell_t)i;
    
    /* Find end of string or next delimiter */
    for (; i < len && str[i] == delimiter; i++) {
        /* Skip trailing delimiters */
    }
    n3 = (cell_t)i;
    
    /* Push results */
    vm_push(vm, addr);  /* addr1 (same as input) */
    vm_push(vm, n1);    /* Start of word */
    vm_push(vm, n2);    /* End of word */
    vm_push(vm, n3);    /* Next position */
}

/* FORTH-79 String Word Registration and Testing */
void register_string_words(VM *vm) {
    log_message(LOG_INFO, "Registering string & text processing words...");
    
    /* Register all string & text processing words */
    register_word(vm, "COUNT", word_count);
    register_word(vm, "EXPECT", word_expect);
    register_word(vm, "SPAN", word_span);
    register_word(vm, "QUERY", word_query);
    register_word(vm, "TIB", word_tib);
    register_word(vm, "WORD", word_word);
    register_word(vm, ">IN", word_to_in);
    register_word(vm, "SOURCE", word_source);
    register_word(vm, "BL", word_bl);
    register_word(vm, "FIND", word_find);
    register_word(vm, "'", word_tick);
    register_word(vm, "[']", word_bracket_tick);
    register_word(vm, "LITERAL", word_literal);
    register_word(vm, "[LITERAL]", word_bracket_literal);
    register_word(vm, "CONVERT", word_convert);
    register_word(vm, "NUMBER", word_number);
    register_word(vm, "ENCLOSE", word_enclose);

    log_message(LOG_INFO, "Note: Interactive words (EXPECT, QUERY, WORD) require manual testing");
}