/*
                                  ***   StarForth   ***

  make_i.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.766-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/csdp-6.1.1/src/lib/make_i.c
 */

/*
  Make an identity matrix.
 */

#include <stdlib.h>
#include <stdio.h>
#include "declarations.h"

void make_i(A)
     struct blockmatrix A;
{
  int blk,i,j;
  double *p;

  for (blk=1; blk<=A.nblocks; blk++)
    {
      switch (A.blocks[blk].blockcategory) 
	{
	case DIAG:
	  p=A.blocks[blk].data.vec;
	  for (i=1; i<=A.blocks[blk].blocksize; i++)
	    p[i]=1.0;
	  break;
	case MATRIX:
	  p=A.blocks[blk].data.mat;
#pragma omp parallel for schedule(dynamic,64) default(none) private(i,j) shared(blk,p,A)
	  for (j=1; j<=A.blocks[blk].blocksize; j++)
	    for (i=1; i<=A.blocks[blk].blocksize; i++)
	      p[ijtok(i,j,A.blocks[blk].blocksize)]=0.0;

	  for (i=1; i<=A.blocks[blk].blocksize; i++)
	    p[ijtok(i,i,A.blocks[blk].blocksize)]=1.0;
	  break;
	default:
	  printf("make_i illegal block type\n");
	  exit(12);
	};
    }
}






