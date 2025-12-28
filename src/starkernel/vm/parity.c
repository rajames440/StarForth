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

#include "starkernel/vm/parity.h"
#include "vm.h"

#ifdef __STARKERNEL__
#include "console.h"
#include "starkernel/hal/hal.h"
#else
#include <stdio.h>
#include <stdlib.h>
#endif

#include <stdbool.h>
#include <string.h>

#ifndef SK_PARITY_DEBUG
#define SK_PARITY_DEBUG 0
#endif

static void print_u32(uint32_t val);
static void print_hex64(uint64_t val);
static void print_str(const char *s);
static void print_nl(void);

#if SK_PARITY_DEBUG
#define SK_PARITY_DEBUG_PREFIX "SKPD:"
enum sk_parity_ptr_region {
    SK_PTR_REGION_NULL = 0,
    SK_PTR_REGION_VM_ARENA,
    SK_PTR_REGION_TEXT,
    SK_PTR_REGION_RODATA,
    SK_PTR_REGION_DATA,
    SK_PTR_REGION_BSS,
    SK_PTR_REGION_DIRECTMAP,
    SK_PTR_REGION_UNKNOWN
};

static void sk_parity_debug_log_msg(const char *msg);
static bool sk_parity_debug_is_canonical(uint64_t addr);
static enum sk_parity_ptr_region sk_parity_classify_region(struct VM *vm, const void *ptr);
static const char *sk_parity_region_name(enum sk_parity_ptr_region region);
static void sk_parity_debug_print_word_name(const DictEntry *entry);
static void sk_parity_debug_panic(struct VM *vm,
                                  const char *reason,
                                  const DictEntry *entry,
                                  uint32_t word_index,
                                  const void *header_ptr,
                                  const void *xt_ptr,
                                  const void *bad_ptr,
                                  enum sk_parity_ptr_region region,
                                  bool canonical);
static void sk_parity_debug_log_ptr(const char *label,
                                    const void *ptr,
                                    enum sk_parity_ptr_region region,
                                    bool canonical);
static void sk_parity_debug_check_ptr(struct VM *vm,
                                      const char *label,
                                      const DictEntry *entry,
                                      uint32_t word_index,
                                      const void *header_ptr,
                                      const void *xt_ptr,
                                      const void *ptr);
#else
#define sk_parity_debug_log_msg(msg) do { (void)(msg); } while (0)
#define sk_parity_debug_check_ptr(vm,label,entry,idx,hptr,xt,ptr) \
    do { (void)(vm); (void)(label); (void)(entry); (void)(idx); (void)(hptr); (void)(xt); (void)(ptr); } while (0)
#endif

/* Maximum dictionary entries for traversal array */
#define MAX_DICT_ENTRIES 2048

#if SK_PARITY_DEBUG
#define SK_PARITY_CANONICAL_MASK 0xffff800000000000ULL
#define SK_PARITY_DIRECTMAP_BASE 0xffff800000000000ULL
#ifdef __STARKERNEL__
extern char __text_start[];
extern char __text_end[];
extern char __rodata_start[];
extern char __rodata_end[];
extern char __data_start[];
extern char __data_end[];
extern char __bss_start[];
extern char __bss_end[];
#endif

static void sk_parity_debug_log_msg(const char *msg)
{
    print_str(SK_PARITY_DEBUG_PREFIX);
    print_str(msg ? msg : "<null>");
    print_nl();
}

static bool sk_parity_debug_is_canonical(uint64_t addr)
{
    uint64_t sign = (addr >> 47) & 1ULL;
    uint64_t mask = SK_PARITY_CANONICAL_MASK;
    return sign ? ((addr & mask) == mask) : ((addr & mask) == 0);
}

