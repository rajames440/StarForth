#ifndef __CCFV_MOD_H
#define __CCFV_MOD_H

#include "instantiation/free-vars.h"
#include "instantiation/unify.h"
#include "undo.h"

/*
  \brief Handles DAGs modulo a unifier */

/*
  --------------------------------------------------------------
  Data structure
  --------------------------------------------------------------
*/

/*
  TODO: for predecessors use a struct with the DAG and the position it appears
  in its arguments list? I have that info ready when building the list

  TODO: talk with Pascal about this sig... */
typedef struct TDAG_modulo
{
	unsigned ground_args; /*< bitmask for current args groundess */
	TDAG term; /*< ground term DAG is associated with; if DAG is
                           not a variable, term must be a signature */
	Tstack_DAG stack_pred; /*< stack of predecessors */
} * TDAG_modulo;

extern TDAG_modulo* DAGs_modulo;

/** \remark f(g(x,y), h(z)), with x,z assigned, has mask 10; position 1 (arg1)
    is ground, not position 0 (arg0) */
#define set_arg_ground(D, arg_pos) DAGs_modulo[D]->ground_args |= 1u << arg_pos

#define unset_arg_ground(D, arg_pos) \
	DAGs_modulo[D]->ground_args ^= 1u << arg_pos

#define check_arg_ground(D, arg_pos) \
	((DAGs_modulo[D]->ground_args >> arg_pos) & 1u)

#define has_sig(D) \
	(DAG_arity(D) ? DAGs_modulo[D]->term : ground(DAGs_modulo[D]->term))

/** \brief a non-var DAG_modulo is ground if all its args are ground. A var is
    ground if it's set */
#define ground_mod(D) \
	(ground(D) || \
	 (DAG_arity(D) ? DAGs_modulo[D]->ground_args == ((1 << DAG_arity(D)) - 1) \
								 : DAGs_modulo[D]->ground_args))

/*
  --------------------------------------------------------------
  Handling DAGs modulo current solution
  --------------------------------------------------------------
*/

/**
   \brief sets DAGs_modulo for non-ground DAG and all its non-ground
   sub-DAGs */
extern void set_DAGs_modulo(TDAG DAG);

/**
   \brief frees DAGs_modulo for DAG and all its sub-DAGs */
extern void unset_DAGs_modulo(TDAG DAG);

/*
  --------------------------------------------------------------
  Backtracking modulo
  --------------------------------------------------------------
*/

/**
   What I think it should happen: I push what I do to the vars, so that when a
   backtracking happens that thing should be undone.

   Therefore there is only one trigger for the backtrack. To unset this we will
   go through the predecessors list of the vars and its predecessors, "fixing"
   the ground_args thing. If it was ground before the instance must be
   removed. That should be a start. */

/* TODO: change this UNDO_EMATCH placeholder in undo.h */
enum
{
	CCFV_UNDO_VAR = UNDO_EMATCH
};

/** \brief undo_level when search started */
extern unsigned init_undo_lvl;

#define needs_backtrack(s) \
	(undo_level > unify_nb_ground_vars(s) + init_undo_lvl + 1)

#define BACKTRACK_TO(s) \
	undo_level_del_to_level(unify_nb_ground_vars(s) + init_undo_lvl + 1)

extern void set_var_modulo(TDAG var, TDAG term);

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

extern void CCFV_mod_init(void);
extern void CCFV_mod_done(void);

#endif
