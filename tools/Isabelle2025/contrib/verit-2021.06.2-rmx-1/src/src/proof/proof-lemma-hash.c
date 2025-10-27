#include "proof/proof-lemma-hash.h"

static Thash lemma_hash;

static Tproof_lemma
lemma_hash_new(TDAG DAG, Tproof proof_id)
{
	Tproof_lemma proof_lemma;
	MY_MALLOC(proof_lemma, sizeof(struct TSproof_lemma));
	proof_lemma->DAG = DAG_dup(DAG);
	proof_lemma->proof_id = proof_id;
	return proof_lemma;
}

static unsigned int
lemma_hash_function(Tproof_lemma proof_lemma)
{
	return DAG_key(proof_lemma->DAG);
}

static unsigned int
lemma_hash_equal(Tproof_lemma proof_lemma1, Tproof_lemma proof_lemma2)
{
	return proof_lemma1->DAG == proof_lemma2->DAG;
}

static void
lemma_hash_free(Tproof_lemma proof_lemma)
{
	DAG_free(proof_lemma->DAG);
	free(proof_lemma);
}

void
proof_lemma_push(TDAG DAG, Tproof proof_id)
{
	hash_insert(lemma_hash, lemma_hash_new(DAG, proof_id));
}

Tproof
proof_lemma_get(TDAG DAG)
{
	struct TSproof_lemma Sproof_lemma;
	Tproof_lemma proof_lemma;
	Sproof_lemma.DAG = DAG;
	Sproof_lemma.proof_id = 0;
	proof_lemma = hash_lookup(lemma_hash, &Sproof_lemma);
	if (proof_lemma) return proof_lemma->proof_id;
	return 0;
}

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

void
proof_lemma_init(void)
{
	lemma_hash = hash_new(
		100, (TFhash)lemma_hash_function, (TFequal)lemma_hash_equal,
		(TFfree)lemma_hash_free);
}

void
proof_lemma_done(void)
{
	hash_free(&lemma_hash);
}
