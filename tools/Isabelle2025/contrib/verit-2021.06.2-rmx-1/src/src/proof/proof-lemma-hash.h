#ifndef __PROOF_LEMMA_HASH_H
#define __PROOF_LEMMA_HASH_H

#include "proof/proof-id.h"
#include "symbolic/DAG.h"
#include "utils/hash.h"

typedef struct TSproof_lemma
{
	TDAG DAG;
	Tproof proof_id;
} * Tproof_lemma;

/*
  --------------------------------------------------------------
  Interface
  --------------------------------------------------------------
*/

extern void proof_lemma_push(TDAG DAG, Tproof proof_id);
extern Tproof proof_lemma_get(TDAG DAG);

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

extern void proof_lemma_init(void);
extern void proof_lemma_done(void);

#endif
