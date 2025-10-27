/*
  --------------------------------------------------------------
  Module for skolemizing quantified formulas
  --------------------------------------------------------------
*/

#ifndef __SKOLEM_NEW_H
#define __SKOLEM_NEW_H

#include "proof/proof.h"
#include "symbolic/DAG.h"

/* DD+PF We assume no variable appears both free and bound.
   (In SMT, this is a requirement)
   We also assume that the formula is lambda-free.  Indeed,
   lambda may intervene also in the quantification process:
   \lambda u. \exists u (x) --> \lambda u. u (f(u)).
   Update : on 20070816, PF+DD do not understand the previous comment
   anymore, but agree that lambda may hurt
   More generally, we assume (and check) that the input is FOL.
   No macro should be used anywhere.
   The process is linear with respect to the DAG size of the
   ground part of the DAG, and linear with respect of the tree
   size of the DAG.  In other words, it is linear (% DAG) for
   ground formulas, but potentially exponential for formulas with
   quantifiers.
   Process is structural, outer Skolemization.
   It would be better to implement Andrews' (inner) Skolemization, but
   there are some complexity issues and technical difficulties that are
   not that easy to solve quickly, in a first approach.
   To be left as a student work?  If it is needed.
   No quantifier should appear inside a term.
   Non-destructive
   The reference counter of result DAG is at least one.
   TODO: this is a restriction for ite terms.

   Code reviewed by PF in august 2008

   DD 20110510 Added option for so-called shallow Skolemization. Skolemization
   is not applied within essentially universal quantifiers: only
   Skolem constants are generated. If universal quantifiers are instantiated,
   Skolemization is applied to the instances and may generate new Skolem
   constants.

   HB 20160725 Standardizing for context recursion (and proofs)
*/

extern void skolemize_init(void);
extern void skolemize_done(void);
extern TDAG skolemize(TDAG src);

extern TDAG skolemize_proof(TDAG src, Tproof* Pproof);

#endif /* __SKOLEM_H */
