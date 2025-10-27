/**
   \file veriT-DAG.h

   \brief API to build formulas, terms, symbols and sorts to use with libveriT.

   Type Tsort is for the representation of sorts, type Tsymb is for symbols,
   and type TDAG is for terms and formulas.

   A term or formula is made of a symbol and the sub-terms or
   sub-formulas.  A symbol has a sort. The sort for predicate and
   function symbols is a tuple of sorts, where the last element of the
   tuple is the sort of the result and the remaining elements of the
   type are the sorts of the parameters.

   Some technical details:

   - Formulas and terms are represented by DAGs.

   - Maximal sharing is used (two identical DAGs are merged.)

   - Facilities for sorts are provided.

   - Facilities for symbols are provided. Each symbol has a sort.

   - Declaring several times a same symbol is allowed, as long as
   declaration are coherent (i.e. the same).

   - Using an undeclared symbol issues a warning, but no error
   message.

   - DAGs are associated reference counters. For any used DAG, its
   reference counter should be greater than 0. DAGs are nonetheless
   created with a reference counter set to zero in the case of DAGs
   that are immediately set as subDAGs (think bottom-up construction
   of terms in a parser). In other situations, the reference_counter
   should be set explicitly to one by DAG_dup.

*/

#ifndef __VERIT_DAG_H
#define __VERIT_DAG_H

#include "symbolic/DAG-sort.h"
#include "symbolic/DAG-symb.h"
#include "symbolic/DAG.h"
#include "utils/assoc.h"
#include "utils/stack.h"
#include "utils/types.h"

#include <stdarg.h>

/*
  --------------------------------------------------------------
  Predefined sorts
  --------------------------------------------------------------
*/

/**
   \brief Name of predefined boolean sort */
extern const char* const SORT_BOOLEAN_STR;
/**
   \brief Name of predefined integer sort */
extern const char* const SORT_INTEGER_STR;
/**
   \brief Name of predefined real/rational sort */
extern const char* const SORT_REAL_STR;
/**
   \brief Name of predefined array sort */
extern const char* const SORT_ARRAY_STR;

/**
   \brief Predefined boolean sort */
extern Tsort SORT_BOOLEAN;
/**
   \brief Predefined integer sort */
extern Tsort SORT_INTEGER;
/**
   \brief Predefined real/rational sort */
extern Tsort SORT_REAL;

/*
  Rationale for adding the following two variables:

  In SMT-LIB2, a numeral constant may be either of sort Int or Real, depending
  on the logic used. The DAG library has routine to build the signature of a
  given logic. The parser calls this routine and thus sets up to following
  variables to the correct sort. Later, when the reads a constant, it only needs
  to call the DAG library routine according to the syntactic nature
  (numeral/decimal) */
/** \brief Sort used to build NUMERAL constants (SMT-LIB2) */
extern Tsort SORT_NUMERAL;

/*
  --------------------------------------------------------------
  Predefined symbols
  --------------------------------------------------------------
*/

/** \brief String for the predefined symbol TRUE */
extern const char* const BOOLEAN_TRUE_STR;
/** \brief String for the predefined symbol FALSE */
extern const char* const BOOLEAN_FALSE_STR;
/** \brief Predefined symbol for boolean constant TRUE */
extern Tsymb BOOLEAN_TRUE;
/** \brief Predefined symbol for boolean constant FALSE */
extern Tsymb BOOLEAN_FALSE;

