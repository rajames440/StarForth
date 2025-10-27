#include "utils/nonce.h"

#include "utils/general.h"

#include <stdio.h>
#include <string.h>

void
nonce_init(Tnonce* P, const char* prefix)
{
	P->prefix = strmake(prefix);
	P->sz = ((unsigned)strlen(P->prefix)) + 2;
	P->n = 0;
	P->resize_n = 10;
	MY_MALLOC(P->buffer, sizeof(char) * P->sz);
}

void
nonce_free(Tnonce* P)
{
	free(P->prefix);
	free(P->buffer);
}

void
nonce_next(Tnonce* P)
{
	if (P->n == P->resize_n) {
		P->sz += 1;
		P->resize_n *= 10;
		MY_REALLOC(P->buffer, sizeof(char) * P->sz);
	}
	sprintf(P->buffer, "%s%u", P->prefix, P->n);
	++P->n;
}
