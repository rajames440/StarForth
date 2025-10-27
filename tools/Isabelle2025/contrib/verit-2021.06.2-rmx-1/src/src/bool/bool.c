/*
  --------------------------------------------------------------
  SAT solver interface module
  --------------------------------------------------------------
*/

#include "symbolic/DAG-ptr.h"
#include "utils/general.h"
#include "utils/options.h"
#include "utils/statistics.h"
#include "utils/types.h"
#include "utils/veriT-qsort.h"
#include "veriT-config.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* #define DEBUG_BOOL */
#ifdef DEBUG_BOOL
#include "symbolic/DAG-print.h"
#endif
#include "bool/bool.h"
#include "bool/cnf.h"
#include "proof/proof.h"
#include "symbolic/veriT-status.h"

/* IMPROVE redundant: this can be deduced from status */
unsigned bool_model_complete = 0;

/* #define STAT_MIN_MODEL */
#ifdef STAT_MIN_MODEL
static unsigned stat_minimize_time;
static unsigned stat_minimize_lit1a;
static unsigned stat_minimize_lit1b;
static unsigned stat_minimize_lit2a;
static unsigned stat_minimize_lit2b;
static unsigned stat_minimize_lit3a;
static unsigned stat_minimize_lit3b;
static unsigned stat_minimize_lit4a;
static unsigned stat_minimize_lit4b;
static unsigned stat_minimize_lit_origa;
static unsigned stat_minimize_lit_origb;
#endif

/*
  --------------------------------------------------------------
  Real things begin here
  --------------------------------------------------------------
*/

#if 0
void
bool_prepare(void)
{
#if 0
  /* disable SAT solver decision on variables introduced for definitional cnf */
  unsigned i;
  for (i = 1; i < var_max; i++)
    if (boolean_connector(DAG_symb(literal_to_DAG(i))))
      SAT_var_block_decide(i);
#endif
}
#endif

#include "instantiation/inst-del.h"

extern bool inst_marking;
extern bool inst_deletion_track_vars;

void
bool_add(TDAG formula)
{
	Tstack_clause clauses;
	unsigned i;
	stack_INIT(clauses);
	cnf(formula, &clauses);
	for (i = 0; i < stack_size(clauses); ++i) bool_add_c(stack_get(clauses, i));
	stack_free(clauses);
}

void
bool_add_proof(TDAG formula, Tproof proof)
{
	Tstack_clause clauses;
	unsigned i;
	stack_INIT(clauses);
	cnf_proof(formula, &clauses, proof);
	for (i = 0; i < stack_size(clauses); ++i) bool_add_c(stack_get(clauses, i));
	stack_free(clauses);
}

void
bool_add_c(Tclause clause)
{
	SAT_Tlit* lit;
	clause = clause_clean(clause);
	if (!clause) return;
#ifdef DEBUG_BOOL
	printf("bool_add_c : ");
	for (unsigned i = 0; i < clause->nb_lits; i++) printf("%d ", clause->lits[i]);
	printf("\n");
#endif
#ifdef POLARITY_FILTER
	for (unsigned i = 0; i < clause->nb_lits; i++) bool_lit_inc(clause->lits[i]);
#endif
	MY_MALLOC(lit, clause->nb_lits * sizeof(Tlit));
	/* TODO: IMPROVE it is a pity to have to duplicate this information */
	memcpy(lit, clause->lits, clause->nb_lits * sizeof(SAT_Tlit));
	if (proof_on) proof_SAT_insert(clause);
	if (inst_marking && !inst_deletion_track_vars)
		inst_mark_clause(
			clause, SAT_clause_new(clause->nb_lits, lit, SAT_CLAUSE_EXT));
	else
		SAT_clause_new(clause->nb_lits, lit, SAT_CLAUSE_EXT);
	clause_free(clause);
}

/* TODO: IMPROVE bool_add_c vs. bool_add_c_conflict is a dirty quick hack */
void
bool_add_c_conflict(Tclause clause)
{
	unsigned i;
	SAT_Tlit* lit;
	clause = clause_clean(clause);
	if (!clause) return;
#ifdef DEBUG_BOOL
	printf("bool_add_c_conflict : ");
	for (i = 0; i < clause->nb_lits; i++) printf("%d ", clause->lits[i]);
	printf("\n");
#endif
	for (i = 0; i < clause->nb_lits; i++)
		SAT_var_new_id(lit_var(clause->lits[i]));
	MY_MALLOC(lit, clause->nb_lits * sizeof(Tlit));
	/* IMPROVE it is a pity to have to duplicate this information */
	memcpy(lit, clause->lits, clause->nb_lits * sizeof(SAT_Tlit));
	if (proof_on) proof_SAT_insert(clause);
	SAT_clause_new(clause->nb_lits, lit, SAT_CLAUSE_EXT);
	clause_free(clause);
}

