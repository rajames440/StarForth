/*
  --------------------------------------------------------------
  Glibc-like general functions
  --------------------------------------------------------------
*/

#ifndef __GENERAL_H
#define __GENERAL_H

#ifdef DEBUG
#define SAFE_MALLOC
#endif

#include <stdlib.h>
#ifdef SAFE_MALLOC
#include <string.h>
#endif

/* PF use MY_MALLOC and MY_REALLOC with caution
   In particular, make sure it is not cut (if ( ) then MY_MALLOC; */

#ifdef DMEM
#define MY_MALLOC(v, s) \
	do { \
		v = malloc(s); \
		if ((s != 0) && !v) \
			my_error("malloc error on line %d in file " __FILE__ "\n", __LINE__); \
		memset(v, 0xFF, s); \
	} while (0)
#define MY_REALLOC_DMEM(v, s, os) \
	do { \
		void* P; \
		P = malloc(s); \
		memset(P, 0xFF, s); \
		memcpy(P, v, os); \
		free(v); \
		v = P; \
		if ((s != 0) && !v) \
			my_error("realloc error on line %d in file " __FILE__ "\n", __LINE__); \
	} while (0)
#else
#define MY_MALLOC(v, s) \
	do { \
		v = malloc(s); \
		if ((s != 0) && !v) \
			my_error("malloc error on line %d in file " __FILE__ "\n", __LINE__); \
	} while (0)
#endif
#define MY_REALLOC(v, s) \
	do { \
		v = realloc(v, s); \
		if ((s != 0) && !v) \
			my_error("realloc error on line %d in file " __FILE__ "\n", __LINE__); \
	} while (0)
#undef assert
#ifdef DEBUG
#ifdef __STRING
#define assert(expr) \
	{ \
		if (expr) \
			; \
		else \
			my_error( \
				"Assert "__STRING(expr) " failed (" __FILE__ ":%d)\n", __LINE__); \
	}
#else
#define assert(expr) \
	{ \
		if (expr) \
			; \
		else \
			my_error( \
				"Assert failed (" __FILE__ ":%d)\n", __LINE__); \
	}
#endif
#else
#define assert(A) {};
#endif

#define MAC_BREAK_MALLOC_DEBUG \
	(msg) \
	{ \
		char c; \
		printf("%s\n", msg); \
		scanf("%c", &c); \
	}

/* PF do-nothing function for breakpoints in debugging */
void breakpoint(void);

/* PF if SILENT set, message functions do nothing */
extern int SILENT;
/* PF error messages functions */
void my_error(char* format, ...);
void my_warning(char* format, ...);
void my_message(char* format, ...);
void my_message_return(void);

/* PF takes a null terminated string.  Returns a new allocated copy */
char* strmake(const char* const astr);
/* PF returns a new allocated copy, using sprintf function (255 char at most)*/
char* strmake_printf(char* format, ...);
/* DD dest is NULL or a null-terminated string, s.t. (strlen(dest) == *destlen)
   appends null-terminated src at the end of dest, updates *destlen */
char* strapp(char* dest, size_t* destlen, const char* src);

/* DD computes the number of digits to represent an unsigned long */
unsigned ul_str_size(unsigned long val);
/* DD computes the number of digits to represent an long */
unsigned l_str_size(long val);
/* DD computes the number of tokens in a space-separated string */
int str_nb_words(char* str);

/* HB From literature, string split */
char** str_split(char* a_str, const char a_delim);

#define SWAP(V1, V2) \
	do { \
		typeof(V1) __tmp = V1; \
		V1 = V2; \
		V2 = __tmp; \
	} while (0)

#define MIN(v1, v2) ((v1) <= (v2)) ? (v1) : (v2)

#endif /* __GENERAL_H */
