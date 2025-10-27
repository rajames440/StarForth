/*
  --------------------------------------------------------------
  Module for removing ite functions in formulas
  --------------------------------------------------------------
*/

#ifndef ITE_ELIM_H
#define ITE_ELIM_H

#include "symbolic/DAG.h"

void ite_elim_init(void);
void ite_elim_done(void);
/**
   computes a ite-free equisatisfiable formula
   \param DAG the formula with ite inside terms
   \return The ite-free equisatisfiable formula
   \remarks Non destructive
   \remarks DAG-linear
   \remarks Does not work with ite within quantifiers */
TDAG ite_elim(TDAG DAG);

#endif
