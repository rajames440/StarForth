/*        list structure                                        */

#include "utils/dll.h"

#include "utils/general.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

TSdll* dll_array = NULL;
static unsigned dll_array_size = 0;
static Tdll recycle_bin = DLL_NULL; /*< freed cells, forward chained */

#ifdef DEBUG
#define DEBUG_MEM_LIST
#endif

static inline Tdll
dll_alloc(void)
{
	Tdll dll;
	if (recycle_bin == DLL_NULL) {
		unsigned i;
		dll_array_size <<= 1;
		MY_REALLOC(dll_array, dll_array_size * sizeof(TSdll));
		for (i = dll_array_size >> 1; i < dll_array_size; i++) {
			dll_array[i].data = GEN_NULL;
			dll_array[i].next = i + 1;
			dll_array[i].prev = i;
		}
		dll_array[i - 1].next = DLL_NULL;
		recycle_bin = dll_array_size >> 1;
	}
	dll = recycle_bin;
	recycle_bin = dll_array[recycle_bin].next;
	dll_array[dll].next = dll;
	assert(dll_array[dll].data == GEN_NULL);
	assert(dll_array[dll].prev == dll);
	return dll;
}

static inline void
dll_disalloc(Tdll dll)
{
	dll_array[dll].next = recycle_bin;
	dll_array[dll].prev = dll;
	dll_array[dll].data = GEN_NULL;
	recycle_bin = dll;
}

static inline void
dll_disalloc_all(Tdll dll)
{
	if (!dll) return;
	dll_array[dll_array[dll].prev].next = recycle_bin;
	recycle_bin = dll;
	while (dll_array[dll].prev != dll) {
		Tdll dll2 = dll_array[dll].prev;
		dll_array[dll].prev = dll;
		dll_array[dll].data = GEN_NULL;
		dll = dll2;
	}
}

Tdll
dll_new_args(GEN_TYPE data, ...)
{
	va_list adpar;
	GEN_TYPE tmp;
	Tdll dll = DLL_NULL;
	dll = dll_add(dll, data);
	va_start(adpar, data);
	while ((tmp = va_arg(adpar, GEN_TYPE)) != GEN_NULL) dll = dll_add(dll, tmp);
	return dll;
}

void
dll_free(Tdll* Pdll)
{
	dll_disalloc_all(*Pdll);
	*Pdll = DLL_NULL;
}

unsigned
dll_length(Tdll dll)
{
	Tdll tmp = dll;
	unsigned i = 0;
	if (dll == DLL_NULL) return 0;
	do {
		tmp = dll_array[tmp].next;
		i++;
	} while (tmp != dll);
	return i;
}

Tdll
dll_cpy(Tdll dll)
{
	Tdll tmp = dll, cpy = DLL_NULL;
	if (dll == DLL_NULL) return DLL_NULL;
	do {
		tmp = dll_array[tmp].prev;
		cpy = dll_cons(dll_array[tmp].data, cpy);
	} while (tmp != dll);
	return cpy;
}

Tdll
dll_one(GEN_TYPE data)
{
	Tdll tmp;
	tmp = dll_alloc();
	dll_array[tmp].next = tmp;
	dll_array[tmp].prev = tmp;
	dll_array[tmp].data = data;
	return tmp;
}

Tdll
dll_cons(GEN_TYPE data, Tdll dll)
{
	Tdll tmp;
	tmp = dll_alloc();
	assert(dll_array[tmp].next == tmp);
	assert(dll_array[tmp].prev == tmp);
	dll_array[tmp].data = data;
	if (dll) {
		dll_array[tmp].next = dll;
		dll_array[tmp].prev = dll_array[dll].prev;
		dll_array[dll].prev = tmp;
		dll_array[dll_array[tmp].prev].next = tmp;
	}
	return tmp;
}

Tdll
dll_add(Tdll dll, GEN_TYPE data)
{
	Tdll tmp = dll_alloc();
	assert(dll_array[tmp].next == tmp);
	assert(dll_array[tmp].prev == tmp);
	dll_array[tmp].data = data;
	if (!dll) return tmp;
	dll_array[tmp].next = dll;
	dll_array[tmp].prev = dll_array[dll].prev;
	dll_array[dll].prev = tmp;
	dll_array[dll_array[tmp].prev].next = tmp;
	return dll;
}

Tdll
dll_set_car(Tdll dll, GEN_TYPE data)
{
	dll_array[dll].data = data;
	return dll;
}

GEN_TYPE
dll_n(Tdll dll, unsigned n)
{
	Tdll tmp;
	if (!dll) return GEN_NULL;
	tmp = dll;
	while (n > 0) {
		tmp = dll_cdr(tmp);
		if (tmp == dll) return GEN_NULL;
		n--;
	}
	return dll_car(tmp);
}

