/*
  --------------------------------------------------------------
  Module for doing structural recursion on DAGs
  --------------------------------------------------------------
*/

#include "symbolic/recursion.h"

#include "proof/proof.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "utils/general.h"

/* #define DEBUG_RECURSION */

/*
  --------------------------------------------------------------
  Structural recursion - function
  --------------------------------------------------------------
*/

static TDAG (*structural_rec_func)(TDAG) = NULL;

static void
structural_rec_aux(TDAG src)
{
	unsigned i;
	TDAG *PDAG, dest, tmp;
	if (DAG_tmp_DAG[src]) return;
	switch (DAG_arity(src)) {
		bool changed;
		case 0: dest = DAG_dup(src); break;
		case 1:
			structural_rec_aux(DAG_arg0(src));
			if (DAG_arg0(src) != DAG_tmp_DAG[DAG_arg0(src)])
				dest =
					DAG_dup(DAG_new_unary(DAG_symb(src), DAG_tmp_DAG[DAG_arg0(src)]));
			else
				dest = DAG_dup(src);
			break;
		case 2:
			structural_rec_aux(DAG_arg0(src));
			structural_rec_aux(DAG_arg1(src));
			if (
				DAG_arg0(src) != DAG_tmp_DAG[DAG_arg0(src)] ||
				DAG_arg1(src) != DAG_tmp_DAG[DAG_arg1(src)])
				dest = DAG_dup(DAG_new_binary(
					DAG_symb(src), DAG_tmp_DAG[DAG_arg0(src)],
					DAG_tmp_DAG[DAG_arg1(src)]));
			else
				dest = DAG_dup(src);
			break;
		default:
			MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
			for (i = 0, changed = false; i < DAG_arity(src); i++) {
				structural_rec_aux(DAG_arg(src, i));
				PDAG[i] = DAG_tmp_DAG[DAG_arg(src, i)];
				changed |= PDAG[i] != DAG_arg(src, i);
			}
			if (changed)
				dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
			else {
				free(PDAG);
				dest = DAG_dup(src);
			}
	}
		/* PF tmp is necessary: DAG_tmp_DAG affected by structural_rec_func */
#ifdef DEBUG_RECURSION
	my_DAG_message("before structural_rec_func %d %D\n", src, src);
	my_DAG_message("intermediate %d %D\n", dest, dest);
#endif /* DEBUG_RECURSION */
	tmp = structural_rec_func(dest);
#ifdef DEBUG_RECURSION
	my_DAG_message("after structural_rec_func %D\n", tmp);
#endif /* DEBUG_RECURSION */
	DAG_tmp_DAG[src] = tmp;
}

TDAG
structural_recursion(TDAG src, TDAG (*f)(TDAG))
{
	TDAG dest;
	structural_rec_func = f;
	DAG_tmp_reserve();
	structural_rec_aux(src);
	dest = DAG_dup(DAG_tmp_DAG[src]);
	DAG_tmp_free_DAG(src);
	DAG_tmp_release();
	return dest;
}

void
structural_recursion_array(unsigned n, TDAG* Psrc, TDAG (*f)(TDAG))
{
	unsigned i;
	TDAG* dest;
	structural_rec_func = f;
	DAG_tmp_reserve();
	MY_MALLOC(dest, n * sizeof(TDAG));
	/* PF this is a bit tricky because we have to free everything in one go.
     To be convinced, consider repeated occurrence of same formula in Psrc */
	for (i = 0; i < n; i++) {
		structural_rec_aux(Psrc[i]);
		dest[i] = DAG_dup(DAG_tmp_DAG[Psrc[i]]);
	}
	for (i = 0; i < n; i++) {
		DAG_tmp_free_DAG(Psrc[i]);
		DAG_free(Psrc[i]);
		Psrc[i] = dest[i];
	}
	free(dest);
	DAG_tmp_release();
}

void
cond_structural_recursion_array(
	unsigned n, TDAG* Psrc, TDAG (*f)(TDAG), bool (*cont)(TDAG))
{
	unsigned i;
	TDAG* dest;
	structural_rec_func = f;
	DAG_tmp_reserve();
	MY_MALLOC(dest, n * sizeof(TDAG));
	/* PF this is a bit tricky because we have to free everything in one go.
     To be convinced, consider repeated occurrence of same formula in Psrc */
	for (i = 0; i < n; i++)
		if (cont(Psrc[i])) {
			structural_rec_aux(Psrc[i]);
			dest[i] = DAG_dup(DAG_tmp_DAG[Psrc[i]]);
		} else
			dest[i] = DAG_dup(Psrc[i]);
	for (i = 0; i < n; i++) {
		DAG_tmp_free_DAG(Psrc[i]);
		DAG_free(Psrc[i]);
		Psrc[i] = dest[i];
	}
	free(dest);
	DAG_tmp_release();
}