/* IMPROVE bool_add_c vs. bool_add_c_hint is a dirty quick hack */
void
bool_add_c_hint(Tclause clause)
{
	unsigned i;
	SAT_Tlit* lit;
	clause = clause_clean(clause);
	if (!clause) return;
#ifdef DEBUG_BOOL
	printf("bool_add_c_hint : ");
	for (i = 0; i < clause->nb_lits; i++) printf("%d ", clause->lits[i]);
	printf("\n");
#endif
	for (i = 0; i < clause->nb_lits; i++)
		SAT_var_new_id(lit_var(clause->lits[i]));
	MY_MALLOC(lit, clause->nb_lits * sizeof(Tlit));
	/* IMPROVE it is a pity to have to duplicate this information */
	memcpy(lit, clause->lits, clause->nb_lits * sizeof(SAT_Tlit));
	if (proof_on) proof_SAT_insert(clause);
	SAT_clause_new_lazy(clause->nb_lits, lit);
	clause_free(clause);
}

void
bool_recheck(
	char* filename, Tstatus status, Tstack_DAG formulas, Tstack_clause clauses)
{
	unsigned i, j, var_max = 0;
	Tstack_clause cnf_clauses;
	FILE* file = fopen(filename, "w");
	cnf_reset();
	stack_INIT(cnf_clauses);
	for (i = 0; i < stack_size(formulas); i++)
		cnf(stack_get(formulas, i), &cnf_clauses);
	for (i = 0; i < stack_size(clauses); i++)
		stack_push(cnf_clauses, clause_dup(stack_get(clauses, i)));
	for (i = 0; i < stack_size(cnf_clauses); i++) {
		Tclause clause = stack_get(cnf_clauses, i);
		for (j = 0; j < clause->nb_lits; j++)
			if ((unsigned)lit_var(clause->lits[j]) > var_max)
				var_max = lit_var(clause->lits[j]);
	}
	fprintf(file, "p cnf %d %u\n", var_max, stack_size(cnf_clauses));
	fprintf(file, "c s %s\n", status == UNSAT ? "UNSATISFIABLE" : "SATISFIABLE");
	for (i = 0; i < stack_size(cnf_clauses); i++) {
		Tclause clause = stack_get(cnf_clauses, i);
		for (j = 0; j < clause->nb_lits; j++)
			fprintf(
				file, "%d ",
				SAT_lit_pol(clause->lits[j]) ? SAT_lit_var(clause->lits[j])
																		 : -SAT_lit_var(clause->lits[j]));
		fprintf(file, "0\n");
	}
	for (i = 0; i < stack_size(cnf_clauses); i++)
		clause_free(stack_get(cnf_clauses, i));
	stack_free(cnf_clauses);
	fprintf(stderr, "File %s written.\n", filename);
	fclose(file);
}

void
bool_reset(void)
{
	bool_model_complete = 0;

	SAT_done();
	SAT_init();
	if (proof_on) proof_SAT_reset();
	cnf_reset();
}

#ifdef STAT_MIN_MODEL
static int
bool_model_sort_literals(const void* Pvoid1, const void* Pvoid2)
{
	const SAT_Tlit *Plit1 = Pvoid1, *Plit2 = Pvoid2;
	if (boolean_connector(DAG_symb(var_to_DAG(lit_var(*Plit2))))) return -1;
	if (boolean_connector(DAG_symb(var_to_DAG(lit_var(*Plit1))))) return 1;
	return 0;
}
#endif

