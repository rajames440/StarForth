/* -------------------------------------------------------------- */

/**
   \brief Module for configuring the library to handle SMT-LIB logics.

   Internal functions create signatures for the following SMT-LIB
   theories: Core, Ints, Reals, ArraysEx, Reals_Ints.

   There is no support to create signatures for the following
   theories: Fixed_Size_BitVectors.

   The module provides the possibility to set up the DAG module with
   sorts and symbols corresponding to the following logics :
   ALIA, AUFLIA, AUFLIRA, AUFNIRA, LIA, LRA, NIA, NRA,
   QF_ALIA, QF_AUFLIA, QF_AX, QF_IDL, QF_LIA, QF_LRA,
   QF_NIA, QF_NRA, QF_RDL, QF_UF, QF_UFIDL, QF_UFLIA, QF_UFLRA,
   QF_UFNIA, QF_UFNRA, UF, UFIDL, UFLIA, UFLRA, UFNIA

   There is currently no support for the following logics:
   BV, QF_ABV, QF_AUFBV, QF_BV, QF_UFBV, UFBV.

   TODO: check if zero variable is always necessary or only for difference
   logic theories.
   TODO: logic Reals_Ints */

/* -------------------------------------------------------------- */

#include "response.h"
#include "symbolic/DAG-arith.h"
#include "symbolic/DAG.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"
#include "utils/options.h"
#include "veriT-config.h"
#include "veriT-state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Tsort SORT_BOOLEAN = DAG_SORT_NULL;
const char* const SORT_BOOLEAN_STR = "Bool";

Tsort SORT_INTEGER = DAG_SORT_NULL;
Tsort SORT_REAL = DAG_SORT_NULL;
Tsort SORT_ARRAY = DAG_SORT_NULL;
const char* const SORT_INTEGER_STR = "Int";
const char* const SORT_REAL_STR = "Real";
const char* const SORT_ARRAY_STR = "Array";

Tsort SORT_NUMERAL =
	DAG_SORT_NULL; /**< sort for numeral constants, as per SMT-LIB 2 */

/** \brief Predefined symbol for let construction. */
Tsymb LET = DAG_SYMB_NULL;
/** \brief String of the predefined symbol for let construction. */
const char* const LET_STR = "let";

/** \brief Predefined symbol for lambda-abstraction operator. */
Tsymb LAMBDA = DAG_SYMB_NULL;
/** \brief Predefined symbol for beta reduction. */
Tsymb APPLY_LAMBDA = DAG_SYMB_NULL;
/** \brief String of the predefined symbol for lambda abstraction. */
const char* const LAMBDA_STR = "@lambda";
/** \brief String of the predefined symbol for beta reduction. */
const char* const APPLY_LAMBDA_STR = "@apply";

/** \brief Predefined symbol for choice functions */
Tsymb CHOICE_FUNCTION = DAG_SYMB_NULL;
/** \brief String of the predefined symbol for choice functions */
const char* const CHOICE_STR = "choice";

Tsymb BOOLEAN_TRUE = DAG_SYMB_NULL;
Tsymb BOOLEAN_FALSE = DAG_SYMB_NULL;
const char* const BOOLEAN_TRUE_STR = "true";
const char* const BOOLEAN_FALSE_STR = "false";

Tsymb CONNECTOR_NOT = DAG_SYMB_NULL;
Tsymb CONNECTOR_OR = DAG_SYMB_NULL;
Tsymb CONNECTOR_XOR = DAG_SYMB_NULL;
Tsymb CONNECTOR_AND = DAG_SYMB_NULL;
Tsymb CONNECTOR_IMPLIES = DAG_SYMB_NULL;
Tsymb CONNECTOR_EQUIV = DAG_SYMB_NULL;
Tsymb CONNECTOR_ITE = DAG_SYMB_NULL;
const char* const CONNECTOR_NOT_STR = "not";
const char* const CONNECTOR_OR_STR = "or";
const char* const CONNECTOR_XOR_STR = "xor";
const char* const CONNECTOR_AND_STR = "and";
const char* const CONNECTOR_IMPLIES_STR = "=>";
const char* const CONNECTOR_EQUIV_STR = "=";
const char* const CONNECTOR_ITE_STR = "ite";

Tsymb FUNCTION_ITE = DAG_SYMB_NULL;
const char* const FUNCTION_ITE_STR = "ite";

Tsymb PREDICATE_DISTINCT = DAG_SYMB_NULL;
const char* const PREDICATE_DISTINCT_STR = "distinct";

