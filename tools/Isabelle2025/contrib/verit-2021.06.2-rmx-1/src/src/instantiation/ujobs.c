#include "instantiation/ujobs.h"

#include "limits.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG.h"

/*
  TODO: There must be a better index for the job, so it can be accessed more
  quickly. An identifier derived from the DAGs being unified?

  TODO: Use fingerprints? <fx,ft> and <fy,ft> should be seen as the same
  ujobs.

  TODO: This should probably be done in a similar way as E-matching Code
  Trees are. It would allow incrementality and backtracking.

  TODO: Since Tunifiers are not indexed, there will be a lot of duplicated
  info. How to index them?
*/

/**
   \brief assuming a fixed external DAG and polarity, stores all unifiers
   resulting from unification with DAG */
typedef struct Ujob
{
	TDAG DAG; /*< DAG being unified with in the ujob */
	Tstack_unifier result; /*< computed unifiers */
} Ujob;

TSstack(_Ujob, Ujob); /* typedefs Tstack_Ujob */

/**
   \brief each DAG may be associated with two ujob stacks, one for unification
   in equalities and another in disequalities */
typedef Tstack_Ujob Ujobs_DAG[2];

/*
  --------------------------------------------------------------
  Global state
  --------------------------------------------------------------
*/

/**
   \brief index of ujobs by the nonground DAGs */
Ujobs_DAG* ujobs_index = NULL;

/**
   \brief set of nonground DAGs that had ujobs indexed in a given instantiation
   cycle */
Tstack_DAG ujobs_DAGs; /* TODO: Do away with this workaround? */

int
ujob_cmp_q(Ujob* Pujob1, Ujob* Pujob2)
{
	return (int)Pujob1->DAG - (int)Pujob2->DAG;
}

#define UJOB_INDEX_NOT_FOUND UINT_MAX

/**
   \brief get index of DAG's ujob in set of ujobs
   \param ujobs a set of ujobs
   \param DAG a DAG
   \return the DAG's ujob index or a value marking it not being found */
static unsigned
find_ujob(Tstack_Ujob ujobs, TDAG DAG)
{
	int imid, imin = 0, imax = stack_size(ujobs) - 1;
	while (imin <= imax) {
		imid = imin + (imax - imin) / 2;
		if (DAG == stack_get(ujobs, imid).DAG) return imid;
		if (DAG < stack_get(ujobs, imid).DAG)
			imax = imid - 1;
		else if (DAG > stack_get(ujobs, imid).DAG)
			imin = imid + 1;
	}
	return UJOB_INDEX_NOT_FOUND;
}

void
set_ujob(TDAG NGDAG, TDAG UDAG, bool pol, Tstack_unifier result)
{
	unsigned i;
	Ujob ujob;
	if (!ujobs_index[NGDAG][pol]) {
		stack_INIT(ujobs_index[NGDAG][pol]);
		/* TODO: This may push the same DAG twice? */
		stack_push(ujobs_DAGs, NGDAG);
	}
	assert(find_ujob(ujobs_index[NGDAG][pol], UDAG) == UJOB_INDEX_NOT_FOUND);
	ujob.DAG = UDAG;
	stack_INIT(ujob.result);
	if (result)
		for (i = 0; i < stack_size(result); ++i)
			stack_push(ujob.result, unify_copy(stack_get(result, i)));
	stack_push(ujobs_index[NGDAG][pol], ujob);
	stack_sort(ujobs_index[NGDAG][pol], ujob_cmp_q);
}

bool
retrieve_ujob(
	Tstack_unifier* result, TDAG NGDAG, TDAG UDAG, bool pol, Tstack_DAG new_vars)
{
	unsigned i;
	if (!ujobs_index[NGDAG][pol]) return false;
	i = find_ujob(ujobs_index[NGDAG][pol], UDAG);
	if (i == UJOB_INDEX_NOT_FOUND) return false;
	*result = unify_reset(stack_get(ujobs_index[NGDAG][pol], i).result, new_vars);
	return true;
}

void
ujobs_index_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	unsigned i;
	MY_REALLOC(ujobs_index, new_alloc * sizeof(Ujobs_DAG));
	for (i = old_alloc; i < new_alloc; ++i) {
		ujobs_index[i][0] = NULL;
		ujobs_index[i][1] = NULL;
	}
}

void
ujobs_done_cycle(void)
{
	unsigned i, j, k;
	Ujob unifying_job;
	Tstack_Ujob unifying_jobs;
	stack_sort(ujobs_DAGs, DAG_cmp_q);
	stack_uniq(ujobs_DAGs);
	for (i = 0; i < stack_size(ujobs_DAGs); ++i)
		for (j = 0; j < 2; ++j)
			if (ujobs_index[stack_get(ujobs_DAGs, i)][j]) {
				unifying_jobs = ujobs_index[stack_get(ujobs_DAGs, i)][j];
				if (!unifying_jobs) continue;
				for (k = 0; k < stack_size(unifying_jobs); ++k) {
					unifying_job = stack_get(unifying_jobs, k);
					if (!unifying_job.result) continue;
					stack_apply(unifying_job.result, unify_free);
					stack_free(unifying_job.result);
				}
				stack_free(unifying_jobs);
				ujobs_index[stack_get(ujobs_DAGs, i)][j] = NULL;
			}
	stack_reset(ujobs_DAGs);
}

void
ujobs_init(void)
{
	stack_INIT(ujobs_DAGs);
	DAG_set_hook_resize(ujobs_index_hook_resize);
}

void
ujobs_done(void)
{
	stack_free(ujobs_DAGs);
}

/*
  --------------------------------------------------------------
  Printing stuff
  --------------------------------------------------------------
*/

static void
print_ujobs_index(void)
{
	unsigned i, j, k, l;
	stack_sort(ujobs_DAGs, DAG_cmp_q);
	stack_uniq(ujobs_DAGs);
	for (i = 0; i < stack_size(ujobs_DAGs); ++i) {
		TDAG DAG = stack_get(ujobs_DAGs, i);
		my_DAG_message("[%d/%D]:\n", DAG, DAG);
		for (j = 0; j < 2; ++j)
			if (ujobs_index[DAG][j])
				for (k = 0; k < stack_size(ujobs_index[DAG][j]); ++k) {
					Ujob index = stack_get(ujobs_index[DAG][j], k);
					my_DAG_message(
						"\t[%d/%D]{%s}:\n", index.DAG, index.DAG, j ? "1" : "0");
					for (l = 0; l < stack_size(index.result); ++l)
						unify_print(stack_get(index.result, l));
				}
	}
}

static void
print_ujobs_index_DAG(TDAG DAG)
{
	unsigned i, j, k;
	my_DAG_message("[%d/%D]:\n", DAG, DAG);
	for (i = 0; i < 2; ++i)
		if (ujobs_index[DAG][i])
			for (j = 0; j < stack_size(ujobs_index[DAG][i]); ++j) {
				Ujob index = stack_get(ujobs_index[DAG][i], j);
				my_DAG_message(
					"\t[%d/%D]{%s}:\n\n", index.DAG, index.DAG, i ? "1" : "0");
				for (k = 0; k < stack_size(index.result); ++k)
					unify_print(stack_get(index.result, k));
			}
}
