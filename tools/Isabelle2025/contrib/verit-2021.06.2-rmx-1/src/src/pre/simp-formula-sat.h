/**
   \brief Simplification of formulas by eliminating uninterpreted
   constants.

*/

#ifndef __SIMP_FORMULA_SAT_H
#define __SIMP_FORMULA_SAT_H

#include "symbolic/veriT-DAG.h"

/**
   \brief Finds some equalities that apply to the whole formula (unit
   clause propagation).

   \param src the DAG of a formula

   \return the DAG of a formula equisatisfiable to <b>src</b> where
   some uninterpreted constants may have been eliminated. For instance
   if the input formula is <tt>(and (= x t) (f x))</tt>, then the
   result is <tt>(f t)</tt>.


   \note <b>src</b> may not contain free variables!

   \note Do not use with lemmas, parts of formulas, formulas with free
   variables.  Only use with a monolithic input to the prover

*/
extern TDAG simplify_formula_sat(TDAG src);
#endif
