/*
  --------------------------------------------------------------
  Obsolete functions about DAG and symbols
  --------------------------------------------------------------
*/

#ifndef DAG_FLAG_H
#define DAG_FLAG_H

#include "symbolic/DAG.h"

extern int* obsolete_DAG_flag;
extern void** obsolete_DAG_Pflag;

#define DAG_flag(A) obsolete_DAG_flag[A]
#define DAG_flag_set(A, B) \
	{ \
		obsolete_DAG_flag[A] = B; \
	}

#define DAG_Pflag(A) obsolete_DAG_Pflag[A]
#define DAG_Pflag_set(A, B) \
	{ \
		obsolete_DAG_Pflag[A] = B; \
	}

extern void DAG_reset_flag(TDAG src);
extern void DAG_free_Pflag(TDAG src);
extern void DAG_reset_Pflag(TDAG src);
extern void DAG_check_Pflag(void);

extern void DAG_flag_init(void);
extern void DAG_flag_done(void);

#endif
