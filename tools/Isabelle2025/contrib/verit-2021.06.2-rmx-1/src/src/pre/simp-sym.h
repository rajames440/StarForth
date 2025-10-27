/*
  --------------------------------------------------------------
  Module for symmetry-based simplification
  --------------------------------------------------------------
*/

#ifndef SIMP_SYM_H
#define SIMP_SYM_H

#include "symbolic/DAG.h"

void simp_sym_init(void);
void simp_sym_done(void);

/**
   \brief notify the module that later formulas may be invariant by
   permutation of the n constants
   \remarks Non destructive
   \param n the number of constants
   \param cts array of constants
*/
void simp_sym_notify(unsigned n, TDAG* cts);
/**
   \brief computes a formula where symmetries have been used to
   restrict the value of some uninterpreted constants
   \param src the input (formula)
   \return The equi-satisfiable formula with cts fixed
   \remarks Non destructive
   \remarks DAG-linear
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers)
*/
TDAG simp_sym(TDAG src);

#endif
