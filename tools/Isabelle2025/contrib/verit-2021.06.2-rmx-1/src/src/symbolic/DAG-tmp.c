#include "symbolic/DAG.h"
#include "utils/general.h"

#include <string.h>
#ifdef DEBUG
#include "symbolic/DAG-print.h"
#endif

#include "symbolic/DAG-tmp.h"

char* DAG_tmp = NULL;

#define SIZE1 (sizeof(void*))
#define SIZE2 (sizeof(TDAG) + sizeof(unsigned))
#define DAG_MAX_SIZE (SIZE1 <= SIZE2 ? SIZE2 : SIZE1)

#ifdef DEBUG

static unsigned DAG_size = 0;
static unsigned reserved = 0;

void
DAG_tmp_reserve(void)
{
	assert(!reserved);
	reserved = 1;
	MY_MALLOC(DAG_tmp, DAG_size * DAG_MAX_SIZE);
	memset(DAG_tmp, 0, DAG_size * DAG_MAX_SIZE);
}

void
DAG_tmp_release(void)
{
	unsigned i;
	assert(reserved);
	for (i = 0; i < DAG_size * DAG_MAX_SIZE; i++) assert(!DAG_tmp[i]);
	reserved = 0;
	free(DAG_tmp);
	DAG_tmp = NULL;
}

#endif

static void
DAG_tmp_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
#ifdef DEBUG
	DAG_size = new_alloc;
	if (!reserved) return;
#endif
#ifdef DMEM
	MY_REALLOC_DMEM(DAG_tmp, new_alloc * DAG_MAX_SIZE, old_alloc * DAG_MAX_SIZE);
#else
	MY_REALLOC(DAG_tmp, new_alloc * DAG_MAX_SIZE);
#endif
	if (new_alloc > old_alloc)
		memset(
			DAG_tmp + old_alloc * DAG_MAX_SIZE, 0,
			(new_alloc - old_alloc) * DAG_MAX_SIZE);
}

void
DAG_tmp_reset_bool(TDAG DAG)
{
	unsigned i;
	if (!DAG_tmp_bool[DAG]) return;
	DAG_tmp_bool[DAG] = 0;
	for (i = 0; i < DAG_arity(DAG); i++) DAG_tmp_reset_bool(DAG_arg(DAG, i));
}

void
DAG_tmp_reset_DAG(TDAG DAG)
{
	unsigned i;
	if (!DAG_tmp_DAG[DAG]) return;
	DAG_tmp_DAG[DAG] = DAG_NULL;
	for (i = 0; i < DAG_arity(DAG); i++) DAG_tmp_reset_DAG(DAG_arg(DAG, i));
}

void
DAG_tmp_free_DAG(TDAG DAG)
{
	unsigned i;
	if (!DAG_tmp_DAG[DAG]) return;
	for (i = 0; i < DAG_arity(DAG); i++) DAG_tmp_free_DAG(DAG_arg(DAG, i));
	DAG_free(DAG_tmp_DAG[DAG]);
	DAG_tmp_DAG[DAG] = DAG_NULL;
}

void
DAG_tmp_reset_unsigned(TDAG DAG)
{
	unsigned i;
	if (!DAG_tmp_unsigned[DAG]) return;
	DAG_tmp_unsigned[DAG] = 0;
	for (i = 0; i < DAG_arity(DAG); i++) DAG_tmp_reset_unsigned(DAG_arg(DAG, i));
}

void
DAG_tmp_reset_int(TDAG DAG)
{
	unsigned i;
	if (!DAG_tmp_int[DAG]) return;
	DAG_tmp_int[DAG] = 0;
	for (i = 0; i < DAG_arity(DAG); i++) DAG_tmp_reset_int(DAG_arg(DAG, i));
}

void
DAG_tmp_reset_stack_DAG(TDAG DAG)
{
	unsigned i;
	if (!DAG_tmp_stack_DAG[DAG]) return;
	stack_free(DAG_tmp_stack_DAG[DAG]);
	for (i = 0; i < DAG_arity(DAG); i++) DAG_tmp_reset_stack_DAG(DAG_arg(DAG, i));
}

/*
  --------------------------------------------------------------
  Initialization-release functions
  --------------------------------------------------------------
*/

void
DAG_tmp_init(void)
{
	DAG_set_hook_resize(DAG_tmp_hook_resize);
}

void
DAG_tmp_done(void)
{}

#ifdef DEBUG
void
DAG_tmp_bool_debug(void)
{
	unsigned i;
	for (i = 0; i < DAG_size; i++)
		if (DAG_tmp_bool[i]) my_DAG_message("1: %D\n", i);
	for (i = 0; i < DAG_size; i++)
		if (!DAG_tmp_bool[i]) my_DAG_message("0: %D\n", i);
}

void
DAG_tmp_DAG_debug(void)
{
	unsigned i;
	for (i = 0; i < DAG_size; i++) {
		if (DAG_tmp_DAG[i] != DAG_NULL)
			my_DAG_message("%D -> %D\n", i, DAG_tmp_DAG[i]);
	}
}
#endif
