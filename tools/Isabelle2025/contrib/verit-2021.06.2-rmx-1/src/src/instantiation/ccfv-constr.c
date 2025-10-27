/*
                                  ***   StarForth   ***

  ccfv-constr.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.052-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/verit-2021.06.2-rmx-1/src/src/instantiation/ccfv-constr.c
 */

#include "instantiation/ccfv-constr.h"

#include "pre/pre.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-symb.h"
#include "symbolic/DAG.h"
#include "utils/general.h"

#include <limits.h>

/*
  --------------------------------------------------------------
  Ordering
  --------------------------------------------------------------
*/

/**
   \brief ordering function on components' score: returns neg, zero or pos
   value like a compare for qsort */
int
comps_cmp_q_score(Tcomp* Pcomp1, Tcomp* Pcomp2)
{
	return (int)Pcomp2->score - (int)Pcomp1->score;
}

/**
   \brief ordering function on components' score: returns neg, zero or pos
   value like a compare for qsort */
int
constrs_cmp_q_score(Tconstr* Pconstr1, Tconstr* Pconstr2)
{
	return (int)Pconstr2->score - (int)Pconstr1->score;
}

/**
   \brief ordering function on components' score: returns neg, zero or pos
   value like a compare for qsort */
int
constrs_cmp_q_t_score(Tconstr* Pconstr1, Tconstr* Pconstr2)
{
	/* if ((int) Pconstr1->t_score != (int) Pconstr2->t_score) */
	/*   return (int) Pconstr1->score - (int) Pconstr2->score; */
	return (int)Pconstr1->t_score - (int)Pconstr2->t_score;
}

#define set_registered(r, v_pos) r |= 1u << v_pos
#define is_registered(r, v_pos) ((r >> v_pos) & 1u)

Tstack_comp
sort_constraints(Tstack_constr constraints)
{
	unsigned i, j, pos, first, tmp_comp, score, *components, *literals,
		registered;
	Tconstr constr;
	Tstack_DAG vars;
	Tcomp* tmp_components;
	Tstack_comp found_components;
	MY_MALLOC(components, stack_size(constraints) * sizeof(unsigned));
	memset(components, 0, stack_size(constraints) * sizeof(unsigned));
	MY_MALLOC(literals, stack_size(current_vars) * sizeof(unsigned));
	memset(literals, 0, stack_size(current_vars) * sizeof(unsigned));
	registered = 0;
	stack_INIT(vars);
	/* Link constraints according to variables */
	for (i = 0; i < stack_size(constraints); ++i) {
		components[i] = i;
		constr = stack_get(constraints, i);
		stack_reset(vars);
		if (!ground(constr.D0)) stack_merge(vars, get_fvars(constr.D0));
		/* TODO: this test is OK for predicates? */
		if (!ground(constr.D1)) stack_merge(vars, get_fvars(constr.D1));
		/* Constraint is ground */
		if (stack_is_empty(vars)) continue;
		/* Remove duplicate vars */
		stack_sort(vars, DAG_cmp_q);
		stack_uniq(vars);
		for (j = 0; j < stack_size(vars); ++j) {
			pos = var_pos(stack_get(vars, j));
			/* Var first occurrence */
			if (!is_registered(registered, pos)) {
				literals[pos] = i;
				set_registered(registered, pos);
				continue;
			}
			/* Var has occurred before; link all previous respective components to
             this one */
			first = literals[pos];
			while (components[first] != first) {
				tmp_comp = components[first];
				components[first] = i;
				first = tmp_comp;
			}
			components[first] = i;
		}
	}
	free(literals);
	/* Determine components by going backwards */
	for (i = stack_size(constraints); i > 0; --i) {
		if (components[i - 1] != i - 1)
			components[i - 1] = components[components[i - 1]];
		/* stack_get(constraints, i - 1) belongs to the component components[i - 1] */
	}
	/* Collect components */
	MY_MALLOC(tmp_components, stack_size(constraints) * sizeof(Tcomp));
	for (i = 0; i < stack_size(constraints); ++i) {
		tmp_components[i].constrs = NULL;
		tmp_components[i].score = 0;
	}
	for (i = 0; i < stack_size(constraints); ++i) {
		tmp_comp = components[i];
		if (!tmp_components[tmp_comp].constrs)
			stack_INIT(tmp_components[tmp_comp].constrs);
		stack_push(tmp_components[tmp_comp].constrs, stack_get(constraints, i));
	}
	free(components);
	stack_INIT(found_components);
	for (i = 0; i < stack_size(constraints); ++i)
		if (tmp_components[i].constrs)
			stack_push(found_components, tmp_components[i]);
	free(tmp_components);
	stack_free(constraints);
	assert(!stack_is_empty(found_components));
	/* Compute score of constrainst and thus of components */
	for (i = 0; i < stack_size(found_components); ++i) {
		stack_reset(vars);
		score = 0;
		for (j = 0; j < stack_size(stack_get(found_components, i).constrs); ++j) {
			constr = stack_get(stack_get(found_components, i).constrs, j);
			constr.score = 0;
			if (!ground(constr.D0)) stack_merge(vars, get_fvars(constr.D0));
			/* TODO: this test is OK for predicates? */
			if (!ground(constr.D1)) stack_merge(vars, get_fvars(constr.D1));
			/* TODO: refine for branching potential */
			score += stack_size(vars);
			stack_get(stack_get(found_components, i).constrs, j).score =
				stack_size(vars);
		}
		/* Order consraints from bigger to smaller score */
		stack_sort(stack_get(found_components, i).constrs, constrs_cmp_q_score);
		stack_get(found_components, i).score = score;
	}
	/* Order components from bigger to smaller score */
	stack_sort(found_components, comps_cmp_q_score);
	stack_free(vars);
	return found_components;
}

