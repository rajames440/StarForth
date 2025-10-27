#ifndef __QSIMP_BY_UNIFICATION_H
#define __QSIMP_BY_UNIFICATION_H

#include "symbolic/DAG.h"

/**
    \brief Simplifies formulas by removing nested quantifiers using unification
    \param src The toplevel formula
    \param delete_simplified Removes simplified formulas (incomplete!)
    \param simplify_eagerly Apply simplification not only on quantified subterms
    \param use_solitary_heuristic Apply simplification when it might remove a variable
    \return The simplified formula */
TDAG qsimp_by_unification(
	TDAG src, bool delete_simplified, bool simplify_eagerly,
	bool use_solitary_heuristic);

#endif /* __QSIMP_BY_UNIFICATION_H */