Tdll
dll_remove(Tdll dll)
{
	Tdll tmp;
	assert(dll);
	if (dll_array[dll].next == dll) {
		dll_disalloc(dll);
		return DLL_NULL;
	}
	tmp = dll_array[dll].next;
	dll_array[dll_array[dll].prev].next = tmp;
	dll_array[tmp].prev = dll_array[dll].prev;
	dll_disalloc(dll);
	return tmp;
}

Tdll
dll_pop_back(Tdll dll)
{
	Tdll tmp;
	assert(dll);
	if (dll_array[dll].next == dll) {
		dll_disalloc(dll);
		return DLL_NULL;
	}
	tmp = dll_array[dll].prev;
	dll_array[dll_array[tmp].prev].next = dll;
	dll_array[dll].prev = dll_array[tmp].prev;
	dll_disalloc(tmp);
	return dll;
}

Tdll
dll_rev(Tdll dll)
{
	Tdll tmp1, tmp2;
	if (dll == DLL_NULL) return dll;
	tmp1 = dll;
	do {
		tmp2 = dll_array[tmp1].next;
		dll_array[tmp1].next = dll_array[tmp1].prev;
		dll_array[tmp1].prev = tmp2;
		tmp1 = tmp2;
	} while (tmp1 != dll);
	return dll_array[dll].next;
}

Tdll
dll_merge(Tdll dll1, Tdll dll2)
{
	Tdll tmp1, tmp2;
	if (dll2 == DLL_NULL) return dll1;
	if (dll1 == DLL_NULL) return dll2;
	if (dll1 == dll2) return DLL_NULL;
	tmp1 = dll_array[dll1].prev;
	tmp2 = dll_array[dll2].prev;
	dll_array[dll1].prev = tmp2;
	dll_array[dll2].prev = tmp1;
	dll_array[tmp1].next = dll2;
	dll_array[tmp2].next = dll1;
	return dll1;
}

Tdll
dll_anti_merge(Tdll dll1, Tdll dll2)
{
	return dll_merge(dll1, dll2);
}

int
dll_consistency(Tdll dll)
{
	Tdll tmp = dll;
	if (dll == DLL_NULL) return 1;
	do {
		if (dll_prev(dll_cdr(tmp)) != tmp) return 0;
		tmp = dll_cdr(tmp);
	} while (tmp != dll);
	return 1;
}

void
dll_apply(Tdll dll, void (*f)(GEN_TYPE))
{
	Tdll tmp = dll;
	if (dll == DLL_NULL) return;
	do {
		(*f)(dll_car(tmp));
		tmp = dll_cdr(tmp);
	} while (tmp != dll);
}

Tdll
dll_filter(Tdll dll, int (*pred)(GEN_TYPE))
{
	Tdll tmp = dll;
	Tdll result = DLL_NULL;
	if (tmp) do {
			if (pred(dll_car(tmp))) result = dll_add(result, dll_car(tmp));
			tmp = dll_cdr(tmp);
		} while (tmp != dll);
	return result;
}

Tdll
dll_remove_all(Tdll dll, int (*pred)(GEN_TYPE))
{
	Tdll tmp = dll;
	while (tmp && pred(dll_car(tmp))) tmp = dll_remove(tmp);
	if (tmp) {
		Tdll ptr = dll_cdr(tmp);
		while (ptr != tmp)
			if (pred(dll_car(ptr)))
				ptr = dll_remove(ptr);
			else
				ptr = dll_cdr(ptr);
	}
	return tmp;
}

int
dll_same(Tdll dll1, Tdll dll2)
{
	Tdll tmp;
	if (dll1 == DLL_NULL) return (dll2 == DLL_NULL);
	if (dll2 == DLL_NULL) return 0;
	tmp = dll1;
	do {
		if (tmp == dll2) return 1;
		tmp = dll_cdr(tmp);
	} while (tmp != dll1);
	return 0;
}

void
dll_init(void)
{
	unsigned i;
	assert(!dll_array);
	dll_array_size = 1 << 8;
	MY_MALLOC(dll_array, dll_array_size * sizeof(TSdll));
	for (i = 0; i < dll_array_size; i++) {
		dll_array[i].data = GEN_NULL;
		dll_array[i].next = i + 1;
		dll_array[i].prev = i;
	}
	dll_array[i - 1].next = DLL_NULL;
	recycle_bin = 1;
}

void
dll_done(void)
{
	free(dll_array);
	dll_array_size = 0;
	recycle_bin = DLL_NULL;
}
