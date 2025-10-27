#ifndef __CONTEXT_RECURSION_H
#define __CONTEXT_RECURSION_H

#include "symbolic/DAG.h"
#include "symbolic/context-handling.h"
#include "symbolic/polarities.h"

/*
  --------------------------------------------------------------
  Interface
  --------------------------------------------------------------
*/

extern void ctx_rec_init(void);
extern void ctx_rec_done(void);

/**
   \brief does replacement of variables in DAG
   \param src a DAG
   \return DAG after recursive application of replacements
   \remark results are dupped
   \remark uses substitution embedded in DAG_symb_DAG
   \remark tree-linear */
extern TDAG DAG_tree_subst(TDAG src);

/**
   \brief builds a new DAG by applying a function
   \param src the input DAG
   \return src in which f_pre or f_pos have been applied top down or bottom up
   \remarks only called from context_structural_recursion or reduction functions
   called from it
   \remarks Non destructive */
extern TDAG context_tree_rec(TDAG src);

/**
   \brief builds a new DAG by applying a function
   \param src the input DAG
   \param f_pre a NON-destructive function, applied before than in sub DAGs
   \param f_pos a NON-destructive function, applied before than in sub DAGs
   \param cont a continuation condition
   \param context_type which type of context is kept during recursion
   \return src in which or f_pre has been applied from root to leaves or f_pos
   has been applied recursively from leaves to root
   \remarks linear on DAG size for binder free subforms, tree-linear otherwise
   \remarks Non destructive */
extern TDAG context_structural_recursion(
	TDAG src, void (*f_init)(void), void (*f_push)(TDAG, unsigned*),
	void (*f_pop)(TDAG, unsigned), TDAG (*f_reduce)(TDAG), bool (*cont)(TDAG));

/**
   \remark Destructive */
extern void context_structural_recursion_array(
	unsigned n, TDAG* Psrc, void (*f_init)(void), void (*f_push)(TDAG, unsigned*),
	void (*f_pop)(TDAG, unsigned), TDAG (*f_reduce)(TDAG), bool (*cont)(TDAG));

extern TDAG let_context_structural_recursion(
	TDAG src, void (*f_init)(void), void (*f_push)(TDAG, unsigned*),
	void (*f_pop)(TDAG, unsigned), TDAG (*f_reduce)(TDAG));

extern TDAG nosub_context_structural_recursion(
	TDAG src, void (*f_init)(void), void (*f_push)(TDAG, unsigned*),
	void (*f_pop)(TDAG, unsigned), TDAG (*f_reduce)(TDAG), bool (*cont)(TDAG));

/* Does not do tree recursion under binders */
extern void nosub_context_structural_recursion_array(
	unsigned n, TDAG* Psrc, void (*f_init)(void), void (*f_push)(TDAG, unsigned*),
	void (*f_pop)(TDAG, unsigned), TDAG (*f_reduce)(TDAG), bool (*cont)(TDAG));

typedef union TDAG_ctx
{
	TDAG DAG;
	TDAG pol_DAG[4];
} * TDAG_ctx;

#define DAG_ctx ((TDAG_ctx*)DAG_tmp)

#endif
