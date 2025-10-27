#ifndef VERIT_STATE_H
#define VERIT_STATE_H

#include "bool/bool.h"
#include "symbolic/DAG.h"
#include "utils/stack.h"

/**
   \brief bool value stating if quantifiers are active
   \remark true for all categories with quantifiers */
extern bool Q_active;

/**
   \brief bool value stating if LA decision procedure is active
   \remark false mainly for QF_UF */
extern bool LA_active;

/**
   \brief stack of literals to compute conflict */
extern Tstack_lit veriT_conflict;

/**
   \brief stack of DAGs that are pairwise equal, to compute conflict */
extern Tstack_DAG veriT_conflict_eqs;

/**
   \brief stack of arbitrary lemmas to add to SAT solver */
extern Tstack_DAG veriT_lemmas;

#if 0
#define XMASK (1u << 31u)

/**
   \brief checks if literal is an information exchange */
#define is_xlit(lit) (lit & XMASK)
#endif

/*
  --------------------------------------------------------------
  Init/done
  --------------------------------------------------------------
*/

/**
   \brief initializes the module
   \remarks must be called before any other function of the module */
extern void veriT_state_init(void);

/**
   \brief releases the module */
extern void veriT_state_done(void);

/*
  --------------------------------------------------------------
  Exchange Queue
  --------------------------------------------------------------
*/

typedef unsigned Txtype;

enum
{
	XTYPE_CC_EQ = 0, /*< Equality from CC to LA */
	XTYPE_CC_INEQ = 1, /*< Disequality from CC to LA */
	XTYPE_LA_MODEL_EQ = 2, /*< Model equality from LA to CC */
};

/**
   \brief stack of things to exchange between CC and arith */
extern Tstack_unsigned xqueue;

/**
   \brief enqueues an information for DP interchange
   \param type the type of information */
static inline void
veriT_xenqueue_type(const Txtype type)
{
	stack_push(xqueue, type);
}

/**
   \brief enqueues an information for DP interchange
   \param DAG a DAG */
static inline void
veriT_xenqueue_DAG(const TDAG DAG)
{
	stack_push(xqueue, (unsigned)DAG);
}

static inline Txtype
veriT_xqueue_get_type(const unsigned i)
{
	return (Txtype)stack_get(xqueue, i);
}

static inline TDAG
veriT_xqueue_get_DAG(const unsigned i)
{
	return (TDAG)stack_get(xqueue, i);
}

static inline void
veriT_xqueue_clear(void)
{
	stack_reset(xqueue);
}

#endif /* VERIT_STATE_H */