/*
  --------------------------------------------------------------
  Creation and classification
  --------------------------------------------------------------
*/

#define type_score(t) \
	(t == CCFV_UNDEF \
		 ? 0 \
		 : ((t == CCFV_GROUND_SIG || t == CCFV_GROUND_PRED) \
					? 1 \
					: (t == CCFV_ASSIGN \
							 ? 2 \
							 : ((t == CCFV_PRED || t == CCFV_EMATCH_FRESH || \
									 t == CCFV_EMATCH) \
										? 3 \
										: (t == CCFV_EUNI_VAR ? 4 \
																					: (t == CCFV_EUNI_FAPP ? 5 : 6))))))

/* pt or px */
static void
classify_pred(Tconstr* constr)
{
	if (ground_mod(constr->D0)) {
		/* Fresh ground predicates are undefined, thus cannot be entailed */
		if (!ground(constr->D0) && !has_sig(constr->D0)) {
			constr->type = CCFV_UNDEF;
			return;
		}
		constr->D0 =
			ground(constr->D0) ? constr->D0 : DAGs_modulo[constr->D0]->term;
		constr->type = CCFV_GROUND_PRED;
	}
	constr->type = CCFV_PRED;
}

/* TODO: Workaround since options are set in inst-man */
extern bool ccfv_all_CI;

