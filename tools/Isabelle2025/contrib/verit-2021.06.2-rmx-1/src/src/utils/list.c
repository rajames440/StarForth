/*        list structure                                        */

#include "utils/list.h"

#include "utils/general.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static Tlist recycle_bin = NULL; /*< freed cells, forward chained */

#ifdef DEBUG
#define DEBUG_MEM_LIST
#endif

static inline Tlist
list_allocate(void)
{
	Tlist list;
#ifdef DEBUG_LIST
	MY_MALLOC(list, sizeof(struct TSlist));
#else
	if (recycle_bin != NULL) {
		list = recycle_bin;
		recycle_bin = recycle_bin->next;
	} else
		MY_MALLOC(list, sizeof(struct TSlist));
	list->P = NULL;
	list->next = list;
	list->prev = list;
#endif
	return list;
}

static inline void
list_disallocate(Tlist list)
{
#ifdef DEBUG_MEM_LIST
	free(list);
#else
	list->next = recycle_bin;
	recycle_bin = list;
#endif
}

static inline void
list_disallocate_all(Tlist list)
{
#ifdef DEBUG_MEM_LIST
	Tlist tmp = list;
	if (!list) return;
	do {
		Tlist tmp2 = list_cdr(tmp);
		free(tmp);
		tmp = tmp2;
	} while (tmp != list);
#else
	list->prev->next = recycle_bin;
	recycle_bin = list;
#endif
}

static inline Tlist
remove_element(Tlist list, Tlist elmt)
{
	if (elmt->next == elmt) {
		list_disallocate(elmt);
		return NULL;
	}
	elmt->next->prev = elmt->prev;
	elmt->prev->next = elmt->next;
	if (elmt == list) list = elmt->next;
	list_disallocate(elmt);
	return list;
}

Tlist
list_new_args(void* P, ...)
{
	va_list adpar;
	void* tmp;
	Tlist list = NULL;
	list = list_add(list, P);
	va_start(adpar, P);
	while ((tmp = va_arg(adpar, void*)) != NULL) { list = list_add(list, tmp); }
	return list;
}

void
list_free(Tlist* Plist)
{
	if (*Plist == NULL) return;
	list_disallocate_all(*Plist);
	*Plist = NULL;
}

unsigned
list_length(Tlist list)
{
	Tlist tmp = list;
	unsigned i = 0;
	if (list == NULL) return 0;
	do {
		tmp = tmp->next;
		i++;
	} while (tmp != list);
	return i;
}

Tlist
list_cpy(Tlist list)
{
	Tlist tmp = list, cpy = NULL;
	if (list == NULL) return NULL;
	do {
		cpy = list_cons(tmp->P, cpy);
		tmp = tmp->next;
	} while (tmp != list);
	return cpy;
}

Tlist
list_cpy2(Tlist list)
{
	Tlist tmp = list, cpy = NULL;
	if (list == NULL) return NULL;
	do {
		tmp = tmp->prev;
		cpy = list_cons(tmp->P, cpy);
	} while (tmp != list);
	return cpy;
}

Tlist
list_one(void* P)
{
	Tlist tmp;
	tmp = list_allocate();
	tmp->next = tmp;
	tmp->prev = tmp;
	tmp->P = P;
	return tmp;
}

Tlist
list_cons(void* P, Tlist list)
{
	Tlist tmp;
	tmp = list_allocate();
	if (list) {
		tmp->next = list;
		tmp->prev = list->prev;
		list->prev = tmp;
		tmp->prev->next = tmp;
	} else {
		tmp->next = tmp;
		tmp->prev = tmp;
	}
	tmp->P = P;
	return tmp;
}

Tlist
list_add(Tlist list, void* P)
{
	Tlist tmp = list_allocate();
	if (list) {
		tmp->next = list;
		tmp->prev = list->prev;
		list->prev = tmp;
		tmp->prev->next = tmp;
	} else {
		tmp->next = tmp;
		tmp->prev = tmp;
		list = tmp;
	}
	tmp->P = P;
	return list;
}

Tlist
list_set_car(Tlist list, void* P)
{
	list->P = P;
	return list;
}

void*
list_n(Tlist list, int n)
{
	Tlist tmp;
	assert(n >= 0);
	if (!list) return NULL;
	tmp = list;
	while (n > 0) {
		tmp = list_cdr(tmp);
		if (tmp == list) return NULL;
		n--;
	}
	return list_car(tmp);
}

Tlist
list_remove(Tlist list)
{
	return remove_element(list, list);
}

Tlist
list_pop_back(Tlist list)
{
	return remove_element(list, list->prev);
}

Tlist
list_rev(Tlist list)
{
	Tlist tmp1, tmp2;
	if (list == NULL) return list;
	tmp1 = list;
	do {
		tmp2 = tmp1->next;
		tmp1->next = tmp1->prev;
		tmp1->prev = tmp2;
		tmp1 = tmp1->prev;
	} while (tmp1 != list);
	return list->next;
}

Tlist
list_merge(Tlist list1, Tlist list2)
{
	Tlist tmp1, tmp2;
	if (list2 == NULL) return list1;
	if (list1 == NULL) return list2;
	if (list1 == list2) return NULL;
	tmp1 = list1->prev;
	tmp2 = list2->prev;
	list1->prev = tmp2;
	list2->prev = tmp1;
	tmp1->next = list2;
	tmp2->next = list1;
	return list1;
}

Tlist
list_anti_merge(Tlist list1, Tlist list2)
{
	return list_merge(list1, list2);
}

int
list_consistency(Tlist list)
{
	Tlist tmp;
	if (list == NULL) return 1;
	tmp = list;
	do {
		if (tmp->next->prev != tmp) return 0;
		tmp = tmp->next;
	} while (tmp != list);
	return 1;
}

void
list_apply(Tlist list, void (*f)(void*))
{
	Tlist tmp;
	if (list == NULL) return;
	tmp = list;
	do {
		(*f)(tmp->P);
		tmp = tmp->next;
	} while (tmp != list);
}

Tlist
list_filter(Tlist list, TFtest test)
{
	Tlist hd = list;
	Tlist result = NULL;
	if (hd) do {
			if (test(list_car(hd))) result = list_add(result, list_car(hd));
			hd = list_cdr(hd);
		} while (hd != list);
	return result;
}

Tlist
list_remove_all(Tlist list, TFtest test)
{
	Tlist hd = list;
	while (hd && (*test)(list_car(hd))) hd = list_remove(hd);
	if (hd) {
		Tlist ptr = list_cdr(hd);
		while (ptr != hd) {
			if ((*test)(list_car(ptr)))
				ptr = list_remove(ptr);
			else
				ptr = list_cdr(ptr);
		}
	}
	return hd;
}

int
list_same(Tlist list1, Tlist list2)
{
	Tlist tmp;
	if (list1 == NULL) return (list2 == NULL);
	if (list2 == NULL) return 0;
	tmp = list1;
	do {
		if (tmp == list2) return 1;
		tmp = tmp->next;
	} while (tmp != list1);
	return 0;
}

void
list_init(void)
{}

void
list_done(void)
{
	while (recycle_bin) {
		Tlist list = recycle_bin->next;
		free(recycle_bin);
		recycle_bin = list;
	}
}