Tsymb PREDICATE_LESS = DAG_SYMB_NULL;
Tsymb PREDICATE_LEQ = DAG_SYMB_NULL;
Tsymb PREDICATE_GREATER = DAG_SYMB_NULL;
Tsymb PREDICATE_GREATEREQ = DAG_SYMB_NULL;
Tsymb PREDICATE_ISINT = DAG_SYMB_NULL;
const char* const PREDICATE_LESS_STR = "<";
const char* const PREDICATE_LEQ_STR = "<=";
const char* const PREDICATE_GREATER_STR = ">";
const char* const PREDICATE_GREATEREQ_STR = ">=";
const char* const PREDICATE_ISINT_STR = "is_int";

Tsymb FUNCTION_TOREAL = DAG_SYMB_NULL;
Tsymb FUNCTION_TOINT = DAG_SYMB_NULL;
const char* const FUNCTION_TOREAL_STR = "to_real";
const char* const FUNCTION_TOINT_STR = "to_int";

Tsymb FUNCTION_SUM = DAG_SYMB_NULL;
Tsymb FUNCTION_PROD = DAG_SYMB_NULL;
Tsymb FUNCTION_UNARY_MINUS = DAG_SYMB_NULL;
Tsymb FUNCTION_UNARY_MINUS_ALT = DAG_SYMB_NULL;
Tsymb FUNCTION_MINUS = DAG_SYMB_NULL;
Tsymb FUNCTION_DIV = DAG_SYMB_NULL;
Tsymb FUNCTION_ABS = DAG_SYMB_NULL;
Tsymb FUNCTION_MOD = DAG_SYMB_NULL;
/** \brief String of the predefined symbol for addition. */
const char* const FUNCTION_SUM_STR = "+";
/** \brief String of the predefined symbol for multiplication. */
const char* const FUNCTION_PROD_STR = "*";
/** \brief String of the predefined symbol for opposite. */
const char* const FUNCTION_UNARY_MINUS_STR = "-";
/** \brief String of the predefined symbol for subtraction. */
const char* const FUNCTION_MINUS_STR = "-";
/** \brief String of the predefined symbol for integer division. */
const char* const FUNCTION_INT_DIV_STR = "div";
/** \brief String of the predefined symbol for division. */
const char* const FUNCTION_DIV_STR = "/";
/** \brief String of the predefined symbol for absolute value. */
const char* const FUNCTION_ABS_STR = "abs";
/** \brief String of the predefined symbol for modulo. */
const char* const FUNCTION_MOD_STR = "mod";

Tsymb FUNCTION_SELECT = DAG_SYMB_NULL;
Tsymb FUNCTION_STORE = DAG_SYMB_NULL;

/** \brief String of predefined symbol for array element selection. */
const char* const FUNCTION_SELECT_STR = "select";
/** \brief String of predefined symbol for array element assignment. */
const char* const FUNCTION_STORE_STR = "store";

TDAG DAG_TRUE = DAG_NULL;
TDAG DAG_FALSE = DAG_NULL;

TDAG DAG_ONE = DAG_NULL;
TDAG DAG_ZERO = DAG_NULL;

/**
   \brief String of the predefined symbol for variable 0 (for difference logic) */
const char* const FUNCTION_ZERO_VARIABLE_STR = "veriT__zero";
Tsymb FUNCTION_ZERO_VARIABLE = DAG_SYMB_NULL;

static char* logic = NULL;
static bool init = false;

/*
  ----------------------------------------------------------------
  Theory stuff
  ----------------------------------------------------------------
*/

/**
   \brief Creates symbols for quantifiers */
static void
DAG_sig_quantifiers(void)
{
	Tsymb_type type = SYMB_INTERPRETED + SYMB_PREDEFINED + SYMB_QUANTIFIER;
	QUANTIFIER_EXISTS = DAG_symb_new(QUANTIFIER_EXISTS_STR, type, SORT_BOOLEAN);
	QUANTIFIER_FORALL = DAG_symb_new(QUANTIFIER_FORALL_STR, type, SORT_BOOLEAN);
}

/* -------------------------------------------------------------- */

static void
DAG_sig_let(void)
{
	Tsymb_type type = SYMB_INTERPRETED + SYMB_PREDEFINED;
	LET = DAG_symb_new(LET_STR, type, DAG_SORT_NULL);
}

/* -------------------------------------------------------------- */

