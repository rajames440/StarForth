/**
   \file veriT-SAT.h
   \brief veriT SAT solver */
#ifndef VERIT_SAT_H
#define VERIT_SAT_H

#include <stdbool.h>

/*
  --------------------------------------------------------------
  main types and public datastructures
  --------------------------------------------------------------
*/

typedef unsigned SAT_Tvar; /**< var index into stack_var */
typedef unsigned
	SAT_Tlit; /**< lit is var<<1 or var<<1+1 according to polarity */
typedef unsigned SAT_Tclause; /**< clause index into stack_clause */
typedef unsigned SAT_Tlevel; /**< level */

enum
{
	SAT_VAR_UNDEF = 0 /**< the undefined variable */
};
enum
{
	SAT_LIT_UNDEF = 0 /**< the undefined literal */
};
enum
{
	SAT_CLAUSE_UNDEF = 0 /**< the undefined clause */
};

/**
   \brief used to return status state */
typedef enum
{
	SAT_STATUS_UNSAT = 0,
	SAT_STATUS_SAT = 1,
	SAT_STATUS_UNDEF = 2
} SAT_Tstatus;

typedef unsigned char SAT_Tvalue; /**< type for value of var or literal */

enum
{
	SAT_VAL_FALSE = 0,
	SAT_VAL_TRUE = 1,
	SAT_VAL_UNDEF = 2
};

/**
   \brief array of literals assigned by the SAT solver
   \remark it is the full model if status is SAT
   \remark it is not relevant if status is UNSAT */
extern SAT_Tlit* SAT_literal_stack;
/**
   \brief number of literals assigned by the SAT solver
   \remark pointer to the end of SAT_literal_stack
   \remark it should be the number of literals if status is SAT
   \remark it is not relevant if status is UNSAT */
extern unsigned SAT_literal_stack_n;
/**
   \brief number of literals kept unmodified in the stack by the SAT solver
   \remark pointer to the end of preserved part of SAT_literal_stack
   \remark user should set it to SAT_literal_stack_n to reset it */
extern unsigned SAT_literal_stack_hold;
/**
   \brief number of unit literals in the stack
   \remark pointer to the end of invariant part of SAT_literal_stack
   \remark these literals will be true in all subsequent partial models */
extern unsigned SAT_literal_stack_unit;
/**
   \brief pointer to first literal to propagate
   \remark do not modify.
   Just to keep track of hints, if call to SAT_propagate() is required
   \invariant is SAT_literal_stack_n after SAT_propagate() */
extern unsigned SAT_literal_stack_to_propagate;

/**
   \brief decision level of SAT solver
   \remark it is basically the number of decisions */
extern SAT_Tlevel SAT_level;
/**
   \brief array of levels
   \remark stores the size of literal stack (SAT_literal_stack_n) at each level
   \remark it is not relevant if status is UNSAT */
extern unsigned* SAT_level_stack;
/**
   \brief number of levels kept unmodified in the stack
   \remark user should set it to SAT_level to reset it */
extern unsigned SAT_level_stack_hold;

/**
   \brief status of SAT solver */
extern SAT_Tstatus SAT_status;

/*
  --------------------------------------------------------------
  SAT_Tvar
  --------------------------------------------------------------
*/

/**
   \name SAT_var
   Handling SAT_Tvar objects */
/**\{*/

/**
   \brief creates new propositional variable
   \return the identifier of the variable
   \remark ensures that internal data structures are resized if necessary to
   accomodate the variable
   \remark at most 2^31-1 variables */
SAT_Tvar SAT_var_new(void);
/**
   \brief ensures variable of a given id exists
   creates propositional variables up to id
   \param id the largest required variable identifier
   \remark at most 2^31 variables */
void SAT_var_new_id(const unsigned id);
/**
   \brief get the variable value
   \param var the variable
   \return the value (SAT_VAL_FALSE, SAT_VAL_TRUE, or SAT_VAL_UNDEF) */
SAT_Tvalue SAT_var_value(const SAT_Tvar var);
/**
   \brief get the variable level
   \param var the variable
   \return the level at which variable has been assigned */
SAT_Tlevel SAT_var_level(SAT_Tvar var);
/**
   \brief disallow decision on var
   \param var the variable
   \remark while using this function you should know what you are doing.
   The semantics of SAT/UNSAT depends on this */
