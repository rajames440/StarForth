/*
  --------------------------------------------------------------
  Module for removing let constructs in formulas
  --------------------------------------------------------------
*/

#ifndef BINDER_RENAME_H
#define BINDER_RENAME_H

#include "symbolic/DAG.h"

/**
   \brief computes an equivalent term (or formula) where each bound variable
   is bounded by one binder only
   \param DAG the input term (or formula)
   \return the term (or formula) with binder renamed
   \remarks uses Pflag on DAG descendent nodes that represent bound symbols
   \remarks non destructive
   \remarks tree-linear (DAG-exponential)
   \remarks this high complexity is acceptable because this is applied on the
   input formula before let expansion (i.e, before the tree explodes) so this
   is linear with respect to the size of the input
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers)
   \pre none
   \post two different binders bound different variables in the formula seen
   as a tree

   \remarks there is a mismatch in the semantics of the let between
   SMT-LIB 1 and SMT-LIB 2.  This function should only be called with
   SMT-LIB 2.  This let is a parallel let i.e. for (let ((x t1) (y
   t2)) t3), if t2 contains x, it is NOT the one associated to t1 but
   a free constant in this let construction.
   This is not a problem since SMT-LIB 1 lets are suppressed on parsing */
TDAG binder_rename(TDAG DAG);

/**
   \brief computes an equivalent term (or formula) where each bound variable
   is bounded by one binder only
   \param n the number of input terms (or formulas)
   \param Psrc the input terms (or formulas)
   \remarks uses Pflag on DAG descendent nodes that represent bound symbols
   \remarks destructive
   \remarks tree-linear (DAG-exponential)
   \remarks this high complexity is acceptable because this is applied on the
   input formula before let expansion (i.e, before the tree explodes) so this
   is linear with respect to the size of the input
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers)
   \pre none
   \post two different binders bound different variables in the formula seen
   as a tree

   \remarks there is a mismatch in the semantics of the let between
   SMT-LIB 1 and SMT-LIB 2.  This function should only be called with
   SMT-LIB 2.  This let is a parallel let i.e. for (let ((x t1) (y
   t2)) t3), if t2 contains x, it is NOT the one associated to t1 but
   a free constant in this let construction.
   This is not a problem since SMT-LIB 1 lets are suppressed on parsing */
void binder_rename_array(unsigned n, TDAG* Psrc);

#endif
