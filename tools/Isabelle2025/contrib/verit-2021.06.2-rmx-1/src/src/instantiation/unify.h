/**
   \file unify.h
   \brief Module for handling computed unifications */
#ifndef __UNIFY_H
#define __UNIFY_H

#include "symbolic/DAG.h"

/*
  --------------------------------------------------------------
  Main data structure
  --------------------------------------------------------------
*/

/* TODO: Put this in veriT-config.h and check having it throughout veriT */
#define __PACKED __attribute((packed))

/* TODO: the nice memory footprint of Tval is now, after free_vars/free_in,
   wasted */

/**
   \brief variable's valuation in a unifier. Each valuation may be the
   representative of a congruence class for the variables, otherwise pointing
   to another variable's valuation as its representative
   \remark only used as part of a Tunifier, an array of valuations */
typedef struct Tval
{
	TDAG var : 31;
	bool rep : 1; /*< variable is the representative of a congruence
                             class; default is 1 */
	/* TODO: this guys are only in representatives? */
	unsigned free_vars; /*< bitmask for all free vars in var class */
	Tstack_unsigned free_in; /*< indexes of other classes in which var is free */
	union
	{
		struct
		{
			TDAG term; /*< ground term that class is equal to; default is
                             DAG_NULL */
			Tstack_DAG diff; /*< all DAGs that class is disequal to */
		} __PACKED;
		unsigned equal_var; /*< index of var that this var is equal to if it is
                             not a class representative */
	};
} Tval;

/**
   \brief unifier for a set of variables, including restrictions for its
   consistency
   \remark handled as a UNION-FIND data structured with path-compression, no
   ranking. A valid unifier works as a consistent congruence closure
   \remark the set of variables is static, sorted and fixed
   \remark if n variables are equal and their class has a ground term, only one
   has their "term" field set, the representative. The same holds for the
   "diff" stack. The UNION operation accounts for this
   \remark ground_vars is a bitmask for the "groundness" of the unifier:
   whenever a variable is assigned to a ground term its position in the mask is
   set, s.t. a unifier is ground iff (ground_vars = 2^{size} - 1). This should
   only be used if no more than 32 variables in the unifier (for overcoming
   this one should use a bitset) */
typedef struct Tunifier
{
	unsigned size;
	unsigned ground_vars;
	Tval val[];
} * Tunifier;

TSstack(_unifier, Tunifier); /* typedefs Tstack_unifier */

/*
  --------------------------------------------------------------
  Context variables
  --------------------------------------------------------------
*/

/**
   When using this module an invariant must be kept that the set of variables
   *does not change*. All operations assume that the same set of variables is
   used from the creation to the destruction of the unifier. This is to have
   fast retrieval of vars positions, due to the bitmasks.

   The only operation which has a different set of variables is unify_reset,
   which rebases a set of unifiers with different variables into one with the
   current fixed set of variables. A dedicated operation to retrieve vars
   positions is used then. */

/** \brief fixed set of variables */
extern Tstack_DAG current_vars;
/** \brief for quickly retrieving position of var in unifier from its DAG */
extern unsigned var_offset, *vars_pos;

#define var_pos(v) vars_pos[v - var_offset]

/**
   \brief sets context for unifiers
   \param DAGs a set of literals */
extern void set_context_vars(Tstack_DAG DAGs);

/**
   \brief checks whether var occurs in variables of UDAG
   \param unifier a unifier
   \param var_DAG a var
   \param UDAG a non-ground term
   \return true iff var occurs in UDAG, false otherwise */
extern bool unify_occurs(Tunifier unifier, TDAG var_DAG, TDAG UDAG);

/**
   \brief checks whether var is not equal to any non-variable term
   \param unifier a unifier
   \param var a var
   \return true iff var is in FVs of its class */
extern bool unify_is_var(Tunifier unifier, TDAG var);

/*
  --------------------------------------------------------------
  Ground checking
  --------------------------------------------------------------
*/

/**
   \brief list of variables which have been grounded due te last unify_union
   \remark sorted from first to last */
extern Tstack_DAG grounded_var_classes;

#define set_ground_var(u, pos) u->ground_vars |= (1u << pos)
#define unset_ground_var(u, pos) u->ground_vars ^= (1u << pos)
#define check_var(u, pos) ((u->ground_vars >> pos) & 1u)
#define check_class(u, pos) ((u->ground_vars >> unify_find(u, pos)) & 1u)

/* TODO: this is wrong... it would need to be a loop checking that all classes
   are ground. Remove this */
#define unify_grounded(u) (u->ground_vars == ((1u << u->size) - 1u))

/**
   \brief whether variable is equal to a ground term in unifier
   \param unifier the unifier on which the check is made
   \param var a variable
   \return true if var equal to a ground term in unifier, false otherwise
   \remark computed with the ground_vars bitmask */
extern bool unify_ground_var(Tunifier unifier, TDAG var);

/**
   \brief number of ground classes in unifier
   \param unifier the unifier on which the counting is made
   \return number of classes with ground bit set in unifier
   \remark this number should change iff a variable is assigned to a ground
   term
   \remark computed with the ground_vars bitmask */
extern unsigned unify_nb_ground_vars(Tunifier unifier);

