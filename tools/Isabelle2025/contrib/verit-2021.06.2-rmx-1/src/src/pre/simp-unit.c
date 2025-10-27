/*
  --------------------------------------------------------------
  Module for simplification of formulas using unit clauses
  --------------------------------------------------------------
*/

#include "utils/general.h"
#include "veriT-config.h"

/* #define DEBUG_US */

#ifdef DEBUG_US
#include "symbolic/DAG-print.h"
#endif
#include "congruence/congruence.h"
#include "pre/simp-unit.h"
#include "symbolic/DAG-flag.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG.h"
#include "symbolic/polarities.h"
#include "symbolic/veriT-status.h"
#include "undo.h"
#include "utils/options.h"
#include "veriT.h"

/*
  misc & (1 << 0) : DAG has been visited
  misc & (1 << 1) : DAG is unit with positive polarity
  misc & (1 << 2) : DAG is unit with negative polarity
  misc & (1 << 3) : atom/term is clean (quantifier-, apply-, ite-, var-free)
  misc & (1 << 4) : already checked that DAG is clean
*/

#define DAG_visited(DAG) (DAG_misc(DAG) & (1 << 0))
#define DAG_unit_pos(DAG) (DAG_misc(DAG) & (1 << 1))
#define DAG_unit_neg(DAG) (DAG_misc(DAG) & (1 << 2))
#define DAG_unit_pol(DAG) ((DAG_misc(DAG) >> 1) & 3)
#define DAG_clean(DAG) (DAG_misc(DAG) & (1 << 3))
#define DAG_checked(DAG) (DAG_misc(DAG) & (1 << 4))

#define DAG_set_visited(DAG) DAG_misc(DAG) |= 1 << 0;
#define DAG_set_unit_pos(DAG) DAG_misc(DAG) |= 1 << 1;
#define DAG_set_unit_neg(DAG) DAG_misc(DAG) |= 1 << 2;
#define DAG_set_unit_pol(DAG, pol) DAG_misc(DAG) |= 1 << pol;
#define DAG_set_clean(DAG) DAG_misc(DAG) |= 1 << 3;
#define DAG_set_checked(DAG) DAG_misc(DAG) |= 1 << 4;

/*
  --------------------------------------------------------------
  Cleaning
  --------------------------------------------------------------
*/

static void
us_clean(TDAG src)
/* PF empty NO queue and clean bits on the DAG */
{
	unsigned i;
	if (!DAG_misc(src) && !DAG_Pflag(src)) return;
	DAG_misc_set(src, 0);
	DAG_Pflag_set(src, NULL);
	for (i = 0; i < DAG_arity(src); i++) us_clean(DAG_arg(src, i));
}

/*
  --------------------------------------------------------------
  Finding unit clauses
  --------------------------------------------------------------
*/

/* PF
   A clean literal is ite, lamba, apply, and quantifier-free.
   It can then been fully handled by the NO module.
   This sub-module is responsible to compute
   - the number of clean unit literals (us_nb_units)
   - the number of clean non-unit literals (us_nb_non_units)
   - the number of clauses (us_nb_clauses), i.e. an indication of
   the importance of the Boolean structure

   All literals are marked such that
   DAG_unit_pos, DAG_unit_neg, DAG_clean are accurate

   Also, it adds all clean unit literals to NO, and
   corresponding DAGs to us_units.
*/

static bool us_conflict = false;
static unsigned us_nb_clauses = 0;
static Tstack_DAG us_units = NULL;

/**
   \brief check if input is an atom or term without quantifier,
   lambda, apply, ite and variable
   \param src an atom or term
   \return true if src is quantifier, lambda, apply, ite-free ground term */
static bool
DAG_is_clean(TDAG src)
{
	unsigned i;
	if (DAG_checked(src)) return DAG_clean(src);
	DAG_set_checked(src);
	if (
		quantifier(DAG_symb(src)) || DAG_symb(src) == FUNCTION_ITE ||
		DAG_symb(src) == LAMBDA || DAG_symb(src) == APPLY_LAMBDA ||
		(DAG_symb_type(DAG_symb(src)) & SYMB_VARIABLE))
		return false;
	for (i = 0; i < DAG_arity(src); i++)
		if (!DAG_is_clean(DAG_arg(src, i))) return false;
	DAG_set_checked(src);
	return true;
}

/**
   \brief collect unit literals in us_units
   \param src a formula
   \param pol the polarity of the formula */
static void
us_collect_units(TDAG src, Tpol pol)
{
	unsigned i;
	assert(pol == POL_POS || pol == POL_NEG);
	/* if src visited with same polarity, return */
	if (DAG_visited(src)) {
		assert(DAG_unit_pos(src) || DAG_unit_neg(src));
		assert(!DAG_unit_pos(src) || !DAG_unit_neg(src));
		if ((DAG_unit_pol(src) ^ pol) & 0x3) us_conflict = true;
		return;
	}
	assert(!DAG_unit_pol(src));
	if (!boolean_connector(DAG_symb(src))) {
		/* PF do not push because it contains
         quantifier, lambda, apply, and ite, not well handled by CC */
		if (DAG_is_clean(src) && DAG_arity(src))
			stack_push(us_units, DAG_dup((pol == POL_POS) ? src : DAG_not(src)));
	} else if (DAG_symb(src) == CONNECTOR_NOT)
		us_collect_units(DAG_arg0(src), INV_POL(pol));
	else if (
		(pol == POL_POS && DAG_symb(src) == CONNECTOR_AND) ||
		(pol == POL_NEG && DAG_symb(src) == CONNECTOR_OR))
		for (i = 0; i < DAG_arity(src); i++) us_collect_units(DAG_arg(src, i), pol);
	else if (pol == POL_NEG && DAG_symb(src) == CONNECTOR_IMPLIES) {
		us_collect_units(DAG_arg0(src), POL_POS);
		us_collect_units(DAG_arg1(src), POL_NEG);
	} else
		us_nb_clauses++;
	DAG_set_visited(src);
	DAG_set_unit_pol(src, pol);
}

