/**
 * parity.c - Parity packet collection and canonical hash
 *
 * Implements M7 parity validation for hosted vs kernel comparison.
 *
 * M7 Normative Rules enforced here:
 * - Rule 1: word_id is monotonic creation index
 * - Rule 2: Colon bodies hashed as word_id sequence
 * - Rule 3: Dictionary traversal in creation order
 */

#include "../../include/starkernel/parity.h"
#include "../../include/vm.h"

#ifdef __STARKERNEL__
#include "console.h"
#else
#include <stdio.h>
#endif

/* Maximum dictionary entries for traversal array */
#define MAX_DICT_ENTRIES 2048

/**
 * fnv1a_64 - FNV-1a 64-bit hash function
 */
uint64_t fnv1a_64(const uint8_t *data, size_t len, uint64_t hash) {
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= FNV1A_64_PRIME;
    }
    return hash;
}

/**
 * fnv1a_64_u8 - Hash a single byte
 */
static uint64_t fnv1a_64_u8(uint8_t byte, uint64_t hash) {
    hash ^= byte;
    hash *= FNV1A_64_PRIME;
    return hash;
}

/**
 * fnv1a_64_u32 - Hash a 32-bit value (little-endian)
 */
static uint64_t fnv1a_64_u32(uint32_t val, uint64_t hash) {
    hash = fnv1a_64_u8((uint8_t)(val & 0xFF), hash);
    hash = fnv1a_64_u8((uint8_t)((val >> 8) & 0xFF), hash);
    hash = fnv1a_64_u8((uint8_t)((val >> 16) & 0xFF), hash);
    hash = fnv1a_64_u8((uint8_t)((val >> 24) & 0xFF), hash);
    return hash;
}

/**
 * sk_dict_word_count - Count dictionary entries
 */
uint32_t sk_dict_word_count(struct VM *vm) {
    if (!vm) return 0;

    uint32_t count = 0;
    for (DictEntry *e = vm->latest; e != NULL; e = e->link) {
        count++;
        if (count >= MAX_DICT_ENTRIES) break;
    }
    return count;
}

/**
 * hash_colon_body - Hash compiled word body as word_id sequence
 *
 * Traverses threaded code, hashes word_id of each called word.
 * Literals are hashed as their value (not address).
 *
 * @param vm    VM instance
 * @param entry Dictionary entry (must have WORD_COMPILED flag)
 * @param hash  Current hash value
 * @return Updated hash value
 */
static uint64_t hash_colon_body(struct VM *vm, DictEntry *entry, uint64_t hash) {
    if (!vm || !entry || !(entry->flags & WORD_COMPILED)) {
        return hash;
    }

    /* Get body address from data field */
    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        return hash;
    }

    vaddr_t body_addr = (vaddr_t)(uint64_t)(*df);
    if (body_addr == 0 || body_addr >= VM_MEMORY_SIZE) {
        return hash;
    }

    /* Traverse threaded code until we hit EXIT or end */
    /* Each cell is a DictEntry* (word to call) */
    /* LIT is followed by a literal value */

    DictEntry *lit_entry = vm_find_word(vm, "LIT", 3);
    DictEntry *exit_entry = vm_find_word(vm, "EXIT", 4);

    cell_t *ip = (cell_t *)vm_ptr(vm, body_addr);
    if (!ip) {
        return hash;
    }

    /* Safety limit on body length */
    size_t max_cells = (VM_MEMORY_SIZE - body_addr) / sizeof(cell_t);
    if (max_cells > 1024) max_cells = 1024;

    for (size_t i = 0; i < max_cells; i++) {
        DictEntry *w = (DictEntry *)(uintptr_t)(ip[i]);

        if (w == NULL) {
            break;  /* End of body */
        }

        if (w == exit_entry) {
            /* Hash EXIT's word_id and stop */
            hash = fnv1a_64_u32(w->word_id, hash);
            break;
        }

        /* Hash the called word's word_id */
        hash = fnv1a_64_u32(w->word_id, hash);

        if (w == lit_entry) {
            /* Next cell is literal value - hash it */
            i++;
            if (i < max_cells) {
                cell_t lit_val = ip[i];
                /* Hash as 64-bit value */
                for (int b = 0; b < 8; b++) {
                    hash = fnv1a_64_u8((uint8_t)(lit_val & 0xFF), hash);
                    lit_val >>= 8;
                }
            }
        }
    }

    return hash;
}

/**
 * sk_dict_canonical_hash - Compute canonical dictionary hash
 *
 * Traverses dictionary in creation order, hashes structural fields only.
 */