/*
  --------------------------------------------------------------
  Handling unifiers
  --------------------------------------------------------------
*/

/**
   \brief creates a new unifier with the given variables
   \param vars the variables in the unifier
   \return the new unifier */
extern Tunifier unify_new(Tstack_DAG vars);

/**
   \brief frees unifier
   \param unifier the unifier being freed
   \remark each variable's valuation must have its diff stack freed if it was
   initialized */
extern void unify_free(Tunifier unifier);

/**
   \brief produces a copy of the given unifier
   \param unifier the unifier being copied
   \return the copied unifier */
extern Tunifier unify_copy(Tunifier unifier);

/**
   \brief get the index of the representative variable
   \param unifier a unifier
   \param orig the index of variable to get the representative
   \return the index of the representative variable
   \remark the second loop is intended to perform "path compression"
   s.t. afterwards every variable in the chain points directly to the
   representative */
extern unsigned unify_find(Tunifier unifier, unsigned orig);

/**
   \brief applies the unifier on a term
   \param unifier the unifier to apply
   \param DAG a term
   \return the result of applying the unifier to the term
   \remark if DAG is not a variable returns the DAG itself
   \remark if DAG is a variable and its representative is itself then the
   variable is free in the unifier */
extern TDAG unify_find_DAG(Tunifier unifier, TDAG DAG);

/**
   \brief sets the valuation of a variable to a term
   \param unifier the unifier on which the assignement is made
   \param i the index of the variable being set
   \param DAG the term
   \remark This operation does not lead to a UNION, so the result may be an
   inconsistent unifier w.r.t. CC */
#define unify_set(unifier, i, DAG) unifier->val[i].term = DAG

/**
   \brief sets a variable equal to a term
   \param unifier the unifier on which the assertion is made
   \param D0 a variable
   \param D1 either a ground term or a variable variable's ground term
   representative is disequal to all elements in its "diff"
   \return true if the resulting unifier is consistent, false otherwise */
extern bool unify_union(Tunifier unifier, TDAG D0, TDAG D1);

/**
   \brief sets a variable disequal to a term
   \param unifier the unifier on which the assertion is made
   \param D0 a variable
   \param D1 either a ground term or a variable
   \return true if the resulting unifier is consistent, false otherwise */
extern bool unify_union_diff(Tunifier unifier, TDAG D0, TDAG D1);

/**
   \brief assigns a set a variables to a ground term
   \param unifier the unifier on which the assertion is made
   \param rep_ind index of representative class of all given vars
   \param vars_inds indexes of a number of variables
   \param term a ground term
   \param force_disequal whether ...
   \return true if the resulting unifier is consistent, false otherwise */
extern bool unify_union_ground(
	Tunifier unifier, TDAG rep_ind, Tstack_unsigned vars_inds, TDAG term,
	bool force_disequal);

/**
   \brief merges two unifiers, with the result, if consistent, being kept in u1
   \param u1 the unifier being merged on, keeping the resulting merge
   \param u2 the unifier being merged into
   \return true if the merge is consistent, false otherwise */
extern bool unify_merge(Tunifier u1, Tunifier u2);

/**
   \brief restructures a set of unifiers to a new base of variables
   \param unifiers a set of unifiers
   \param new_vars a set of variables
   \return set of unifiers based on new variables
   \remark there is a non-empty intersection between the old and new variables
   \remark all the associations of variables in old unifiers that intersect the
   new ones are preserved in the result */
extern Tstack_unifier unify_reset(Tstack_unifier unifiers, Tstack_DAG new_vars);

/**
   \brief grounds the unifier, choosing ground terms arbitrarily if necessary
   \param unifier unifier being grounded
   \param cap max number of terms per variable's sort class to consider
   \param all_CIs whether only ground conflicting instantiations should be
   generated from unifier
   \return true if there is a consistent grounding for unifier, false otherwise
   \remark all_CIs on effectively forces the unifier to respect the invariant
   that a variable's representative is disequal to all terms in its "diff"
   \remark if a free valiable has no ground terms in its sort class it is
   assumed the unifier has no consistent grounding */
extern Tstack_unifier unify_ground(
	Tunifier unifier, unsigned cap, bool all_CIs);

/**
   \brief check if unifier is redundant w.r.t. a set of unifiers
   \param unifiers set of unifiers
   \param unifier unifier to check for redundancy
   \return true if unifier is redundant, false otherwise
   \remark two unifiers are redundant if, for each variable, both unifiers
   having the same representative's term and diff (it does not need to be the
   same variable, just the same valuation), e.g. {<x, y>, <y, b>} is redundant
   with {<x, b>, <y, x>} */
extern bool unifier_redundant(Tstack_unifier unifiers, Tunifier unifier);

/* TODO: Find a way to use this internally only; only here because of
   "ground_unifier" */
extern bool diff_breaks(
	Tunifier unifier, unsigned var_pos, TDAG DAG, bool force_disequal);

/*
  --------------------------------------------------------------
  Printing functions for debugging
  --------------------------------------------------------------
*/

const char* byte_to_binary(unsigned x, unsigned size);
extern void unify_print(Tunifier unifier);
extern void print_Tstack_unifier(Tstack_unifier stack);

#endif
