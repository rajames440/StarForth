#ifndef __INST_CREATE_H
#define __INST_CREATE_H

/*
  --------------------------------------------------------------
  Handles instances creation
  --------------------------------------------------------------
*/

#include "instantiation/inst-strategy.h"
#include "instantiation/unify.h"

/*
  --------------------------------------------------------------
  Data structures
  --------------------------------------------------------------
*/

typedef struct Tinst Tinst;

/** \brief A tree of instantiations. The first node is the DAG being
    instantiated */
struct Tinst
{
	TDAG DAG;
	unsigned size;
	Tinst* next;
};

TSstack(_Tinst, Tinst); /* typedefs Tstack_Tinst */

/**
   \brief Associates a quantified formula with its intended instantiations. If a
   clause is set, then the clause should be instantiated, not the body. */
typedef struct TDAGinst
{
	TDAG qform;
	Tstack_DAG clause;
	Tstack_unifier insts;
} TDAGinst;

TSstack(_DAGinst, TDAGinst); /* typedefs Tstack_DAGinst */

/*
  --------------------------------------------------------------
  Global variables
  --------------------------------------------------------------
*/

/* Created lemmas */
extern Tstack_DAG lemmas;

/*
  --------------------------------------------------------------
  IO interface
  --------------------------------------------------------------
*/

extern void inst_create_init(void);
extern void inst_create_done(void);

/**

   \brief Attemps to creates a new instance for each given instantiation
   \param DAGinst object for the quantified formula whose body or clause is
   gonna be instantiated, with the given substitutions
   \remark destructive for the substitutions
   \remark created instances are added to the global 'lemmas' accumulator
   \remark if a clause is given, the generated instance is of the clause,
   otherwise of qform's body

   TODO: For proof production, the proof id of the clause being a consequence
   of the formula's body should be available.
   \remark An instance is only created if the substitution has not been
   considered before for this same formula
   \remark A bitset with the generated instances is used for a second level of
   redundancy check for the instantiations */
extern void inst_unifiers(TDAGinst DAGinst);

/**
   \brief Attemps to create a fresh instance for the quantified formula with the
   given substitution
   \param qform a quantified formula
   \param clause a clause part of qform's CNF
   \param unifier a substitution
   \return true if an instance is created, false otherwise
   \remark not destructive for the substitution
   \remark created instances are added to the global 'lemmas' accumulator
   \remark if a clause is given, the generated instance is of the clause,
   otherwise of qform's body

   TODO: For proof production, the proof id of the clause being a consequence
   of the formula's body should be available.
   \remark An instance is only created if the substitution has not been
   considered before for this same formula
   \remark A bitset with the generated instances is used for a second level of
   redundancy check for the instantiations */
extern bool inst_unifier(TDAG qform, Tstack_DAG clause, Tunifier unifier);

/*
  --------------------------------------------------------------
  Debugging
  --------------------------------------------------------------
*/

extern void print_Tinst(Tinst inst, unsigned depth);
extern void print_Tstack_Tinst(Tstack_Tinst stack);

#endif
