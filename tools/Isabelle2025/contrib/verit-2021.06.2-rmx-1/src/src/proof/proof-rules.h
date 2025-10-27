#ifndef __PROOF_RULES_H
#define __PROOF_RULES_H

#include "proof/proof-step-table.h"

/*
  --------------------------------------------------------------
  [Context dependent] Transformations
  --------------------------------------------------------------
*/

/**
   \brief adds a proof step with premisses and a single conclusion
   \param type rule type
   \param DAG the equivalence or equality
   \param reasons the set of premisses
   \return proof id of built step
   \remark set of premisses may be empty */
extern Tproof proof_step_conclusion(
	Tproof_type type, TDAG conclusion, Tstack_proof reasons);

/**
   \brief creates proof of transformation from src into dest
   \param type a proof rule
   \param src a DAG
   \param dest a DAG and its proof
   \param reasons a list of proof steps leading to the proof */
extern Tproof proof_transformation(
	Tproof_type type, TDAG src, TDAG dest, Tstack_proof reasons);

/*
  --------------------------------------------------------------
  Signature extension transformations
  --------------------------------------------------------------
*/

extern void push_choices(TDAG src, Tstack_DAG DAGs);
extern void pop_choices(TDAG src);

extern void notify_ites(Tstack_DAG_assoc ite_terms, unsigned n);
extern Tproof proof_ite_intro(TDAG src, TDAG dest, Tproof id);

/*
  --------------------------------------------------------------
  Resolution
  --------------------------------------------------------------
*/

/**
   \brief computes the clause from resolution of both first arguments
   \param proof_step1 first proof step
   \param proof_step2 second proof step
   \param DAG resolvent */
extern Tproof_step proof_step_resolve(
	Tproof_step proof_step1, Tproof_step proof_step2, TDAG DAG);

/* PF add a clause resolved from others, and returns its context id */
extern Tproof proof_resolve(unsigned nb_clauses, ...);
extern Tproof proof_resolve_array(unsigned nb_clauses, Tproof* clauses);

/**
   \brief computes the resolution of two clauses, and returns its context id
   \param i_clause1 first clause id
   \param i_clause2 second clause id
   \return id of resolved clause
   \remark restricted version of resolution:
   a OR a OR b   resolved with c OR \neg a
   gives a OR b OR c */
extern Tproof proof_bin_resolve(Tproof i_clause1, Tproof i_clause2);

/*
  --------------------------------------------------------------
  SAT, UNSAT, Input and Lemmas
  --------------------------------------------------------------
*/

/**
   \brief notify the module of the satisfiability of the formula */
extern void proof_satisfiable(void);

/**
   \brief notify the module of the unsatisfiability of the formula
   \remark fails if no empty clause has been derived, an error (a
   warning at the present time) is issued */
extern void proof_unsatisfiable(void);

/**
   \brief adds a formula as input in the proof
   \param DAG is the formula to add
   \remark the formula either comes from the user input or
   is an hypothesis (for subproofs)
   \return the id of the formula in the proof
   \remark The formula is not checked, and not deduced */
extern Tproof proof_add_input(TDAG DAG);

/**
   \brief adds a formula (a disequality lemma) in the context
   \param DAG is the formula to add
   \return the id of the formula in the proof
   \attention The formula is not checked, and not deduced */
/* TODO: CHECK!!!!! */
extern Tproof proof_add_disequality_lemma(TDAG DAG);

/**
   \brief adds a formula (a totality lemma) in the context
   \param DAG is the formula to add
   \return the id of the formula in the proof
   \attention The formula is not checked, and not deduced */
/* TODO: CHECK!!!!! */
extern Tproof proof_add_totality_lemma(TDAG DAG);

/**
   \brief adds a formula (an arithmetical tautology) in the context
   \param DAG is the formula to add
   \return the id of the formula in the proof
   \attention The formula is not checked, and not deduced */
extern Tproof proof_add_la_tautology(TDAG DAG);

/**
   \brief adds a formula (a universal instantiation lemma) in the context
   \param DAG is the formula to add
   \param n is the number of elements in PDAG
   \param PDAG is an array of instantiated variable, instance terms
   \return the id of the formula in the proof
   \note an instance lemma is \f$ \forall X . A(X) \rightarrow A(X \ t) \f$
*/
/* TODO: CHECK!!! */
extern Tproof proof_add_forall_inst_lemma(TDAG DAG, unsigned n, TDAG* PDAG);

/**
   \brief get the id associated with the lemma
   \param DAG is the lemma
   \return the id of the lemma in the proof */
extern Tproof proof_get_lemma_id(TDAG DAG);
/**
   \brief link a DAG and a proof_id for later lemma addition
   \param DAG the formula
   \param id the proof id of the unit clause with DAG */
extern void proof_set_lemma_id(TDAG DAG, Tproof id);

/**
   \brief deduce formula through elimination of functions with Boolean argument
   \param id is the original formula
   \param DAG the result formula
   \return the id of the proof
   \remark this is a temporary, high level, proof rule */
extern Tproof proof_bfun_elim(Tproof id, TDAG DAG);

extern Tproof proof_deep_skolemize(Tproof id, TDAG DAG);

#endif
