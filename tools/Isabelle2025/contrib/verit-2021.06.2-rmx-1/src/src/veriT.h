/**
   \file veriT.h


   \brief API for the SMT solver VERI(T).

   This module provides satisfiability modulo theory (SMT) solving
   functionality and can be accessed from any tool requiring formal
   verification capabilities that fall within the scope of VERI(T),
   namely: quantified, or quantifier-free, first-order logic with
   uninterpreted functions, and linear arithmetics over integers
   or reals.

   The module requires that formulas representations with the TDAG type
   be built with veriT-DAG API.
*/

/**
   \mainpage libveriT documentation

   \section intro_sec Introduction

   The libveriT library provides a C-language API to access
   veriT's Satisfiability Modulo Theory (SMT) solver.

   The solver is currently integrating decision procedures for the
   following: uninterpreted functions with equality, difference
   arithmetics (integers and reals), linear arithmetics (integers and
   reals). The solver integrates some level of quantifier reasoning,
   using Skolemization and instantiation heuristics based on
   triggers. The superposition-based prover E is integrated as a
   fall-back verification engine for verification conditions with a
   unique sort.

   \section install_sec Installation

   There is currently no installation procedure available for using
   veriT as a library. From the distribution, all the src directory,
   but the parsers sub-directory and the main.c file, are required to
   package veriT as a library.

   \section licence_sec Licence

   The library uses third-party components and, as such, is subject to some
   constraints. To relieve our potential users from such constraints we are
   providing libveriT under two licenses BSD and LGPL.

   The functionality between these licenses is the same.

   LGPL-libveriT has arbitrary precision arithmetics.

   BSD-libveriT has fixed precision arithmetics, using native data
   types. Should overflow occur at run time, they are detected and an
   error is reported.

   \section authors_sec Authors

   Pascal Fontaine, David Deharbe are the two main developers. Diego
   Caminha has developed the difference logic verification engine
   and contributed to the design of the combination schema of the
   different "little engines" for reasoning. Thomas Bouton has
   contributed improvements to the interaction with the Boolean
   satisfiability engine as well as the QA infrastructure.

   This library is a product of the collaboration between the <a
   href="http://sites.google.com/site/forallufrn/people">ForAll</a>
   group at <a href="http://www.ufrn.br">Universidade Federal do Rio
   Grande do Norte</a> (Brazil) and the <a
   href="http://www.loria.fr/equipes/mosel/">Mosel</a> group at <a
   href="http://www.uhp-nancy.fr">Universit&eacute; Nancy 2</a> and
   <a href="http://www.loria.fr">LORIA</a> (France).
*/

#ifndef __VERIT_H
#define __VERIT_H

#include "symbolic/veriT-DAG.h"
#include "symbolic/veriT-status.h"
#include "utils/stack.h"
#include "config.h"

#ifdef HAVE_POSIX_TIMER
#include <sys/time.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#if defined(HAVE_POSIX_TIMER) || defined(HAVE_WINDOWS_H)
/** \addtogroup arguments_user

    - --max-time=n

    Sets maximal solving time to n in milliseconds. Measures wall
    clock time.  The timer is disarmed after veriT_check_sat returns to
    not interrupt e.g. proof production. Uses POSIX timers. Overwrites
    option_max_virtual_time. */
extern int option_max_time;
#endif

#if defined(HAVE_POSIX_TIMER)
/** \addtogroup arguments_user
    - --max-virtual-time=n

    Sets maximal solving time to n in milliseconds. Measures user-mode
    CPU time. The timer is disarmed after veriT_check_sat returns to
    not interrupt e.g. proof production.  Uses POSIX timers. */
extern int option_max_virtual_time;
#endif

#ifdef HAVE_WINDOWS_H
extern HANDLE windows_timer_queue;
#endif

/** \addtogroup arguments_developer

    - --print-simp-and-exit

    Loads formula, expands macros, applies selected simplifications,
    and prints on stdout in SMT format. */
/* TODO: update 2.0: Only for SMT-LIB 1.2. From 2.0 on, use commands. */
extern bool option_print_simp_and_exit;

/** \addtogroup arguments_developer

    - --output=smtlib2

    Sets the output format (smtlib2 is default). Meaningful only when output formulas are produced. */
extern char* output_format;

/** \addtogroup arguments_developer

   - --parse-only

   As print-simp-and-exit, but do not even parse. */
/* TODO: update 2.0: Only for SMT-LIB 1.2. From 2.0 on, use commands. */
extern bool option_parse_only;

