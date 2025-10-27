/*
                                  ***   StarForth   ***

  bclauses.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:01.759-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/verit-2021.06.2-rmx-1/src/src/pre/bclauses.c
 */

/*
  --------------------------------------------------------------
  add some binary clauses for speed-up
  --------------------------------------------------------------
*/

#include "pre/bclauses.h"

#include "instantiation/free-vars.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"
#include "utils/hash.h"
#include "utils/list.h"
#include "utils/statistics.h"
#include "utils/table.h"
#include "veriT-config.h"

#if STATS_LEVEL > 1
static unsigned stat_nb_bclauses;
#endif

static Thash assoc_table = NULL;

struct TSbassoc
{
	TDAG DAG1;
	TDAG DAG2;
	Tlist atoms;
};
typedef struct TSbassoc* Tbassoc;

static unsigned
assoc_hash(Tbassoc assoc)
{
	return (unsigned)((uintptr_t)assoc->DAG1 ^ (uintptr_t)assoc->DAG2);
}

static int
assoc_equal(Tbassoc assoc1, Tbassoc assoc2)
{
	return assoc1->DAG1 == assoc2->DAG1 && assoc1->DAG2 == assoc2->DAG2;
}

static void
assoc_free(Tbassoc assoc)
{
	if (assoc->atoms) list_free(&assoc->atoms);
	free(assoc);
}

static void
assoc_add(TDAG DAG1, TDAG DAG2, TDAG atom)
{
	struct TSbassoc Sassoc;
	Tbassoc assoc;
	Sassoc.DAG1 = DAG1;
	Sassoc.DAG2 = DAG2;
	assoc = hash_lookup(assoc_table, &Sassoc);
	if (assoc) {
		assoc->atoms = list_cons(DAG_ptr_of(atom), assoc->atoms);
		return;
	}
	MY_MALLOC(assoc, sizeof(struct TSbassoc));
	assoc->DAG1 = DAG1;
	assoc->DAG2 = DAG2;
	assoc->atoms = list_cons(DAG_ptr_of(atom), NULL);
	hash_insert(assoc_table, assoc);
}

static Tlist
assoc_check(TDAG DAG1, TDAG DAG2)
{
	struct TSbassoc Sassoc;
	Tbassoc assoc;
	Sassoc.DAG1 = DAG1;
	Sassoc.DAG2 = DAG2;
	assoc = hash_lookup(assoc_table, &Sassoc);
	if (!assoc) return NULL;
	return assoc->atoms;
}

static void
add_atom(TDAG src)
{
	unsigned i;
	TDAG DAG1 = DAG_NULL, DAG2 = DAG_NULL;
	if (!ground(src)) return;
	if (DAG_symb(src) != PREDICATE_EQ) return;
	if (
		DAG_symb(DAG_arg0(src)) != DAG_symb(DAG_arg1(src)) ||
		!DAG_arity(DAG_arg0(src)) ||
		DAG_arity(DAG_arg0(src)) != DAG_arity(DAG_arg1(src)))
		return;
	for (i = 0; i < DAG_arity(DAG_arg0(src)); i++)
		if (DAG_arg(DAG_arg0(src), i) == DAG_arg(DAG_arg1(src), i))
			continue;
		else if (DAG_arg(DAG_arg0(src), i) < DAG_arg(DAG_arg1(src), i)) {
			if (!DAG1) {
				DAG1 = DAG_arg(DAG_arg0(src), i);
				DAG2 = DAG_arg(DAG_arg1(src), i);
				continue;
			}
			if (
				DAG1 == DAG_arg(DAG_arg0(src), i) && DAG2 == DAG_arg(DAG_arg1(src), i))
				continue;
			else
				return;
		} else {
			if (!DAG1) {
				DAG1 = DAG_arg(DAG_arg1(src), i);
				DAG2 = DAG_arg(DAG_arg0(src), i);
				continue;
			}
			if (
				DAG1 == DAG_arg(DAG_arg1(src), i) && DAG2 == DAG_arg(DAG_arg0(src), i))
				continue;
			else
				return;
		}
	assoc_add(DAG1, DAG2, src);
}

static Ttable binary_clauses = NULL;

static void
check_atom(TDAG src)
{
	Tlist tmp, atoms;
	if (DAG_symb(src) != PREDICATE_EQ || DAG_arg0(src) == DAG_arg1(src)) return;
	if (DAG_arg0(src) < DAG_arg1(src))
		atoms = assoc_check(DAG_arg0(src), DAG_arg1(src));
	else
		atoms = assoc_check(DAG_arg1(src), DAG_arg0(src));
	if (!atoms) return;
	tmp = atoms;
	do {
		TDAG DAG = DAG_dup(DAG_or2(DAG_not(src), DAG_of_ptr(list_car(tmp))));
		table_push(binary_clauses, DAG_ptr_of(DAG));
		tmp = list_cdr(tmp);
	} while (tmp != atoms);
}

#ifdef TERNARY_CLAUSES

static Ttable aux_symb_table = NULL;

