/*

                                 ***   StarForth   ***
  string_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/10/25, 4:23 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/*

                                 ***   StarForth   ***
  string_words.c - FORTH-79 Standard and ANSI C99 ONLY
  Last modified - 8/10/25, 5:35 PM
  Public Domain / CC0

 */

/* string_words.c - FORTH-79 String & Text Processing Words */
#include "include/string_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"
#include  "../../include/vm_api.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* === Helpers ============================================================= */

/* Convert a C pointer that points into vm->memory into a VM address (offset) */
static inline vaddr_t vaddr_from_ptr(VM *vm, const void *p) {
    return (vaddr_t)((const uint8_t*)p - vm->memory);
}

/* Ensure we have input buffers/vars available */
static inline int ensure_input(VM *vm) {
    int err = vm_input_ensure(vm);
    if (err) { vm->error = 1; }
    return err;
}

/* Static VM-backed scratch buffer for WORD (allocated on first use) */
#define WORD_SCRATCH_CAP 64
static vaddr_t word_scratch_addr = 0;

/* Helper function to find word in dictionary */
static DictEntry* find_word_in_dict(VM *vm, const char *name, size_t name_len) {
    DictEntry *entry = vm->latest;
    while (entry != NULL) {
        if (entry->name_len == name_len && memcmp(entry->name, name, name_len) == 0) {
            return entry;
        }
        entry = entry->link;
    }
    return NULL;
}

/* Helper function to convert string to number (base-10 only here) */
static int convert_string_to_number(const char *str, size_t len, cell_t *result) {
    char temp_str[32];
    char *endptr;
    long value;
    if (len >= sizeof(temp_str)) return 0;
    memcpy(temp_str, str, len);
    temp_str[len] = '\0';
    value = strtol(temp_str, &endptr, 10);
    if (endptr != temp_str + len) return 0;
    *result = (cell_t)value;
    return 1;
}

/* === Words =============================================================== */

/* COUNT ( addr1 -- addr2 u )  Get string length and address */
void string_word_count(VM *vm) {
    if (vm->dsp < 0) { log_message(LOG_ERROR, "COUNT: Data stack underflow"); vm->error = 1; return; }
    cell_t addr1 = vm_pop(vm);
    vaddr_t a = VM_ADDR(addr1);
    if (!vm_addr_ok(vm, a, 1)) { log_message(LOG_ERROR, "COUNT: Address out of bounds"); vm->error = 1; return; }
    uint8_t count = vm_load_u8(vm, a);
    if (!vm_addr_ok(vm, a + 1, (size_t)count)) { log_message(LOG_ERROR, "COUNT: String extends beyond memory"); vm->error = 1; return; }
    vm_push(vm, CELL(a + 1));         /* addr2 - skip count byte */
    vm_push(vm, (cell_t)count);       /* u - length */
}

/* EXPECT ( addr u -- )  Accept input line */
void string_word_expect(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    cell_t u = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    if (u < 0) { vm->error = 1; return; }
    vaddr_t a = VM_ADDR(addr);
    if (!vm_addr_ok(vm, a, (size_t)u)) { vm->error = 1; log_message(LOG_ERROR, "EXPECT: invalid buffer range"); return; }
    char *buffer = (char*)vm_ptr(vm, a);
    if (!buffer) { vm->error = 1; return; }

    char *input = fgets(buffer, (int)u, stdin);
    cell_t actual_len = 0;
    if (input) {
        size_t L = strlen(buffer);
        if (L > 0 && buffer[L-1] == '\n') { buffer[L-1] = '\0'; L--; }
        actual_len = (cell_t)L;
    }
    /* Update SPAN */
    if (!ensure_input(vm)) { cell_t *span = vm_input_span(vm); if (span) *span = actual_len; }
}

/* SPAN ( -- addr )  Variable: number of characters input (VM address) */
void string_word_span(VM *vm) {
    if (ensure_input(vm)) return;
    cell_t *p = vm_input_span(vm);
    if (!p) { vm->error = 1; return; }
    vaddr_t v = vaddr_from_ptr(vm, p);
    vm_push(vm, CELL(v));
}

/* QUERY ( -- )  Accept input into TIB */
void string_word_query(VM *vm) {
    if (ensure_input(vm)) return;
    unsigned char *tib = vm_input_tib(vm);
    cell_t *inp = vm_input_in(vm);
    cell_t *span = vm_input_span(vm);
    if (!tib || !inp || !span) { vm->error = 1; return; }

    char *dst = (char*)tib;
    char *input = fgets(dst, (int)INPUT_BUFFER_SIZE, stdin);
    if (!input) {
        tib[0] = 0;
        *span = 0;
        *inp  = 0;
        return;
    }
    size_t L = strlen(dst);
    if (L > 0 && dst[L-1] == '\n') { dst[L-1] = '\0'; L--; }
    *span = (cell_t)L;
    *inp  = 0;
}

