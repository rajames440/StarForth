/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

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

/**
 * @brief Convert a C pointer that points into vm->memory into a VM address (offset)
 * @param vm Pointer to VM instance
 * @param p Pointer into VM memory space
 * @return Virtual address (offset) corresponding to the pointer
 */
static inline vaddr_t vaddr_from_ptr(VM *vm, const void *p) {
    return (vaddr_t)((const uint8_t *) p - vm->memory);
}

/**
 * @brief Ensure input buffers and variables are available
 * @param vm Pointer to VM instance
 * @return 0 on success, non-zero on error
 */
static inline int ensure_input(VM *vm) {
    int err = vm_input_ensure(vm);
    if (err) { vm->error = 1; }
    return err;
}

/* Static VM-backed scratch buffer for WORD (allocated on first use) */
#define WORD_SCRATCH_CAP 64
static vaddr_t word_scratch_addr = 0;

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
    *result = (cell_t) value;
    return 1;
}

/* === Words =============================================================== */

/**
 * @brief COUNT ( addr1 -- addr2 u )
 *
 * Get string length and address. Converts a counted string at addr1 
 * into an address (addr2) and length (u) pair.
 *
 * @param vm Pointer to VM instance
 */
void string_word_count(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "COUNT: Data stack underflow");
        vm->error = 1;
        return;
    }
    cell_t addr1 = vm_pop(vm);
    vaddr_t a = VM_ADDR(addr1);
    if (!vm_addr_ok(vm, a, 1)) {
        log_message(LOG_ERROR, "COUNT: Address out of bounds");
        vm->error = 1;
        return;
    }
    uint8_t count = vm_load_u8(vm, a);
    if (!vm_addr_ok(vm, a + 1, (size_t) count)) {
        log_message(LOG_ERROR, "COUNT: String extends beyond memory");
        vm->error = 1;
        return;
    }
    vm_push(vm, CELL(a + 1)); /* addr2 - skip count byte */
    vm_push(vm, (cell_t) count); /* u - length */
}

/* EXPECT ( addr u -- )  Accept input line */
void string_word_expect(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    cell_t u = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    if (u < 0) {
        vm->error = 1;
        return;
    }
    vaddr_t a = VM_ADDR(addr);
    if (!vm_addr_ok(vm, a, (size_t) u)) {
        vm->error = 1;
        log_message(LOG_ERROR, "EXPECT: invalid buffer range");
        return;
    }
    char *buffer = (char *) vm_ptr(vm, a);
    if (!buffer) {
        vm->error = 1;
        return;
    }

    char *input = fgets(buffer, (int) u, stdin);
    cell_t actual_len = 0;
    if (input) {
        size_t L = strlen(buffer);
        if (L > 0 && buffer[L - 1] == '\n') {
            buffer[L - 1] = '\0';
            L--;
        }
        actual_len = (cell_t) L;
    }
    /* Update SPAN */
    if (!ensure_input(vm)) {
        cell_t *span = vm_input_span(vm);
        if (span) *span = actual_len;
    }
}

/* SPAN ( -- addr )  Variable: number of characters input (VM address) */
void string_word_span(VM *vm) {
    if (ensure_input(vm)) return;
    cell_t *p = vm_input_span(vm);
    if (!p) {
        vm->error = 1;
        return;
    }
    vaddr_t v = vaddr_from_ptr(vm, p);
    vm_push(vm, CELL(v));
}

/* QUERY ( -- )  Accept input into TIB */
void string_word_query(VM *vm) {
    if (ensure_input(vm)) return;
    unsigned char *tib = vm_input_tib(vm);
    cell_t *inp = vm_input_in(vm);
    cell_t *span = vm_input_span(vm);
    if (!tib || !inp || !span) {
        vm->error = 1;
        return;
    }

    char *dst = (char *) tib;
    char *input = fgets(dst, (int) INPUT_BUFFER_SIZE, stdin);
    if (!input) {
        tib[0] = 0;
        *span = 0;
        *inp = 0;
        return;
    }
    size_t L = strlen(dst);
    if (L > 0 && dst[L - 1] == '\n') {
        dst[L - 1] = '\0';
        L--;
    }
    *span = (cell_t) L;
    *inp = 0;
}