void
bool_stats_model(void)
{
#ifdef STAT_MIN_MODEL
	unsigned i, n = 0, m;
	SAT_Tlit* Plit = NULL;
	if (SAT_status != SAT_STATUS_SAT) return;
	n = SAT_literal_stack_n;
	for (i = 0, m = n; i < n; i++)
		if (boolean_connector(DAG_symb(var_to_DAG(lit_var(SAT_literal_stack[i])))))
			m--;
	stats_counter_set(stat_minimize_lit_origa, n);
	stats_counter_set(stat_minimize_lit_origb, m);
	n = 0;
	stats_timer_start(stat_minimize_time);
	/* Basic minimalization */
	SAT_minimal_model(&Plit, &n, 0);
	for (i = 0, m = n; i < n; i++)
		if (boolean_connector(DAG_symb(var_to_DAG(lit_var(Plit[i]))))) m--;
	stats_counter_set(stat_minimize_lit1a, n);
	stats_counter_set(stat_minimize_lit1b, m);
	n = 0;
	free(Plit);
	Plit = NULL;
	/* Skip propagated */
	SAT_minimal_model(&Plit, &n, SAT_MIN_SKIP_PROPAGATED);
	for (i = 0, m = n; i < n; i++)
		if (boolean_connector(DAG_symb(var_to_DAG(lit_var(Plit[i]))))) m--;
	stats_counter_set(stat_minimize_lit2a, n);
	stats_counter_set(stat_minimize_lit2b, m);
	n = 0;
	free(Plit);
	Plit = NULL;
	/* Use tautologies */
	SAT_minimal_model(&Plit, &n, SAT_MIN_USE_TAUTOLOGIES);
	for (i = 0, m = n; i < n; i++)
		if (boolean_connector(DAG_symb(var_to_DAG(lit_var(Plit[i]))))) m--;
	stats_counter_set(stat_minimize_lit3a, n);
	stats_counter_set(stat_minimize_lit3b, m);
	n = 0;
	free(Plit);
	Plit = NULL;
	/* Remove abstract last */
	n = SAT_literal_stack_n;
	MY_MALLOC(Plit, n * sizeof(SAT_Tlit));
	memcpy(Plit, SAT_literal_stack, n * sizeof(SAT_Tlit));
	/* put abstract variables at the end */
	veriT_qsort(Plit, n, sizeof(SAT_Tlit), bool_model_sort_literals);
	/* check abstract variables are at the end */
	for (i = 0, m = 0; i < n; i++) {
		assert(!m || boolean_connector(DAG_symb(var_to_DAG(lit_var(Plit[i])))));
		if (boolean_connector(DAG_symb(var_to_DAG(lit_var(Plit[i]))))) m = 1;
	}
	SAT_minimal_model(
		&Plit, &n, SAT_MIN_SKIP_PROPAGATED | SAT_MIN_USE_TAUTOLOGIES);
	for (i = 0, m = n; i < n; i++)
		if (boolean_connector(DAG_symb(var_to_DAG(lit_var(Plit[i]))))) m--;
	stats_counter_set(stat_minimize_lit4a, n);
	stats_counter_set(stat_minimize_lit4b, m);
	n = 0;
	free(Plit);
	Plit = NULL;
	stats_timer_stop(stat_minimize_time);
#endif
}

void
bool_init(void)
{
	bool_model_complete = 0;
	SAT_init();
	cnf_init();
#ifdef STAT_MIN_MODEL
	stat_minimize_time = stats_timer_new(
		"SAT_lit_min_time", "Model minimizing time", "%7.2f", STATS_TIMER_ALL);
	stat_minimize_lit_origa = stats_counter_new(
		"SAT_lit_min0a", "N. of lits in unminimized model", "%5d");
	stat_minimize_lit_origb = stats_counter_new(
		"SAT_lit_min0b", "N. of lits in unminimized model", "%5d");
	stat_minimize_lit1a =
		stats_counter_new("SAT_lit_min1a", "N. of lits in minimized model", "%5d");
	stat_minimize_lit1b =
		stats_counter_new("SAT_lit_min1b", "N. of lits in minimized model", "%5d");
	stat_minimize_lit2a =
		stats_counter_new("SAT_lit_min2a", "N. of lits in minimized model", "%5d");
	stat_minimize_lit2b =
		stats_counter_new("SAT_lit_min2b", "N. of lits in minimized model", "%5d");
	stat_minimize_lit3a =
		stats_counter_new("SAT_lit_min3a", "N. of lits in minimized model", "%5d");
	stat_minimize_lit3b =
		stats_counter_new("SAT_lit_min3b", "N. of lits in minimized model", "%5d");
	stat_minimize_lit4a =
		stats_counter_new("SAT_lit_min4a", "N. of lits in minimized model", "%5d");
	stat_minimize_lit4b =
		stats_counter_new("SAT_lit_min4b", "N. of lits in minimized model", "%5d");
#endif
}

void
bool_done(void)
{
	cnf_done();
	SAT_done();
}
