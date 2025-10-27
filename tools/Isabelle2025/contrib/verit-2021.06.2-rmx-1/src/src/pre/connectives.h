/*
  --------------------------------------------------------------
  Module for connectives elimination
  --------------------------------------------------------------
*/

#ifndef __CONNECTIVES_H
#define __CONNECTIVES_H

#include "proof/proof.h"
#include "symbolic/DAG.h"
#include "symbolic/context-recursion-proof.h"
#include "symbolic/context-recursion.h"

/* TODO: both these functions should be applied with post? */

/**
   \brief rewrites formulas with weak existentials into universals
   \param src a formula with quantifiers
   \param context the recursion context
   \return DAG with topmost weak existential, if any, rewritten into a negative
   universal over the negation of its matrix
*/

extern bool rewrite_w_exist_cont(TDAG DAG);
extern void rewrite_w_exist_init(void);
extern void rewrite_w_exist_push(TDAG src, unsigned* pos);
extern void rewrite_w_exist_pop(TDAG src, unsigned pos);
extern TDAG rewrite_w_exist_reduce(TDAG src);

/* DD+PF this is useful for removing connectors that would make a
   subformula (in a tree representation) having at the same time
   positive and negative polarities.  As a consequence, a quantifier
   could be at the same time strong and weak, and Skolemization would
   be impossible. */

/**
   \brief ensures that no formula has quantifiers with both polarities
   \param src a formula
   \return formula such that every quantifier appears with one polarity only
   \remark essentially removes xor, equiv, and ite where problematic
*/

extern void single_pol_qnt_init(void);
extern void single_pol_qnt_push(TDAG src, unsigned* pos);
extern void single_pol_qnt_pop(TDAG src, unsigned pos);
extern TDAG single_pol_qnt_reduce(TDAG src);

extern void rewrite_w_exist_push_proof(TDAG src, unsigned* pos);
extern Tproof rewrite_w_exist_replacement(
	TDAG new_src, TDAG src, Tstack_proof reasons);
extern TDAG_proof rewrite_w_exist_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_transf);

extern void single_pol_qnt_push_proof(TDAG src, unsigned* pos);
extern Tproof single_pol_qnt_replacement(
	TDAG new_src, TDAG src, Tstack_proof reasons);
extern TDAG_proof single_pol_qnt_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_transf);

#endif /* __CONNECTIVES_H */
