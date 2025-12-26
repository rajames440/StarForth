/**
 * parity.h - Parity packet for VM validation
 *
 * Defines structures and functions for comparing hosted vs kernel VM state.
 * Parity is verified via canonical dictionary hash, not raw memory comparison.
 *
 * M7 Normative Rules:
 * - word_id is monotonic creation index starting at 0
 * - Dictionary traversal is creation order (oldest to newest)
 * - Hash excludes pointers, padding, runtime fields
 * - Colon bodies are hashed as word_id sequences, not addresses
 */

#ifndef STARKERNEL_PARITY_H
#define STARKERNEL_PARITY_H

#include <stdint.h>

/**
 * Bootstrap result codes
 */
#define SK_BOOTSTRAP_OK         0
#define SK_BOOTSTRAP_ARENA_FAIL 1
#define SK_BOOTSTRAP_INIT_FAIL  2
#define SK_BOOTSTRAP_DICT_FAIL  3

/**
 * ParityPacket - Summary of VM state for comparison
 *
 * M7.1a fields are sufficient for bootstrap validation.
 * M7.1b fields are added for POST validation.
 */
typedef struct {
    /* === M7.1a: Bootstrap Parity === */
    uint32_t word_count;        /* Number of dictionary entries */
    uint32_t here_offset;       /* vm->here (bytes used in dictionary) */
    uint32_t latest_word_id;    /* vm->latest->word_id (stable ID) */
    uint64_t header_hash64;     /* Canonical dictionary hash (FNV-1a) */

    /* === M7.1b: POST Parity === */
    uint32_t tests_total;       /* Total tests executed */
    uint32_t tests_passed;      /* Tests passed */
    uint32_t tests_failed;      /* Tests failed */
    uint32_t tests_skipped;     /* Tests skipped */
    uint32_t tests_errors;      /* Tests with errors */

    /* === Optional: Rolling Window Hash === */
    uint64_t window_hash64;     /* Hash of execution history (if deterministic) */

    /* === Status === */
    int      bootstrap_result;  /* SK_BOOTSTRAP_* code */
} ParityPacket;

/**
 * Forward declaration of VM struct
 */
struct VM;

/**
 * sk_parity_collect - Collect parity data from VM
 *
 * Traverses dictionary in creation order, computes canonical hash.
 * Does NOT include runtime fields (execution_heat, physics, etc.).
 *
 * @param vm  Pointer to VM instance
 * @param out Pointer to ParityPacket to fill
 */
void sk_parity_collect(struct VM *vm, ParityPacket *out);

/**
 * sk_parity_print - Print parity packet to console
 *
 * Output format:
 *   PARITY:M7.1a word_count=N here=0xHHHH latest_id=N hash=0xHHHHHHHHHHHHHHHH
 *   PARITY:M7.1b tests=N pass=N fail=N skip=N err=N
 *
 * @param pkt Pointer to ParityPacket to print
 */
void sk_parity_print(const ParityPacket *pkt);

/**
 * sk_dict_canonical_hash - Compute canonical dictionary hash
 *
 * Hashes structural fields only:
 *   - flags, name_len, name[], acl_default, word_id
 *   - For colon words: body as word_id sequence
 *
 * Excludes:
 *   - link (pointer)
 *   - func (function pointer)
 *   - execution_heat (runtime)
 *   - physics (runtime)
 *   - transition_metrics (pointer)
 *
 * @param vm Pointer to VM instance
 * @return 64-bit FNV-1a hash
 */
uint64_t sk_dict_canonical_hash(struct VM *vm);

/**
 * sk_dict_word_count - Count dictionary entries
 *
 * Traverses from vm->latest to NULL.
 *
 * @param vm Pointer to VM instance
 * @return Number of dictionary entries
 */
uint32_t sk_dict_word_count(struct VM *vm);

/**
 * FNV-1a constants
 */
#define FNV1A_64_OFFSET_BASIS 0xCBF29CE484222325ULL
#define FNV1A_64_PRIME        0x100000001B3ULL

/**
 * fnv1a_64 - FNV-1a 64-bit hash function
 *
 * @param data  Pointer to data to hash
 * @param len   Length of data
 * @param hash  Current hash value (use FNV1A_64_OFFSET_BASIS for initial)
 * @return Updated hash value
 */
uint64_t fnv1a_64(const uint8_t *data, size_t len, uint64_t hash);

#endif /* STARKERNEL_PARITY_H */