/** \brief Name of predefined negation symbol */
extern const char* const CONNECTOR_NOT_STR;
/** \brief Name of predefined disjunction symbol */
extern const char* const CONNECTOR_OR_STR;
/** \brief Name of predefined exclusive disjunction symbol */
extern const char* const CONNECTOR_XOR_STR;
/** \brief Name of predefined conjunction symbol */
extern const char* const CONNECTOR_AND_STR;
/** \brief Name of predefined implication symbol */
extern const char* const CONNECTOR_IMPLIES_STR;
/** \brief Name of predefined equivalence symbol */
extern const char* const CONNECTOR_EQUIV_STR;
/** \brief Name of predefined (boolean) conditional symbol */
extern const char* const CONNECTOR_ITE_STR;
/** \brief Predefined symbol for negation */
extern Tsymb CONNECTOR_NOT;
/** \brief Predefined symbol for disjunction (n-ary) */
extern Tsymb CONNECTOR_OR;
/** \brief Predefined symbol for exclusive or (n-ary) */
extern Tsymb CONNECTOR_XOR;
/** \brief Predefined symbol for conjunction (n-ary) */
extern Tsymb CONNECTOR_AND;
/** \brief Predefined symbol for implication */
extern Tsymb CONNECTOR_IMPLIES;
/** \brief Predefined symbol for equivalence */
extern Tsymb CONNECTOR_EQUIV;
/** \brief Predefined symbol for (boolean) conditional */
extern Tsymb CONNECTOR_ITE;

/** \brief String for the equality symbol */
extern const char* const PREDICATE_EQ_STR;
/** \brief Predefined symbol for the equality operator */
extern Tsymb PREDICATE_EQ;
/** \brief String for the distinct symbol */
extern const char* const PREDICATE_DISTINCT_STR;
/** \brief Predefined symbol for the distinct operator */
extern Tsymb PREDICATE_DISTINCT;
/** \brief String for the predefined relational symbol "smaller than" */
extern const char* const PREDICATE_LESS_STR;
/** \brief String for the predefined relational symbol "smaller or equal" */
extern const char* const PREDICATE_LEQ_STR;
/** \brief String for the predefined relational symbol "greater than" */
extern const char* const PREDICATE_GREATER_STR;
/** \brief String for the predefined relational symbol "greater or equal" */
extern const char* const PREDICATE_GREATEREQ_STR;
/** \brief String for the predefined predicate IsInt */
extern const char* const PREDICATE_ISINT_STR;

/** \brief Symbol for the strictly smaller than relational operator */
extern Tsymb PREDICATE_LESS;
/** \brief Symbol for the smaller or equal relational operator */
extern Tsymb PREDICATE_LEQ;
/** \brief Symbol for the strictly greater than relational operator */
extern Tsymb PREDICATE_GREATER;
/** \brief Symbol for the greater or equal relational operator */
extern Tsymb PREDICATE_GREATEREQ;
/** \brief Symbol for the predicate testing integrality on real numbers */
extern Tsymb PREDICATE_ISINT;

/** \brief String for variable 0 (for difference logic) */
extern const char* const FUNCTION_ZERO_VARIABLE_STR;
/** \brief Symbol for variable 0 (for difference logic) */
extern Tsymb FUNCTION_ZERO_VARIABLE;

/** \brief String of the predefined symbol for the functional conditional */
extern const char* const FUNCTION_ITE_STR;
/** \brief Symbol for the functional operator IF THEN ELSE */
extern Tsymb FUNCTION_ITE;

/** \brief String of the predefined symbol for addition */
extern const char* const FUNCTION_SUM_STR;
/** \brief String of the predefined symbol for multiplication */
extern const char* const FUNCTION_PROD_STR;
/** \brief String of the predefined symbol for opposite */
extern const char* const FUNCTION_UNARY_MINUS_STR;
/** \brief String of the predefined symbol for subtraction */
extern const char* const FUNCTION_MINUS_STR;
/** \brief String of the predefined symbol for division */
extern const char* const FUNCTION_DIV_STR;