/**
   \brief identify unit literals, add them in NO
   \return true if and only if it is useful to continue the process, i.e.
   there is some Boolean structure and there are some unit literals */
static bool
us_identify_units(TDAG src)
{
	us_conflict = false;
	us_nb_clauses = 0;
	us_collect_units(src, POL_POS);
#ifdef DEBUG_US
	my_message("us_nb_units %d\n", stack_size(us_units));
	my_message("us_nb_clauses %d\n", us_nb_clauses);
#endif
	return (us_conflict || (stack_size(us_units) && us_nb_clauses));
}

/*
  --------------------------------------------------------------
  Replacing deduced literals
  --------------------------------------------------------------
*/

/**
   \brief check if the value of any literal within src is implied by
   unit literals.  Populates Pflag with the rewritten formula
   \param src input formula
   \return true iff rewritten formula is different from original */
static bool
us_check_atoms_rewrite_aux(TDAG src)
{
	Tlit lit;
	if (DAG_Pflag(src)) return src != DAG_of_ptr(DAG_Pflag(src));
	if (boolean_connector(DAG_symb(src))) {
		unsigned i;
		bool changed = false;
		DAG_set_visited(src);
		for (i = 0; i < DAG_arity(src); i++)
			changed |= us_check_atoms_rewrite_aux(DAG_arg(src, i));
		if (changed) {
			TDAG *PDAG, tmp;
			MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
			for (i = 0; i < DAG_arity(src); i++)
				PDAG[i] = DAG_of_ptr(DAG_Pflag(DAG_arg(src, i)));
			tmp = DAG_new(DAG_symb(src), DAG_arity(src), PDAG);
			DAG_Pflag_set(src, DAG_ptr_of(tmp));
			return true;
		}
		DAG_Pflag_set(src, DAG_ptr_of(src));
		return false;
	}
	if (!DAG_is_clean(src) || DAG_arity(src) == 0) {
		DAG_Pflag_set(src, DAG_ptr_of(src));
		return false;
	}
	lit = DAG_to_lit(src);
	undo_level_new();
	if (CC_assert(lit) == UNSAT) {
#ifdef DEBUG_US
		my_DAG_message("Replacing %D with FALSE\n", src);
#endif
		DAG_Pflag_set(src, DAG_ptr_of(DAG_FALSE));
		undo_level_del();
		return true;
	}
	undo_level_del();
	undo_level_new();
	lit = lit_neg(lit);
	if (CC_assert(lit) == UNSAT) {
#ifdef DEBUG_US
		my_DAG_message("Replacing %D with TRUE\n", src);
#endif
		DAG_Pflag_set(src, DAG_ptr_of(DAG_TRUE));
		undo_level_del();
		return true;
	}
	undo_level_del();
	/* HERE INTEGRATE REWRITING */
	DAG_Pflag_set(src, DAG_ptr_of(src));
	return false;
}

/*
  --------------------------------------------------------------
  Public functions
  --------------------------------------------------------------
*/

TDAG
simplify_unit(TDAG src)
{
	TDAG dest;
	unsigned i;
	Tstatus status = SAT;
	stack_INIT(us_units);
#ifdef DEBUG_US
	my_DAG_message("SRC: %D\n", src);
#endif
	/* PF this is only useful for formulas that
     have some boolean structure */
	if (!us_identify_units(src))
		dest = DAG_dup(src);
	else if (us_conflict)
		dest = DAG_dup(DAG_FALSE);
	else {
		undo_top_level_new();
		undo_level_new();
		CC_notify_formula(src);
		/* my_message("simp_unit: got here\n"); */
		for (i = 0; status != UNSAT && i < stack_size(us_units); ++i) {
			TDAG unit = stack_get(us_units, i);
			Tlit lit;
			if (DAG_symb(unit) == CONNECTOR_NOT) {
				lit = lit_neg(DAG_to_lit(DAG_arg0(unit)));
				DAG_Pflag_set(DAG_arg0(unit), DAG_ptr_of(DAG_FALSE));
			} else {
				lit = DAG_to_lit(unit);
				DAG_Pflag_set(unit, DAG_ptr_of(DAG_TRUE));
			}
			status = CC_assert(lit);
		}
		if (status == UNSAT)
			dest = DAG_dup(DAG_FALSE);
		else {
			us_check_atoms_rewrite_aux(src);
			dest = DAG_dup(DAG_of_ptr(DAG_Pflag(src)));
			if (DAG_symb(dest) == CONNECTOR_AND) {
				for (i = 0; i < DAG_arity(dest); i++)
					stack_push(us_units, DAG_dup(DAG_arg(dest, i)));
			} else
				stack_push(us_units, DAG_dup(dest));
			DAG_free(dest);
			dest = DAG_dup(DAG_new_stack(CONNECTOR_AND, us_units));
		}
		undo_level_del();
		undo_top_level_del();
		literal_reset();
	}
	for (i = 0; i < stack_size(us_units); ++i) DAG_free(stack_get(us_units, i));
	stack_free(us_units);
	us_clean(src);
	SAT_reset();
#ifdef DEBUG_US
	if (src != dest) {
		my_DAG_message("simplify_unit src: %D\n", src);
		my_DAG_message("simplify_unit dest: %D\n", dest);
	}
#endif
	return dest;
}

void
simplify_unit_init(void)
{}

void
simplify_unit_done(void)
{}
