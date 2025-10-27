/*
  --------------------------------------------------------------
  Module for simplification of formulas using unit clauses
  --------------------------------------------------------------
*/

#ifndef __UNIT_SIMPLIFICATION_H
#define __UNIT_SIMPLIFICATION_H

#include "symbolic/DAG.h"

TDAG simplify_unit(TDAG src);
void simplify_unit_init(void);
void simplify_unit_done(void);

#endif /* __UNIT_SIMPLIFICATION_H */
