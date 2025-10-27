/*
                                  ***   StarForth   ***

  Rebalancing.cpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.504-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Kernel/Rebalancing.cpp
 */
#include "Rebalancing.hpp"

namespace Kernel {
namespace Rebalancing {

std::ostream& operator<<(std::ostream& out, const Node& n) {
  out << n.term() << "@" << n.index();
  return out;
}

std::ostream& operator<<(std::ostream& out, const InversionContext& n) {
  out << n._toInvert << "@" << n._unwrapIdx << " = " << n._toWrap;
  return out;
}

} //namespace Kernel 
} // namespace Rebalancing 