/* TIB ( -- addr )  Terminal input buffer address (VM address) */
void string_word_tib(VM *vm) {
    if (ensure_input(vm)) return;
    unsigned char *p = vm_input_tib(vm);
    if (!p) {
        vm->error = 1;
        return;
    }
    vaddr_t v = vaddr_from_ptr(vm, p);
    vm_push(vm, CELL(v));
}

/* WORD ( c -- addr )  Parse next word, delimited by c (returns VM addr of counted string) */
void string_word_word(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    if (ensure_input(vm)) return;

    cell_t c = vm_pop(vm);
    char delimiter = (char) (c & 0xFF);

    unsigned char *tib = vm_input_tib(vm);
    cell_t *inp = vm_input_in(vm);
    cell_t *span = vm_input_span(vm);
    if (!tib || !inp || !span) {
        vm->error = 1;
        return;
    }

    size_t tib_len = (size_t) ((*span < 0) ? 0 : *span);
    cell_t ip = (*inp < 0) ? 0 : *inp;

    /* Skip leading delimiters */
    while ((size_t) ip < tib_len && tib[ip] == (unsigned char) delimiter) ip++;

    size_t start = (size_t) ip;
    while ((size_t) ip < tib_len && tib[ip] != (unsigned char) delimiter) ip++;
    size_t end = (size_t) ip;
    size_t word_len = end - start;
    if (word_len > WORD_SCRATCH_CAP - 2) word_len = WORD_SCRATCH_CAP - 2;

    /* Allocate scratch once */
    if (word_scratch_addr == 0) {
        void *p = vm_allot(vm, WORD_SCRATCH_CAP);
        if (!p) {
            vm->error = 1;
            return;
        }
        word_scratch_addr = vaddr_from_ptr(vm, p);
    }

    /* Build counted string at scratch */
    uint8_t *wb = vm_ptr(vm, word_scratch_addr);
    if (!wb) {
        vm->error = 1;
        return;
    }
    wb[0] = (uint8_t) word_len;
    if (word_len) memcpy(&wb[1], &tib[start], word_len);
    if (word_len + 1 < WORD_SCRATCH_CAP) wb[word_len + 1] = '\0'; /* safety */

    /* Update >IN to char after the delimiter (if any) */
    while ((size_t) ip < tib_len && tib[ip] == (unsigned char) delimiter) ip++;
    *inp = ip;

    vm_push(vm, CELL(word_scratch_addr));
}

/* >IN ( -- addr )  Input stream pointer cell (VM address) */
void string_word_to_in(VM *vm) {
    if (ensure_input(vm)) return;
    cell_t *p = vm_input_in(vm);
    if (!p) {
        vm->error = 1;
        return;
    }
    vaddr_t v = vaddr_from_ptr(vm, p);
    vm_push(vm, CELL(v));
}

/* SOURCE ( -- addr u )  Input source specification (TIB addr, length) */
void string_word_source(VM *vm) {
    if (ensure_input(vm)) return;
    unsigned char *tib = vm_input_tib(vm);
    cell_t *span = vm_input_span(vm);
    if (!tib || !span) {
        vm->error = 1;
        return;
    }
    vaddr_t v = vaddr_from_ptr(vm, tib);
    cell_t len = (*span < 0) ? 0 : *span;
    vm_push(vm, CELL(v));
    vm_push(vm, len);
}

/* BL ( -- c )  ASCII space character (32) */
void string_word_bl(VM *vm) { vm_push(vm, 32); }

/* ' ( -- xt )  Get execution token of next word (Forth-79) */
void string_word_tick(VM *vm) {
    /* Parse the next word using WORD with BL (32) as delimiter */
    vm_push(vm, 32); /* BL */
    string_word_word(vm);
    if (vm->error) return;

    /* The counted string addr is now on TOS */
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t addr = vm->data_stack[vm->dsp];
    vaddr_t a = VM_ADDR(addr);
    if (!vm_addr_ok(vm, a, 1)) {
        vm->error = 1;
        return;
    }

    uint8_t *counted = vm_ptr(vm, a);
    if (!counted) {
        vm->error = 1;
        return;
    }
    uint8_t name_len = counted[0];
    if (!vm_addr_ok(vm, a + 1, name_len)) {
        vm->error = 1;
        return;
    }
    const char *name = (const char *) &counted[1];

    /* Local search (same criteria as FIND) */
    DictEntry *entry = vm->latest;
    while (entry != NULL) {
        if (entry->name_len == name_len && memcmp(entry->name, name, name_len) == 0) {
            /* Replace counted-string addr with xt; drop flag semantics (tick returns xt) */
            vm->data_stack[vm->dsp] = (cell_t)(uintptr_t)
            entry;
            return;
        }
        entry = entry->link;
    }

    /* Not found → error (79 semantics: tick must succeed) */
    vm->error = 1;
}