static enum sk_parity_ptr_region sk_parity_classify_region(struct VM *vm, const void *ptr)
{
    if (!ptr) {
        return SK_PTR_REGION_NULL;
    }
    uintptr_t addr = (uintptr_t)ptr;
    if (vm && vm->memory) {
        uintptr_t arena_start = (uintptr_t)vm->memory;
        uintptr_t arena_end = arena_start + VM_MEMORY_SIZE;
        if (addr >= arena_start && addr < arena_end) {
            return SK_PTR_REGION_VM_ARENA;
        }
    }
#ifdef __STARKERNEL__
    uintptr_t text_start = (uintptr_t)__text_start;
    uintptr_t text_end = (uintptr_t)__text_end;
    if (addr >= text_start && addr < text_end) {
        return SK_PTR_REGION_TEXT;
    }
    uintptr_t ro_start = (uintptr_t)__rodata_start;
    uintptr_t ro_end = (uintptr_t)__rodata_end;
    if (addr >= ro_start && addr < ro_end) {
        return SK_PTR_REGION_RODATA;
    }
    uintptr_t data_start = (uintptr_t)__data_start;
    uintptr_t data_end = (uintptr_t)__data_end;
    if (addr >= data_start && addr < data_end) {
        return SK_PTR_REGION_DATA;
    }
    uintptr_t bss_start = (uintptr_t)__bss_start;
    uintptr_t bss_end = (uintptr_t)__bss_end;
    if (addr >= bss_start && addr < bss_end) {
        return SK_PTR_REGION_BSS;
    }
    if (addr >= SK_PARITY_DIRECTMAP_BASE) {
        return SK_PTR_REGION_DIRECTMAP;
    }
#endif
    return SK_PTR_REGION_UNKNOWN;
}

static const char *sk_parity_region_name(enum sk_parity_ptr_region region)
{
    switch (region) {
        case SK_PTR_REGION_NULL: return "null";
        case SK_PTR_REGION_VM_ARENA: return "vm_arena";
        case SK_PTR_REGION_TEXT: return "text";
        case SK_PTR_REGION_RODATA: return "rodata";
        case SK_PTR_REGION_DATA: return "data";
        case SK_PTR_REGION_BSS: return "bss";
        case SK_PTR_REGION_DIRECTMAP: return "directmap";
        default: return "unknown";
    }
}

static void sk_parity_debug_print_word_name(const DictEntry *entry)
{
    if (!entry) {
        print_str("<none>");
        return;
    }
    uint8_t len = entry->name_len;
    if (len == 0 || len > WORD_NAME_MAX) {
        len = (len > WORD_NAME_MAX) ? WORD_NAME_MAX : len;
    }
    char buf[WORD_NAME_MAX + 1];
    if (len > 0) {
        memcpy(buf, entry->name, len);
    }
    buf[len] = '\0';
    print_str(buf);
}

static void sk_parity_debug_log_ptr(const char *label,
                                    const void *ptr,
                                    enum sk_parity_ptr_region region,
                                    bool canonical)
{
    print_str(SK_PARITY_DEBUG_PREFIX);
    print_str(label ? label : "ptr");
    print_str("=");
    print_hex64((uint64_t)(uintptr_t)ptr);
    print_str(" canon=");
    print_str(canonical ? "Y" : "N");
    print_str(" region=");
    print_str(sk_parity_region_name(region));
    print_nl();
}