/** \brief Predefined symbol for arithmetic sum (n-ary) */
extern Tsymb FUNCTION_SUM;
/** \brief Predefined symbol for arithmetic product (n-ary) */
extern Tsymb FUNCTION_PROD;
/** \brief Predefined symbol for arithmetic unary minus */
extern Tsymb FUNCTION_UNARY_MINUS;
/** \brief Alternative predefined symbol for arithmetic unary minus */
extern Tsymb FUNCTION_UNARY_MINUS_ALT;
/** \brief Predefined symbol for arithmetic binary minus */
extern Tsymb FUNCTION_MINUS;
/** \brief Predefined symbol for arithmetic division minus */
extern Tsymb FUNCTION_DIV;
/** \brief Predefined symbol for arithmetic modulo */
extern Tsymb FUNCTION_MOD;
/** \brief Predefined symbol for arithmetic absolute value */
extern Tsymb FUNCTION_ABS;

/** \brief String of the predefined symbol for existential quantification */
extern const char* const QUANTIFIER_EXISTS_STR;
/** \brief String of the predefined symbol for universal quantification */
extern const char* const QUANTIFIER_FORALL_STR;
/** \brief Predefined symbol for existential quantification */
extern Tsymb QUANTIFIER_EXISTS;
/** \brief Predefined symbol for universal quantification */
extern Tsymb QUANTIFIER_FORALL;

/** \brief String of the predefined symbol for let construction */
extern const char* const LET_STR;
/** \brief Predefined symbol for let construction */
extern Tsymb LET;

/** \brief String of the predefined symbol for lambda abstraction */
extern const char* const LAMBDA_STR;
/** \brief String of the predefined symbol for beta reduction */
extern const char* const APPLY_LAMBDA_STR;
/** \brief Predefined symbol for lambda-abstraction operator */
extern Tsymb LAMBDA;
/** \brief Predefined symbol for beta reduction */
extern Tsymb APPLY_LAMBDA;

/** \brief Predefined symbol for choice functions */
extern Tsymb CHOICE_FUNCTION;
/** \brief String of the predefined symbol for choice functions */
extern const char* const CHOICE_STR;

/** \brief String of predefined symbol for array element selection */
extern const char* const FUNCTION_SELECT_STR;
/** \brief String of predefined symbol for array element assignment */
extern const char* const FUNCTION_STORE_STR;
/** \brief Predefined symbol for array element selection */
extern Tsymb FUNCTION_SELECT;
/** \brief Predefined symbol for array element assignment */
extern Tsymb FUNCTION_STORE;

/**
   \brief Tests if symbol c is a boolean connector
   \param c shall be an expression of type Tsymb */
#define boolean_connector(c) \
	((c == CONNECTOR_NOT) || (c == CONNECTOR_OR) || (c == CONNECTOR_XOR) || \
	 (c == CONNECTOR_AND) || (c == CONNECTOR_IMPLIES) || \
	 (c == CONNECTOR_EQUIV) || (c == CONNECTOR_ITE))

/**
   \brief Tests if symbol c is a boolean constant
   \param c shall be an expression of type Tsymb */
#define boolean_constant(c) ((c == BOOLEAN_TRUE) || (c == BOOLEAN_FALSE))

/**
   \brief Tests if symbol c is a quantifier
   \param c shall be an expression of type Tsymb */
#define quantifier(c) ((c == QUANTIFIER_EXISTS) || (c == QUANTIFIER_FORALL))

/**
   \brief Tests if symbol c is a quantifier
   \param c shall be an expression of type Tsymb */
#define binder(c) \
	((c == QUANTIFIER_EXISTS) || (c == QUANTIFIER_FORALL) || (c == LAMBDA))

/**
   \brief Tests if symbol c is an arithmetic operator
   \param c shall be an expression of type Tsymb */
#define arith_function(c) \
	((c == FUNCTION_SUM) || (c == FUNCTION_PROD) || \
	 (c == FUNCTION_UNARY_MINUS) || (c == FUNCTION_UNARY_MINUS_ALT) || \
	 (c == FUNCTION_MINUS) || (c == FUNCTION_DIV))

/**
   \brief Tests if symbol c is a arithmetic comparison operator
   \param c shall be an expression of type Tsymb */
