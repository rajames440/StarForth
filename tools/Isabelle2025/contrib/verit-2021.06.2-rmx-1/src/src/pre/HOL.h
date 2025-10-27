/*
  --------------------------------------------------------------
  Module for handling higher order constants, beta reduction,...
  --------------------------------------------------------------
*/

#ifndef __HOL_H
#define __HOL_H

#include "proof/proof.h"
#include "symbolic/DAG.h"

/**
   \brief check a formula for higher-order constructions
   \param src the term (or formula) to check
   \return true iff src is FOL (no HOL construction), false otherwise
   \remarks Non destructive
   \remarks DAG-linear
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers)
   \remarks beware though that it does not check for non-expanded definitions
   e.g. define-fun that would not be applied */
bool is_FOL(TDAG src);

/**
   \brief check a formula for higher-order constructions
   \param src the term (or formula) to check
   \return true if src is FOL (no HOL construction), false otherwise
   \remarks Non destructive
   \remarks DAG-linear
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers)
   \remarks Compared to is_FOL, checks also for let, polymorphic sorts,
   and booleans as arguments of functions or as quantified vars
   \remarks beware though that it does not check for non-expanded definitions
   e.g. define-fun that would not be applied */
bool is_FOL_strict(TDAG src);

/**
   \brief check a formula for binders (lambda, quantifier, let)
   \param src the term (or formula) to check
   \return true if src does not contain binders, false otherwise
   \remarks Non destructive
   \remarks DAG-linear
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers) */
bool is_binder_free(TDAG src);

/**
   \brief check a formula for quantifiers
   \param src the term (or formula) to check
   \return true if src does not contain quantifiers, false otherwise
   \remarks Non destructive
   \remarks DAG-linear
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers) */
bool is_quantifier_free(TDAG src);

/**
   \brief eliminates higher-order constructions
   - applies beta-reduction
   - replaces defined-functions by their definition, i.e. handles the
   define-fun SMT-LIB construct
   - eliminates let
   - applies equality lowering wherever it can be.  Rewrites equalities
   T1 = T2 where T1 and T2 have function (or predicate) sort into
   forall x_1 ... x_n . T1(x_1, ... , x_n) = T2(x_1, ... , x_n)
   - rename every quantified variable to a fresh variable
   - check that the result is strictly FOL
   \param src the term (or formula) to rewrite
   \return the rewritten term
   \remark non destructive
   \remark DAG-linear on the binder-free part.  Tree linear below binders
   \remark no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers)
   \remark ite-, quantifier-, lambda-, apply-tolerant, let-tolerant
   \remark does not handle every HOL constructs, but if such an unsupported
   construct is met, an error message is issued
   \pre no requirement
   \post binder_rename invariant is satisfied */
TDAG HOL_to_FOL(TDAG src);

/**
   \brief array version of the HOL_to_FOL function
   \remark Destructive
   \see HOL_to_FOL */
void HOL_to_FOL_array(unsigned n, TDAG* Psrc);

extern TDAG let_elim_ctx(TDAG src);
extern TDAG var_renaming_ctx(TDAG src);

extern TDAG let_elim_ctx_proof(TDAG src, Tproof* Pproof);
extern TDAG var_renaming_ctx_proof(TDAG src, Tproof* Pproof);

#endif /* __HOL_H */
