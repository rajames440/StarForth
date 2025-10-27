/*
  --------------------------------------------------------------
  Module for removing ite functions in formulas
  --------------------------------------------------------------
*/

#ifndef BFUN_ELIM_H
#define BFUN_ELIM_H

#include "symbolic/DAG.h"

/**
   computes an equisatisfiable formula such that no function
   except ite and Boolean connectors have boolean arguments (bfun)
   Also eliminates quantifications on boolean variables (in a naïve way)
   \param DAG the formula with bfuns
   \return The bfun-free equisatisfiable formula
   \remarks Non destructive
   \remarks DAG-linear
   \remarks Works with bfun within quantifiers, but may introduce ite
   \remarks Does not like lambdas and let */
TDAG bfun_elim(TDAG DAG);

/**
   \brief array version of the bfun_elim function
   \remark Destructive
   \see bfun_elim */
void bfun_elim_array(unsigned n, TDAG* Psrc);

#endif
