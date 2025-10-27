/*
  --------------------------------------------------------------
  Simplex variable
  --------------------------------------------------------------
*/

#ifndef SIMPLEX_H
#define SIMPLEX_H

#include "symbolic/veriT-status.h"
#include "utils/stack.h"

/**
   \typedef Tsimplex_var
   \brief Linear Arithmetic variable
   \remark variables are unsigned
   \remark variable 0 is used for the independant term, i.e. v_0 = 1 */
typedef unsigned Tsimplex_var;

TSstack(_simplex_var, Tsimplex_var);

extern Tstatus simplex_status;

/*
  --------------------------------------------------------------
  Access to integer variables: each integer variable has a 
  unique index (starting from 0 upwards).

  To implement branch-and-bound, we need to iterate through 
  all integer variables, and access to the current assignment
  (that may not be an integer value).
  --------------------------------------------------------------
*/

/**
   \brief Number of integer variables in simplex
   \remark 
*/
extern unsigned simplex_integer_var_begin(void);

/**
   \brief Number of integer variables in simplex
   \remark 
*/
extern unsigned simplex_integer_var_end(void);

/**
   \brief Number of integer variables in simplex
   \remark 
*/
extern Tsimplex_var simplex_integer_var_get(unsigned);

/**
   \var integer_stack
   \brief array with integer-valued variables
*/
extern Tstack_simplex_var integer_stack;

#endif
