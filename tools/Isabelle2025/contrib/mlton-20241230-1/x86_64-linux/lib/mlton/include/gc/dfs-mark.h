/*
                                  ***   StarForth   ***

  dfs-mark.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.651-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/mlton-20241230-1/x86_64-linux/lib/mlton/include/gc/dfs-mark.h
 */

#if (defined (MLTON_GC_INTERNAL_TYPES))

typedef enum {
  MARK_MODE,
  UNMARK_MODE,
} GC_markMode;

typedef struct GC_markState {
  GC_markMode mode;
  bool shouldHashCons;
  bool shouldLinkWeaks;
  size_t size;
} *GC_markState;

#endif /* (defined (MLTON_GC_INTERNAL_TYPES)) */

#if (defined (MLTON_GC_INTERNAL_FUNCS))

static inline bool isPointerMarked (pointer p);
static inline bool isPointerMarkedByMode (pointer p, GC_markMode m);
static void dfsMark (GC_state s, pointer root, GC_markState markState);
static void dfsMarkObjptr (GC_state s, objptr *root, GC_markState markState);
static void dfsMarkObjptrFun (GC_state s, objptr *root, void *env);

#endif /* (defined (MLTON_GC_INTERNAL_FUNCS)) */
