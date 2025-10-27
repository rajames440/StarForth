/*
  --------------------------------------------------------------
  Strategies
  --------------------------------------------------------------
*/

#ifndef __INST_STRATEGY_H
#define __INST_STRATEGY_H

#include "limits.h"

typedef enum Tstrategy
{
	inst_CIs,
	inst_TRIGGERS,
	inst_ENUM
} Tstrategy;

#define UNDEF_LVL UINT_MAX

/** Activates basic debugging in instantiation heuristics */
/* #define DEBUG_HEURISTICS */

#define STATS_INST

#endif
