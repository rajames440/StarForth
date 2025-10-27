/*
  --------------------------------------------------------------
  Property stuff
  --------------------------------------------------------------
*/
#include "symbolic/DAG-prop.h"

#include "symbolic/DAG.h"
#include "utils/general.h"
#include "utils/stack.h"

#include <stdlib.h>
#include <string.h>

Tprop_id DAG_PROP_TRIGGER;
Tprop_id DAG_PROP_NAMED;
Tprop_id DAG_PROP_PNNF;
Tprop_id DAG_PROP_CNF;
Tprop_id DAG_PROP_SYMBS;

typedef struct Tprop_type
{
	TFfree destroy;
	unsigned size;
} Tprop_type;

TSstack(_prop_type, Tprop_type);

/** \brief stores property size and destroy function */
static Tstack_prop_type prop_type_stack;

char** properties;

/*
  --------------------------------------------------------------
  General purpose functions
  --------------------------------------------------------------
*/

Tprop_id
DAG_prop_new(TFfree destroy, unsigned size)
{
	stack_inc(prop_type_stack);
	stack_top(prop_type_stack).destroy = destroy;
	stack_top(prop_type_stack).size = size;
	return stack_size(prop_type_stack) - 1;
}

void
DAG_prop_set(TDAG DAG, Tprop_id pid, void* value)
{
	/* my_DAG_message("prop_set[%d]: got {%d}%D\n", pid, DAG, DAG); */
	unsigned size, size2;
	char* P = properties[DAG];
	if (P) {
		Tprop_id type;
		while ((type = *(Tprop_id*)P))
			if (type == pid) {
				prop_type_stack->data[pid].destroy(P + sizeof(Tprop_id));
				memcpy(P + sizeof(Tprop_id), value, prop_type_stack->data[pid].size);
				return;
			} else
				P += sizeof(Tprop_id) + prop_type_stack->data[type].size;
	}
	size = (unsigned)(P - properties[DAG]);
	size2 =
		size + (unsigned)(2 * sizeof(Tprop_id)) + prop_type_stack->data[pid].size;
#ifdef DMEM
	MY_REALLOC_DMEM(properties[DAG], size2, size);
#else
	MY_REALLOC(properties[DAG], size2);
#endif
	P = properties[DAG] + size;
	*(Tprop_id*)P = pid;
	P += sizeof(Tprop_id);
	memcpy(P, value, prop_type_stack->data[pid].size);
	P += prop_type_stack->data[pid].size;
	*(Tprop_id*)P = 0;
}

bool
DAG_prop_remove(TDAG DAG, Tprop_id pid)
{
	unsigned size = 0, size2;
	char* P = properties[DAG];
	Tprop_id type;
	if (!P) return false;
	while ((type = *(Tprop_id*)P)) {
		if (type == pid) size = (unsigned)(P - properties[DAG]);
		P += sizeof(Tprop_id) + prop_type_stack->data[type].size;
	}
	if (!*(Tprop_id*)(properties[DAG] + size)) return false;
	size2 =
		(unsigned)(P - properties[DAG]) - size - prop_type_stack->data[pid].size;
	memmove(
		properties[DAG] + size,
		properties[DAG] + size + sizeof(Tprop_id) + prop_type_stack->data[pid].size,
		size2);
	return true;
}

void*
DAG_prop_get(TDAG DAG, Tprop_id pid)
{
	char* tmp = properties[DAG];
	Tprop_id type;
	if (!tmp) return NULL;
	while ((type = *(Tprop_id*)tmp))
		if (type == pid)
			return tmp + sizeof(Tprop_id);
		else
			tmp += sizeof(Tprop_id) + prop_type_stack->data[type].size;
	return NULL;
}

bool
DAG_prop_check(TDAG DAG, Tprop_id pid)
{
	char* tmp = properties[DAG];
	Tprop_id type;
	if (!tmp) return false;
	while ((type = *(Tprop_id*)tmp))
		if (type == pid)
			return true;
		else
			tmp += sizeof(Tprop_id) + prop_type_stack->data[type].size;
	return false;
}

static void
DAG_prop_trigger_free(Tstack_DAGstack* Pannot)
{
	unsigned i, j;
	for (i = 0; i < stack_size(*Pannot); ++i) {
		Tstack_DAG trigger = stack_get(*Pannot, i);
		for (j = 0; j < stack_size(trigger); ++j) DAG_free(stack_get(trigger, j));
		stack_free(trigger);
	}
	stack_free(*Pannot);
}

static void
DAG_prop_name_free(char** str)
{
	free(*str);
}

static void
DAG_prop_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	unsigned i;
	MY_REALLOC(properties, new_alloc * sizeof(char*));
	for (i = old_alloc; i < new_alloc; i++) properties[i] = NULL;
}

static void
DAG_prop_hook_free(TDAG DAG)
{
	/* my_DAG_message("prop_hook_free: got %d\n", DAG); */
	char* tmp = properties[DAG];
	Tprop_id type;
	if (!tmp) return;
	while ((type = *(Tprop_id*)tmp)) {
		*(Tprop_id*)tmp = 0;
		tmp += sizeof(Tprop_id);
		if (prop_type_stack->data[type].destroy)
			prop_type_stack->data[type].destroy(tmp);
		tmp += prop_type_stack->data[type].size;
	}
	free(properties[DAG]);
	properties[DAG] = NULL;
}

void
DAG_prop_init(void)
{
	stack_INIT(prop_type_stack);
	/* PF reserve 0 */
	stack_inc(prop_type_stack);

	DAG_PROP_TRIGGER =
		DAG_prop_new((TFfree)DAG_prop_trigger_free, sizeof(Tstack_DAGstack));
	DAG_PROP_NAMED = DAG_prop_new((TFfree)DAG_prop_name_free, sizeof(char*));

	DAG_set_hook_resize(DAG_prop_hook_resize);
	DAG_set_hook_free(DAG_prop_hook_free);
}

void
DAG_prop_done(void)
{
	stack_free(prop_type_stack);
}
