#ifndef __INST_PRE_H
#define __INST_PRE_H

#include "congruence/congruence.h"

/*
  --------------------------------------------------------------
  Preprocessing quantified formulas for instantiation
  --------------------------------------------------------------
*/

/*
  --------------------------------------------------------------
  Normal forms
  --------------------------------------------------------------
*/

/**
   \brief computes prenexed NNF and CNF of quantified DAG with arbitrary boolean structure
   \param DAG a quantified formula
   \remark assumes tree structure (no "lets")

   \remark No miniscoping/Skolemization is done. Only prenex universal
   quantifiers not under the scope of existential ones. */
extern void set_NFs(TDAG DAG);

#endif