static void
DAG_sig_extensions(void)
{
	Tsymb_type type = SYMB_INTERPRETED + SYMB_PREDEFINED;
	LAMBDA = DAG_symb_new(LAMBDA_STR, type, DAG_SORT_NULL);
	APPLY_LAMBDA = DAG_symb_new(APPLY_LAMBDA_STR, type, DAG_SORT_NULL);
}

/* -------------------------------------------------------------- */

static void
DAG_proof_extensions(void)
{
	Tsymb_type type = SYMB_INTERPRETED + SYMB_PREDEFINED;
	CHOICE_FUNCTION = DAG_symb_new(CHOICE_STR, type, DAG_SORT_NULL);
}

/* -------------------------------------------------------------- */

/**
   \brief Creates symbols for core operators
   \note Reference: The SMT-LIB Standard Version 2.0. p.30.

   \verbatim
   :sorts ((Bool 0))

   :funs ((true Bool)
   (false Bool)
   (not Bool Bool)
   (=> Bool Bool Bool :right-assoc)
   (and Bool Bool Bool :left-assoc)
   (or Bool Bool Bool :left-assoc)
   (xor Bool Bool Bool :left-assoc)
   (par (A) (= A A Bool :chainable))
   (par (A) (distinct A A Bool :pairwise))
   (par (A) (ite Bool A A A))
   \endverbatim
*/
static void
DAG_sig_smtlib2_Core(void)
{
	Tsymb_type type;
	Tsort sort;
	Tsort sortv = DAG_sort_new_var(NULL);

	SORT_BOOLEAN = DAG_sort_new(SORT_BOOLEAN_STR, 0, NULL);
	DAG_sort_set_predefined(SORT_BOOLEAN);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED + SYMB_BOOLEAN_CONSTANT;
	BOOLEAN_TRUE = DAG_symb_new(BOOLEAN_TRUE_STR, type, SORT_BOOLEAN);
	BOOLEAN_FALSE = DAG_symb_new(BOOLEAN_FALSE_STR, type, SORT_BOOLEAN);

	DAG_TRUE = DAG_dup(DAG_new_nullary(BOOLEAN_TRUE));
	DAG_FALSE = DAG_dup(DAG_new_nullary(BOOLEAN_FALSE));

	type = SYMB_INTERPRETED + SYMB_PREDEFINED + SYMB_BOOLEAN_CONNECTOR;
	/* not */
	sort = DAG_sort_new_args(NULL, 2, SORT_BOOLEAN, SORT_BOOLEAN, NULL);
	CONNECTOR_NOT = DAG_symb_new(CONNECTOR_NOT_STR, type, sort);

	/* => and or xor = */
	sort =
		DAG_sort_new_args(NULL, DAG_SORT_NARY, SORT_BOOLEAN, SORT_BOOLEAN, NULL);
	CONNECTOR_IMPLIES = DAG_symb_new(CONNECTOR_IMPLIES_STR, type, sort);
	CONNECTOR_AND = DAG_symb_new(CONNECTOR_AND_STR, type, sort);
	CONNECTOR_OR = DAG_symb_new(CONNECTOR_OR_STR, type, sort);
	CONNECTOR_XOR = DAG_symb_new(CONNECTOR_XOR_STR, type, sort);
	CONNECTOR_EQUIV = DAG_symb_new(CONNECTOR_EQUIV_STR, type, sort);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED + SYMB_PREDICATE;
	/* = distinct */
	sort = DAG_sort_new_args(NULL, DAG_SORT_NARY, sortv, SORT_BOOLEAN, NULL);
	PREDICATE_EQ = DAG_symb_new(PREDICATE_EQ_STR, type, sort);
	PREDICATE_DISTINCT = DAG_symb_new(PREDICATE_DISTINCT_STR, type, sort);

	/* ite */
	type = SYMB_INTERPRETED + SYMB_PREDEFINED;
	sort = DAG_sort_new_args(NULL, 4, SORT_BOOLEAN, sortv, sortv, sortv, NULL);
	FUNCTION_ITE = DAG_symb_new(FUNCTION_ITE_STR, type, sort);
	type = SYMB_INTERPRETED + SYMB_PREDEFINED + SYMB_BOOLEAN_CONNECTOR;
	sort = DAG_sort_new_args(
		NULL, 4, SORT_BOOLEAN, SORT_BOOLEAN, SORT_BOOLEAN, SORT_BOOLEAN, NULL);
	CONNECTOR_ITE = DAG_symb_new(CONNECTOR_ITE_STR, type, sort);
}