static void
add_pred(TDAG src)
{
	Ttable table;
	if (DAG_symb(src) == PREDICATE_EQ) {
		if (DAG_arg0(src) < DAG_arg1(src))
			assoc_add(DAG_arg0(src), DAG_arg1(src), src);
		else
			assoc_add(DAG_arg1(src), DAG_arg(src, 2), src);
		return;
	}
	if (
		boolean_connector(DAG_symb(src)) || DAG_sort(src) != SORT_BOOLEAN ||
		quantifier(DAG_symb(src)) || DAG_symb_interpreted(DAG_symb(src)))
		return;
	if (DAG_symb(src)->misc)
		table = table_get(aux_symb_table, DAG_symb(src)->misc);
	else {
		DAG_symb(src)->misc = table_length(aux_symb_table);
		table = table_new(10, 10);
		table_push(aux_symb_table, table);
	}
	table_push(table, DAG_ptr_of(src));
}

static void
tclause_aux(TDAG DAGa, TDAG DAGb)
{
	unsigned i;
	TDAG DAG1 = NULL, DAG2, DAG3;
	for (i = 0; i < DAG_arity(DAGa); i++)
		if (DAG_arg(DAGa, i) == DAG_arg(DAGb, i))
			continue;
		else if (DAG_arg(DAGa, i) < DAG_arg(DAGb, i)) {
			if (!DAG1) {
				DAG1 = DAG_arg(DAGa, i);
				DAG2 = DAG_arg(DAGb, i);
				continue;
			}
			if (DAG1 == DAG_arg(DAGa, i) && DAG2 == DAG_arg(DAGb, i))
				continue;
			else
				return;
		} else {
			if (!DAG1) {
				DAG1 = DAG_arg(DAGa, i);
				DAG2 = DAG_arg(DAGb, i);
				continue;
			}
			if (DAG1 == DAG_arg(DAGa, i) && DAG2 == DAG_arg(DAGb, i))
				continue;
			else
				return;
		}
	if (!assoc_check(DAG1, DAG2)) return;
	DAG3 = DAG_not(DAG_eq(DAG1, DAG2));
	table_push(
		binary_clauses, DAG_ptr_of(DAG_dup(DAG_or3(DAG3, DAGa, DAG_not(DAGb)))));
	table_push(
		binary_clauses, DAG_ptr_of(DAG_dup(DAG_or3(DAG3, DAG_not(DAGa), DAGb))));
}

static void
tclause(TDAG src)
{
	unsigned i, j, k;
	aux_symb_table = table_new(10, 10);
	table_push(aux_symb_table, NULL);
	structural_recursion_void(src, add_pred);
	for (i = 1; i < table_length(aux_symb_table); i++) {
		Ttable table = table_get(aux_symb_table, i);
		for (j = 0; j < table_length(table); j++)
			for (k = j + 1; k < table_length(table); k++)
				tclause_aux(
					DAG_of_ptr(table_get(table, j)), DAG_of_ptr(table_get(table, k)));
	}
	for (i = 1; i < table_length(aux_symb_table); i++) {
		Ttable table = (Ttable)table_get(aux_symb_table, i);
		TDAG DAG = DAG_of_ptr(table_get(table, 0));
		DAG_symb_set_misc(DAG_symb(DAG), 0);
		table_free(&table);
	}
	table_free(&aux_symb_table);
}

#endif

TDAG
bclauses_add(TDAG src)
{
	unsigned i;
	TDAG dest, *PDAG;
	binary_clauses = table_new(10, 10);
	assoc_table =
		hash_new(256, (TFhash)assoc_hash, (TFequal)assoc_equal, (TFfree)assoc_free);
	structural_recursion_void(src, add_atom);
	structural_recursion_void(src, check_atom);
	hash_free(&assoc_table);

#ifdef TERNARY_CLAUSES
	assoc_table =
		hash_new(256, (TFhash)assoc_hash, (TFequal)assoc_equal, (TFfree)assoc_free);
	tclause(src);
	hash_free(&assoc_table);
#endif

	i = table_length(binary_clauses) +
		((DAG_symb(src) == CONNECTOR_AND) ? DAG_arity(src) : 1u);
	MY_MALLOC(PDAG, i * sizeof(TDAG));
	for (i = 0; i < table_length(binary_clauses); i++)
		PDAG[i] = DAG_of_ptr(table_get(binary_clauses, i));
	if (DAG_symb(src) != CONNECTOR_AND) {
		PDAG[i] = src;
		i++;
	} else {
		unsigned n = table_length(binary_clauses);
		for (i = 0; i < DAG_arity(src); i++) PDAG[i + n] = DAG_arg(src, i);
		i = n + DAG_arity(src);
	}
#if STATS_LEVEL > 1
	stats_counter_add(stat_nb_bclauses, table_length(binary_clauses));
#endif
	dest = DAG_dup(DAG_new(CONNECTOR_AND, i, PDAG));
	table_apply(binary_clauses, (TFapply)DAG_free);
	table_free(&binary_clauses);
	return dest;
}

void
bclauses_init(void)
{
#if STATS_LEVEL > 1
	stat_nb_bclauses = stats_counter_new(
		"bclauses", "Number of binary clauses generated in preprocessing", "%5d");
#endif
}

void
bclauses_done(void)
{}
