#include "proof/proof-step-hash.h"

#include "proof/proof-output.h"
#include "utils/h-util.h"
#include "utils/stack.h"

Thash steps_hash;

Tproof_step_hash
steps_hash_new(Tproof_step proof_step, Tproof proof_id)
{
	Tproof_step_hash proof_step_hash;
	MY_MALLOC(proof_step_hash, sizeof(struct TSproof_step_hash));
	proof_step_hash->proof_step = proof_step;
	proof_step_hash->proof_id = proof_id;
	return proof_step_hash;
}

unsigned int
steps_hash_function(Tproof_step_hash proof_step_hash)
{
	Tproof_step proof_step = proof_step_hash->proof_step;
	unsigned i, key;
	key = hash_one_at_a_time_u_inc(0, stack_size(proof_step->DAGs));
	for (i = 0; i < stack_size(proof_step->DAGs); ++i)
		key =
			hash_one_at_a_time_u_inc(key, DAG_key(stack_get(proof_step->DAGs, i)));
	return key;
}

unsigned int
steps_hash_equal(
	Tproof_step_hash proof_step_hash1, Tproof_step_hash proof_step_hash2)
{
	unsigned i;
	if (
		stack_size(proof_step_hash1->proof_step->DAGs) !=
		stack_size(proof_step_hash2->proof_step->DAGs))
		return 0;
	for (i = 0; i < stack_size(proof_step_hash1->proof_step->DAGs); ++i)
		if (
			stack_get(proof_step_hash1->proof_step->DAGs, i) !=
			stack_get(proof_step_hash2->proof_step->DAGs, i))
			return 0;
	return 1;
}

void
steps_hash_free(Tproof_step_hash proof_step_hash)
{
	free(proof_step_hash);
}

void
steps_hash_push(Tproof_step proof_step, Tproof proof_id)
{
	hash_insert(steps_hash, steps_hash_new(proof_step, proof_id));
}

Tproof
steps_hash_get(Tproof_step proof_step)
{
	struct TSproof_step_hash Sproof_step_hash;
	Tproof_step_hash proof_step_hash;
	Sproof_step_hash.proof_step = proof_step;
	Sproof_step_hash.proof_id = 0;
	proof_step_hash = hash_lookup(steps_hash, &Sproof_step_hash);
	if (proof_step_hash) return proof_step_hash->proof_id;
	return 0;
}

void
steps_hash_remove(Tproof_step proof_step)
{
	struct TSproof_step_hash Sproof_step_hash;
	Tproof_step_hash proof_step_hash;
	Sproof_step_hash.proof_step = proof_step;
	Sproof_step_hash.proof_id = 0;
	proof_step_hash = hash_lookup(steps_hash, &Sproof_step_hash);
	if (proof_step_hash) hash_remove(steps_hash, &Sproof_step_hash);
}
