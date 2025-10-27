#ifndef DAG_SORT_PM_H
#define DAG_SORT_PM_H

#include "symbolic/DAG-sort.h"
#include "symbolic/DAG.h"
#include "utils/assoc.h"
#include "utils/list.h"

/**
   \brief Checks if sort1 subsumes sort2
   \param sort1 a sort
   \param sort2 a sort
   \returns 1 if sort1 subsumes sort2, 0 otherwise.
   \remark sort1 subsumes sort2 iff there is a substitution of sort variables
   s such that s(sort1) = sort2 */
extern bool DAG_sort_subsumes(Tsort sort1, Tsort sort2);

/*
  --------------------------------------------------------------
  sort unification
  --------------------------------------------------------------
*/

/*
  Sort unification is used to handle polymorphism.

  Sort unification is done in four steps.

  First all relevant sort unification constraints are collected in a
  list, using function DAG_sort_unif_constrain. Second DAG_sort_unif_solve
  is applied to the list. The result is the most general unifier (mgu) and
  is represented as a list of assocs with key a sort variable and
  value a sort. 

  Third, the mgu defines a sort substitution and may be applied to
  sorts and DAGs (DAG_sort_subst_DAG).

  Finally, when the mgu is no longer useful it shall be destructed
  with DAG_sort_unif_delete.
*/

/**
   \brief computes the unification of sort1 and sort2, if any
   \return the unifier, or DAG_SORT_NULL if there is no unification
   \sa DAG_sort_unify_n_v2, DAG_sort_unify_n_v1, DAG_sort_unify_n */
extern Tsort DAG_sort_unif_pair(Tsort sort1, Tsort sort2);

/**
   \param PDAG Array of DAGs for function application arguments
   \param Psort Array of sorts of function formal parameters
   \param n Size of PDAG and Psort
   \param sort Sort of function result
   \return returns the result of the unification of sort by the most
   general unifier of sorts of PDAG and Psort, if any.
   \return NULL if there is no most general unifier.
   \sa DAG_sort_subst_sort, DAG_sort_unif_pair */
extern Tsort DAG_sort_unif_apply(
	const TDAG* PDAG, const Tsort* Psort, const unsigned n, const Tsort sort);

/**
   \param PDAG Array of DAGs for function application arguments
   \param sort1 Sort of function formal parameters
   \param n Size of PDAG
   \param sort2 Sort of function result
   \return returns the result of the unification of sort2 by the most
   general unifier of sorts of PDAG and sort1, if any
   \return NULL if there is no most general unifier 
   \sa DAG_sort_unify_apply */
extern Tsort DAG_sort_unif_apply_polyadic(
	const TDAG* PDAG, const Tsort sort1, const unsigned n, const Tsort sort2);

/**
   \brief Applies a sort substitution to a DAG
   \param list a list of sort association pairs
   \param src a DAG
   \return the result of the sort substitution <b>list</b> applied to 
   <b>DAG</b>.
   \pre Every key in <b>list</b> is a sort variable. The DAG
   <b>src</b> may contain binders, but should satisfy the post-condition
   of <tt>binder_rename</tt>.
   \remark Calling this routine has side-effects:
   - on the field  <tt>binding</tt> of existing sorts;
   - on the fields <tt>Pflag</tt> and <tt>flag</tt> of <b>src</b> 
   and its sub-DAGs.
   \remark Complexity: linear in the size of <b>src</b>,
   \remark Does not decrease <tt>src</tt>'s reference counter.
*/
extern TDAG DAG_sort_subst_DAG(Tlist list, TDAG src);

#endif /* DAG_SORT_PM_H */