/* t1 = t2 or t1 != t2 */
static void
classify_grounds(Tconstr* constr)
{
	TDAG sig_D0, sig_D1, fresh_D0, fresh_D1;
	sig_D0 = sig_D1 = fresh_D0 = fresh_D1 = DAG_NULL;
	/* If term has FVs and SIG, retrieve it, otherwise term itself unless
     variable */
	if (!ground(constr->D0)) {
		if (has_sig(constr->D0))
			sig_D0 = DAGs_modulo[constr->D0]->term;
		else
			fresh_D0 =
				DAG_arity(constr->D0) ? constr->D0 : DAGs_modulo[constr->D0]->term;
	} else
		sig_D0 = constr->D0;
	if (!ground(constr->D1)) {
		if (has_sig(constr->D1))
			sig_D1 = DAGs_modulo[constr->D1]->term;
		else
			fresh_D1 =
				DAG_arity(constr->D1) ? constr->D1 : DAGs_modulo[constr->D1]->term;
	} else
		sig_D1 = constr->D1;
	/* sig/fresh are exclusive conditions */
	assert(
		(!sig_D0 || !fresh_D0) && (sig_D0 || fresh_D0) && (!sig_D1 || !fresh_D1) &&
		(sig_D1 || fresh_D1));
	/* either both have sigs, both fresh or alternated */
	assert(
		(sig_D0 && sig_D1) || (fresh_D0 && fresh_D1) || (fresh_D0 && sig_D1) ||
		(fresh_D1 && sig_D0));
	/* TODO: simplify this somehow */
	/* DAG is a var, leave to be handled later */
	if (
		!constr->pol && !ccfv_all_CI && !ground(constr->D0) &&
		!DAG_arity(constr->D0)) {
		/* TODO: reallly do this? */
		if (fresh_D1) {
			constr->type = CCFV_UNDEF;
			return;
		}
		constr->D0 = constr->D0;
		constr->D1 = sig_D1;
		constr->type = CCFV_ASSIGN;
		return;
	}
	if (
		!constr->pol && !ccfv_all_CI && !ground(constr->D1) &&
		!DAG_arity(constr->D1)) {
		if (fresh_D0) {
			constr->type = CCFV_UNDEF;
			return;
		}
		constr->D0 = constr->D1;
		constr->D1 = sig_D0;
		constr->type = CCFV_ASSIGN;
		return;
	}

	/* If sigs, just check with ground model, otherwise try E-matching args/assigns (!pol) */
	if (sig_D0 && sig_D1) {
		constr->D0 = sig_D0;
		constr->D1 = sig_D1;
		constr->type = CCFV_GROUND_SIG;
		return;
	}
	if (constr->pol) {
		if (fresh_D0) {
			constr->D0 = fresh_D0;
			constr->D1 = fresh_D1 ? fresh_D1 : sig_D1;
		} else {
			constr->D0 = fresh_D1;
			constr->D1 = sig_D0;
		}
		constr->type = CCFV_EMATCH_FRESH;
		return;
	}
	/* If either grounded is fresh, constraint can't be entailed */
	assert(!sig_D0 || !sig_D1);
	constr->type = CCFV_UNDEF;
}

/* TODO: check that stuff is correct */
/* x = y or x = t or ... */
static void
classify_var_gr_or_var(
	Tunifier solution, TDAG NGDAG, TDAG UDAG, Tconstr* constr)
{
	TDAG rep_NGDAG, rep_UDAG;
	rep_NGDAG = solution ? unify_find_DAG(solution, NGDAG) : NGDAG;
	rep_UDAG = solution ? unify_find_DAG(solution, UDAG) : UDAG;

	if (ground_mod(rep_UDAG)) {
		/* UDAG is fresh */
		if (!constr->pol && !ground(rep_UDAG) && !has_sig(rep_UDAG)) {
			constr->type = CCFV_UNDEF;
			return;
		}
		UDAG = ground(rep_UDAG) ? rep_UDAG
														: ((has_sig(rep_UDAG) || !DAG_arity(rep_UDAG))
																 ? DAGs_modulo[rep_UDAG]->term
																 : rep_UDAG);
		if (DAG_arity(rep_NGDAG)) {
			constr->D0 = rep_NGDAG;
			constr->D1 = UDAG;
			constr->type = CCFV_EMATCH;
			return;
		}
		constr->D0 = rep_NGDAG;
		constr->D1 = UDAG;
		constr->type = CCFV_ASSIGN;
		return;
	}
	/* NGDAG is fx */
	if (DAG_arity(rep_NGDAG)) {
		/* UDAG in gy */
		if (DAG_arity(UDAG)) {
			constr->D0 = rep_NGDAG;
			constr->D1 = rep_UDAG;
			constr->type = CCFV_EUNI_FAPP;
			return;
		}
		/* y != fx or y = fy */
		if (!constr->pol || unify_occurs(solution, UDAG, rep_NGDAG)) {
			constr->D0 = rep_UDAG;
			constr->D1 = rep_NGDAG;
			constr->type = CCFV_EUNI_VAR;
			return;
		}
		/* y = fx */
		constr->D0 = UDAG;
		constr->D1 = rep_NGDAG;
		constr->type = CCFV_ASSIGN_FAPP;
		return;
	}
	/* NGDAG is x or the var representative of its class */
	assert(NGDAG == rep_NGDAG || unify_is_var(solution, rep_NGDAG));
	/* y is gy */
	if (DAG_arity(rep_UDAG)) {
		/* x != gy or x = gx */
		if (unify_occurs(solution, NGDAG, rep_UDAG)) {
			constr->D0 = NGDAG;
			constr->D1 = rep_UDAG;
			constr->type = CCFV_EUNI_VAR;
			return;
		}
		constr->D0 = NGDAG;
		constr->D1 = UDAG;
		constr->type = CCFV_ASSIGN_FAPP;
		return;
	}
	/* x and y */
	constr->D0 = NGDAG;
	constr->D1 = UDAG;
	constr->type = CCFV_ASSIGN;
	return;
}

