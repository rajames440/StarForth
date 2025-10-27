/*
  --------------------------------------------------------------
  Glibc-like general functions
  --------------------------------------------------------------
*/

#include "utils/general.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
breakpoint(void)
{
	static unsigned n = 0;
	fprintf(stderr, "breakpoint %d\n", n++);
}

void
my_error(char* format, ...)
{
	va_list params;
	va_start(params, format);
	fprintf(stderr, "error : ");
	vfprintf(stderr, format, params);
	va_end(params);
#ifdef DEBUG
	abort();
#else
	exit(EXIT_FAILURE);
#endif
}

void
my_warning(char* format, ...)
{
	va_list params;
	va_start(params, format);
	fprintf(stderr, "warning : ");
	vfprintf(stderr, format, params);
	va_end(params);
}

int SILENT = 0;

void
my_message(char* format, ...)
{
	va_list params;
	if (SILENT) return;
	va_start(params, format);
	fprintf(stderr, "MSG : ");
	vfprintf(stderr, format, params);
	va_end(params);
}

void
my_message_return(void)
{
	if (SILENT) return;
	fprintf(stderr, "\n");
}

char*
strmake(const char* const astr)
{
	char* ptr;
	if (astr == NULL) return NULL;
	MY_MALLOC(ptr, strlen(astr) + 1);
	strcpy(ptr, astr);
	return ptr;
}

char*
strapp(char* dest, size_t* destlen, const char* src)
{
	size_t srclen;
	if (!src) return dest;
	srclen = strlen(src);
	MY_REALLOC(dest, *destlen + srclen + 1);
	strcpy(dest + *destlen, src);
	*destlen += srclen;
	return dest;
}

char*
strmake_printf(char* format, ...)
{
	char str[255];
	va_list params;
	va_start(params, format);
	/*  vsnprintf (str, 255, format, params); */
	vsprintf(str, format, params);
	va_end(params);
	return strmake(str);
}

unsigned
ul_str_size(unsigned long val)
{
	unsigned result;
	result = 1;
	while (val > 9) {
		val /= 10;
		result++;
	}
	return result;
}

unsigned
l_str_size(long val)
{
	unsigned result;
	result = 1;
	while (val > 9) {
		val /= 10;
		result++;
	}
	return result;
}

int
str_nb_words(char* str)
{
	int result = 0;
	while (*str == ' ') str++;
	while (*str != 0) {
		result++;
		while (*str != ' ' && *str != 0) str++;
		while (*str == ' ') str++;
	}
	return result;
}

char**
str_split(char* a_str, const char a_delim)
{
	char** result = 0;
	size_t count = 0;
	char* tmp = a_str;
	char* last_comma = 0;
	char delim[2];
	delim[0] = a_delim;
	delim[1] = 0;

	/* Count how many elements will be extracted. */
	while (*tmp) {
		if (a_delim == *tmp) {
			count++;
			last_comma = tmp;
		}
		tmp++;
	}

	/* Add space for trailing token. */
	count += last_comma < (a_str + strlen(a_str) - 1);

	/* Add space for terminating null string so caller
     knows where the list of returned strings ends. */
	count++;

	result = malloc(sizeof(char*) * count);

	if (result) {
		size_t idx = 0;
		char* token = strtok(a_str, delim);

		while (token) {
			assert(idx < count);
			*(result + idx++) = strdup(token);
			token = strtok(0, delim);
		}
		assert(idx == count - 1);
		*(result + idx) = 0;
	}

	return result;
}
