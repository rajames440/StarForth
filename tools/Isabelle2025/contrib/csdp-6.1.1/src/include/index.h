/*
                                  ***   StarForth   ***

  index.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.292-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/csdp-6.1.1/src/include/index.h
 */

/*
  Declarations needed to handle indexing into Fortran arrays and packed
  arrays.
*/

#ifndef BIT64

/*
  First, to convert fortran i,j indices into a C vector index.
*/

#define ijtok(iiii,jjjj,lda) ((jjjj-1)*lda+iiii-1)

/* 
   Packed indexing.
 */

#define ijtokp(iii,jjj,lda) ((iii+jjj*(jjj-1)/2)-1)

/*
  Next, to convert C vector index into Fortran i,j indices.
*/

#define ktoi(k,lda) ((k % lda)+1)
#define ktoj(k,lda) ((k/lda)+1)

#else

/*
  First, to convert fortran i,j indices into a C vector index.
*/

#define ijtok(iiii,jjjj,lda) ((jjjj-1L)*lda+iiii-1L)

/* 
   Packed indexing.
 */

#define ijtokp(iii,jjj,lda) (((long int)iii+(long int)jjj*(jjj-1L)/2-1L))

/*
  Next, to convert C vector index into Fortran i,j indices.
*/

#define ktoi(k,lda) (((long int)k % lda)+1L)
#define ktoj(k,lda) (((long int)k/lda)+1L)


#endif