/* -------------------------------------------------------------- */

/**
   \note From www.smtlib.org

   \verbatim
   :funs ((NUMERAL Int)
   (- Int Int)                 ; negation
   (- Int Int Int :left-assoc) ; subtraction
   (+ Int Int Int :left-assoc)
   (* Int Int Int :left-assoc)
   (div Int Int Int :left-assoc)
   (mod Int Int Int)
   (abs Int Int)
   (<= Int Int Bool :chainable)
   (<  Int Int Bool :chainable)
   (>= Int Int Bool :chainable)
   (>  Int Int Bool :chainable)
   )
   \endverbatim
*/
static void
DAG_sig_smtlib2_Ints(void)
{
	Tsymb_type type;
	Tsort sort;

	/* Int */
	SORT_INTEGER = DAG_sort_new(SORT_INTEGER_STR, 0, NULL);
	DAG_sort_set_predefined(SORT_INTEGER);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED;
	/* - (negation), abs */
	sort = DAG_sort_new_args(NULL, 2, SORT_INTEGER, SORT_INTEGER, NULL);
	FUNCTION_UNARY_MINUS = DAG_symb_new(FUNCTION_UNARY_MINUS_STR, type, sort);
	FUNCTION_ABS = DAG_symb_new(FUNCTION_ABS_STR, type, sort);
	/* - (subtraction), +, *, div */
	sort =
		DAG_sort_new_args(NULL, DAG_SORT_NARY, SORT_INTEGER, SORT_INTEGER, NULL);
	FUNCTION_MINUS = DAG_symb_new(FUNCTION_MINUS_STR, type, sort);
	FUNCTION_SUM = DAG_symb_new(FUNCTION_SUM_STR, type, sort);
	FUNCTION_PROD = DAG_symb_new(FUNCTION_PROD_STR, type, sort);
	FUNCTION_DIV = DAG_symb_new(FUNCTION_INT_DIV_STR, type, sort);
	/* mod */
	sort =
		DAG_sort_new_args(NULL, 3, SORT_INTEGER, SORT_INTEGER, SORT_INTEGER, NULL);
	FUNCTION_MOD = DAG_symb_new(FUNCTION_MOD_STR, type, sort);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED + SYMB_PREDICATE;
	/* <= < >= > */
	sort =
		DAG_sort_new_args(NULL, DAG_SORT_NARY, SORT_INTEGER, SORT_BOOLEAN, NULL);
	PREDICATE_LESS = DAG_symb_new(PREDICATE_LESS_STR, type, sort);
	PREDICATE_LEQ = DAG_symb_new(PREDICATE_LEQ_STR, type, sort);
	PREDICATE_GREATER = DAG_symb_new(PREDICATE_GREATER_STR, type, sort);
	PREDICATE_GREATEREQ = DAG_symb_new(PREDICATE_GREATEREQ_STR, type, sort);

	SORT_NUMERAL = SORT_INTEGER;
}

/* -------------------------------------------------------------- */

/**
   \brief Creates signature for SMT-LIB2 logic Reals
   \verbatim
   :sorts ((Real 0))

   :funs ((NUMERAL Real)
   (DECIMAL Real)
   (- Real Real)                  ; negation
   (- Real Real Real :left-assoc) ; subtraction
   (+ Real Real Real :left-assoc)
   (* Real Real Real :left-assoc)
   (/ Real Real Real :left-assoc)
   (<= Real Real Bool :chainable)
   (<  Real Real Bool :chainable)
   (>= Real Real Bool :chainable)
   (>  Real Real Bool :chainable)
   )
   \verbatim
*/
static void
DAG_sig_smtlib2_Reals(void)
{
	Tsymb_type type;
	Tsort sort;

	/* Real */
	SORT_REAL = DAG_sort_new(SORT_REAL_STR, 0, NULL);
	DAG_sort_set_predefined(SORT_REAL);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED;
	/* - (negation) */
	sort = DAG_sort_new_args(NULL, 2, SORT_REAL, SORT_REAL, NULL);
	FUNCTION_UNARY_MINUS = DAG_symb_new(FUNCTION_UNARY_MINUS_STR, type, sort);
	/* - (subtraction), +, *, / */
	sort = DAG_sort_new_args(NULL, DAG_SORT_NARY, SORT_REAL, SORT_REAL, NULL);
	FUNCTION_MINUS = DAG_symb_new(FUNCTION_MINUS_STR, type, sort);
	FUNCTION_SUM = DAG_symb_new(FUNCTION_SUM_STR, type, sort);
	FUNCTION_PROD = DAG_symb_new(FUNCTION_PROD_STR, type, sort);
	FUNCTION_DIV = DAG_symb_new(FUNCTION_DIV_STR, type, sort);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED + SYMB_PREDICATE;
	/* <= < >= > */
	sort = DAG_sort_new_args(NULL, DAG_SORT_NARY, SORT_REAL, SORT_BOOLEAN, NULL);
	PREDICATE_LESS = DAG_symb_new(PREDICATE_LESS_STR, type, sort);
	PREDICATE_LEQ = DAG_symb_new(PREDICATE_LEQ_STR, type, sort);
	PREDICATE_GREATER = DAG_symb_new(PREDICATE_GREATER_STR, type, sort);
	PREDICATE_GREATEREQ = DAG_symb_new(PREDICATE_GREATEREQ_STR, type, sort);

	SORT_NUMERAL = SORT_REAL;
}

