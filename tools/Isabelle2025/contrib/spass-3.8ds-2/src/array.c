/************************************************************/
/************************************************************/
/**                                                        **/
/**                       ARRAY                            **/
/**                                                        **/
/**          Growable array with bound checks.             **/
/**                                                        **/
/**                                                        **/
/**                Author: Martin Suda                     **/
/**                                                        **/
/**  This program is free software; you can redistribute   **/
/**  it and/or modify it under the terms of the FreeBSD    **/
/**  Licence.                                              **/
/**                                                        **/
/**  This program is distributed in the hope that it will  **/
/**  be useful, but WITHOUT ANY WARRANTY; without even     **/
/**  the implied warranty of MERCHANTABILITY or FITNESS    **/
/**  FOR A PARTICULAR PURPOSE.  See the LICENCE file       **/ 
/**  for more details.                                     **/
/**                                                        **/
/************************************************************/
/************************************************************/


#include "array.h"

/**************************************************************/
/* Private memory module extension                            */
/**************************************************************/

#ifdef NO_MEMORY_MANAGEMENT

static POINTER memory_Realloc(POINTER Ptr, unsigned int OldSize, unsigned int NewSize) {  
  return realloc(Ptr,NewSize);
}

#else

static POINTER memory_Realloc(POINTER Ptr, unsigned int OldSize, unsigned int NewSize) {
  POINTER NewMem;
  NewMem = memory_Malloc(NewSize);
  memcpy(NewMem,Ptr,OldSize>NewSize?NewSize:OldSize);
  memory_Free(Ptr,OldSize);
  return NewMem;
}

#endif

/**************************************************************/
/* Functions                                                  */
/**************************************************************/

void array_Init(void)
/**************************************************************
  INPUT:   None.
  RETURNS: void.
  SUMMARY: Placebo satisfying the general module layout.
***************************************************************/
{
  return;
}


void array_Free(void)
/**************************************************************
  INPUT:   None.
  RETURNS: void.
  SUMMARY: Placebo satisfying the general module layout.
***************************************************************/
{
  return;
}


ARRAY array_Create(int init)
/**************************************************************
  INPUT:   Initial capacity of the array. Should be positive!
  RETURNS: A new array of the specified capacity and size = 0.
  MEMORY:  Allocates a new array struct
           and the internal array itself plus NULL initialization.		   .
***************************************************************/
{
  ARRAY ar;
  int   i;

  ar = (ARRAY)memory_Malloc(sizeof(ARRAY_NODE));
  ar->size = 0;
  ar->capacity = init;
  ar->data = memory_Malloc(init*sizeof(intptr_t));

  for(i=0;i<init;i++)
    ar->data[i] = (intptr_t)NULL;

  return ar;
}

void array_Claim(ARRAY ar)
/**************************************************************
  EFFECT:  Let all the allocated part be accessible.
           The content the newly accessible cells is init to zeros.
***************************************************************/
{
  int i;
  for(i = ar->size; i < ar->capacity; i++)
    ar->data[i] = (intptr_t)NULL;

  ar->size = ar->capacity;
}

ARRAY  array_DeleteIndex(ARRAY ar, int idx)
/**************************************************************
  INPUT:   Pointer to the array and an index.
  RETURNS: The updated array with the element at index deleted
         and the rest (higher indexed elemets) shitfed left by 1.
***************************************************************/
{
  int i;

  /*ASSERT((idx >= 0) && (idx < ar->size));*/

  for(i = idx+1; i < ar->size; i++ )
    ar->data[i-1] = ar->data[i];

  ar->size--;

  return ar;
}

ARRAY  array_Clean(ARRAY ar)
/**************************************************************
  INPUT:   Pointer to the array.
  RETURNS: The same array with all elements deleted.
  NOTE:    Just sets size to 0, capacity remains untoutched.
***************************************************************/
{
  ar->size = 0;
  return ar;
}

int     array_GetSize(ARRAY ar)
/**************************************************************
  INPUT:   Pointer to the array.
  RETURNS: The current size of the array.
***************************************************************/
{
  return ar->size;
}

ARRAY   array_Add(ARRAY ar, intptr_t el)
/**************************************************************
  INPUT:   A pointer to the array and element.
  RETURNS: The updated array with the element added to the end
             and size increased by one.
  MEMORY:  If the capacity is reached, capacity is doubled,
            new internal array allocated and the content copied.
***************************************************************/
{
  if (ar->size==ar->capacity)
  {
    int oldcap;
    oldcap = ar->capacity;
    ar->capacity *= 2;
    ar->data = memory_Realloc(ar->data,oldcap*sizeof(intptr_t),ar->capacity*sizeof(intptr_t));
  }            

  ar->data[ar->size] = el;
  ar->size++;

  return ar;
}

