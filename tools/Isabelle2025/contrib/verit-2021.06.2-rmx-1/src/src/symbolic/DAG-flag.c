/*
  --------------------------------------------------------------
  Obsolete functions about DAG and symbols
  --------------------------------------------------------------
*/

#include "symbolic/DAG-flag.h"

#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG.h"
#include "utils/general.h"

#include <string.h>

int* obsolete_DAG_flag = NULL;
void** obsolete_DAG_Pflag = NULL;

static unsigned size = 0;

void
DAG_free_Pflag(TDAG src)
{
	unsigned i;
	if (!DAG_Pflag(src)) return;
	for (i = 0; i < DAG_arity(src); i++) DAG_free_Pflag(DAG_arg(src, i));
	DAG_free(DAG_of_ptr(DAG_Pflag(src)));
	DAG_Pflag_set(src, NULL);
}

void
DAG_reset_flag(TDAG src)
{
	unsigned i;
	if (!DAG_flag(src)) return;
	for (i = 0; i < DAG_arity(src); i++) DAG_reset_flag(DAG_arg(src, i));
	DAG_flag_set(src, 0);
}

void
DAG_reset_Pflag(TDAG src)
{
	unsigned i;
	if (!DAG_Pflag(src)) return;
	for (i = 0; i < DAG_arity(src); i++) DAG_reset_Pflag(DAG_arg(src, i));
	DAG_Pflag_set(src, NULL);
}

void
DAG_check_Pflag(void)
{
	unsigned i;
	for (i = 0; i < size; i++) assert(!DAG_Pflag(i));
}

static void
DAG_flag_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	assert(size == old_alloc);
#ifndef DMEM
	MY_REALLOC(obsolete_DAG_flag, new_alloc * sizeof(int));
	MY_REALLOC(obsolete_DAG_Pflag, new_alloc * sizeof(void*));
#else
	MY_REALLOC_DMEM(
		obsolete_DAG_flag, new_alloc * sizeof(int), old_alloc * sizeof(int));
	MY_REALLOC_DMEM(
		obsolete_DAG_Pflag, new_alloc * sizeof(void*), old_alloc * sizeof(void*));
#endif
	if (new_alloc > old_alloc) {
		memset(
			obsolete_DAG_flag + old_alloc, 0, (new_alloc - old_alloc) * sizeof(int));
		memset(
			obsolete_DAG_Pflag + old_alloc, 0,
			(new_alloc - old_alloc) * sizeof(void*));
	}
	size = new_alloc;
}

void
DAG_flag_init(void)
{
	DAG_set_hook_resize(DAG_flag_hook_resize);
}

void
DAG_flag_done(void)
{}