static void
classify_var_ng(Tunifier solution, TDAG NGDAG, TDAG UDAG, Tconstr* constr)
{
	TDAG rep_NGDAG;
	assert(DAG_arity(UDAG));
	rep_NGDAG = solution ? unify_find_DAG(solution, NGDAG) : NGDAG;
	if (DAG_arity(NGDAG)) {
		constr->D0 = rep_NGDAG;
		constr->D1 = UDAG;
		constr->type = CCFV_EUNI_FAPP;
		return;
	}
	/* NGDAG is x or the var representative of its class */
	assert(NGDAG == rep_NGDAG || unify_is_var(solution, rep_NGDAG));
	if (!constr->pol || (solution && unify_occurs(solution, NGDAG, UDAG))) {
		constr->D0 = NGDAG;
		constr->D1 = UDAG;
		constr->type = CCFV_EUNI_VAR;
		return;
	}
	constr->D0 = NGDAG;
	constr->D1 = UDAG;
	constr->type = CCFV_ASSIGN_FAPP;
	return;
}

#define DEBUG_CONSTR 0

static void
classify_constraint(Tconstr* constr, Tunifier solution)
{
	TDAG NGDAG, UDAG;
	/* predicates */
	if (!constr->D1) {
		classify_pred(constr);
		return;
	}
	/* t1 = t2 or t1 != t2 */
	if (ground_mod(constr->D0) && ground_mod(constr->D1)) {
#if DEBUG_CONSTR
		my_message("among grounds constr:\n");
		print_constr(UINT_MAX, *constr);
#endif
		classify_grounds(constr);
		return;
	}
	ORDER_CONSTRAINT(NGDAG, UDAG, constr->D0, constr->D1);
#ifdef DEBUG
	/* fx = y or fx != y can't happen due to constraint ordering */
	assert(!(DAG_arity(NGDAG) && !ground_mod(UDAG) && !DAG_arity(UDAG)));
	if (solution) {
		TDAG rep_NGDAG = unify_find_DAG(solution, NGDAG);
		TDAG rep_UDAG = unify_find_DAG(solution, UDAG);
		if (!DAG_arity(NGDAG))
			assert(
				unify_is_var(solution, NGDAG) ||
				(DAG_arity(rep_NGDAG) && !ground_mod(rep_NGDAG)));
		if (!ground_mod(UDAG) && !DAG_arity(UDAG))
			assert(
				unify_is_var(solution, UDAG) ||
				(DAG_arity(rep_UDAG) && !ground_mod(rep_UDAG)));
	}
#endif
	/* x = y or x = t or x != t or x != y */
	if (!DAG_arity(NGDAG) && (ground_mod(UDAG) || !DAG_arity(UDAG))) {
#if DEBUG_CONSTR
		my_message("var to gr/var constr:\n");
		print_constr(UINT_MAX, *constr);
#endif
		classify_var_gr_or_var(solution, NGDAG, UDAG, constr);
		return;
	}
	/* x = gy or x != gy */
	if (!DAG_arity(NGDAG) && !ground_mod(UDAG)) {
#if DEBUG_CONSTR
		my_message("var to ng fapp constr:\n");
		print_constr(UINT_MAX, *constr);
#endif
		classify_var_ng(solution, NGDAG, UDAG, constr);
		return;
	}
	/* fx = t or fx != t */
	assert(DAG_arity(NGDAG));
	if (ground_mod(UDAG)) {
#if DEBUG_CONSTR
		my_message("ematch constr:\n");
		print_constr(UINT_MAX, *constr);
#endif
		if (!constr->pol && !ground(UDAG) && !has_sig(UDAG)) {
			constr->type = CCFV_UNDEF;
			return;
		}
		UDAG = ground(UDAG)
			? UDAG
			: ((has_sig(UDAG) || !DAG_arity(UDAG)) ? DAGs_modulo[UDAG]->term : UDAG);
		constr->D0 = NGDAG;
		constr->D1 = UDAG;
		constr->type = CCFV_EMATCH;
		return;
	}
	/* fx = gy or fx != gy */
#if DEBUG_CONSTR
	my_message("euni_fapp constr:\n");
	print_constr(UINT_MAX, *constr);
#endif
	assert(DAG_arity(UDAG));
	constr->D0 = NGDAG;
	constr->D1 = UDAG;
	constr->type = CCFV_EUNI_FAPP;
}

