#ifndef DAG_SYMB_DAG_H
#define DAG_SYMB_DAG_H

#include "symbolic/DAG.h"

extern TDAG* DAG_symb_DAG;

/**
   \brief declares symb as being a macro, and bind DAG to it
   \param symb the symbol
   \param DAG the DAG */
void DAG_symb_macro(Tsymb symb, TDAG DAG);

void DAG_symb_DAG_init(void);
void DAG_symb_DAG_done(void);

#endif /* DAG_SYMB_DAG_H */
