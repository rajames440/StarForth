/*
                                  ***   StarForth   ***

  Alg.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:01.491-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/minisat-2.2.1-1/src/minisat/mtl/Alg.h
 */

#ifndef Minisat_Alg_h
#define Minisat_Alg_h

#include "minisat/mtl/Vec.h"

namespace Minisat {

//=================================================================================================
// Useful functions on vector-like types:

//=================================================================================================
// Removing and searching for elements:
//

template<class V, class T>
static inline void remove(V& ts, const T& t)
{
    int j = 0;
    for (; j < (int)ts.size() && ts[j] != t; j++);
    assert(j < (int)ts.size());
    for (; j < (int)ts.size()-1; j++) ts[j] = ts[j+1];
    ts.pop();
}


template<class V, class T>
static inline bool find(V& ts, const T& t)
{
    int j = 0;
    for (; j < (int)ts.size() && ts[j] != t; j++);
    return j < (int)ts.size();
}


//=================================================================================================
// Copying vectors with support for nested vector types:
//

// Base case:
template<class T>
static inline void copy(const T& from, T& to)
{
    to = from;
}

// Recursive case:
template<class T>
static inline void copy(const vec<T>& from, vec<T>& to, bool append = false)
{
    if (!append)
        to.clear();
    for (int i = 0; i < from.size(); i++){
        to.push();
        copy(from[i], to.last());
    }
}

template<class T>
static inline void append(const vec<T>& from, vec<T>& to){ copy(from, to, true); }

//=================================================================================================
}

#endif
