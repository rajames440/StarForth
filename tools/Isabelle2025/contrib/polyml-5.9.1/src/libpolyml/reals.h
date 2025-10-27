/*
                                  ***   StarForth   ***

  reals.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.499-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/polyml-5.9.1/src/libpolyml/reals.h
 */

#ifndef _REALS_H

#define _REALS_H

class SaveVecEntry;
typedef SaveVecEntry *Handle;
class TaskData;

extern double real_arg(Handle x); // Also used in "foreign.cpp"
extern Handle real_result(TaskData *mdTaskData, double x); // Also used in "foreign.cpp"

extern int getrounding();
extern int setrounding(int rounding);

#define POLY_ROUND_TONEAREST    0
#define POLY_ROUND_DOWNWARD     1
#define POLY_ROUND_UPWARD       2
#define POLY_ROUND_TOZERO       3

extern struct _entrypts realsEPT[];

#endif