/*
  --------------------------------------------------------------
  Structural recursion - predicate
  --------------------------------------------------------------
*/

static bool (*structural_rec_pred)(TDAG);

static bool
structural_rec_pred_aux(TDAG src)
{
	unsigned i;
	if (DAG_tmp_bool[src]) return 1;
	DAG_tmp_bool[src] = true;
	if (!structural_rec_pred(src)) return false;
	for (i = 0; i < DAG_arity(src); i++)
		if (!structural_rec_pred_aux(DAG_arg(src, i))) return false;
	return true;
}

bool
structural_recursion_pred(TDAG src, bool (*f)(TDAG))
{
	bool res;
	DAG_tmp_reserve();
	structural_rec_pred = f;
	res = structural_rec_pred_aux(src);
	DAG_tmp_reset_bool(src);
	DAG_tmp_release();
	return res;
}

/*
  --------------------------------------------------------------
  Structural recursion - void
  --------------------------------------------------------------
*/

static void (*structural_rec_void)(TDAG);

static void
structural_rec_void_aux(TDAG src)
{
	unsigned i;
	if (DAG_tmp_bool[src]) return;
	DAG_tmp_bool[src] = true;
	structural_rec_void(src);
	for (i = 0; i < DAG_arity(src); i++) structural_rec_void_aux(DAG_arg(src, i));
}

void
structural_recursion_void(TDAG src, void (*f)(TDAG))
{
	DAG_tmp_reserve();
	structural_rec_void = f;
	structural_rec_void_aux(src);
	DAG_tmp_reset_bool(src);
	DAG_tmp_release();
}

/*
  --------------------------------------------------------------
  Conditional structural recursion - function
  --------------------------------------------------------------
*/

static bool (*cond_structural_rec_cont)(TDAG);
static TDAG (*cond_structural_rec_func)(TDAG);

static void
cond_structural_rec_aux(TDAG src)
{
	unsigned i;
	TDAG *PDAG, dest, tmp;
	if (DAG_tmp_DAG[src]) return;
	if (!cond_structural_rec_cont(src)) {
		DAG_tmp_DAG[src] = DAG_dup(src);
		return;
	}
	switch (DAG_arity(src)) {
		case 0: dest = DAG_dup(src); break;
		case 1:
			cond_structural_rec_aux(DAG_arg0(src));
			dest = DAG_dup(DAG_new_unary(DAG_symb(src), DAG_tmp_DAG[DAG_arg0(src)]));
			break;
		case 2:
			cond_structural_rec_aux(DAG_arg0(src));
			cond_structural_rec_aux(DAG_arg1(src));
			dest = DAG_dup(DAG_new_binary(
				DAG_symb(src), DAG_tmp_DAG[DAG_arg0(src)], DAG_tmp_DAG[DAG_arg1(src)]));
			break;
		default:
			MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
			for (i = 0; i < DAG_arity(src); i++) {
				cond_structural_rec_aux(DAG_arg(src, i));
				PDAG[i] = DAG_tmp_DAG[DAG_arg(src, i)];
			}
			dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
	}
	tmp = cond_structural_rec_func(dest);
	DAG_tmp_DAG[src] = tmp;
}

TDAG
cond_structural_recursion(TDAG src, TDAG (*f)(TDAG), bool (*cont)(TDAG))
{
	TDAG dest;
	DAG_tmp_reserve();
	cond_structural_rec_func = f;
	cond_structural_rec_cont = cont;
	cond_structural_rec_aux(src);
	dest = DAG_dup(DAG_tmp_DAG[src]);
	DAG_tmp_free_DAG(src);
	DAG_tmp_release();
	return dest;
}

/*
  --------------------------------------------------------------
  Conditional structural recursion - void
  --------------------------------------------------------------
*/

static void
cond_structural_rec_void_aux(TDAG src)
{
	unsigned i;
	if (DAG_tmp_bool[src]) return;
	DAG_tmp_bool[src] = true;
	if (!cond_structural_rec_cont(src)) return;
	structural_rec_void(src);
	for (i = 0; i < DAG_arity(src); i++)
		cond_structural_rec_void_aux(DAG_arg(src, i));
}

void
cond_structural_recursion_void(TDAG src, void (*f)(TDAG), bool (*cont)(TDAG))
{
	DAG_tmp_reserve();
	structural_rec_void = f;
	cond_structural_rec_cont = cont;
	cond_structural_rec_void_aux(src);
	DAG_tmp_reset_bool(src);
	DAG_tmp_release();
}
