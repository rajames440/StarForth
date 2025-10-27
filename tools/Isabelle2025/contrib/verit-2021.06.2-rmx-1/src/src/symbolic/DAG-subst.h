#ifndef DAG_SUBST_H
#define DAG_SUBST_H

#include "symbolic/DAG.h"

/**
   \brief substitutes every node by DAG_tmp_DAG[node] in a term
   \param src the term
   \return true iff the term is modified (false otherwise)
   \attention the user should restore the DAG_tmp (see DAG_tmp.h),
   for instance using DAG_tmp_reset_DAG */
extern bool DAG_tmp_subst(TDAG src);

/**
   \brief conditionally substitutes every node by DAG_tmp_DAG[node] in a term
   \param src the term
   \return true iff the term is modified (false otherwise)
   \remark like the above, but only get inside term if cont(term)
   \attention the user should restore the DAG_tmp (see DAG_tmp.h),
   for instance using DAG_tmp_reset_DAG */
extern bool DAG_tmp_subst_cond(TDAG src, bool (*cont)(TDAG DAG));

/**
   \brief Simple substitution
   \param src Term where the substitution is realized
   \param origin Term that will be substituted
   \param subst Term that will replace origin
   \sa DAG_subst_multiple */
extern TDAG DAG_subst(TDAG src, TDAG origin, TDAG subst);

/**
   \brief Multiple substitution
   \param src Term where the substitution is realized
   \param n The number of terms that will be substituted
   \param origin Array of n terms that will be substituted
   \param subst Array of n term that will replace substituted terms */
extern TDAG DAG_subst_multiple(TDAG src, unsigned n, TDAG* origin, TDAG* subst);

/**
   \brief Tests DAG inclusion
   \param src
   \param find
   \return true if find is a subterm of src, false otherwise */
extern bool DAG_contain(TDAG src, TDAG find);

#endif