#define arith_comparison(c) \
	((c == PREDICATE_LESS) || (c == PREDICATE_LEQ) || \
	 (c == PREDICATE_GREATER) || (c == PREDICATE_GREATEREQ))

#define unary_minus(c) \
	((c == FUNCTION_UNARY_MINUS) || (c == FUNCTION_UNARY_MINUS_ALT))

/*
  --------------------------------------------------------------
  Predefined symbols
  --------------------------------------------------------------
*/

/** \brief Conveniency macro constructor for binary conjunctions */
#define DAG_and2(A, B) DAG_new_binary(CONNECTOR_AND, A, B)

/** \brief Conveniency macro constructor for equalities  */
#define DAG_eq(A, B) DAG_new_binary(PREDICATE_EQ, A, B)

/** \brief Conveniency macro constructor for equivalences */
#define DAG_equiv(A, B) DAG_new_binary(CONNECTOR_EQUIV, A, B)

/** \brief Conveniency macro constructor for equivalences */
#define DAG_implies(A, B) DAG_new_binary(CONNECTOR_IMPLIES, A, B)

/** \brief Conveniency macro constructor for negations */
#define DAG_not(A) DAG_new_unary(CONNECTOR_NOT, A)

/** \brief Conveniency macro for negating a DAG (literal) */
#define DAG_neg(A) DAG_symb(A) == CONNECTOR_NOT ? DAG_arg_last(A) : DAG_not(A)

/** \brief Conveniency macro constructor for negations of equality */
#define DAG_neq(A, B) DAG_not(DAG_eq(A, B))

/** \brief Conveniency macro constructor for binary disjunctions */
#define DAG_or2(A, B) DAG_new_binary(CONNECTOR_OR, A, B)

/** \brief Conveniency macro constructor for ternary disjunctions */
#define DAG_or3(A, B, C) DAG_new_args(CONNECTOR_OR, A, B, C, DAG_NULL)

/** \brief Conveniency macro constructor for addition */
#define DAG_plus(A, B) DAG_new_binary(FUNCTION_SUM, A, B)

/** \brief Conveniency macro constructor for subtraction */
#define DAG_minus(A, B) DAG_new_binary(FUNCTION_MINUS, A, B)

/** \brief Conveniency macro constructor for ite boolean terms */
#define DAG_ite(A, B, C) DAG_new_args(CONNECTOR_ITE, A, B, C, DAG_NULL)

extern TDAG DAG_TRUE; /*< \brief Represents the formula TRUE */
extern TDAG DAG_FALSE; /*< \brief Represents the formula FALSE */
extern TDAG DAG_ONE; /*< \brief Represents the term ONE */
extern TDAG DAG_ZERO; /*< \brief Represents the term ZERO */

/*
  --------------------------------------------------------------
  Misc functions
  --------------------------------------------------------------
*/

/**
   \brief returns a compatible sort, if any
   \param sort1 a sort
   \param sort2 a sort
   \return The combination of two sorts \f$s_1\f$ and \f$s_2\f$ is defined as
   - <tt>Real</tt> when eiter either \f$s_1\f$ or \f$s_2\f$ is <tt>Integer</tt>
   and the other is <tt>Real</tt>,
   - <tt>sort1</tt> when they are equal,
   - NULL, otherwise.
   \todo TODO: Make the result of this test dependent on theory. */
extern Tsort DAG_sort_combine(const Tsort sort1, const Tsort sort2);

extern void DAG_smtlib_logic_init(void);
/**
   \brief Sets the logic.
   \param str is the name of the logic, NULL for default setup. Nomenclature is that of SMT-LIB 2.0
   \note This function must be called at most once  */
extern void DAG_smtlib_logic_set(const char* str);

extern char* DAG_smtlib_logic(void);

extern void DAG_smtlib_logic_done(void);

#endif /* __VERIT_DAG_H */