/* ['] ( -- xt )  Compile execution token (immediate) */
void string_word_bracket_tick(VM *vm) { string_word_tick(vm); }

/* LITERAL / [LITERAL] are placeholders here */
void string_word_literal(VM *vm) {
}

void string_word_bracket_literal(VM *vm) { string_word_literal(vm); }

/* CONVERT ( d1 addr1 -- d2 addr2 )  Convert string to number (very simplified) */
void string_word_convert(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        return;
    }
    cell_t addr1 = vm_pop(vm);
    cell_t dlow = vm_pop(vm);
    cell_t dhigh = vm_pop(vm);

    vaddr_t a = VM_ADDR(addr1);
    if (!vm_addr_ok(vm, a, 1)) {
        vm->error = 1;
        return;
    }
    char *str_ptr = (char *) vm_ptr(vm, a);
    if (!str_ptr) {
        vm->error = 1;
        return;
    }

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
    if (vm->dsp < 0) {
        vm->error = 1;
        return;
    }
    cell_t addr = vm_pop(vm);
    vaddr_t a = VM_ADDR(addr);
    if (!vm_addr_ok(vm, a, 1)) {
        vm_push(vm, 0);
        vm_push(vm, 0);
        return;
    }
    uint8_t *counted = vm_ptr(vm, a);
    if (!counted) {
        vm_push(vm, 0);
        vm_push(vm, 0);
        return;
    }
    uint8_t len = counted[0];
    if (!vm_addr_ok(vm, a + 1, len)) {
        vm_push(vm, 0);
        vm_push(vm, 0);
        return;
    }
    const char *data = (const char *) &counted[1];

    cell_t result;
    if (convert_string_to_number(data, (size_t) len, &result)) {
        vm_push(vm, result);
        vm_push(vm, 1);
    } else {
        vm_push(vm, 0);
        vm_push(vm, 0);
    }
}

/* ENCLOSE ( addr c -- addr1 n1 n2 n3 )  Parse for delimiter */
void string_word_enclose(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        return;
    }
    cell_t c = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    vaddr_t a = VM_ADDR(addr);
    if (!vm_addr_ok(vm, a, 1)) {
        vm->error = 1;
        return;
    }
    char *str = (char *) vm_ptr(vm, a);
    if (!str) {
        vm->error = 1;
        return;
    }

    char delimiter = (char) (c & 0xFF);
    size_t len = strlen(str);
    size_t i = 0;
    cell_t n1 = 0, n2 = 0, n3 = 0;

    for (i = 0; i < len && str[i] == delimiter; i++) {
    }
    n1 = (cell_t) i;
    for (; i < len && str[i] != delimiter; i++) {
    }
    n2 = (cell_t) i;
    for (; i < len && str[i] == delimiter; i++) {
    }
    n3 = (cell_t) i;

    vm_push(vm, addr); /* same addr back */
    vm_push(vm, n1);
    vm_push(vm, n2);
    vm_push(vm, n3);
}

/* S" ( addr -- addr u ) — write counted string at addr from source text */
/* S" ( addr -- addr u ) — write counted string at addr from interpreter input */
static void string_word_s_quote(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "S\": data stack underflow");
        return;
    }

    /* Destination VM address (offset) */
    cell_t addr_cell = vm_pop(vm);
    vaddr_t dst = VM_ADDR(addr_cell);

    /* Parse from interpreter input buffer, not TIB */
    const char *src = vm->input_buffer;
    size_t pos = vm->input_pos;
    size_t end = vm->input_length;

    if (!src || pos > end) {
        vm->error = 1;
        log_message(LOG_ERROR, "S\": input buffer not ready");
        return;
    }

    /* Optional single leading space per `S\" Test\"` style */
    if (pos < end && src[pos] == ' ') pos++;

    /* Collect until next double quote */
    size_t start = pos;
    while (pos < end && src[pos] != '"') pos++;

    if (pos >= end) {
        /* no closing quote */
        vm->error = 1;
        log_message(LOG_ERROR, "S\": missing closing quote");
        return;
    }

    size_t n = pos - start;
    if (n > 255) {
        vm->error = 1;
        log_message(LOG_ERROR, "S\": string too long (%zu)", n);
        return;
    }
    if (!vm_addr_ok(vm, dst, 1 + n)) {
        vm->error = 1;
        log_message(LOG_ERROR, "S\": dest out of bounds");
        return;
    }

    /* Write counted string into VM memory */
    vm_store_u8(vm, dst, (uint8_t) n);
    for (size_t i = 0; i < n; i++) {
        vm_store_u8(vm, dst + 1 + i, (uint8_t) src[start + i]);
    }

    /* Advance interpreter input past the closing quote */
    pos++; /* skip the '"' itself */
    vm->input_pos = pos;

    /* If caller used HERE, advance HERE to the end of stored string */
    size_t old_here = vm->here;
    if ((vaddr_t) old_here == dst) {
        vm->here = (size_t) (dst + 1 + n);
    }

    /* Leave addr and length */
    vm_push(vm, addr_cell);
    vm_push(vm, (cell_t) n);

    log_message(LOG_DEBUG, "S\": wrote %zu bytes at %ld; HERE=%zu", n, (long) addr_cell, vm->here);
}

