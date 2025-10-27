/*
  --------------------------------------------------------------
  Module for removing let constructs in formulas
  --------------------------------------------------------------
*/

#ifndef LET_ELIM_H
#define LET_ELIM_H

#include "proof/proof.h"
#include "symbolic/DAG.h"
#include "symbolic/context-recursion-proof.h"
#include "symbolic/context-recursion.h"

/**
   \brief computes a let-free equivalent term (or formula)
   \param DAG the term (or formula) with let
   \param context the term (or formula) with let
   \return The let-free equivalent term (or formula) */

extern void let_elim_init(void);
extern void let_elim_push(TDAG src, unsigned* pos);
extern void let_elim_pop(TDAG src, unsigned pos);
extern TDAG let_elim_reduce(TDAG src);

extern void let_elim_init_proof(void);
extern void let_elim_push_proof(TDAG src, unsigned* pos);
extern void let_elim_pop_proof(TDAG src, unsigned pos);
extern Tproof let_elim_replacement(
	TDAG new_src, TDAG src, Tstack_proof reasons);
extern TDAG_proof let_elim_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_transf);

#endif
