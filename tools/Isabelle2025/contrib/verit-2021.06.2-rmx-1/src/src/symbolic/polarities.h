/*
  --------------------------------------------------------------
  Polarities
  --------------------------------------------------------------
*/

#ifndef __POLARITIES_H
#define __POLARITIES_H

#include "symbolic/DAG.h"
#include "symbolic/veriT-DAG.h"

typedef unsigned char Tpol;

TSstack(_pol, Tpol); /* typedefs Tstack_pol */

enum
{
	POL_NONE = 0,
	POL_POS = 1,
	POL_NEG = 2,
	POL_BOTH = 3
};

static inline Tpol
INV_POL(const Tpol x)
{
	static Tpol INV[4] = { /* POL_NONE: */ POL_NONE,
												 /* POL_POS: */ POL_NEG,
												 /* POL_NEG: */ POL_POS,
												 /* POL_BOTH: */ POL_BOTH };
	return INV[x];
}

/**
  \brief returns `POL_POS` if `DAG` starts with zero or an
  even number of negation symbols and `POS_NEG` otherwise.
  \param DAG The DAG under consideration
 */
Tpol DAG_polarity(TDAG DAG);

/**
  \brief returns the first child DAG which does not start
  with a negation symbol.
  \param DAG The DAG under consideration
 */
static inline TDAG
DAG_atom(TDAG DAG)
{
	while (DAG_symb(DAG) == CONNECTOR_NOT) { DAG = DAG_arg0(DAG); }
	return DAG;
}

#define QUANTIFIED_STRONG(DAG, pol) \
	((DAG_symb(DAG) == QUANTIFIER_EXISTS && pol == POL_POS) || \
	 (DAG_symb(DAG) == QUANTIFIER_FORALL && pol == POL_NEG))
#define QUANTIFIED_WEAK(DAG, pol) \
	((DAG_symb(DAG) == QUANTIFIER_EXISTS && pol == POL_NEG) || \
	 (DAG_symb(DAG) == QUANTIFIER_FORALL && pol == POL_POS))

#endif /* __POLARITIES_H */
