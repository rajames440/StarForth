/* -*- mode: C -*-
   --------------------------------------------------------------
   Simplex
   --------------------------------------------------------------
*/

#ifndef __SIMPLEX_HW_H
#define __SIMPLEX_HW_H

#include "arith/simplex.h"
#include "bool/literal.h"
#include "numbers-hw.h"
#include "symbolic/veriT-status.h"

extern Tstatus simplex_status;

TSstack(_LAsigned_hw, TLAsigned_hw); /* typedefs Tstack_LAsigned_hw */

/**
   \brief create a new variable
   \param integer the variable is integer-valued
   \return the new variable
   \remark all created variables are non-basic */
extern Tsimplex_var simplex_hw_var_new(bool integer);

/**
   \brief declare that a new variable may have bounds
   \param var the variable */
extern void simplex_hw_var_bounded(Tsimplex_var var);

/**
   \brief number of variables in the Simplex */
extern unsigned simplex_hw_var_n(void);

/**
   \brief sets an upper bound for variable i
   \param i the variable
   \param delta the value
   \param lit the literal responsible for the bound
   \return UNDEF, SAT or UNSAT depending of the status
   \remark \c simplex_solve() \c is required for completeness if UNDEF is
   returned */
extern Tstatus simplex_hw_assert_upper_bound(
	Tsimplex_var i, TLAdelta_hw delta, Tlit lit);

/**
   \brief sets a lower bound for variable i
   \param i the variable
   \param delta the value
   \param lit the literal responsible for the bound
   \return UNDEF, SAT or UNSAT depending of the status
   \remark \c simplex_solve() \c is required for completeness if UNDEF is
   returned */
extern Tstatus simplex_hw_assert_lower_bound(
	Tsimplex_var i, TLAdelta_hw delta, Tlit lit);

/**
   \brief solve the set of constraints */
extern Tstatus simplex_hw_solve(void);

/**
   \brief simplify the set of constraints by removing unbounded variables
   \remark called once all variables which will have bounds are known */
extern void simplex_hw_simp_unbound(void);

/**
   \brief simplify the set of constraints by substituting all variables
   having a fixed value by their value.
   \remark non-backtrackable */
extern void simplex_hw_simp_const(void);

/**
   \brief puts the module back in a normal state
   \remark should be call on backtrack from an UNDEF or UNSAT state
   before any assert */
extern void simplex_hw_repair(void);

/**
   \brief get the set of literals leading to conflict  */
extern void simplex_hw_conflict(
	Tstack_lit* Pconflict, Tstack_simplex_var* Pvar_eq);

/**
   \brief Same as `simplex_hw_conflict`, but also returns a set of coefficients */
extern void simplex_hw_conflict_proof(
	Tstack_lit* Pconflict, Tstack_simplex_var* Pvar_eq,
	Tstack_LAsigned_hw* Pcoeffs);

/**
   \brief add a linear equality
   \param n the number of monoms in the linear equality vars * coefs = 0
   \param vars the variables
   \param coefs their coefficient */
extern void simplex_hw_constraint_push(
	unsigned n, Tsimplex_var* vars, TLAsigned_hw* coefs);

/**
   \brief comparison function defining a total order on simplex
   variables
   \param[in] v1 a simplex variable
   \param[in] v2 a simplex variable
   \pre all variables should have a satisfying assignment
   \return 0 if v1 and v2 are assigned the same value
   a negative value, if the value assigned to v1 is smaller than
   the value assigned to v2, a positive value otherwise */
extern int simplex_hw_var_cmp(Tsimplex_var v1, Tsimplex_var v2);

/**
   \brief tests if two variables have same assignment
   \param[in] v1 a simplex variable
   \param[in] v2 a simplex variable */
extern bool simplex_hw_var_eq(Tsimplex_var v1, Tsimplex_var v2);

/**
   \brief gets the assignment of a simplex variable
   \param[in] v a simplex variable
   \result address where the assignment is stored */
extern TLAdelta_hw* simplex_hw_var_assign(Tsimplex_var v);

/**
   \brief tests if a variable is an integer
   \param[in] v a simplex variable */
extern bool simplex_hw_var_integer(Tsimplex_var v);

/**
   \brief brings the current variable assignment of all simplex variables
   consistent with the current model. */
extern void simplex_hw_update_assign(void);

/**
   \brief Initializes the module */
extern void simplex_hw_init(void);

extern void simplex_hw_done(void);

#endif