Tconstr
create_constr_lit(TDAG lit, Tunifier solution)
{
	bool pol;
	TDAG DAG;
	Tconstr constr;
	if (DAG_symb(lit) == CONNECTOR_NOT) {
		pol = true;
		DAG = DAG_arg0(lit);
	} else {
		pol = false;
		DAG = lit;
	}
	constr.pol = pol;
	constr.type = CCFV_UNDEF;
	if (DAG_symb(DAG) == PREDICATE_EQ) {
		constr.D0 = DAG_arg0(DAG);
		constr.D1 = DAG_arg1(DAG);
	} else {
		constr.D0 = DAG;
		constr.D1 = DAG_NULL;
	}
	classify_constraint(&constr, solution);
	constr.t_score = type_score(constr.type);
	constr.score = 0;
	return constr;
}

Tconstr
create_constr_eq(TDAG D0, TDAG D1, Tunifier solution)
{
	Tconstr constr;
	constr.D0 = D0;
	constr.D1 = D1;
	constr.pol = true;
	constr.type = CCFV_UNDEF;
	classify_constraint(&constr, solution);
	constr.t_score = type_score(constr.type);
	constr.score = 0;
	return constr;
}

Tconstr
create_constr(TDAG D0, TDAG D1, Tconstr_type type)
{
	Tconstr constr;
	constr.D0 = D0;
	constr.D1 = D1;
	constr.pol = true;
	constr.type = type;
	constr.t_score = type_score(constr.type);
	constr.score = 0;
	return constr;
}

/* TODO: to handle in a more directed way than to simply call classify_constraint
   CCFV_ASSIGN,
   CCFV_PRED,
   CCFV_EMATCH,
   CCFV_EUNI_VAR,
   CCFV_EUNI_FAPP,
   CCFV_ASSIGN_FAPP
*/
void
update_constr(Tconstr* constr, Tunifier solution)
{
	/* Immutable types */
	if (
		constr->type == CCFV_UNDEF || constr->type == CCFV_GROUND_SIG ||
		constr->type == CCFV_GROUND_PRED || constr->type == CCFV_EMATCH_FRESH)
		return;
#if DEBUG_CONSTR
	TDAG old_D0 = constr->D0, old_D1 = constr->D1;
	Tconstr_type old_type = constr->type;
#endif
	classify_constraint(constr, solution);
	constr->t_score = type_score(constr->type);
#if DEBUG_CONSTR
	if (
		old_type != constr->type || old_D0 != constr->D0 || old_D1 != constr->D1) {
		my_message("Previous constr:\n");
		print_constr(UINT_MAX, *constr);
		my_message("New constr:\n");
		print_constr(UINT_MAX, *constr);
	}
#endif
	/* TODO: update score, which may change even if type does not */
}

/*
  --------------------------------------------------------------
  Pruning
  --------------------------------------------------------------
*/

/* TODO: the way this should actually be implemented is either (ideally) using
   the CC hash table (could it allow partial narrowings? From the hash of the
   symbol and some of the arguments to have the subset of ground applications)
   or an indexing by arg tuples into sets of ground applications */
