/*
                                  ***   StarForth   ***

  DAG-subst.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.574-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/verit-2021.06.2-rmx-1/src/src/symbolic/DAG-subst.c
 */

#include "symbolic/DAG-subst.h"

#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "symbolic/qnt-utils.h"

/*
  --------------------------------------------------------------
  Substitution
  --------------------------------------------------------------
*/

bool
DAG_tmp_subst(TDAG src)
{
	unsigned i, res = 0;
	Tstack_DAGstack *Ptriggers, triggers;
	if (DAG_tmp_DAG[src]) return DAG_tmp_DAG[src] != src;
	for (i = 0; i < DAG_arity(src); i++) res |= DAG_tmp_subst(DAG_arg(src, i));
	if (res) {
		TDAG *PDAG, tmp;
		MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
		for (i = 0; i < DAG_arity(src); i++) PDAG[i] = DAG_tmp_DAG[DAG_arg(src, i)];
		tmp = DAG_new(DAG_symb(src), DAG_arity(src), PDAG);
		/* Copy triggers */
		{
			Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
			if (Ptriggers) {
				triggers = copy_triggers(*Ptriggers);
				DAG_prop_set(tmp, DAG_PROP_TRIGGER, &triggers);
			}
		}
		/* PF tmp is necessary since DAG_new may change DAG_tmp_DAG */
		DAG_tmp_DAG[src] = tmp;
		return true;
	}
	DAG_tmp_DAG[src] = src;
	return false;
}

bool
DAG_tmp_subst_cond(TDAG src, bool (*cont)(TDAG DAG))
{
	unsigned i, res = 0;
	Tstack_DAGstack *Ptriggers, triggers;
	if (DAG_tmp_DAG[src]) return DAG_tmp_DAG[src] != src;
	if (cont(src))
		for (i = 0; i < DAG_arity(src); i++) res |= DAG_tmp_subst(DAG_arg(src, i));
	if (res) {
		TDAG *PDAG, tmp;
		MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
		for (i = 0; i < DAG_arity(src); i++) PDAG[i] = DAG_tmp_DAG[DAG_arg(src, i)];
		tmp = DAG_new(DAG_symb(src), DAG_arity(src), PDAG);
		/* Copy triggers */
		{
			Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
			if (Ptriggers) {
				triggers = copy_triggers(*Ptriggers);
				DAG_prop_set(tmp, DAG_PROP_TRIGGER, &triggers);
			}
		}
		/* PF tmp is necessary since DAG_new may change DAG_tmp_DAG */
		DAG_tmp_DAG[src] = tmp;
		return true;
	}
	DAG_tmp_DAG[src] = src;
	return false;
}

TDAG
DAG_subst(TDAG src, TDAG origin, TDAG subst)
{
	TDAG dest;
	DAG_tmp_reserve();
	DAG_tmp_DAG[origin] = subst;
	DAG_tmp_DAG[subst] = subst;
	DAG_tmp_subst(src);
	dest = DAG_tmp_DAG[src];
	DAG_tmp_reset_DAG(src);
	DAG_tmp_reset_DAG(origin);
	DAG_tmp_reset_DAG(subst);
	DAG_tmp_release();
	return dest;
}

TDAG
DAG_subst_multiple(TDAG src, unsigned n, TDAG* origin, TDAG* subst)
{
	TDAG dest;
	unsigned i;
	DAG_tmp_reserve();
	for (i = 0; i < n; ++i) {
		DAG_tmp_DAG[origin[i]] = subst[i];
		DAG_tmp_DAG[subst[i]] = subst[i];
	}
	DAG_tmp_subst(src);
	dest = DAG_tmp_DAG[src];
	DAG_tmp_reset_DAG(src);
	for (i = 0; i < n; ++i) {
		DAG_tmp_reset_DAG(origin[i]);
		DAG_tmp_reset_DAG(subst[i]);
	}
	DAG_tmp_release();
	return dest;
}

static bool
DAG_contain_aux(TDAG src)
{
	unsigned i;
	bool res = false;
	if (DAG_tmp_bool[src]) return (DAG_tmp_bool[src] == 2);
	for (i = 0; i < DAG_arity(src); i++) res |= DAG_contain_aux(DAG_arg(src, i));
	DAG_tmp_bool[src] = 1;
	return res;
}

bool
DAG_contain(TDAG src, TDAG find)
{
	bool res;
	DAG_tmp_reserve();
	DAG_tmp_bool[find] = 2;
	res = DAG_contain_aux(src);
	DAG_tmp_reset_bool(find);
	DAG_tmp_reset_bool(src);
	DAG_tmp_release();
	return res;
}