/* TIB ( -- addr )  Terminal input buffer address (VM address) */
void string_word_tib(VM *vm) {
    if (ensure_input(vm)) return;
    unsigned char *p = vm_input_tib(vm);
    if (!p) { vm->error = 1; return; }
    vaddr_t v = vaddr_from_ptr(vm, p);
    vm_push(vm, CELL(v));
}

/* WORD ( c -- addr )  Parse next word, delimited by c (returns VM addr of counted string) */
void string_word_word(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    if (ensure_input(vm)) return;

    cell_t c = vm_pop(vm);
    char delimiter = (char)(c & 0xFF);

    unsigned char *tib = vm_input_tib(vm);
    cell_t *inp = vm_input_in(vm);
    cell_t *span = vm_input_span(vm);
    if (!tib || !inp || !span) { vm->error = 1; return; }

    size_t tib_len = (size_t)((*span < 0) ? 0 : *span);
    cell_t ip = (*inp < 0) ? 0 : *inp;

    /* Skip leading delimiters */
    while ((size_t)ip < tib_len && tib[ip] == (unsigned char)delimiter) ip++;

    size_t start = (size_t)ip;
    while ((size_t)ip < tib_len && tib[ip] != (unsigned char)delimiter) ip++;
    size_t end = (size_t)ip;
    size_t word_len = end - start;
    if (word_len > WORD_SCRATCH_CAP - 2) word_len = WORD_SCRATCH_CAP - 2;

    /* Allocate scratch once */
    if (word_scratch_addr == 0) {
        void *p = vm_allot(vm, WORD_SCRATCH_CAP);
        if (!p) { vm->error = 1; return; }
        word_scratch_addr = vaddr_from_ptr(vm, p);
    }

    /* Build counted string at scratch */
    uint8_t *wb = vm_ptr(vm, word_scratch_addr);
    if (!wb) { vm->error = 1; return; }
    wb[0] = (uint8_t)word_len;
    if (word_len) memcpy(&wb[1], &tib[start], word_len);
    if (word_len + 1 < WORD_SCRATCH_CAP) wb[word_len+1] = '\0'; /* safety */

    /* Update >IN to char after the delimiter (if any) */
    while ((size_t)ip < tib_len && tib[ip] == (unsigned char)delimiter) ip++;
    *inp = ip;

    vm_push(vm, CELL(word_scratch_addr));
}

/* >IN ( -- addr )  Input stream pointer cell (VM address) */
void string_word_to_in(VM *vm) {
    if (ensure_input(vm)) return;
    cell_t *p = vm_input_in(vm);
    if (!p) { vm->error = 1; return; }
    vaddr_t v = vaddr_from_ptr(vm, p);
    vm_push(vm, CELL(v));
}

/* SOURCE ( -- addr u )  Input source specification (TIB addr, length) */
void string_word_source(VM *vm) {
    if (ensure_input(vm)) return;
    unsigned char *tib = vm_input_tib(vm);
    cell_t *span = vm_input_span(vm);
    if (!tib || !span) { vm->error = 1; return; }
    vaddr_t v = vaddr_from_ptr(vm, tib);
    cell_t len = (*span < 0) ? 0 : *span;
    vm_push(vm, CELL(v));
    vm_push(vm, len);
}

/* BL ( -- c )  ASCII space character (32) */
void string_word_bl(VM *vm) { vm_push(vm, 32); }

/* FIND ( addr -- addr|xt flag )  Find word in dictionary */
void string_word_find(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t addr = vm->data_stack[vm->dsp];  /* keep on stack */
    vaddr_t a = VM_ADDR(addr);
    if (!vm_addr_ok(vm, a, 1)) { vm_push(vm, 0); return; }
    uint8_t *counted = vm_ptr(vm, a);
    if (!counted) { vm_push(vm, 0); return; }
    uint8_t name_len = counted[0];
    if (!vm_addr_ok(vm, a+1, name_len)) { vm_push(vm, 0); return; }
    const char *name = (const char*)&counted[1];

    DictEntry *entry = find_word_in_dict(vm, name, (size_t)name_len);
    if (entry) {
        vm->data_stack[vm->dsp] = (cell_t)(uintptr_t)entry; /* xt */
        vm_push(vm, 1);
    } else {
        vm_push(vm, 0);
    }
}

/* ' ( -- xt )  Get execution token of next word */
void string_word_tick(VM *vm) {
    vm_push(vm, 32);
    string_word_word(vm);
    if (vm->error) return;
    string_word_find(vm);
    if (vm->error) return;
    if (vm->data_stack[vm->dsp] == 0) { vm->error = 1; return; }
    (void)vm_pop(vm); /* drop flag */
}

/* ['] ( -- xt )  Compile execution token (immediate) */
void string_word_bracket_tick(VM *vm) { string_word_tick(vm); }

