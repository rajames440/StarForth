/*
                                  ***   StarForth   ***

  clb_permastrings.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.323-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/e-3.1-1/src/BASICS/clb_permastrings.c
 */

#include "clb_permastrings.h"



/*---------------------------------------------------------------------*/
/*                        Global Variables                             */
/*---------------------------------------------------------------------*/

static StrTree_p perma_anchor = NULL;


/*---------------------------------------------------------------------*/
/*                      Forward Declarations                           */
/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/*                         Internal Functions                          */
/*---------------------------------------------------------------------*/



/*---------------------------------------------------------------------*/
/*                         Exported Functions                          */
/*---------------------------------------------------------------------*/

/*-----------------------------------------------------------------------
//
// Function: PermaString()
//
//   Register a string. Will return a pointer to a permanent (possibly
//   shared) copy of the string that is valid until PermaStringsFree()
//   is called.
//
// Global Variables: perma_anchor
//
// Side Effects    : Memory operations, reorganises stored tree.
//
/----------------------------------------------------------------------*/

char* PermaString(char* str)
{
   StrTree_p handle, old;
   char *res;

   if(!str)
   {
      return NULL;
   }
   handle = StrTreeCellAlloc();
   handle->key = SecureStrdup(str);
   assert(handle->key != str);
   handle->val1.i_val = 0;
   handle->val2.i_val = 0;

   old = StrTreeInsert(&perma_anchor, handle);

      if(!old)
   {
      res = handle->key;
      assert(res!=str);
   }
   else
   {
      FREE(handle->key);
      StrTreeCellFree(handle);
      res = old->key;
      assert(res!=str);
   }
   return res;
}


/*-----------------------------------------------------------------------
//
// Function: PermaStringStore()
//
//   As PermaString, but will FREE the original.
//
// Global Variables: perma_anchor
//
// Side Effects    : Memory operations, reorganises stored tree.
//
/----------------------------------------------------------------------*/

char* PermaStringStore(char* str)
{
   if(!str)
   {
      return str;
   }
   char* res = PermaString(str);
   FREE(str);

   return res;
}


/*-----------------------------------------------------------------------
//
// Function: PermaStringsFree()
//
//   Free all permastrings (and their admin data structure).
//
// Global Variables: perma_anchor
//
// Side Effects    : Memory operations
//
/----------------------------------------------------------------------*/

void  PermaStringsFree(void)
{
   StrTreeFree(perma_anchor);
   perma_anchor = NULL;
}


/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/