ARRAY   array_AddAtIndex(ARRAY ar, int idx, intptr_t el)
/**************************************************************
  INPUT:   A pointer to the array and index and element.
  RETURNS: The updated array with the element added at position <idx>
           and of the array increased arrordingly.
  MEMORY:  If the capacity is reached, capacity is doubled until <idx> available,
           new internal array allocated and the content copied.
***************************************************************/
{
  int oldcap;

  oldcap = ar->capacity;
  
  while (ar->capacity <= idx)
    ar->capacity *= 2;

  if (oldcap !=ar->capacity)
    ar->data = memory_Realloc(ar->data,oldcap*sizeof(intptr_t),ar->capacity*sizeof(intptr_t));

  if (ar->size < idx)
    ar->size =  idx;

  ar->data[idx] = el;

  return ar;
}

ARRAY   array_AddGetIndex(ARRAY ar, intptr_t el, int *index)
/**************************************************************
  INPUT:   Pointer to the array, an element and a pointer to int
  RETURNS: The updated array with the element added to the end
           and size increased by one, the actual index is returned in <*index>
  MEMORY:  If the capacity is reached, capacity is doubled,
           new internal array allocated and the content copied.
***************************************************************/
{
  if (ar->size==ar->capacity)
  {
    int oldcap;
    oldcap = ar->capacity;
    ar->capacity *= 2;
    ar->data = memory_Realloc(ar->data,oldcap*sizeof(void *),ar->capacity*sizeof(void *));
  }            

  ar->data[ar->size] = el;
  *index             = ar->size;
  ar->size++;

  return ar;
}

intptr_t  array_GetElement(ARRAY ar, int idx)
/**************************************************************
  INPUT:   Pointer to the array and a specific index.
  RETURNS: Checks if the index is in the currently allocated range
           and if so, returns the specified element.
           Otherwise an error occurs.
***************************************************************/
{
  /*ASSERT((idx >= 0) && (idx < ar->size));*/
  return ar->data[idx];
}

void   array_SetElement(ARRAY ar, int idx, intptr_t new_el)
/**************************************************************
  INPUT:   Pointer to the array, specific index and a new value.
  EFFECT:  Checks if the index is in the currently allocated range
            and if so, the array is modified in such a way
                  that idx-th element is new_el.
           Otherwise an error occurs.
***************************************************************/
{
  /*ASSERT((idx >= 0) && (idx < ar->size));  */
  ar->data[idx] = new_el;
}

BOOL    array_ContainsElement(ARRAY ar, intptr_t el, int * index)
/**************************************************************
  INPUT:   Pointer to the array, an element and a pointer to integer.
  RETURNS: TRUE iff the array contains that int as its element.
           If so, <*index> is set to index of the first found occurence.
***************************************************************/
{
  int i;

  for(i = 0; i < ar->size; i++)
    if ((intptr_t)ar->data[i] == el) {
      *index = i;
      return TRUE;
    }

  return FALSE;
}

void   array_Sort(ARRAY ar, int (* compartor) ( const void *, const void * ))
/**************************************************************
  INPUT:   Pointer to the array and a comparator.
  EFFECT:  Uses stdlib qsort to sort the array with the help
           of the given comparator.
***************************************************************/
{
  qsort(ar->data, ar->size, sizeof(ar->data[0]), compartor);
}
                    
int    array_BSearch(ARRAY ar, const void * key, int (* comparator) ( const void *, const void * ))
/**************************************************************
  INPUT:   Pointer to the array, an element to look for and a comparator.
  RETURNS: an index of the element if found, -1 otherwise
  CAUTION: Assumes the array has been sorted previously using the same comparator "relation"
***************************************************************/
{
  intptr_t* result;
  result = (intptr_t*)bsearch(key,ar->data,ar->size,sizeof(ar->data[0]),comparator);
  if (result == NULL)
    return -1;
  else
    return (result - ar->data);
}

void    array_Delete(ARRAY ar)
/**************************************************************
  INPUT:   Pointer to the array to be deleted.
  MEMORY:  Frees the memory associated with the given array.
***************************************************************/
{
  memory_Free(ar->data,ar->capacity*sizeof(intptr_t));
  memory_Free(ar,sizeof(ARRAY_NODE));
}