/* LITERAL / [LITERAL] are placeholders here */
void string_word_literal(VM *vm) {}
void string_word_bracket_literal(VM *vm) { string_word_literal(vm); }

/* CONVERT ( d1 addr1 -- d2 addr2 )  Convert string to number (very simplified) */
void string_word_convert(VM *vm) {
    if (vm->dsp < 2) { vm->error = 1; return; }
    cell_t addr1 = vm_pop(vm);
    cell_t dlow  = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);

    vaddr_t a = VM_ADDR(addr1);
    if (!vm_addr_ok(vm, a, 1)) { vm->error = 1; return; }
    char *str_ptr = (char*)vm_ptr(vm, a);
    if (!str_ptr) { vm->error = 1; return; }

    char *end_ptr = str_ptr;
    cell_t digit_value;
    for (size_t i = 0; str_ptr[i] != '\0'; i++) {
        char c = str_ptr[i];
        if (c >= '0' && c <= '9') digit_value = c - '0';
        else if (c >= 'A' && c <= 'Z') digit_value = c - 'A' + 10;
        else if (c >= 'a' && c <= 'z') digit_value = c - 'a' + 10;
        else break;
        if (digit_value >= 10) break; /* base 10 only here */
        dlow = dlow * 10 + digit_value;
        if (dlow < 0) dhigh++;
        end_ptr++;
    }

    vm_push(vm, dhigh);
    vm_push(vm, dlow);
    /* return VM addr for end_ptr */
    vaddr_t a2 = vaddr_from_ptr(vm, end_ptr);
    vm_push(vm, CELL(a2));
}

/* NUMBER ( addr -- n flag )  Convert counted string to number (base-10 only) */
void string_word_number(VM *vm) {
    if (vm->dsp < 0) { vm->error = 1; return; }
    cell_t addr = vm_pop(vm);
    vaddr_t a = VM_ADDR(addr);
    if (!vm_addr_ok(vm, a, 1)) { vm_push(vm, 0); vm_push(vm, 0); return; }
    uint8_t *counted = vm_ptr(vm, a);
    if (!counted) { vm_push(vm, 0); vm_push(vm, 0); return; }
    uint8_t len = counted[0];
    if (!vm_addr_ok(vm, a+1, len)) { vm_push(vm, 0); vm_push(vm, 0); return; }
    const char *data = (const char*)&counted[1];

    cell_t result;
    if (convert_string_to_number(data, (size_t)len, &result)) {
        vm_push(vm, result);
        vm_push(vm, 1);
    } else {
        vm_push(vm, 0);
        vm_push(vm, 0);
    }
}

/* ENCLOSE ( addr c -- addr1 n1 n2 n3 )  Parse for delimiter */
void string_word_enclose(VM *vm) {
    if (vm->dsp < 1) { vm->error = 1; return; }
    cell_t c = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    vaddr_t a = VM_ADDR(addr);
    if (!vm_addr_ok(vm, a, 1)) { vm->error = 1; return; }
    char *str = (char*)vm_ptr(vm, a);
    if (!str) { vm->error = 1; return; }

    char delimiter = (char)(c & 0xFF);
    size_t len = strlen(str);
    size_t i = 0;
    cell_t n1=0, n2=0, n3=0;

    for (i = 0; i < len && str[i] == delimiter; i++) {}
    n1 = (cell_t)i;
    for (; i < len && str[i] != delimiter; i++) {}
    n2 = (cell_t)i;
    for (; i < len && str[i] == delimiter; i++) {}
    n3 = (cell_t)i;

    vm_push(vm, addr); /* same addr back */
    vm_push(vm, n1);
    vm_push(vm, n2);
    vm_push(vm, n3);
}

/* FORTH-79 String Word Registration */
void register_string_words(VM *vm) {
    log_message(LOG_INFO, "Registering string & text processing words...");

    register_word(vm, "COUNT",   string_word_count);
    register_word(vm, "EXPECT",  string_word_expect);
    register_word(vm, "SPAN",    string_word_span);
    register_word(vm, "QUERY",   string_word_query);
    register_word(vm, "TIB",     string_word_tib);
    register_word(vm, "WORD",    string_word_word);
    register_word(vm, ">IN",     string_word_to_in);
    register_word(vm, "SOURCE",  string_word_source);
    register_word(vm, "BL",      string_word_bl);
    register_word(vm, "FIND",    string_word_find);
    register_word(vm, "'",       string_word_tick);
    register_word(vm, "[']",     string_word_bracket_tick);
    register_word(vm, "LITERAL", string_word_literal);
    register_word(vm, "[LITERAL]", string_word_bracket_literal);
    register_word(vm, "CONVERT", string_word_convert);
    register_word(vm, "NUMBER",  string_word_number);
    register_word(vm, "ENCLOSE", string_word_enclose);
}