/* -TRAILING ( addr u -- addr u' )
   Trims trailing spaces from a string. Accepts:
   - direct string at addr of length u, OR
   - counted string at addr (first byte = u), auto-detected and adjusted. */
static void string_word_minus_trailing(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        log_message(LOG_ERROR, "-TRAILING: stack underflow");
        return;
    }

    cell_t u = vm_pop(vm);
    cell_t addr = vm_pop(vm);
    if (u < 0) u = 0;

    vaddr_t a = VM_ADDR(addr);
    vaddr_t s = a; /* start of chars */
    size_t n = (size_t) u; /* length of chars */

    /* If this looks like a counted string and the region exists, adjust to chars */
    if (n <= 255 && vm_addr_ok(vm, a, 1) && vm_load_u8(vm, a) == (uint8_t) n) {
        if (!vm_addr_ok(vm, a + 1, n)) {
            vm->error = 1;
            log_message(LOG_ERROR, "-TRAILING: counted range OOB");
            return;
        }
        s = a + 1;
    } else {
        if (!vm_addr_ok(vm, a, n)) {
            vm->error = 1;
            log_message(LOG_ERROR, "-TRAILING: range OOB");
            return;
        }
    }

    /* Trim trailing spaces (ASCII 32) */
    while (n > 0) {
        uint8_t ch = vm_load_u8(vm, s + (n - 1));
        if (ch == 32) { n--; } else { break; }
    }

    vm_push(vm, CELL(s));
    vm_push(vm, (cell_t) n);
}

/* CMOVE ( addr1 addr2 u -- )
   Copy u bytes from addr1 to addr2 in ascending address order.
   Works correctly for overlapping regions when addr2 >= addr1 (forward move).
   All addresses are VM byte offsets. */
static void string_word_cmove(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        log_message(LOG_ERROR, "CMOVE: stack underflow");
        return;
    }

    cell_t u = vm_pop(vm);
    cell_t addr2 = vm_pop(vm);
    cell_t addr1 = vm_pop(vm);

    if (u < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "CMOVE: negative count");
        return;
    }
    if (u == 0) return;

    vaddr_t src = VM_ADDR(addr1);
    vaddr_t dst = VM_ADDR(addr2);
    size_t n = (size_t) u;

    if (!vm_addr_ok(vm, src, n) || !vm_addr_ok(vm, dst, n)) {
        vm->error = 1;
        log_message(LOG_ERROR, "CMOVE: out-of-bounds (src=%ld dst=%ld n=%ld)",
                    (long) addr1, (long) addr2, (long) u);
        return;
    }

    /* Ascending copy (low->high) */
    for (size_t i = 0; i < n; i++) {
        uint8_t b = vm_load_u8(vm, src + i);
        vm_store_u8(vm, dst + i, b);
    }
}

/* CMOVE> ( addr1 addr2 u -- )
   Copy u bytes from addr1 to addr2 in descending address order (high->low).
   Handles overlap correctly for dst < src. VM addresses (byte offsets). */
