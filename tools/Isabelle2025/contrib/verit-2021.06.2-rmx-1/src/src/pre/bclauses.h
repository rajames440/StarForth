/*
                                  ***   StarForth   ***

  bclauses.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:01.771-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/verit-2021.06.2-rmx-1/src/src/pre/bclauses.h
 */

/*
  --------------------------------------------------------------
  add some binary clauses for speed-up
  --------------------------------------------------------------
*/

#ifndef BCLAUSES_H
#define BCLAUSES_H

#include "symbolic/DAG.h"

TDAG bclauses_add(TDAG DAG);
void bclauses_init(void);
void bclauses_done(void);

#endif
