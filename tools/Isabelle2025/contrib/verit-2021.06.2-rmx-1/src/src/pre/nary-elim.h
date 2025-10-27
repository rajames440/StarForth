/*
  --------------------------------------------------------------
  Module for removing nary constructs in formulas
  --------------------------------------------------------------
*/

#ifndef NARY_ELIM_H
#define NARY_ELIM_H

#include "proof/proof.h"
#include "symbolic/DAG.h"
#include "symbolic/context-recursion-proof.h"
#include "symbolic/context-recursion.h"

/**
   computes an equivalent term (or formula) where all n-ary symbols
   are reduced to binary ones (except AND and OR)
   \param DAG the term (or formula) with n-ary symbols
   \return The equivalent term (or formula) with only binary symbols
   \remarks Non destructive
   \remarks DAG-linear
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers)
   \remarks works now for implication, equiv, xor, -, <=, <, =, >=, >
   \pre none
   \post no n-ary for implication, equiv, xor, -, <=, <, =, >=, >
*/
extern TDAG nary_elim_node(TDAG DAG);

extern void nary_elim_node_init(void);
extern void nary_elim_node_push(TDAG src, unsigned* pos);
extern void nary_elim_node_pop(TDAG src, unsigned pos);
extern TDAG nary_elim_node_reduce(TDAG src);

extern void nary_elim_node_push_proof(TDAG src, unsigned* pos);
extern Tproof nary_elim_node_replacement(
	TDAG new_src, TDAG src, Tstack_proof reasons);
extern TDAG_proof nary_elim_node_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_transf);

#endif