static void string_word_cmove_greater(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        log_message(LOG_ERROR, "CMOVE>: stack underflow");
        return;
    }

    cell_t u = vm_pop(vm);
    cell_t addr2 = vm_pop(vm);
    cell_t addr1 = vm_pop(vm);

    if (u < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "CMOVE>: negative count");
        return;
    }
    if (u == 0) return;

    vaddr_t src = VM_ADDR(addr1);
    vaddr_t dst = VM_ADDR(addr2);
    size_t n = (size_t) u;

    if (!vm_addr_ok(vm, src, n) || !vm_addr_ok(vm, dst, n)) {
        vm->error = 1;
        log_message(LOG_ERROR, "CMOVE>: out-of-bounds (src=%ld dst=%ld n=%ld)",
                    (long) addr1, (long) addr2, (long) u);
        return;
    }

    /* Descending copy (high->low) */
    for (size_t i = n; i > 0; i--) {
        uint8_t b = vm_load_u8(vm, src + (i - 1));
        vm_store_u8(vm, dst + (i - 1), b);
    }
}

/* COMPARE ( addr1 u1 addr2 u2 -- n )
   Lexicographic compare, case-sensitive.
   Returns -1 if s1<s2, 0 if equal, +1 if s1>s2.
   Accepts either direct (addr/u) or counted strings at addr (when *addr == u). */
static void string_word_compare(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        log_message(LOG_ERROR, "COMPARE: stack underflow");
        return;
    }

    cell_t u2 = vm_pop(vm);
    cell_t addr2 = vm_pop(vm);
    cell_t u1 = vm_pop(vm);
    cell_t addr1 = vm_pop(vm);

    if (u1 < 0) u1 = 0;
    if (u2 < 0) u2 = 0;

    vaddr_t a1 = VM_ADDR(addr1);
    vaddr_t a2 = VM_ADDR(addr2);

    /* detect counted string form (first byte equals u) and adjust */
    vaddr_t s1 = a1;
    size_t n1 = (size_t) u1;
    if (n1 <= 255 && vm_addr_ok(vm, a1, 1) && vm_load_u8(vm, a1) == (uint8_t) n1) {
        if (!vm_addr_ok(vm, a1 + 1, n1)) {
            vm->error = 1;
            log_message(LOG_ERROR, "COMPARE: s1 OOB");
            return;
        }
        s1 = a1 + 1;
    } else {
        if (!vm_addr_ok(vm, a1, n1)) {
            vm->error = 1;
            log_message(LOG_ERROR, "COMPARE: s1 OOB");
            return;
        }
    }

    vaddr_t s2 = a2;
    size_t n2 = (size_t) u2;
    if (n2 <= 255 && vm_addr_ok(vm, a2, 1) && vm_load_u8(vm, a2) == (uint8_t) n2) {
        if (!vm_addr_ok(vm, a2 + 1, n2)) {
            vm->error = 1;
            log_message(LOG_ERROR, "COMPARE: s2 OOB");
            return;
        }
        s2 = a2 + 1;
    } else {
        if (!vm_addr_ok(vm, a2, n2)) {
            vm->error = 1;
            log_message(LOG_ERROR, "COMPARE: s2 OOB");
            return;
        }
    }

    /* byte-wise compare */
    size_t m = (n1 < n2) ? n1 : n2;
    int result = 0;
    for (size_t i = 0; i < m; i++) {
        uint8_t c1 = vm_load_u8(vm, s1 + i);
        uint8_t c2 = vm_load_u8(vm, s2 + i);
        if (c1 != c2) {
            result = (c1 < c2) ? -1 : +1;
            break;
        }
    }
    if (result == 0) {
        if (n1 < n2) result = -1;
        else if (n1 > n2) result = +1;
        else result = 0;
    }

    vm_push(vm, (cell_t) result);
}

/* SEARCH ( addr1 u1 addr2 u2 -- addr3 u3 flag )
   Find first occurrence of s2 (addr2/u2) inside s1 (addr1/u1).
   Returns:
     - if found: addr3/u3 = tail of s1 starting at the match, flag = -1 (true)
     - if not:   addr3/u3 = original s1 (as chars),       flag = 0
   Accepts either direct (addr/u) or counted (addr points to count byte == u). */
