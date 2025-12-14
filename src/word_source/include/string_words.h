/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

#ifndef STRING_WORDS_H
#define STRING_WORDS_H

#include "../../../include/vm.h"

/** @file string_words.h
 * @brief FORTH-79 Standard string and text processing words implementation
 * 
 * This file contains the implementation of FORTH-79 standard string and text processing words.
 * These words provide functionality for string manipulation, input handling, and parsing.
 */

/** @name FORTH-79 String & Text Processing Words
 * @{
 */

/**
 * @brief COUNT ( addr1 -- addr2 u ) Get string length and address
 * Returns the character count and address of a counted string
 */

/**
 * @brief EXPECT ( addr u -- ) Accept input line
 * Accepts up to u characters of input and stores them at addr
 */

/**
 * @brief SPAN ( -- addr ) Variable: number of characters input
 * Contains the count of characters actually received by EXPECT
 */

/**
 * @brief QUERY ( -- ) Accept input into TIB
 * Accepts a line of input into the Terminal Input Buffer
 */

/**
 * @brief TIB ( -- addr ) Terminal input buffer address
 * Returns the address of the Terminal Input Buffer
 */

/**
 * @brief WORD ( c -- addr ) Parse next word, delimited by c
 * Parses the next word from input stream using c as delimiter
 */

/**
 * @brief >IN ( -- addr ) Input stream pointer
 * Contains the offset into the input buffer
 */

/**
 * @brief SOURCE ( -- addr u ) Input source specification
 * Returns current input buffer address and length
 */

/**
 * @brief BL ( -- c ) ASCII space character (32)
 * Pushes ASCII space character value onto stack
 */

/**
 * @brief FIND ( addr -- addr flag ) Find word in dictionary
 * Searches dictionary for word, returns status flag
 */

/**
 * @brief ' ( -- xt ) Get execution token of next word
 * Returns execution token of next word in input stream
 */

/**
 * @brief ['] ( -- xt ) Compile execution token
 * Compiles execution token as literal
 */

/**
 * @brief LITERAL ( n -- ) Compile literal number
 * Compiles top stack value as literal
 */

/**
 * @brief [LITERAL] ( n -- ) Compile literal at compile time
 * Compiles literal during compilation
 */

/**
 * @brief CONVERT ( d1 addr1 -- d2 addr2 ) Convert string to number
 * Converts string starting at addr1 to number
 */

/**
 * @brief NUMBER ( addr -- n flag ) Convert string to single number
 * Converts string to single precision number
 */

/**
 * @brief ENCLOSE ( addr c -- addr1 n1 n2 n3 ) Parse for delimiter
 * Parses string for delimiter character
 */

/** @} */

/**
 * @brief Register all string processing words with VM
 * @param vm Pointer to VM instance where words will be registered
 */
void register_string_words(VM * vm);

#endif /* STRING_WORDS_H */