static void sk_parity_debug_panic(struct VM *vm,
                                  const char *reason,
                                  const DictEntry *entry,
                                  uint32_t word_index,
                                  const void *header_ptr,
                                  const void *xt_ptr,
                                  const void *bad_ptr,
                                  enum sk_parity_ptr_region region,
                                  bool canonical)
{
    print_str("SK_PARITY_PANIC: ");
    print_str(reason ? reason : "unknown");
    print_nl();

    print_str(" word_idx=");
    print_u32(word_index);
    print_str(" name=");
    sk_parity_debug_print_word_name(entry);
    print_nl();

    print_str(" header_ptr=");
    print_hex64((uint64_t)(uintptr_t)header_ptr);
    print_str(" xt_ptr=");
    print_hex64((uint64_t)(uintptr_t)xt_ptr);
    print_nl();

    print_str(" bad_ptr=");
    print_hex64((uint64_t)(uintptr_t)bad_ptr);
    print_str(" canon=");
    print_str(canonical ? "Y" : "N");
    print_str(" region=");
    print_str(sk_parity_region_name(region));
    print_nl();

    if (vm) {
        print_str(" HERE=");
        print_hex64((uint64_t)vm->here);
        print_str(" LATEST=");
        print_hex64((uint64_t)(uintptr_t)vm->latest);
        print_nl();
    }
#ifdef __STARKERNEL__
    sk_hal_panic("parity pointer violation");
#else
    fprintf(stderr, "Parity pointer violation\n");
    abort();
#endif
}

static void sk_parity_debug_check_ptr(struct VM *vm,
                                      const char *label,
                                      const DictEntry *entry,
                                      uint32_t word_index,
                                      const void *header_ptr,
                                      const void *xt_ptr,
                                      const void *ptr)
{
    enum sk_parity_ptr_region region = sk_parity_classify_region(vm, ptr);
    bool canonical = sk_parity_debug_is_canonical((uint64_t)(uintptr_t)ptr);
    sk_parity_debug_log_ptr(label, ptr, region, canonical);
    if (!canonical || region == SK_PTR_REGION_UNKNOWN) {
        sk_parity_debug_panic(vm, label, entry, word_index, header_ptr, xt_ptr, ptr, region, canonical);
    }
}
#endif /* SK_PARITY_DEBUG */

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
 * @param word_index Dictionary ordinal (creation order)
 * @param hash  Current hash value
 * @return Updated hash value
 */
