#include "arith/totality.h"

#include "bool/bool.h"
#include "proof/proof.h"
#include "utils/statistics.h"

/* ------------------------------------------------------------------------- */

/* #define DEBUG_TOTALITY */

#ifdef DEBUG_TOTALITY
#include "symbolic/DAG-print.h"

#include <stdlib.h>
#endif

/* ------------------------------------------------------------------------- */

static Tstack_DAG totality_DAG; /*< equalities between arithmetic terms */
static Tvar* totality_atoms;
static size_t totality_atoms_n;
static unsigned stats_totality_lemmas;

/* ------------------------------------------------------------------------- */

void
totality_init(void)
{
	stack_INIT(totality_DAG);
	totality_atoms_n = 16;
	MY_MALLOC(totality_atoms, totality_atoms_n * sizeof(Tvar));
	stats_totality_lemmas =
		stats_counter_new("lemmas/totality", "Totality lemmas produced", "%9d");
}

/* ------------------------------------------------------------------------- */

void
totality_done(void)
{
	size_t i;
	for (i = 0; i < stack_size(totality_DAG); ++i)
		DAG_free(totality_DAG->data[i]);
	stack_free(totality_DAG);
	free(totality_atoms);
	totality_atoms_n = 0;
}

/* ------------------------------------------------------------------------- */
extern Tstack_DAG veriT_lemmas;

void
totality_register(const TDAG DAG)
{
#ifdef DEBUG_TOTALITY
	my_DAG_message("totality_register %D\n", DAG);
#endif
	proof_set_lemma_id(DAG, proof_add_totality_lemma(DAG_dup(DAG)));
	stack_push(veriT_lemmas, DAG_dup(DAG));
	stats_counter_inc(stats_totality_lemmas);
}

/* ------------------------------------------------------------------------- */

/* DAG has the form t1 <= t2 or t2 <= t1 */
void
totality_process(TDAG DAG)
{
	Tvar var0;
	assert(DAG_arity(DAG) == 2);
	var0 = DAG_to_var(DAG_arg0(DAG));
	assert(var0 != 0);
#ifdef DEBUG_TOTALITY
	my_DAG_message("totality_process lemma %D\n", DAG);
#endif
	if (totality_atoms[var0] == 0) {
		Tvar var1 = DAG_to_var(DAG_arg1(DAG));
		size_t max = var0 >= var1 ? var0 : var1;
		assert(var1 != 0);
		if (max >= totality_atoms_n) {
			do
				totality_atoms_n *= 2;
			while (max >= totality_atoms_n);
			MY_REALLOC(totality_atoms, totality_atoms_n * sizeof(Tvar));
		}
		totality_atoms[var0] = var1;
		totality_atoms[var1] = var0;
	}
}

/* ------------------------------------------------------------------------- */

bool
totality_check(Tlit lit1, Tlit lit2)
{
	Tvar var1 = lit_var(lit1);
	return var1 < totality_atoms_n && totality_atoms[var1] == lit_var(lit2);
}
