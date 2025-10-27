/*
  --------------------------------------------------------------
  Module for removing distinct constructs in formulas

  Protocol:
  (distinct_elimit_init; distinct_elim*; distinct_elim_done)*

  --------------------------------------------------------------
*/

#ifndef DISTINCT_ELIM_H
#define DISTINCT_ELIM_H

#include "proof/proof.h"
#include "symbolic/DAG.h"
#include "symbolic/context-recursion.h"

/**
   computes a distinct-free equivalent term (or formula)
   \param DAG the term (or formula) with distinct
   \return The distinct-free equivalent term (or formula)
   \remarks Non destructive
   \remarks DAG-linear (quadratic for distincts)
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers)
   \pre none
   \post distinct-free term */

extern void distinct_elim_init(void);
extern void distinct_elim_push(TDAG src, unsigned* pos);
extern void distinct_elim_pop(TDAG src, unsigned pos);
extern TDAG distinct_elim_reduce(TDAG src);

extern void distinct_elim_push_proof(TDAG src, unsigned* pos);
extern Tproof distinct_elim_replacement(
	TDAG new_src, TDAG src, Tstack_proof reasons);
extern TDAG_proof distinct_elim_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_transf);

#endif
