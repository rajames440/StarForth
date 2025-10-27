/*
  --------------------------------------------------------------
  Clause Module
  --------------------------------------------------------------
*/
#include "bool/bool.h"
#include "utils/general.h"
#include "utils/veriT-qsort.h"
#include "veriT-config.h"

Tclause
clause_new(unsigned nb_lits)
{
	Tclause clause;
	MY_MALLOC(clause, sizeof(struct TSclause));
	clause->nb_lits = nb_lits;
	clause->proof_id = 0;
	MY_MALLOC(clause->lits, nb_lits * sizeof(Tlit));
	return clause;
}

Tclause
clause_new_stack(Tstack_lit lits)
{
	Tclause clause;
	MY_MALLOC(clause, sizeof(struct TSclause));
	clause->nb_lits = stack_size(lits);
	clause->proof_id = 0;
	MY_MALLOC(clause->lits, clause->nb_lits * sizeof(Tlit));
	memcpy(clause->lits, lits->data, clause->nb_lits * sizeof(Tlit));
	return clause;
}

Tclause
clause_dup(Tclause clause)
{
	unsigned i;
	Tclause clause2;
	MY_MALLOC(clause2, sizeof(struct TSclause));
	clause2->nb_lits = clause->nb_lits;
	clause2->proof_id = clause->proof_id;
	MY_MALLOC(clause2->lits, clause->nb_lits * sizeof(Tlit));
	for (i = 0; i < clause->nb_lits; i++) clause2->lits[i] = clause->lits[i];
	return clause2;
}

void
clause_set_literal(Tclause clause, unsigned i, Tlit lit)
{
	assert(i < clause->nb_lits);
	clause->lits[i] = lit;
}

void
clause_add_literal(Tclause clause, Tlit lit)
{
	MY_REALLOC(clause->lits, (clause->nb_lits + 1) * sizeof(Tlit));
	clause->lits[clause->nb_lits++] = lit;
}

static int
compar_literal(const Tlit* lit1, const Tlit* lit2)
{
	return (int)lit_var(*lit1) - (int)lit_var(*lit2);
}

/* PF: IMPROVE.  I think this is duplicated work compared to veriT_SAT */
Tclause
clause_clean(Tclause clause)
/* PF removes repeated literals
   if valid returns NULL
   The problem is linear (see veriT-sat), but implementation is n ln n */
{
	unsigned i;
	unsigned j;
	/* PF Empty clause */
	if (!clause->nb_lits) return clause;
	veriT_qsort(
		clause->lits, clause->nb_lits, sizeof(Tlit), (TFcmp)compar_literal);
	for (i = 0; i < clause->nb_lits - 1; i++)
		if (lit_neg(clause->lits[i]) == clause->lits[i + 1]) {
			/* PF Valid clause */
			clause_free(clause);
			return NULL;
		}
	for (i = 1, j = 0; i < clause->nb_lits; i++)
		if (clause->lits[i] != clause->lits[j]) clause->lits[++j] = clause->lits[i];
	clause->nb_lits = j + 1;
	MY_REALLOC(clause->lits, clause->nb_lits * sizeof(Tlit));
	return clause;
}

Tclause
clause_merge(Tclause clause1, Tclause clause2)
/* PF builds a new clause that is the disjunction of both input
   The problem is linear (see veriT-sat) */
{
	int tmp;
	unsigned i1 = 0, i2 = 0, i = 0;
	Tclause clause;

	if (!clause1 || !clause2) return NULL;

	clause = clause_new(clause1->nb_lits + clause2->nb_lits);
	while (i1 < clause1->nb_lits && i2 < clause2->nb_lits) {
		tmp = compar_literal(&(clause1->lits[i1]), &(clause2->lits[i2]));
		if (tmp < 0)
			clause->lits[i++] = clause1->lits[i1++];
		else if (tmp > 0)
			clause->lits[i++] = clause2->lits[i2++];
		else {
			if (clause1->lits[i1] != clause2->lits[i2]) {
				/* PF Valid clause */
				clause_free(clause);
				return NULL;
			}
			clause->lits[i++] = clause1->lits[i1];
			i1++;
			i2++;
		}
	}
	while (i1 < clause1->nb_lits) clause->lits[i++] = clause1->lits[i1++];
	while (i2 < clause2->nb_lits) clause->lits[i++] = clause2->lits[i2++];
	clause->nb_lits = i;
	MY_REALLOC(clause->lits, clause->nb_lits * sizeof(Tlit));
	return clause;
}

bool
clause_same(Tclause clause1, Tclause clause2)
{
	unsigned i;
	if (clause1->nb_lits != clause2->nb_lits) return false;
	for (i = 0; i < clause1->nb_lits; i++)
		if (clause1->lits[i] != clause2->lits[i]) return false;
	return true;
}

/* PF IMPROVE.  I think most of the time, lits are duplicated, given
   to veriT_SAT, and freed here;  could be more efficient to just
   transfer lits to veriT_SAT */
void
clause_free(Tclause clause)
{
	if (!clause) return;
	free(clause->lits);
	free(clause);
}

void
clause_fprint(FILE* file, Tclause clause)
{
	unsigned i;
	if (!clause)
		fprintf(file, "NULL clause");
	else if (clause->nb_lits == 0)
		fprintf(file, "Empty clause");
	else
		for (i = 0; i < clause->nb_lits; i++) fprintf(file, " %d", clause->lits[i]);
	fprintf(file, "\n");
}
