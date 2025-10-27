/*
  --------------------------------------------------------------
  Module for simplifying formulas and terms
  --------------------------------------------------------------
*/

#ifndef __SIMPLIFY_H
#define __SIMPLIFY_H

/* PF

   simplify_formula:
   simple transformation rules applied to obtain a formula that is
   logically equivalent to the input and hopefully simpler.
   The transformation is linear with respect to the size of the formula.
   src may contain quantifiers, lambda, apply, ...

   Code reviewed by PF in august 2008

   Code reviewed by HB in november 2016 */

#include "pre/simplify.h"
#include "symbolic/DAG.h"
#include "symbolic/context-recursion-proof.h"
#include "symbolic/context-recursion.h"

extern void simplify_init(void);
extern void simplify_done(void);

extern TDAG simplify_formula(TDAG src);

extern TDAG simplify_formula_proof(TDAG src, Tproof* Pproof);

#endif /* __SIMPLIFY_H */