/**
   \brief Initializes the module
   \remarks Must be called before any other function of the module
*/
void veriT_init(void);

/**
   \brief Set according to options
   \remarks Must be called after parsing options
*/
void veriT_set(void);

/**
   \brief Closes down the module
   \remarks Frees all the memory allocated by module functions
*/
void veriT_done(void);

/**
   \brief implement the SMT-LIB 2.0 set-logic command
   \param logic is the name of the logic using the SMT-LIB
   nomenclature, or NULL to set up the default logic.
   \remarks Exits through veriT_error if logic is unknown
*/
void veriT_logic(char* logic);

/**
   \brief implement the SMT-LIB 2.0 push command
   \param n an integer argument.
*/
void veriT_push(unsigned n);

/**
   \brief implement the SMT-LIB 2.0 pop command
   \param n an integer argument.
   \remarks Fails (with error) if initial assertion-set popped,
   i.e. if more pops than pushes
*/
void veriT_pop(unsigned n);

/**
   \brief implement the SMT-LIB 2.0 assert command
   \param DAG a formula
   \remarks Fails if formula is not Boolean
*/
void veriT_assert(TDAG DAG);

/**
   \brief implement the SMT-LIB 2.0 check-sat command
   \return The status of the verification, i.e.:
   - UNSAT, if the current set of assertions is unsatisfiable,
   - SAT, if the current set of assertions is satisfiable,
   - UNKNOWN if veriT fails to show either of the two previous
   outcomes.
   \sa veriT_status
*/
Tstatus veriT_check_sat(void);

#if defined(HAVE_POSIX_TIMER) || defined(HAVE_WINDOWS_H)
/** \brief Disarms the timeout timer if one is active */
void veriT_disarm_timer(void);
#endif

/**
   \brief check if the formulas given to veriT fall in a complete fragment
   handled by the tool
   \return 1 if so, 0 if not
*/
int veriT_complete(void);

/**
   \brief Returns the current status of the solver.
   The status may be:
   - SAT: satisfiable,
   - UNSAT: unsatisfiable,
   - UNDEF: not within the complete fragment handled, and solver was not able to conclude,
   - OPEN: not verified yet, run veriT_check_sat.
*/
Tstatus veriT_status(void);

/**
   \brief Outputs current model
*/
void veriT_model(void);

/**
   \brief Runs solver on the formula(s)

   \return This function runs the solver on the current set of formulas and
   returns the result:
   - SAT: satisfiable,
   - UNSAT: unsatisfiable,
   - UNDEF: not within the complete fragment handled, and solver was
   not able to conclude */
Tstatus veriT_solve(void);
/* IMPROVE duplicate veriT_solve vs. veriT_check_sat */

/**
   \brief Resets module.

   \remarks This function needs to be called to reset the library to
   add a new formula in non-interactive mode, or erase the added
   formulas, in interactive mode */
void veriT_reset(void);

/**
   \brief Builds a model for the conjunction of formulas.

   \pre   veriT_status() yields SAT.

   \param Pnb_literals address where function stores the number of
   literals in the model

   \param Pliterals address where function stores the model (an array
   of literals)

   \remarks If status is SAT, *Pnb_literals gets the number of
   literals in model and *Pliterals is assigned an array of
   *Pnb_literals TDAG representing the FOL literals composing the
   model */
void veriT_get_model(unsigned* Pnb_literals, TDAG** Pliterals);

/**
   \brief Prints proof of unsatisfiability on current output channel.

   \pre veriT_check_sat has been called and the resulting status is
   unsat. */
void veriT_get_proof(void);

void veriT_get_unsat_core(void);
void veriT_exit(int status);
/*
  --------------------------------------------------------------
  Assertion set
  --------------------------------------------------------------
*/

/** \brief contents of a level in the SMT-LIB2 stack of asserts */
typedef struct TSassertion_set
{
	unsigned level; /**< Used for internal sanity checks */
	Tstack_DAG DAG_stack; /**< DAGs pushed at current level */
} Tassertion_set;

TSstack(_assertion_set, Tassertion_set);

/**
    \brief the SMT-LIB2 stack of asserts (a stack of Tassertion_set) */
extern Tstack_assertion_set veriT_assertion_set_stack;

/*
   --------------------------------------------------------------

   User-controllable options

   --------------------------------------------------------------
*/
/** \brief options to print statistics report on exit */
extern bool veriT_print_report;

#endif /* __VERIT_H */