static uint64_t hash_colon_body(struct VM *vm, DictEntry *entry, uint32_t word_index, uint64_t hash) {
    if (!vm || !entry || !(entry->flags & WORD_COMPILED)) {
        return hash;
    }
#if !SK_PARITY_DEBUG
    (void)word_index;
#endif

#if SK_PARITY_DEBUG
    sk_parity_debug_log_msg("hash_colon_body:entry");
    sk_parity_debug_check_ptr(vm, "colon_entry", entry, word_index, entry, entry->func, entry);
#endif
    /* Get body address from data field */
    cell_t *df = vm_dictionary_get_data_field(entry);
    if (!df) {
        return hash;
    }
#if SK_PARITY_DEBUG
    sk_parity_debug_check_ptr(vm, "data_field", entry, word_index, entry, entry->func, df);
#endif

    vaddr_t body_addr = (vaddr_t)(uint64_t)(*df);
    if (body_addr == 0 || body_addr >= VM_MEMORY_SIZE) {
        return hash;
    }
#if SK_PARITY_DEBUG
    sk_parity_debug_check_ptr(vm, "body_addr_ptr", entry, word_index, entry, entry->func,
                              (vm->memory && body_addr < VM_MEMORY_SIZE) ? (vm->memory + body_addr) : NULL);
#endif

    /* Traverse threaded code until we hit EXIT or end */
    /* Each cell is a DictEntry* (word to call) */
    /* LIT is followed by a literal value */

    DictEntry *lit_entry = vm_find_word(vm, "LIT", 3);
    DictEntry *exit_entry = vm_find_word(vm, "EXIT", 4);
#if SK_PARITY_DEBUG
    sk_parity_debug_check_ptr(vm, "lit_entry", lit_entry, word_index, entry, lit_entry ? lit_entry->func : NULL, lit_entry);
    sk_parity_debug_check_ptr(vm, "exit_entry", exit_entry, word_index, entry, exit_entry ? exit_entry->func : NULL, exit_entry);
#endif

    cell_t *ip = (cell_t *)vm_ptr(vm, body_addr);
    if (!ip) {
        return hash;
    }
#if SK_PARITY_DEBUG
    sk_parity_debug_check_ptr(vm, "colon_body_ip", entry, word_index, entry, entry->func, ip);
#endif

    /* Safety limit on body length */
    size_t max_cells = (VM_MEMORY_SIZE - body_addr) / sizeof(cell_t);
    if (max_cells > 1024) max_cells = 1024;

    for (size_t i = 0; i < max_cells; i++) {
        DictEntry *w = (DictEntry *)(uintptr_t)(ip[i]);
#if SK_PARITY_DEBUG
        sk_parity_debug_check_ptr(vm, "body_xt", w, word_index, entry, w ? w->func : NULL, w);
#endif

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

#if SK_PARITY_DEBUG
    sk_parity_debug_log_msg("sk_dict_canonical_hash:walk_latest");
    sk_parity_debug_check_ptr(vm, "latest_head", vm->latest, 0, vm->latest, vm->latest ? vm->latest->func : NULL, vm->latest);
#endif
    for (DictEntry *e = vm->latest; e != NULL; e = e->link) {
#if SK_PARITY_DEBUG
        sk_parity_debug_check_ptr(vm, "dict_entry", e, count, e, e->func, e);
#endif
        if (count < MAX_DICT_ENTRIES) {
            entries[count++] = e;
        }
#if SK_PARITY_DEBUG
        sk_parity_debug_check_ptr(vm, "dict_link", e->link, count, e, e->func, e->link);
#endif
    }

    /* Hash in creation order (oldest to newest = reverse of array) */
    uint64_t hash = FNV1A_64_OFFSET_BASIS;

#if SK_PARITY_DEBUG
    sk_parity_debug_log_msg("sk_dict_canonical_hash:hash_order");
#endif
    for (int i = (int)count - 1; i >= 0; i--) {
        DictEntry *e = entries[i];
        uint32_t ordinal = (uint32_t)((count - 1) - i);
#if SK_PARITY_DEBUG
        sk_parity_debug_check_ptr(vm, "hash_entry", e, ordinal, e, e ? e->func : NULL, e);
        sk_parity_debug_check_ptr(vm, "hash_xt", e, ordinal, e, e ? e->func : NULL, e ? e->func : NULL);
#endif

        /* Hash structural fields only */

        /* 1. flags (1 byte) */
#if SK_PARITY_DEBUG
        sk_parity_debug_log_msg("hash_field:flags");
#endif
        hash = fnv1a_64_u8(e->flags, hash);

        /* 2. name_len (1 byte) */
#if SK_PARITY_DEBUG
        sk_parity_debug_log_msg("hash_field:name_len");
#endif
        hash = fnv1a_64_u8(e->name_len, hash);

        /* 3. name (name_len bytes) */
#if SK_PARITY_DEBUG
        sk_parity_debug_log_msg("hash_field:name_bytes");
#endif
        hash = fnv1a_64((const uint8_t *)e->name, e->name_len, hash);

        /* 4. acl_default (1 byte) */
#if SK_PARITY_DEBUG
        sk_parity_debug_log_msg("hash_field:acl");
#endif
        hash = fnv1a_64_u8(e->acl_default, hash);

        /* 5. word_id (4 bytes) */
#if SK_PARITY_DEBUG
        sk_parity_debug_log_msg("hash_field:word_id");
#endif
        hash = fnv1a_64_u32(e->word_id, hash);

        /* 6. For colon definitions: hash body as word_id sequence */
        if (e->flags & WORD_COMPILED) {
            hash = hash_colon_body(vm, e, ordinal, hash);
        }
    }

    return hash;
}

/**
 * sk_parity_collect - Collect parity data from VM
 */
void sk_parity_collect(struct VM *vm, ParityPacket *out) {
    if (!out) return;
#if SK_PARITY_DEBUG
    sk_parity_debug_log_msg("sk_parity_collect:enter");
#endif

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
