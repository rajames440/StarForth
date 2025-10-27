#ifndef __PROOF_RULES_TAUTOLOGIES_H
#define __PROOF_RULES_TAUTOLOGIES_H

#include "proof/proof-id.h"
#include "proof/proof-type.h"
#include "symbolic/DAG.h"

/*
  --------------------------------------------------------------
  Tautologies for theories
  --------------------------------------------------------------
*/

/**
   \brief add a valid clause, and returns its context id
   \param type is the type of valid clause
   \param nb_lits is the number of component formulas
   \remark the remaining arguments are the component formulas (DAGs)
   \return the id of the tautology in the proof */
extern Tproof proof_step_valid(Tproof_type type, unsigned nb_lits, ...);

/**
   \brief add a valid clause, and returns its context id
   \param type is the type of valid clause
   \param lits is the stack of component formulas (DAGs)
   \return the id of the tautology in the proof */
extern Tproof proof_step_valid_stack(Tproof_type type, Tstack_DAG lits);

/**
   \brief add a valid clause, and returns its context id
   \param type is the type of valid clause
   \param lits is the stack of component formulas (DAGs)
	 \param args a stack of additional annotations for the step
   \return the id of the tautology in the proof */
extern Tproof proof_step_valid_stack_args(
    Tproof_type type, Tstack_DAG lits, Tstack_DAG args);

/*
  --------------------------------------------------------------
  Tautologies as clauses
  --------------------------------------------------------------
*/

/* PF next functions add to the proof a valid clause C
   to define connectors */
/* TRUE */
extern Tproof proof_true(void);

/* NOT FALSE */
extern Tproof proof_false(void);

/* NOT [A_1 AND ... A_n] OR A_i */
extern Tproof proof_and_pos(TDAG DAG, unsigned i);

/* NOT NOT NOT A or A (DAG is NOT NOT A) */
extern Tproof proof_not_not(TDAG DAG);

/* [A_1 AND ... A_n] OR NOT A_1 OR ... NOT A_n */
extern Tproof proof_and_neg(TDAG DAG);

/* NOT [A_1 OR ... A_n] OR A_1 OR ... A_n */
extern Tproof proof_or_pos(TDAG DAG);

/* [A_1 OR ... A_n] OR NOT A_i */
extern Tproof proof_or_neg(TDAG DAG, unsigned i);

/* NOT [A_1 XOR A_2] OR A_1 OR A_2 */
extern Tproof proof_xor_pos1(TDAG DAG);

/* NOT [A_1 XOR A_2] OR NOT A_1 OR NOT A_2 */
extern Tproof proof_xor_pos2(TDAG DAG);

/* [A_1 XOR A_2] OR A_1 OR NOT A_2 */
extern Tproof proof_xor_neg1(TDAG DAG);

/* [A_1 XOR A_2] OR NOT A_1 OR A_2 */
extern Tproof proof_xor_neg2(TDAG DAG);

/* NOT[A_1 IMPLIES A_2] OR NOT A_1 OR A_2 */
extern Tproof proof_implies_pos(TDAG DAG);

/* [A_1 IMPLIES A_2] OR A_1 */
extern Tproof proof_implies_neg1(TDAG DAG);

/* [A_1 IMPLIES A_2] OR NOT A_2 */
extern Tproof proof_implies_neg2(TDAG DAG);

/* NOT [A_1 EQUIV A_2] OR A_1 OR NOT A_2 */
extern Tproof proof_equiv_pos1(TDAG DAG);

/* NOT [A_1 EQUIV A_2] OR NOT A_1 OR A_2 */
extern Tproof proof_equiv_pos2(TDAG DAG);

/* [A_1 EQUIV A_2] OR NOT A_1 OR NOT A_2 */
extern Tproof proof_equiv_neg1(TDAG DAG);

/* [A_1 EQUIV A_2] OR A_1 OR A_2 */
extern Tproof proof_equiv_neg2(TDAG DAG);

/* NOT [IF A THEN B ELSE C] OR A OR C */
extern Tproof proof_ite_pos1(TDAG DAG);

/* NOT [IF A THEN B ELSE C] OR NOT A OR B */
extern Tproof proof_ite_pos2(TDAG DAG);

/* [IF A THEN B ELSE C] OR A OR NOT C */
extern Tproof proof_ite_neg1(TDAG DAG);

/* [IF A THEN B ELSE C] OR NOT A OR NOT B */
extern Tproof proof_ite_neg2(TDAG DAG);

/*
  --------------------------------------------------------------
  Boolean derivations with premisses (mirror the tautologies)
  --------------------------------------------------------------
*/

/* PF next functions add to the proof a clause C' deduced from a clause C, and
   return proof id for C' */
/* A_1 OR ... A_i ... NOT A_i ... OR A_n --> TRUE */
extern Tproof proof_tautology(Tproof clause);

/* A_1 AND ... A_i ... AND A_n --> A_i */
extern Tproof proof_and(Tproof clause, unsigned i);

/* NOT(A_1 OR ... A_i ... OR A_n) --> NOT A_i */
extern Tproof proof_not_or(Tproof clause, unsigned i);

/* A_1 OR ... A_n --> {A_1, ... A_n} */
extern Tproof proof_or(Tproof clause);

/* NOT(A_1 AND ... A_n) --> {NOT A_1, ... NOT A_n} */
extern Tproof proof_not_and(Tproof clause);

/* A XOR B --> {A, B} */
extern Tproof proof_xor1(Tproof clause);

/* A XOR B --> {NOT A, NOT B} */
extern Tproof proof_xor2(Tproof clause);

/* NOT(A XOR B) --> {A, NOT B} */
extern Tproof proof_not_xor1(Tproof clause);

/* NOT(A XOR B) --> {NOT A, B} */
extern Tproof proof_not_xor2(Tproof clause);

/* A IMPLIES B --> {NOT A, B} */
extern Tproof proof_implies(Tproof clause);

/* NOT(A IMPLIES B) --> A */
extern Tproof proof_not_implies1(Tproof clause);

/* NOT(A IMPLIES B) --> NOT B */
extern Tproof proof_not_implies2(Tproof clause);

/* A EQUIV B --> {NOT A, B} */
extern Tproof proof_equiv1(Tproof clause);

/* A EQUIV B --> {A, NOT B} */
extern Tproof proof_equiv2(Tproof clause);

/* NOT(A EQUIV B) --> A OR B */
extern Tproof proof_not_equiv1(Tproof clause);

/* NOT(A EQUIV B) --> NOT A OR NOT B */
extern Tproof proof_not_equiv2(Tproof clause);

/* IF A THEN B ELSE C --> A OR C */
extern Tproof proof_ite1(Tproof clause);

/* IF A THEN B ELSE C --> NOT A OR B */
extern Tproof proof_ite2(Tproof clause);

/* NOT(IF A THEN B ELSE C) --> A OR NOT C */
extern Tproof proof_not_ite1(Tproof clause);

/* NOT(IF A THEN B ELSE C) --> NOT A OR NOT B */
extern Tproof proof_not_ite2(Tproof clause);

#endif