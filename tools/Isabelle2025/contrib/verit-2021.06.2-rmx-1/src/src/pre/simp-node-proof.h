#ifndef __SIMPLIFY_NODE_PROOF_H
#define __SIMPLIFY_NODE_PROOF_H

#include "pre/simplify.h"
#include "proof/proof.h"
#include "symbolic/DAG.h"

/*
  --------------------------------------------------------------
  Equality and ITEs
  --------------------------------------------------------------
*/

extern TDAG_proof simplify_fite_proof(TDAG src);
extern TDAG_proof simplify_ite_proof(TDAG src);
extern TDAG_proof simplify_eq_proof(TDAG src);

/*
  --------------------------------------------------------------
  Arithmetic simplifications
  --------------------------------------------------------------
*/

/**  Rewrite (/ c d) where c and d are numbers */
extern TDAG_proof simplify_div_proof(TDAG src);
extern TDAG_proof simplify_prod_proof(TDAG src);
/**  Rewrite (- c) where c is a number */
extern TDAG_proof simplify_unary_minus_proof(TDAG src);
extern TDAG_proof simplify_minus_proof(TDAG src);
extern TDAG_proof simplify_sum_proof(TDAG src);
extern TDAG_proof simplify_less_proof(TDAG src);
extern TDAG_proof simplify_leq_proof(TDAG src);

/**
   Rewrite t >= t' to t' <= t
   Rewrite t < t' to \neg t' <= t
   Rewrite t > t' to \neg t <= t' */
extern TDAG_proof rewrite_arith_compare_proof(TDAG src);

/*
  --------------------------------------------------------------
  Connectives
  --------------------------------------------------------------
*/

/**
   \brief eliminate duplicate args and sort
   \param[in] src the DAG
   \return the simplified DAG
   \remark destructive
   \remark Useful for A AND A AND B -> A AND B and A OR A OR B -> A OR B */
extern TDAG_proof simplify_neutral_proof(TDAG src, TDAG sub);

/**
   \brief eliminate duplicate args and sort
   \param[in] src the DAG
   \return the simplified DAG
   \remark destructive
   \remark Useful for A AND A AND B -> A AND B and A OR A OR B -> A OR B */
extern TDAG_proof simplify_ACidem_proof(TDAG src);

extern TDAG_proof simplify_and_proof(TDAG src);
extern TDAG_proof simplify_or_proof(TDAG src);
extern TDAG_proof simplify_not_proof(TDAG src);
extern TDAG_proof simplify_implies_proof(TDAG src);
extern TDAG_proof simplify_equiv_proof(TDAG src);

extern TDAG_proof simplify_quantifier_proof(TDAG src);

#endif
