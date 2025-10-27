/*
  ------------------------------------------------------------------
  Module for simplifying symbols based on AC (AND, OR only for now)
  ------------------------------------------------------------------
*/

#ifndef SIMP_AC_H
#define SIMP_AC_H

#include "proof/proof.h"
#include "symbolic/DAG.h"

extern TDAG simplify_AC(TDAG src);

extern TDAG simplify_AC_proof(TDAG src, Tproof* Pproof);

#endif
