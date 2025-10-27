#include "utils/h-util.h"

#include "veriT-config.h"

unsigned
hash_one_at_a_time(char* str)
{
	unsigned hash;
	for (hash = 0; *str; str++) {
		hash += (unsigned)*str;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}

unsigned
hash_one_at_a_time_u(unsigned u)
{
	unsigned hash;
	hash = u & 0xFF;
	hash += (hash << 10);
	hash ^= (hash >> 6);
	hash += (u & 0xFF00) >> 8;
	hash += (hash << 10);
	hash ^= (hash >> 6);
	hash += (u & 0xFF0000) >> 16;
	hash += (hash << 10);
	hash ^= (hash >> 6);
	hash += (u & 0xFF000000) >> 24;
	hash += (hash << 10);
	hash ^= (hash >> 6);
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}

unsigned
hash_one_at_a_time_inc(unsigned hash, char* str)
{
	for (; *str; str++) {
		hash += (unsigned)*str;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	return hash;
}

#ifndef HASH_MACRO
unsigned
hash_one_at_a_time_u_inc(unsigned hash, unsigned u)
{
	hash += u & 0xFF;
	hash += (hash << 10);
	hash ^= (hash >> 6);
	hash += (u & 0xFF00) >> 8;
	hash += (hash << 10);
	hash ^= (hash >> 6);
	hash += (u & 0xFF0000) >> 16;
	hash += (hash << 10);
	hash ^= (hash >> 6);
	hash += (u & 0xFF000000) >> 24;
	hash += (hash << 10);
	hash ^= (hash >> 6);
	return hash;
}
#endif

unsigned
hash_one_at_a_time_inc_end(unsigned hash)
{
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}