/**
   \brief Creates signature for SMT-LIB2 logic QF_UFIDL
   \verbatim
   :theories (Ints)

   :language
   "Closed quantifier-free formulas built over an arbitrary expansion with
   free sort and function symbols of the signature consisting of
   - all the sort and function symbols of Core and
   - the following symbols of Int:

   :sorts ((Int 0))
   :funs ((NUMERAL Int)
   (- Int Int Int)
   (+ Int Int Int)
   (<= Int Int Bool)
   (< Int Int Bool)
   (>= Int Int Bool)
   (> Int Int Bool)
   )
   \endverbatim
*/
static void
DAG_sig_smtlib2_QF_UFIDL(void)
{
	Tsymb_type type;
	Tsort sort;

	/* Int */
	SORT_INTEGER = DAG_sort_new(SORT_INTEGER_STR, 0, NULL);
	DAG_sort_set_predefined(SORT_INTEGER);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED;
	/* DD: Unary minus is not in the logic, but benchmark QF_UFIDL/check/bignum_idl1.smt2 uses it */
	/* - (unary minus) */
	sort = DAG_sort_new_args(NULL, 2, SORT_INTEGER, SORT_INTEGER, NULL);
	FUNCTION_UNARY_MINUS = DAG_symb_new(FUNCTION_UNARY_MINUS_STR, type, sort);

	/* - (subtraction), +, */
	sort =
		DAG_sort_new_args(NULL, DAG_SORT_NARY, SORT_INTEGER, SORT_INTEGER, NULL);
	FUNCTION_MINUS = DAG_symb_new(FUNCTION_MINUS_STR, type, sort);
	FUNCTION_SUM = DAG_symb_new(FUNCTION_SUM_STR, type, sort);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED + SYMB_PREDICATE;
	/* <= < >= > */
	sort =
		DAG_sort_new_args(NULL, 3, SORT_INTEGER, SORT_INTEGER, SORT_BOOLEAN, NULL);
	PREDICATE_LESS = DAG_symb_new(PREDICATE_LESS_STR, type, sort);
	PREDICATE_LEQ = DAG_symb_new(PREDICATE_LEQ_STR, type, sort);
	PREDICATE_GREATER = DAG_symb_new(PREDICATE_GREATER_STR, type, sort);
	PREDICATE_GREATEREQ = DAG_symb_new(PREDICATE_GREATEREQ_STR, type, sort);

	SORT_NUMERAL = SORT_INTEGER;
}

/**
   \brief a variable used to rewrite expressions in difference logic
   \param sort The sort of numbers (SORT_INTEGER or SORT_REAL) */
static void
DAG_sig_zero(Tsort sort)
{
	Tsymb_type type = SYMB_INTERPRETED + SYMB_PREDEFINED;
	FUNCTION_ZERO_VARIABLE = DAG_symb_new(FUNCTION_ZERO_VARIABLE_STR, type, sort);
}

/* TODO: simplify uses DAG_ONE and DAG_ZERO - see how to best code this
   dependency. */
static void
DAG_sig_arith_constants(void)
{
	if (SORT_INTEGER == DAG_SORT_NULL) {
		SORT_INTEGER = DAG_sort_new(SORT_INTEGER_STR, 0, NULL);
		DAG_sort_set_predefined(SORT_INTEGER);
	}
	if (DAG_ZERO == DAG_SYMB_NULL) {
		DAG_ZERO = DAG_dup(DAG_new_integer(0));
		DAG_ONE = DAG_dup(DAG_new_integer(1));
	}
}

