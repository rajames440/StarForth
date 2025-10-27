/*
  --------------------------------------------------------------
  Module for moving trigger attributes at the root of 
  quantified formulas
  --------------------------------------------------------------
*/

#ifndef FIX_TRIGGER_H
#define FIX_TRIGGER_H

#include "symbolic/DAG.h"

/**
   \brief For each quantification occurring in Psrc[i], puts
   quantification patterns in the property list of the DAG for the
   quantification, some benchmarks have it in the DAG of the
   quantified term.  Result is stored in the input 
   \param n the number of terms
   \param Psrc the array of terms to fix
   \remarks Destructive for DAGs and destructive for prop-list of
   quantified DAGs.
   \remarks DAG-linear 
   \remarks Fix discrepancy between AUFLIA benchmarks in SMT-LIB 1.2
   and SMT-LIB 2.0
   \pre none
   \post all triggers are on the quantifier application, not on the body */
void fix_trigger_array(unsigned n, TDAG* Psrc);

#endif