bool
check_ground_apps(Tstack_constr constraints)
{
	return true;
	/* unsigned i, j, pos; */
	/* TDAG DAG, arg_DAG; */
	/* bool pol; */
	/* while (!stack_is_empty(grounded_preds_pos)) */
	/*   { */
	/*     pos = stack_pop(grounded_preds_pos); */
	/*     DAG = stack_pop(grounded_preds_pos); */
	/*     assert(ground_mod(DAG_arg(DAG, pos))); */
	/*     /\* TODO: is this correct to have here? *\/ */
	/*     if (!has_sig(DAG_arg(DAG, pos))) */
	/*       return false; */
	/*     arg_DAG = DAGs_modulo[DAG_arg(DAG, pos)]->term; */
	/*     for (i = 0; i < stack_size(constraints); ++i) */
	/*       if (stack_get(constraints, i).D0 == DAG) */
	/*         { */
	/*           assert(is_predicate(stack_get(constraints, i))); */
	/*           pol = stack_get(constraints, i).pol; */
	/*           Pindex index; */
	/*           /\* If no indexed application, fails *\/ */
	/*           if (!get_Pindex(DAG_symb(DAG), &index) || !index.signatures[pol]) */
	/*             { */
	/*               /\* my_DAG_error("This guy should have been killed before\n"); *\/ */
	/*               return false; */
	/*             } */
	/*           for (j = 0; j < stack_size(index.signatures[pol]); ++j) */
	/*             if (congruent(arg_DAG, */
	/*                           DAG_arg(stack_get(index.signatures[pol], j), pos))) */
	/*               break; */
	/*           if (j == stack_size(index.signatures[pol])) */
	/*             return false; */
	/*         } */
	/*   } */
	/* return true; */
}

/*
  --------------------------------------------------------------
  Debugging
  --------------------------------------------------------------
*/

char*
print_constr_type(Tconstr_type type)
{
	if (type == CCFV_UNDEF) return "UNDEF";
	if (type == CCFV_GROUND_SIG) return "GROUND_SIG";
	if (type == CCFV_GROUND_PRED) return "GROUND_PRED";
	if (type == CCFV_ASSIGN) return "ASSIGN";
	if (type == CCFV_PRED) return "PRED";
	if (type == CCFV_EMATCH_FRESH) return "EMATCH_FRESH";
	if (type == CCFV_EMATCH) return "EMATCH";
	if (type == CCFV_EUNI_VAR) return "EUNI_VAR";
	if (type == CCFV_EUNI_FAPP) return "EUNI_FAPP";
	if (type == CCFV_ASSIGN_FAPP) return "ASSIGN_FAPP";
	my_DAG_error("undef type");
	return "";
}

void
print_constr(unsigned i, Tconstr constr)
{
	if (i != UINT_MAX) {
		if (!constr.D1)
			my_DAG_message(
				"[%d]: %d, %s: {%d}%s%D\n", i, constr.t_score,
				print_constr_type(constr.type), constr.D0, constr.pol ? "" : "not",
				constr.D0);
		else
			my_DAG_message(
				"[%d]: %d, %s: {%d/%d}%s<%D, %D>\n", i, constr.t_score,
				print_constr_type(constr.type), constr.D0, constr.D1,
				constr.pol ? "eq" : "dis", constr.D0, constr.D1);
		return;
	}
	if (!constr.D1)
		my_DAG_message(
			"%d, %s: {%d}%s%D\n", constr.D0, constr.t_score,
			print_constr_type(constr.type), constr.pol ? "" : "not", constr.D0);
	else
		my_DAG_message(
			"%d, %s: {%d/%d}%s<%D, %D>\n", constr.t_score,
			print_constr_type(constr.type), constr.D0, constr.D1,
			constr.pol ? "eq" : "dis", constr.D0, constr.D1);
}

void
print_Tstack_constr(Tstack_constr constraints)
{
	unsigned i;
	for (i = 0; i < stack_size(constraints); ++i)
		print_constr(i, stack_get(constraints, i));
}
