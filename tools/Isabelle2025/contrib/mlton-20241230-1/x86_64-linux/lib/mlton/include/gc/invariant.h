/*
                                  ***   StarForth   ***

  invariant.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.571-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/mlton-20241230-1/x86_64-linux/lib/mlton/include/gc/invariant.h
 */

#if (defined (MLTON_GC_INTERNAL_FUNCS))

#if ASSERT
static inline void assertIsObjptrInFromSpaceOrImmutableMutableOrRootStaticHeap (GC_state s, objptr *opp, void *env);
static inline void assertIsObjptrInImmutableMutableOrRootStaticHeap (GC_state s, objptr *opp, void *env);
static bool invariantForGC (GC_state s);
#endif
static inline bool invariantForMutatorFrontier (GC_state s);
static inline bool invariantForMutatorStack (GC_state s);
#if ASSERT
static bool invariantForMutator (GC_state s, bool frontier, bool stack);
#endif

#endif /* (defined (MLTON_GC_INTERNAL_FUNCS)) */
