/*
                                  ***   StarForth   ***

  repl.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:55:24.735-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/include/repl.h
 */

#ifndef REPL_H
#define REPL_H

#include "vm.h"

/**
 * @brief Starts the REPL (Read-Eval-Print Loop) for the Forth virtual machine
 * @param vm Pointer to the VM instance to run the REPL on
 */
void vm_repl(VM * vm);

#endif /* REPL_H */