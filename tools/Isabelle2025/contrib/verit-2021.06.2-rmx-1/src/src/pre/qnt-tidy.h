/*
  --------------------------------------------------------------
  Module for polishing quantified formulas
  --------------------------------------------------------------
*/

/*
  DD+PF+HB
  Originally created by DD+PF (august 2007?)
  Code review (EXCEPT Canonize) by PF in april 2008
  Code review global by PF in august 2008
  Code review global by HB in november 2016
*/

#ifndef __QNT_TIDY_H
#define __QNT_TIDY_H

#include "proof/proof.h"
#include "symbolic/DAG.h"
#include "symbolic/context-handling.h"
#include "symbolic/context-recursion-proof.h"

#ifdef DEBUG
/**
   \brief checks whether quantified sub-formulas contain free or reused 
          variables, fails otherwise.
   \param src a formula or term
   \remarks DAG-Linear with respect to the quantifier-free part,
   tree-Linear with respect to the quantified part
   \remark uses DAG_tmp */
extern void check_free_shadowed_vars(TDAG src);
#else
static inline void
check_free_shadowed_vars(TDAG src)
{}
#endif

/**
   \brief sequential quantifiers are joined into a single one
   \param src a formula or term
   \return DAG after function application
   \remark uses contextual structural recursion */

extern void qnt_join_init(void);
extern void qnt_join_push(TDAG src, unsigned* pos);
extern void qnt_join_pop(TDAG src, unsigned pos);
extern TDAG qnt_join_reduce(TDAG src);

extern void qnt_join_push_proof(TDAG src, unsigned* pos);
extern Tproof qnt_join_replacement(
	TDAG new_src, TDAG src, Tstack_proof reasons);
extern TDAG_proof qnt_join_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_transf);

/**
   \brief remove from binder variables not used in body
   \param src a formula or term
   \return DAG after function application
   \remark assumes no free nor captured variables
   \remark uses contextual recursion */

extern void qnt_rm_unused_init(void);
extern void qnt_rm_unused_push(TDAG src, unsigned* pos);
extern void qnt_rm_unused_pop(TDAG src, unsigned pos);
extern TDAG qnt_rm_unused_reduce(TDAG src);

extern void qnt_rm_unused_push_proof(TDAG src, unsigned* pos);
extern Tproof qnt_rm_unused_replacement(
	TDAG new_src, TDAG src, Tstack_proof reasons);
extern TDAG_proof qnt_rm_unused_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_transf);

/**
   \brief canonize bounded variables in quantifiers
   \param src a formula or term
   \return DAG after function application
   \remark renames quantified variables so that two identical quantified
   formulas (modulo renaming) are represented by the same DAG.  Moreover, the
   canonization is such that quantified variables are ordered according to a
   topological sort in the DAG. Quantified variables that do not occur in the
   matrix are removed. Thus applying qnt_canonize on the following formulas
   returns the same TDAG:

   \forall x1, y1 . p(x1, y1)
   \forall y2, x2 . p(x2, y2)
   \forall x, y, z . p(x, y)

   *However*: Formula \forall x @ \forall y @ p(x, y) would be
   represented by a different DAG.  This is taken care of by
   qnt_join_quantifiers (better called before)
   src should be lambda-free
   Tolerant with respect to apply, macros, ite terms

   \remark All functions except `qnt_canon_pop` also used for renaming.
   \remark uses contextual recursion */

extern void qnt_canon_rename_init(void);
extern void qnt_canon_rename_push(TDAG src, unsigned* pos);
extern void qnt_canon_pop(TDAG src, unsigned pos);
extern TDAG qnt_canon_rename_reduce(TDAG src);

extern void qnt_canon_rename_push_proof(TDAG src, unsigned* pos);
extern Tproof qnt_canon_rename_replacement(
	TDAG new_src, TDAG src, Tstack_proof reasons);
extern TDAG_proof qnt_canon_rename_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_transf);

/**
	\brief Renames bound variables to ensure that quantifiers have
	disjoint sets of variables.
	\remark This only differs in the pop function from qnt_canon_*
 */

extern void qnt_rename_pop(TDAG src, unsigned pos);

/**
   \brief supresses quantifiers when an equality perfectly defines the
   quantified variable
   \param src a formula or term
   \return DAG after function application
   \remark uses contextual recursion
   \remark assumes no variable used in imbricated quantifiers (qnt_canonize) */

extern void qnt_onepoint_init(void);
extern void qnt_onepoint_push(TDAG src, unsigned* pos);
extern void qnt_onepoint_pop(TDAG src, unsigned pos);
extern TDAG qnt_onepoint_reduce(TDAG src);

extern void qnt_onepoint_push_proof(TDAG src, unsigned* pos);
extern Tproof qnt_onepoint_replacement(
	TDAG new_src, TDAG src, Tstack_proof reasons);
extern TDAG_proof qnt_onepoint_reduce_proof(
	TDAG src, TDAG new_src, Tproof proof_transf);

/**
   \brief resize and clean table accordingly */
extern void DAG_symb_var_resize(unsigned n);

extern void qnt_tidy_init(void);
extern void qnt_tidy_done(void);

#endif /* __QNT_TIDY_NEW_H */
