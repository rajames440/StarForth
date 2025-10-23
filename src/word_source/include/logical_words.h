/*
                                  ***   StarForth   ***

  logical_words.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.704-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/word_source/include/logical_words.h
 */

#ifndef LOGICAL_WORDS_H
#define LOGICAL_WORDS_H

#include "../../../include/vm.h"

/**
 * @defgroup logical_ops FORTH-79 Logical Operations
 * @{
 *
 * @brief FORTH-79 Standard logical and comparison operations
 *
 * @details Implementation of the following FORTH-79 words:
 * - AND       ( n1 n2 -- n3 )             Bitwise AND
 * - OR        ( n1 n2 -- n3 )             Bitwise OR
 * - XOR       ( n1 n2 -- n3 )             Bitwise XOR
 * - NOT       ( n1 -- n2 )                Bitwise NOT
 * - 0=        ( n -- flag )               True if n is zero
 * - 0<        ( n -- flag )               True if n is negative
 * - 0>        ( n -- flag )               True if n is positive
 * - =         ( n1 n2 -- flag )           True if n1 equals n2
 * - <         ( n1 n2 -- flag )           True if n1 < n2
 * - >         ( n1 n2 -- flag )           True if n1 > n2
 * - U<        ( u1 u2 -- flag )           True if u1 < u2 (unsigned)
 * - U>        ( u1 u2 -- flag )           True if u1 > u2 (unsigned)
 * - WITHIN    ( n low high -- flag )      True if low <= n < high
 * @}
 */

/**
 * @brief Registers all logical and comparison words with the Forth virtual machine
 *
 * @param vm Pointer to the Forth virtual machine instance
 */
void register_logical_words(VM * vm);

#endif /* LOGICAL_WORDS_H */