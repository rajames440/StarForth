#ifndef LA_PRE_H
#define LA_PRE_H

#include "proof/proof.h"
#include "symbolic/veriT-DAG.h"

/**
   \brief quick and dirty (and temporary) hack for QF_LRA
   rewrites src to remove all equalities
   a = b --> a <= b and b <= a
   \param n number of formulas
   \param Psrc an array of n formulas
   \remark Psrc is modified to contain the rewritten formulas */
extern void rewrite_eq_init(void);
extern void rewrite_eq_push(TDAG src, unsigned* pos);
extern void rewrite_eq_pop(TDAG src, unsigned pos);
extern TDAG rewrite_eq_reduce(TDAG src);

extern void rewrite_eq_push_proof(TDAG src, unsigned* pos);
extern Tproof rewrite_eq_replacement(
	TDAG new_src, TDAG src, Tstack_proof reasons);
extern TDAG_proof rewrite_eq_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_transf);

#endif /* LA_PRE_H */
