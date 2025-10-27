#include "symbolic/DAG-stat.h"

#include "symbolic/DAG-tmp.h"
#include "symbolic/veriT-DAG.h"

#include <limits.h>

/*
  --------------------------------------------------------------
  Statistics
  --------------------------------------------------------------
*/

static unsigned
DAG_count_nodes_aux(TDAG DAG)
{
	unsigned i, j;
	if (DAG_tmp_unsigned[DAG]) return 0;
	DAG_tmp_unsigned[DAG] = 1;
	for (i = 0, j = 0; i < DAG_arity(DAG); i++)
		j += DAG_count_nodes_aux(DAG_arg(DAG, i));
	return j + 1;
}

unsigned
DAG_count_nodes(TDAG DAG)
{
	unsigned res;
	DAG_tmp_reserve();
	res = DAG_count_nodes_aux(DAG);
	DAG_tmp_reset_unsigned(DAG);
	DAG_tmp_release();
	return res;
}

#define SAFE_ADD(A, B) \
	((A == UINT_MAX || B == UINT_MAX) ? UINT_MAX \
																		: ((A < UINT_MAX - B) ? A + B : UINT_MAX))

static unsigned
DAG_count_nodes_tree_aux(TDAG DAG)
{
	unsigned i;
	if (DAG_tmp_unsigned[DAG]) return DAG_tmp_unsigned[DAG];
	DAG_tmp_unsigned[DAG] = 1;
	for (i = 0; i < DAG_arity(DAG); i++) {
		unsigned k = DAG_count_nodes_tree_aux(DAG_arg(DAG, i));
		DAG_tmp_unsigned[DAG] = SAFE_ADD(DAG_tmp_unsigned[DAG], k);
	}
	return DAG_tmp_unsigned[DAG];
}

unsigned
DAG_count_nodes_tree(TDAG DAG)
{
	unsigned res;
	DAG_tmp_reserve();
	res = DAG_count_nodes_tree_aux(DAG);
	DAG_tmp_reset_unsigned(DAG);
	DAG_tmp_release();
	return res;
}

static unsigned
DAG_count_atoms_aux(TDAG DAG)
{
	unsigned i;
	if (DAG_tmp_unsigned[DAG]) return DAG_tmp_unsigned[DAG];
	if (DAG_symb(DAG) == LET) {
		for (i = 1; i < DAG_arity(DAG); i++, i++)
			DAG_tmp_unsigned[DAG] += DAG_count_atoms_aux(DAG_arg(DAG, i));
		DAG_tmp_unsigned[DAG] += DAG_count_atoms_aux(DAG_arg_last(DAG));
	} else if (quantifier(DAG_symb(DAG)))
		DAG_tmp_unsigned[DAG] = DAG_count_atoms_aux(DAG_arg_last(DAG));
	else if (boolean_connector(DAG_symb(DAG)))
		for (i = 0; i < DAG_arity(DAG); i++)
			DAG_tmp_unsigned[DAG] += DAG_count_atoms_aux(DAG_arg(DAG, i));
	else
		DAG_tmp_unsigned[DAG] = 1;
	return DAG_tmp_unsigned[DAG];
}

unsigned
DAG_count_atoms(TDAG DAG)
{
	unsigned res;
	DAG_tmp_reserve();
	res = DAG_count_atoms_aux(DAG);
	DAG_tmp_reset_unsigned(DAG);
	DAG_tmp_release();
	return res;
}

static unsigned
DAG_depth_aux(TDAG DAG)
{
	unsigned i, j;
	if (!DAG_tmp_unsigned[DAG]) {
		for (i = 0, j = 0; i < DAG_arity(DAG); i++) {
			unsigned k = DAG_depth_aux(DAG_arg(DAG, i));
			if (k > j) j = k;
		}
		DAG_tmp_unsigned[DAG] = j + 1;
	}
	return DAG_tmp_unsigned[DAG];
}

unsigned
DAG_depth(TDAG DAG)
{
	unsigned res;
	DAG_tmp_reserve();
	res = DAG_depth_aux(DAG);
	DAG_tmp_reset_unsigned(DAG);
	DAG_tmp_release();
	return res;
}
