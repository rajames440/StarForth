/*
  --------------------------------------------------------------
  Module for removing rare symbols in formulas
  --------------------------------------------------------------
*/

#ifndef RARE_SYMB_H
#define RARE_SYMB_H

#include "symbolic/DAG.h"

/**
   computes a formula where rare symbols are eliminated, adding a definition
   \param DAG the input (formula)
   \return The equi-satisfiable formula with rare symbols eliminated
   \remarks Non destructive
   \remarks DAG-linear
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers) */
TDAG rare_symb(TDAG DAG);

#endif