static void string_word_search(VM *vm) {
    if (vm->dsp < 3) {
        vm->error = 1;
        log_message(LOG_ERROR, "SEARCH: stack underflow");
        return;
    }

    cell_t u2 = vm_pop(vm);
    cell_t addr2 = vm_pop(vm);
    cell_t u1 = vm_pop(vm);
    cell_t addr1 = vm_pop(vm);

    if (u1 < 0) u1 = 0;
    if (u2 < 0) u2 = 0;

    vaddr_t a1 = VM_ADDR(addr1);
    vaddr_t a2 = VM_ADDR(addr2);

    /* Convert to raw char spans, auto-detect counted strings */
    vaddr_t s1 = a1;
    size_t n1 = (size_t) u1;
    if (n1 <= 255 && vm_addr_ok(vm, a1, 1) && vm_load_u8(vm, a1) == (uint8_t) n1) {
        if (!vm_addr_ok(vm, a1 + 1, n1)) {
            vm->error = 1;
            log_message(LOG_ERROR, "SEARCH: s1 OOB");
            return;
        }
        s1 = a1 + 1;
    } else {
        if (!vm_addr_ok(vm, a1, n1)) {
            vm->error = 1;
            log_message(LOG_ERROR, "SEARCH: s1 OOB");
            return;
        }
    }

    vaddr_t s2 = a2;
    size_t n2 = (size_t) u2;
    if (n2 <= 255 && vm_addr_ok(vm, a2, 1) && vm_load_u8(vm, a2) == (uint8_t) n2) {
        if (!vm_addr_ok(vm, a2 + 1, n2)) {
            vm->error = 1;
            log_message(LOG_ERROR, "SEARCH: s2 OOB");
            return;
        }
        s2 = a2 + 1;
    } else {
        if (!vm_addr_ok(vm, a2, n2)) {
            vm->error = 1;
            log_message(LOG_ERROR, "SEARCH: s2 OOB");
            return;
        }
    }

    /* Edge cases (ANS/FORTH-79 style): empty needle is found at start */
    if (n2 == 0) {
        vm_push(vm, CELL(s1));
        vm_push(vm, (cell_t) n1);
        vm_push(vm, (cell_t) - 1);
        return;
    }
    if (n2 > n1) {
        vm_push(vm, CELL(s1));
        vm_push(vm, (cell_t) n1);
        vm_push(vm, 0);
        return;
    }

    /* Naive first-occurrence search */
    size_t limit = n1 - n2;
    for (size_t i = 0; i <= limit; i++) {
        size_t j = 0;
        for (; j < n2; j++) {
            uint8_t c1 = vm_load_u8(vm, s1 + i + j);
            uint8_t c2 = vm_load_u8(vm, s2 + j);
            if (c1 != c2) break;
        }
        if (j == n2) {
            /* Found: tail of s1 starting at match */
            vm_push(vm, CELL(s1 + i));
            vm_push(vm, (cell_t)(n1 - i));
            vm_push(vm, (cell_t) - 1);
            return;
        }
    }

    /* Not found: return original s1 span and false */
    vm_push(vm, CELL(s1));
    vm_push(vm, (cell_t) n1);
    vm_push(vm, 0);
}

/* SCAN ( addr u char -- addr' u' )
   Find first occurrence of char in the string. If found, return the tail
   starting at that character; else return end-of-string and 0.
   Accepts either direct (addr/u) or counted strings at addr (when *addr == u). */
static void string_word_scan(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        log_message(LOG_ERROR, "SCAN: stack underflow");
        return;
    }

    cell_t ch = vm_pop(vm);
    cell_t u = vm_pop(vm);
    cell_t addr = vm_pop(vm);

    if (u < 0) u = 0;

    vaddr_t a = VM_ADDR(addr);
    vaddr_t s = a;
    size_t n = (size_t) u;

    /* auto-detect counted string form (first byte equals u) */
    if (n <= 255 && vm_addr_ok(vm, a, 1) && vm_load_u8(vm, a) == (uint8_t) n) {
        if (!vm_addr_ok(vm, a + 1, n)) {
            vm->error = 1;
            log_message(LOG_ERROR, "SCAN: counted range OOB");
            return;
        }
        s = a + 1;
    } else {
        if (!vm_addr_ok(vm, a, n)) {
            vm->error = 1;
            log_message(LOG_ERROR, "SCAN: range OOB");
            return;
        }
    }

    uint8_t needle = (uint8_t)(ch & 0xFF);

    /* search */
    size_t i = 0;
    for (; i < n; i++) {
        if (vm_load_u8(vm, s + i) == needle) break;
    }

    if (i < n) {
        vm_push(vm, CELL(s + i));
        vm_push(vm, (cell_t)(n - i));
    } else {
        vm_push(vm, CELL(s + n));
        vm_push(vm, 0);
    }
}