uint64_t sk_dict_canonical_hash(struct VM *vm) {
    if (!vm) return 0;

    /* Build array of entries in reverse order (latest to oldest) */
    DictEntry *entries[MAX_DICT_ENTRIES];
    uint32_t count = 0;

    for (DictEntry *e = vm->latest; e != NULL; e = e->link) {
        if (count < MAX_DICT_ENTRIES) {
            entries[count++] = e;
        }
    }

    /* Hash in creation order (oldest to newest = reverse of array) */
    uint64_t hash = FNV1A_64_OFFSET_BASIS;

    for (int i = (int)count - 1; i >= 0; i--) {
        DictEntry *e = entries[i];

        /* Hash structural fields only */

        /* 1. flags (1 byte) */
        hash = fnv1a_64_u8(e->flags, hash);

        /* 2. name_len (1 byte) */
        hash = fnv1a_64_u8(e->name_len, hash);

        /* 3. name (name_len bytes) */
        hash = fnv1a_64((const uint8_t *)e->name, e->name_len, hash);

        /* 4. acl_default (1 byte) */
        hash = fnv1a_64_u8(e->acl_default, hash);

        /* 5. word_id (4 bytes) */
        hash = fnv1a_64_u32(e->word_id, hash);

        /* 6. For colon definitions: hash body as word_id sequence */
        if (e->flags & WORD_COMPILED) {
            hash = hash_colon_body(vm, e, hash);
        }
    }

    return hash;
}

/**
 * sk_parity_collect - Collect parity data from VM
 */
void sk_parity_collect(struct VM *vm, ParityPacket *out) {
    if (!out) return;

    /* Clear packet */
    out->word_count = 0;
    out->here_offset = 0;
    out->latest_word_id = 0;
    out->header_hash64 = 0;
    out->tests_total = 0;
    out->tests_passed = 0;
    out->tests_failed = 0;
    out->tests_skipped = 0;
    out->tests_errors = 0;
    out->window_hash64 = 0;
    out->bootstrap_result = SK_BOOTSTRAP_OK;

    if (!vm) {
        out->bootstrap_result = SK_BOOTSTRAP_INIT_FAIL;
        return;
    }

    /* M7.1a fields */
    out->word_count = sk_dict_word_count(vm);
    out->here_offset = (uint32_t)vm->here;
    out->latest_word_id = vm->latest ? vm->latest->word_id : 0;
    out->header_hash64 = sk_dict_canonical_hash(vm);

    /* M7.1b fields - from global test stats (if tests were run) */
#ifdef STARFORTH_ENABLE_TESTS
    extern TestStats global_test_stats;
    out->tests_total = (uint32_t)global_test_stats.total_tests;
    out->tests_passed = (uint32_t)global_test_stats.total_pass;
    out->tests_failed = (uint32_t)global_test_stats.total_fail;
    out->tests_skipped = (uint32_t)global_test_stats.total_skip;
    out->tests_errors = (uint32_t)global_test_stats.total_error;
#endif
}

/**
 * Helper: print uint32 as decimal
 */
static void print_u32(uint32_t val) {
    char buf[16];
    int i = 15;
    buf[i--] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0 && i >= 0) {
            buf[i--] = '0' + (val % 10);
            val /= 10;
        }
    }
#ifdef __STARKERNEL__
    console_puts(&buf[i + 1]);
#else
    printf("%s", &buf[i + 1]);
#endif
}

/**
 * Helper: print uint64 as hex (with 0x prefix)
 */
static void print_hex64(uint64_t val) {
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 17; i >= 2; i--) {
        int d = val & 0xF;
        buf[i] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        val >>= 4;
    }
    buf[18] = '\0';
#ifdef __STARKERNEL__
    console_puts(buf);
#else
    printf("%s", buf);
#endif
}

/**
 * Helper: print string
 */
static void print_str(const char *s) {
#ifdef __STARKERNEL__
    console_puts(s);
#else
    printf("%s", s);
#endif
}

/**
 * Helper: print newline
 */
static void print_nl(void) {
#ifdef __STARKERNEL__
    console_println("");
#else
    printf("\n");
#endif
}

/**
 * sk_parity_print - Print parity packet to console
 */
void sk_parity_print(const ParityPacket *pkt) {
    if (!pkt) return;

    /* M7.1a line */
    print_str("PARITY:M7.1a word_count=");
    print_u32(pkt->word_count);
    print_str(" here=");
    print_hex64(pkt->here_offset);
    print_str(" latest_id=");
    print_u32(pkt->latest_word_id);
    print_str(" hash=");
    print_hex64(pkt->header_hash64);
    print_nl();

    /* M7.1b line (only if tests were run) */
    if (pkt->tests_total > 0) {
        print_str("PARITY:M7.1b tests=");
        print_u32(pkt->tests_total);
        print_str(" pass=");
        print_u32(pkt->tests_passed);
        print_str(" fail=");
        print_u32(pkt->tests_failed);
        print_str(" skip=");
        print_u32(pkt->tests_skipped);
        print_str(" err=");
        print_u32(pkt->tests_errors);
        print_nl();
    }

    /* Result line */
    if (pkt->bootstrap_result == SK_BOOTSTRAP_OK &&
        pkt->tests_failed == 0 && pkt->tests_errors == 0) {
        print_str("PARITY:OK");
    } else {
        print_str("PARITY:FAIL code=");
        print_u32(pkt->bootstrap_result);
    }
    print_nl();
}
