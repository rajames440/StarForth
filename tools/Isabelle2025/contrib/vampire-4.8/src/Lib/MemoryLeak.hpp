/*
                                  ***   StarForth   ***

  MemoryLeak.hpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.815-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Lib/MemoryLeak.hpp
 */
/**
 * @file MemoryLeak.hpp
 * Defines the class MemoryLeak used in debugging memory leaks.
 *
 * @since 22/03/2008 Torrevieja
 */


#ifndef __MemoryLeak__
#define __MemoryLeak__

#if CHECK_LEAKS

#include "Allocator.hpp"
#include "Set.hpp"
#include "Hash.hpp"

#include "Kernel/Unit.hpp"

namespace Kernel {
  class Formula;
};

namespace Lib {

class MemoryLeak
{
public:
  void release(Kernel::UnitList*);
  /** Cancel leak report, called when an exception is raised */
  static void cancelReport() { _cancelled = true; }
  /** If true then a report should be made */
  static bool report() { return ! _cancelled; }
private:
  /** If true then no leak reporting is done */
  static bool _cancelled;
  void release(Kernel::Formula*);
  /** Set of generic pointers */
  typedef Set<void*,Hash> PointerSet;
  /** Stores deallocated pointers */
  PointerSet _released;
}; // class MemoryLeak

} // namespace Lib

#endif // CHECK_LEAKS
#endif // __MemoryLeak__