/* SKIP ( addr u char -- addr' u' )
   Skip leading occurrences of char in the string.
   Accepts either direct (addr/u) or counted strings at addr (when *addr == u). */
static void string_word_skip(VM *vm) {
    if (vm->dsp < 2) {
        vm->error = 1;
        log_message(LOG_ERROR, "SKIP: stack underflow");
        return;
    }

    cell_t ch = vm_pop(vm);
    cell_t u = vm_pop(vm);
    cell_t addr = vm_pop(vm);

    if (u < 0) u = 0;

    vaddr_t a = VM_ADDR(addr);
    vaddr_t s = a;
    size_t n = (size_t) u;

    /* auto-detect counted string form (first byte equals u) */
    if (n <= 255 && vm_addr_ok(vm, a, 1) && vm_load_u8(vm, a) == (uint8_t) n) {
        if (!vm_addr_ok(vm, a + 1, n)) {
            vm->error = 1;
            log_message(LOG_ERROR, "SKIP: counted range OOB");
            return;
        }
        s = a + 1;
    } else {
        if (!vm_addr_ok(vm, a, n)) {
            vm->error = 1;
            log_message(LOG_ERROR, "SKIP: range OOB");
            return;
        }
    }

    uint8_t needle = (uint8_t)(ch & 0xFF);

    size_t i = 0;
    while (i < n && vm_load_u8(vm, s + i) == needle) i++;

    vm_push(vm, CELL(s + i));
    vm_push(vm, (cell_t)(n - i));
}

/* BLANK ( addr u -- )
   Fill u bytes with ASCII space (32). If addr looks like a counted string
   (first byte equals u and there’s room), we blank the character area (addr+1,u). */
static void string_word_blank(VM *vm) {
    if (vm->dsp < 1) {
        vm->error = 1;
        log_message(LOG_ERROR, "BLANK: stack underflow");
        return;
    }

    cell_t u = vm_pop(vm);
    cell_t addr = vm_pop(vm);

    if (u < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "BLANK: negative length");
        return;
    }
    if (u == 0) return;

    vaddr_t a = VM_ADDR(addr);
    size_t n = (size_t) u;

    /* If it looks like a counted string, operate on the chars, not the count byte */
    vaddr_t s = a;
    if (n <= 255 && vm_addr_ok(vm, a, 1) && vm_load_u8(vm, a) == (uint8_t) n) {
        if (!vm_addr_ok(vm, a + 1, n)) {
            vm->error = 1;
            log_message(LOG_ERROR, "BLANK: counted range OOB");
            return;
        }
        s = a + 1;
    } else {
        if (!vm_addr_ok(vm, a, n)) {
            vm->error = 1;
            log_message(LOG_ERROR, "BLANK: range OOB");
            return;
        }
    }

    for (size_t i = 0; i < n; i++) vm_store_u8(vm, s + i, 32);
}

/* FORTH-79 String Word Registration */
void register_string_words(VM *vm) {
    register_word(vm, "COUNT", string_word_count);
    register_word(vm, "EXPECT", string_word_expect);
    register_word(vm, "SPAN", string_word_span);
    register_word(vm, "QUERY", string_word_query);
    register_word(vm, "TIB", string_word_tib);
    register_word(vm, "WORD", string_word_word);
    register_word(vm, "S\"", string_word_s_quote);
    register_word(vm, ">IN", string_word_to_in);
    register_word(vm, "SOURCE", string_word_source);
    register_word(vm, "BL", string_word_bl);
    // ' (tick) word is registered in dictionary_manipulation_words.c
    register_word(vm, "[']", string_word_bracket_tick);
    register_word(vm, "LITERAL", string_word_literal);
    register_word(vm, "[LITERAL]", string_word_bracket_literal);
    register_word(vm, "CONVERT", string_word_convert);
    register_word(vm, "NUMBER", string_word_number);
    register_word(vm, "ENCLOSE", string_word_enclose);
    register_word(vm, "-TRAILING", string_word_minus_trailing);
    register_word(vm, "CMOVE", string_word_cmove);
    register_word(vm, "CMOVE>", string_word_cmove_greater);
    register_word(vm, "COMPARE", string_word_compare);
    register_word(vm, "SEARCH", string_word_search);
    register_word(vm, "SCAN", string_word_scan);
    register_word(vm, "SKIP", string_word_skip);
    register_word(vm, "BLANK", string_word_blank);
}