/* -------------------------------------------------------------- */

/**
   \note From www.smtlib.org

   \verbatim
   :sorts ((Array 2))

   :funs ((par (X Y) (select (Array X Y) X Y))
   (par (X Y) (store (Array X Y) X Y (Array X Y))) )
   \endverbatim
*/
static void
DAG_sig_smtlib2_ArraysEx(void)
{
	Tsymb_type type;
	Tsort sort, X, Y, sort_inst;
	Tsort* sub;

	/* Array */
	SORT_ARRAY = DAG_sort_new_param(SORT_ARRAY_STR, 2);
	DAG_sort_set_predefined(SORT_ARRAY);

	type = SYMB_PREDEFINED;
	X = DAG_sort_new_var(NULL);
	Y = DAG_sort_new_var(NULL);

	MY_MALLOC(sub, 2 * sizeof(Tsort));
	sub[0] = X;
	sub[1] = Y;
	sort_inst = DAG_sort_new_inst(NULL, SORT_ARRAY, 2, sub);

	sort = DAG_sort_new_args(NULL, 3, sort_inst, X, Y, NULL);
	FUNCTION_SELECT = DAG_symb_new(FUNCTION_SELECT_STR, type, sort);

	sort = DAG_sort_new_args(NULL, 4, sort_inst, X, Y, sort_inst, NULL);
	FUNCTION_STORE = DAG_symb_new(FUNCTION_STORE_STR, type, sort);
}

/* -------------------------------------------------------------- */

/**
   \note From www.smtlib.org

   \todo TODO: negation, subtraction, addition, multiplication functions
	 and comparison predicates are overloaded - but veriT throughout
   (pre-processing, arithmetic module) only considers one possible
   instance of such symbols.

   \todo TODO: see how the divisible family of predicates should be implemented.

   \verbatim
   :sorts ((Int 0) (Real 0))

   :funs ((NUMERAL Int)
   (- Int Int)                 ; negation
   (- Int Int Int :left-assoc) ; subtraction
   (+ Int Int Int :left-assoc)
   (* Int Int Int :left-assoc)
   (div Int Int Int :left-assoc)
   (mod Int Int Int)
   (abs Int Int)
   (<= Int Int Bool :chainable)
   (<  Int Int Bool :chainable)
   (>= Int Int Bool :chainable)
   (>  Int Int Bool :chainable)
   (DECIMAL Real)
   (- Real Real)                  ; negation
   (- Real Real Real :left-assoc) ; subtraction
   (+ Real Real Real :left-assoc)
   (* Real Real Real :left-assoc)
   (/ Real Real Real :left-assoc)
   (<= Real Real Bool :chainable)
   (<  Real Real Bool :chainable)
   (>= Real Real Bool :chainable)
   (>  Real Real Bool :chainable)
   (to_real Int Real)
   (to_int Real Int)
   (is_int Real Bool)
   )

   :funs_description
   "All ranked function symbols of the form
   ((_ divisible n) Int Bool)
   where n is a positive numeral.
   \endverbatim
*/
static void
DAG_sig_smtlib2_Reals_Ints(void)
{
	Tsymb_type type;
	Tsort sort;
	type = SYMB_PREDEFINED;
	/* Int, Real */
	SORT_INTEGER = DAG_sort_new(SORT_INTEGER_STR, 0, NULL);
	DAG_sort_set_predefined(SORT_INTEGER);
	SORT_REAL = DAG_sort_new(SORT_REAL_STR, 0, NULL);
	DAG_sort_set_predefined(SORT_REAL);
	/* to_real */
	sort = DAG_sort_new_args(NULL, 2, SORT_INTEGER, SORT_REAL, NULL);
	FUNCTION_TOREAL = DAG_symb_new(FUNCTION_TOREAL_STR, type, sort);
	/* to_int */
	sort = DAG_sort_new_args(NULL, 2, SORT_REAL, SORT_INTEGER, NULL);
	FUNCTION_TOINT = DAG_symb_new(FUNCTION_TOINT_STR, type, sort);
	type = SYMB_PREDEFINED + SYMB_PREDICATE;
	/* is_int */
	sort = DAG_sort_new_args(NULL, 2, SORT_REAL, SORT_BOOLEAN, NULL);
	PREDICATE_ISINT = DAG_symb_new(PREDICATE_ISINT_STR, type, sort);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED;
	/* - (negation) */
	sort = DAG_sort_new_args(NULL, 2, SORT_REAL, SORT_REAL, NULL);
	FUNCTION_UNARY_MINUS = DAG_symb_new(FUNCTION_UNARY_MINUS_STR, type, sort);
	/* - (subtraction), +, *, / */
	sort = DAG_sort_new_args(NULL, DAG_SORT_NARY, SORT_REAL, SORT_REAL, NULL);
	FUNCTION_MINUS = DAG_symb_new(FUNCTION_MINUS_STR, type, sort);
	FUNCTION_SUM = DAG_symb_new(FUNCTION_SUM_STR, type, sort);
	FUNCTION_PROD = DAG_symb_new(FUNCTION_PROD_STR, type, sort);
	FUNCTION_DIV = DAG_symb_new(FUNCTION_DIV_STR, type, sort);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED + SYMB_PREDICATE;
	/* <= < >= > */
	sort = DAG_sort_new_args(NULL, DAG_SORT_NARY, SORT_REAL, SORT_BOOLEAN, NULL);
	PREDICATE_LESS = DAG_symb_new(PREDICATE_LESS_STR, type, sort);
	PREDICATE_LEQ = DAG_symb_new(PREDICATE_LEQ_STR, type, sort);
	PREDICATE_GREATER = DAG_symb_new(PREDICATE_GREATER_STR, type, sort);
	PREDICATE_GREATEREQ = DAG_symb_new(PREDICATE_GREATEREQ_STR, type, sort);

	type = SYMB_INTERPRETED + SYMB_PREDEFINED;
	/* abs */
	sort = DAG_sort_new_args(NULL, 2, SORT_INTEGER, SORT_INTEGER, NULL);
	FUNCTION_ABS = DAG_symb_new(FUNCTION_ABS_STR, type, sort);
	/* /\* div *\/ */
	/* sort = DAG_sort_new_args(NULL, DAG_SORT_NARY, SORT_INTEGER, SORT_INTEGER, NULL); */
	/* FUNCTION_INT_DIV = DAG_symb_new(FUNCTION_INT_DIV_STR, type, sort); */

	/* mod */
	sort =
		DAG_sort_new_args(NULL, 3, SORT_INTEGER, SORT_INTEGER, SORT_INTEGER, NULL);
	FUNCTION_MOD = DAG_symb_new(FUNCTION_MOD_STR, type, sort);

	SORT_NUMERAL = SORT_INTEGER;
}