void SAT_var_block_decide(SAT_Tvar var);
/**
   \brief allow decision on var
   \param var the variable
   \remark while using this function you should know what you are doing.
   The semantics of SAT/UNSAT depends on this */
void SAT_var_unblock_decide(SAT_Tvar var);

/**
   \brief get var activity
   \remark can be used to know if a var ever took part in a conflict
   \remark non-null if var took part in conflict */
double SAT_var_get_activity(SAT_Tvar var);

/**\}*/

/*
  --------------------------------------------------------------
  SAT_Tlit
  --------------------------------------------------------------
*/

/**
   \name SAT_lit
   Handling SAT_Tlit objects */
/**\{*/

/**
   \brief build a literal from variable and polarity
   \param var the variable
   \param pol the polarity */
/* #define SAT_lit(var, pol) ((SAT_Tlit)((var << 1) | pol)) */
static inline SAT_Tlit
SAT_lit(const SAT_Tvar var, const unsigned pol)
{
	return (var << 1) | pol;
}

/**
   \brief get variable of literal
   \param lit the literal */
static inline SAT_Tvar
SAT_lit_var(const SAT_Tlit lit)
{
	return lit >> 1;
}

/**
   \brief build negation of literal
   \param lit the literal */
static inline SAT_Tlit
SAT_lit_neg(const SAT_Tlit lit)
{
	return lit ^ 1;
}

/**
   \brief get polarity of literal
   \param lit the literal */
static inline SAT_Tvalue
SAT_lit_pol(const SAT_Tlit lit)
{
	return lit & 1;
}

/**
   \brief get the variable level
   \param var the variable
   \return the level at which variable has been assigned */
SAT_Tlevel SAT_lit_level(SAT_Tlit lit);

/**\}*/

/*
  --------------------------------------------------------------
  SAT_Tclause
  --------------------------------------------------------------
*/

/**
   \name SAT_clause
   Handling SAT_Tclause objects */
/**\{*/

enum
{
	SAT_CLAUSE_EXT = 0,
	SAT_CLAUSE_LEARNT = 1
};

/**
   \brief adds clause in SAT
   \param n the number of literals
   \param lit an array of n literals
   \param flags if not null, clause can be deleted
   \remark destructive for the array of literals
   \remark returns CLAUSE_UNDEF if valid clause or problem already found unsat
   \return clause id or CLAUSE_UNDEF
   \remark at most 2^30 clauses
   \remark at most 2^27 literals in clause */
SAT_Tclause SAT_clause_new(unsigned n, SAT_Tlit* lit, unsigned flags);
/**
   \brief adds a clause as late explanation for a literal
   \param n the number of literals
   \param lit an array of n literals
   \remark this should be called on the SAT solver request for explanation of
   a propagated literal with lazy clause
   \remark destructive for the array of literals
   \pre one of the literals is true
   \pre all other literals are false
   \pre level of true literal and at least one other is maximum level of all
   literals in clause
   \return clause id or CLAUSE_UNDEF */
SAT_Tclause SAT_clause_new_lazy(unsigned n, SAT_Tlit* lit);
/**
   \brief get number of literals in clause */
unsigned SAT_clause_get_n(SAT_Tclause clause);
/**
   \brief get literals in clause */
SAT_Tlit* SAT_clause_get_lit(SAT_Tclause clause);
/**
   \brief get clause activity
   \remark can be used to know if a clause ever took part in a conflict
   \remark non-null if clause took part in conflict */
double SAT_clause_get_activity(SAT_Tclause clause);
/**
   \brief get clause glue (number of different literal's levels) */
unsigned SAT_clause_get_glue(SAT_Tclause clause);

/**\}*/

/*
  --------------------------------------------------------------
  SAT functions
  --------------------------------------------------------------
*/

#ifdef INSIDE_VERIT
#define HINTS
#endif

/**
   \brief propagates until a decision has to be done
   \return SAT_STATUS_SAT, SAT_STATUS_UNSAT, or SAT_STATUS_UNDEF */
SAT_Tstatus SAT_propagate(void);
#ifdef HINTS
/**
   \brief adds hint, i.e. propagated literal with lazy clause
   \remark may be applied repeatedly
   \remark lit is either true (discarded) or undefined but never false */
