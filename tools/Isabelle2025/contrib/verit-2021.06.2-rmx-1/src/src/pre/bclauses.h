/*
  --------------------------------------------------------------
  add some binary clauses for speed-up
  --------------------------------------------------------------
*/

#ifndef BCLAUSES_H
#define BCLAUSES_H

#include "symbolic/DAG.h"

TDAG bclauses_add(TDAG DAG);

void bclauses_init(void);

void bclauses_done(void);

#endif