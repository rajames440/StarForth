#ifndef EQ_STORE_H
#define EQ_STORE_H

#include "arith/simplex.h"
#include "symbolic/DAG.h"

/**
   \brief adds an association between two DAGs and a variable
   \pre DAG0 < DAG1
   \remark we assume that var = DAG0 - DAG1 */
void eq_store(TDAG DAG0, TDAG DAG1, Tsimplex_var var);

/**
   \brief retrieves var associated with two DAGs
   \return the variable associated with the two DAGs, 0 if none
   \pre DAG0 < DAG1
   \remark we assume that var = DAG0 - DAG1 */
Tsimplex_var eq_get_from_DAG(TDAG DAG0, TDAG DAG1);

/**
   \brief retrieves DAGs associated with vars
   \remark fails if none
   \post DAG0 < DAG1
   \remark we assume that var = DAG0 - DAG1 */
void eq_get_from_var(Tsimplex_var var, TDAG* PDAG0, TDAG* PDAG1);

/**
   \brief removes an association */
void eq_remove(Tsimplex_var var);

/**
   \brief records that a lemma has been generated for DAG0!=DAG1
   \pre DAG0 < DAG1 */
void eq_set_lemma_generated(TDAG DAG0, TDAG DAG1);

/**
   \brief query if a lemma has been generated for DAG0!=DAG1
   \pre DAG0 < DAG1 */
bool eq_get_lemma_generated(TDAG DAG0, TDAG DAG1);

/**
   \brief tests if a variable corresponds to an equality */
bool eq_test(Tsimplex_var var);

void eq_init(void);
void eq_done(void);

/**
   \brief resets all stored variables
   \remark keeps information about lemmas
   \remark use when simplex is restart */
void eq_reset_var(void);

#endif /* EQ_STORE_H */