Tsort
DAG_sort_combine(const Tsort sort1, const Tsort sort2)
{
	if (sort1 == sort2) return sort1;
	if (
		(sort1 == SORT_INTEGER && sort2 == SORT_REAL) ||
		(sort2 == SORT_INTEGER && sort1 == SORT_REAL))
		return SORT_REAL;
	return DAG_SORT_NULL;
}

/* -------------------------------------------------------------- */

void
DAG_smtlib_logic_set(const char* str)
{
	if (init)
		veriT_error("more than one set-logic command issued in this session");
	init = true;
	logic = strmake(str);

	DAG_sig_let();
	DAG_sig_smtlib2_Core();
	if (str != NULL) {
		if (strcmp(str, "UNKNOWN") == 0) {
			/* UNKNOWN reverts to AUFLIRA */
			veriT_err("warning: logic UNKNOWN is interpreted as AUFLIRA.");
			DAG_sig_quantifiers();
			DAG_sig_smtlib2_Reals_Ints();
			DAG_sig_smtlib2_ArraysEx();
			DAG_sig_arith_constants();
			Q_active = true;
			LA_active = true;
		} else if (strcmp(str, "ALIA") == 0) {
			DAG_sig_quantifiers();
			DAG_sig_smtlib2_Ints();
			DAG_sig_smtlib2_ArraysEx();
			DAG_sig_arith_constants();
			Q_active = true;
			LA_active = true;
		} else if (strcmp(str, "AUFLIA") == 0) {
			DAG_sig_quantifiers();
			DAG_sig_smtlib2_Ints();
			DAG_sig_smtlib2_ArraysEx();
			DAG_sig_arith_constants();
			Q_active = true;
			LA_active = true;
		} else if (strcmp(str, "AUFLIRA") == 0) {
			DAG_sig_quantifiers();
			DAG_sig_smtlib2_Reals_Ints();
			DAG_sig_smtlib2_ArraysEx();
			DAG_sig_arith_constants();
			Q_active = true;
			LA_active = true;
		} else if (strcmp(str, "LIA") == 0) {
			DAG_sig_quantifiers();
			DAG_sig_smtlib2_Ints();
			DAG_sig_arith_constants();
			Q_active = true;
			LA_active = true;
		} else if (strcmp(str, "LRA") == 0) {
			DAG_sig_quantifiers();
			DAG_sig_smtlib2_Reals();
			DAG_sig_arith_constants();
			Q_active = true;
			LA_active = true;
		} else if (strcmp(str, "QF_ALIA") == 0) {
			DAG_sig_smtlib2_Ints();
			DAG_sig_smtlib2_ArraysEx();
			DAG_sig_arith_constants();
			Q_active = false;
			LA_active = true;
		} else if (strcmp(str, "QF_AUFLIA") == 0) {
			DAG_sig_smtlib2_Ints();
			DAG_sig_smtlib2_ArraysEx();
			DAG_sig_arith_constants();
			LA_active = true;
		} else if (strcmp(str, "QF_AX") == 0) {
			DAG_sig_smtlib2_ArraysEx();
			LA_active = false;
		} else if (strcmp(str, "QF_IDL") == 0) {
			DAG_sig_smtlib2_Ints();
			DAG_sig_arith_constants();
			DAG_sig_zero(SORT_INTEGER);
			LA_active = true;
		} else if (strcmp(str, "QF_LIA") == 0) {
			DAG_sig_smtlib2_Ints();
			DAG_sig_arith_constants();
			LA_active = true;
		} else if (strcmp(str, "QF_LRA") == 0) {
			DAG_sig_smtlib2_Reals();
			DAG_sig_arith_constants();
			LA_active = true;
		} else if (strcmp(str, "QF_LIRA") == 0) {
			DAG_sig_smtlib2_Reals_Ints();
			DAG_sig_arith_constants();
			LA_active = true;
		} else if (strcmp(str, "QF_RDL") == 0) {
			DAG_sig_smtlib2_Reals();
			DAG_sig_arith_constants();
			DAG_sig_zero(SORT_REAL);
			LA_active = true;
		} else if (strcmp(str, "QF_UF") == 0) {
			LA_active = false;
		} else if (strcmp(str, "QF_UFIDL") == 0) {
			DAG_sig_smtlib2_QF_UFIDL();
			DAG_sig_arith_constants();
			DAG_sig_zero(SORT_INTEGER);
			LA_active = true;
		} else if (strcmp(str, "QF_UFLIA") == 0) {
			DAG_sig_smtlib2_Ints();
			DAG_sig_arith_constants();
			LA_active = true;
		} else if (strcmp(str, "QF_UFLRA") == 0) {
			DAG_sig_smtlib2_Reals();
			DAG_sig_arith_constants();
			LA_active = true;
		} else if (strcmp(str, "UF") == 0) {
			DAG_sig_quantifiers();
			Q_active = true;
			LA_active = false;
		} else if (strcmp(str, "UFIDL") == 0) {
			DAG_sig_quantifiers();
			DAG_sig_smtlib2_Ints();
			DAG_sig_arith_constants();
			DAG_sig_zero(SORT_INTEGER);
			Q_active = true;
			LA_active = true;
		} else if (strcmp(str, "UFLIA") == 0) {
			DAG_sig_quantifiers();
			DAG_sig_smtlib2_Ints();
			DAG_sig_arith_constants();
			DAG_sig_zero(SORT_INTEGER);
			Q_active = true;
			LA_active = true;
		} else if (strcmp(str, "UFLRA") == 0) {
			DAG_sig_quantifiers();
			DAG_sig_smtlib2_Reals();
			DAG_sig_arith_constants();
			Q_active = true;
			LA_active = true;
		} else
			veriT_error("unknown logic %s", str);
	}
	DAG_sig_extensions();
	DAG_proof_extensions();
}

/* -------------------------------------------------------------- */

char*
DAG_smtlib_logic(void)
{
	return logic;
}

/* -------------------------------------------------------------- */

void
DAG_smtlib_logic_init(void)
{
	assert(!init);
}

/* -------------------------------------------------------------- */

void
DAG_smtlib_logic_done(void)
{
	if (DAG_TRUE) DAG_free(DAG_TRUE);
	if (DAG_FALSE) DAG_free(DAG_FALSE);
	if (DAG_ZERO) DAG_free(DAG_ZERO);
	if (DAG_ONE) DAG_free(DAG_ONE);
	free(logic);
	init = false;
}
