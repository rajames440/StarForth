#ifndef __PROOF_STEP_HASH_H
#define __PROOF_STEP_HASH_H

#include "proof/proof-step.h"
#include "utils/hash.h"

/* This module is used for proof merging */

typedef struct TSproof_step_hash
{
	Tproof_step proof_step;
	Tproof proof_id;
} * Tproof_step_hash;

extern Thash steps_hash;

extern Tproof_step_hash steps_hash_new(Tproof_step proof_step, Tproof proof_id);
extern unsigned int steps_hash_function(Tproof_step_hash proof_step_hash);
extern unsigned int steps_hash_equal(
	Tproof_step_hash proof_step_hash1, Tproof_step_hash proof_step_hash2);
extern void steps_hash_free(Tproof_step_hash proof_step_hash);
extern void steps_hash_push(Tproof_step proof_step, Tproof proof_id);
extern Tproof steps_hash_get(Tproof_step proof_step);

extern void steps_hash_remove(Tproof_step proof_step);

#endif
