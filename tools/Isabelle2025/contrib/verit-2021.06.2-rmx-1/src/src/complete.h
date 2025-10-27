/*
  --------------------------------------------------------------
  States (programming defined) if the software is complete
  --------------------------------------------------------------
*/

#ifndef __COMPLETE_H
#define __COMPLETE_H

#include "symbolic/DAG.h"

/**
   \brief sets the name of the logic (optional, SMT-LIB taxonomy) */
void complete_logic(char* logic);

/**
   \brief check if the solver remains complete for the formula 
   \remark non-destructive */
void complete_add(TDAG DAG);
/**
   \brief returns if software should be complete on formulas added so far */
bool complete_check(void);

#endif /* __COMPLETE_H */
