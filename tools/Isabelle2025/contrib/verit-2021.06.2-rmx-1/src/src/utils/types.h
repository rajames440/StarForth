/*
  --------------------------------------------------------------
  type definitions
  --------------------------------------------------------------
*/

#ifndef __TYPES_H
#define __TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* unsigned int type that can be safely cast to and from a pointer type */
typedef uintptr_t Tunsigned;
/* signed int type that can be safely cast to and from a pointer type */
typedef intptr_t Tsigned;

typedef unsigned (*TFhash)(void*);
typedef void (*TFapply)(void*);
typedef void (*TFapply2)(void*, void*);
typedef void (*TFfree)(void*);
typedef void* (*TFmap)(void*);
typedef void* (*TFcombine)(void*, void*);
typedef int (*TFtest)(void*);
typedef int (*TFcmp)(const void*, const void*);
typedef int (*TFequal)(void*, void*);

#endif /* __TYPES_H */