void SAT_hint(SAT_Tlit lit);
#endif /* HINTS */
/**
   \brief performs a decision
   \pre SAT_propagate applied just before (no SAT_hint meantime)
   \return false iff there is nothing to decide (therefore SAT) */
bool SAT_decide(void);
/**
   \brief restart SAT solver */
void SAT_restart(void);
/**
   \brief runs until a model is found or unsat
   \return SAT_STATUS_SAT, SAT_STATUS_UNSAT */
SAT_Tstatus SAT_solve(void);

/**
   \brief module initialise */
void SAT_init(void);
/**
   \brief module release */
void SAT_done(void);
/**
   \brief module reset */
void SAT_reset(void);

/*
  --------------------------------------------------------------
  Markups
  --------------------------------------------------------------
*/

extern void (*SAT_markup_function)(void);

/**
   \brief set a markup on the literal stack so that
   any backtrack beyond that point triggers a call to
   SAT_markup_function
   \remark user may set several markups */
bool SAT_set_markup(void);

/*
  --------------------------------------------------------------
  Proofs
  --------------------------------------------------------------
*/

/**
   \brief add all explanations for literals at root level
   \remark it is necessary to call this function before adding
   conflict clauses.  However, this can not be done inside SAT_clause_new,
   since it also introduces new clauses, and it cannot be done in SAT_hint,
   since hints are sometimes sent (notably by CC) when reasons are not ready
   \todo TODO: there may be a better solution */
void SAT_sanitize_root_level(void);

#ifdef INSIDE_VERIT

extern bool SAT_proof; /**< if set true, proofs will be checked and recorded */
extern unsigned SAT_proof_stack_n; /**< number of clauses in resolution chain */
extern SAT_Tlit* SAT_proof_stack_lit; /**< resolvants in chain*/
extern SAT_Tclause* SAT_proof_stack_clause; /**< clauses in chain*/
/**
   \brief notify the module of the id (for the SAT solver) of
   inserted clause
   \param clause_id the clause id for the SAT solver
   \remark hook is necessary because adding clause may introduce several
   of clauses (if simplifies).  Proof module cannot wait for end of clause
   addition to be notified of clause id, since resolution may occur mentioning
   this clause id before end of clause addition */
extern void SAT_proof_set_id(SAT_Tclause clause_id);
/**
   \brief notify the module that a clause is learnt
   \param clause the clause id for the SAT solver
   \remark called by SAT solver
   \remark the full proof in SAT solver datastruct (see corresp. .h file) */
extern void SAT_proof_notify(SAT_Tclause clause);
#endif

#ifdef DEBUG
/**
   \brief print the stack */
extern void print_stack(void);
#endif

/*
  --------------------------------------------------------------
  Prime Implicant
  --------------------------------------------------------------
*/

#ifdef INSIDE_VERIT
/* \brief for each literal on stack, set if in prime implicant */
extern bool* prime_required;

/**
   \brief computes prime implicant of current model
   \remark result is kept in prime_required array */
extern void SAT_prime_implicant(void);
#endif

/*
  --------------------------------------------------------------
  TODO
  --------------------------------------------------------------

  *** phase cache initialisation
  *** variable order initialisation
  ** backtracking
  - timestamp to every clause
  - Make sure no clause is modified at earlier push-level
  - be careful of SAT_lit_seen in SAT_lit_add
  - eliminate root_level literals to previous point
  ** notifying deletion of clauses
  * minimal model
  * maybe use unsigned index rather than pointers for everything that is used often
  * maybe use separate datastructures for anything used rarely
  * removal of clauses, and reuse of variables
  * code for binary clauses needs improvement
  * preprocessing and periodic simplification needs improvement
  * check if REUSE_TRAIL is
  (1) returning something else than ROOT_LEVEL from time to time
  (2) effective
  * separate activity of clauses and variables, and see if better perfs
  * In 3b6649bb3c85dd110c6e59e44dd2aba42d837a0e, removed code for binary clause
  exploration.  Still, might be useful to do unclosure of binary clauses
  See how to do that efficiently, and if useful
  * evaluate time taken by analyse_required
  */

#endif /* VERIT_SAT_H */
