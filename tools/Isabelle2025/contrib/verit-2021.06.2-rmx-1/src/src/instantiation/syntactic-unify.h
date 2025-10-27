#ifndef __SYNTACTIC_UNIFY_H
#define __SYNTACTIC_UNIFY_H

#include "instantiation/free-vars.h"
#include "symbolic/DAG.h"

/**
	\brief Syntictically unifies two quantifier free terms
	\param dag1 a term
	\param dag2 another term
	\return true if unification was successful.
	\remark The resulting substitution is saved in `DAG_tmp_DAG`. It is assumed
	        that `DAG_tmp_DAG` has been reserved before. The user is responsible
	        for resetting this structure by using `reset_substitution`. This is
	        necessary even if unification fails. The implementation is somewhat
	        naive. It is assumed that both arguments are quantifier free. */
bool unify_syntactic(TDAG dag1, TDAG dag2);

/**
	\brief Syntictically unifies two quantifier free terms but consider a some
	       variables as constants
	\param dag1 a term
	\param dag2 another term
	\param strong_vars sorted stack to consider as strong variables. They are never
	                   substituted and are considered as dependent on all other
	                   free variables in dag1 and dag2.
	\return true if unification was successful.
	\remark The resulting substitution is saved in `DAG_tmp_DAG`. It is assumed
	        that `DAG_tmp_DAG` has been reserved before. The user is responsible
	        for resetting this structure by using `reset_substitution`. This is
	        necessary even if unification fails. The implementation is somewhat
	        naive. It is assumed that both arguments are quantifier free. */
bool unify_syntactic_vars(TDAG dag1, TDAG dag2, Tstack_DAG strong_vars);

/**
	\brief Applies the substitution saved in DAG_tmp_DAG to the parameter
	\param dag a term
	\return The dag after the substitution has been applied.
	\remark If the dag is unchanged, its reference counter is increased
	        nevertheless. Furthermore, `set_fvars` is not called on the gernerated
	        DAG, since this function also uses DAG_tmp_DAG. */
TDAG apply_substitution(TDAG dag);

/**
	\brief Resets the entries of DAG_tmp_DAG used to save the subsitution
	\param dag1 the first argument of the corresponding `unify_syntactic` call
	\param dag2 the second argument of the corresponding `unify_syntactic` call
	\remark Has to be called even if `unify_syntactic` failed */
void reset_substitution(TDAG dag1, TDAG dag2);

/**
	\brief Tests if dag1 is as, or more general than dag2
	\parma dag1 A term
	\param dag2 Another term
	\returns true if dag2 is an instance of dag1
	\remark The terms should be quantifier free */
bool subsumes(TDAG dag1, TDAG dag2);

#endif /* __SYNTACTIC_UNIFY_H */
