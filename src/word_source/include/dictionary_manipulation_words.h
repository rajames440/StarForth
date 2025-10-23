/*
                                  ***   StarForth   ***

  dictionary_manipulation_words.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-23T10:54:00.754-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/src/word_source/include/dictionary_manipulation_words.h
 */

#ifndef DICTIONARY_MANIPULATION_WORDS_H
#define DICTIONARY_MANIPULATION_WORDS_H

#include "../../../include/vm.h"

/**
 * @defgroup dictionary_words FORTH-79 Dictionary Manipulation Words
 * @{
 * @brief Standard FORTH-79 words for dictionary manipulation
 *
 * @par [         ( -- )
 * Enter interpretation mode. Switches the system to interpret incoming words.
 *
 * @par ]         ( -- )
 * Enter compilation mode. Switches the system to compile incoming words.
 *
 * @par STATE     ( -- addr )
 * Variable containing current compilation state. Returns address of STATE variable.
 *
 * @par SMUDGE    ( -- )
 * Toggle smudge bit of latest defined word. Used during word definition.
 *
 * @par >BODY     ( xt -- addr )
 * Convert execution token to parameter field address.
 *
 * @par >NAME     ( xt -- addr )
 * Convert execution token to name field address.
 *
 * @par NAME>     ( addr -- xt )
 * Convert name field address to execution token.
 *
 * @par >LINK     ( addr -- addr )
 * Get link field address from name field address.
 *
 * @par LINK>     ( addr -- addr )
 * Get next word's address from link field.
 *
 * @par CFA       ( addr -- xt )
 * Get code field address from name field address.
 *
 * @par LFA       ( addr -- addr )
 * Get link field address from name field address.
 *
 * @par NFA       ( addr -- addr )
 * Get name field address from any field address.
 *
 * @par PFA       ( addr -- addr )
 * Get parameter field address from name field address.
 *
 * @par TRAVERSE  ( addr n -- addr )
 * Move through name field by n characters.
 *
 * @par INTERPRET ( -- )
 * Text interpreter - process input stream.
 * @}
 */

/**
 * @brief Register all dictionary manipulation words with the virtual machine
 *
 * @param vm Pointer to the virtual machine instance
 *
 * This function registers all FORTH-79 standard dictionary manipulation words
 * with the provided virtual machine instance. These words are essential for
 * dictionary management and word definition functionality.
 */
void register_dictionary_manipulation_words(VM * vm);

#endif /* DICTIONARY_MANIPULATION_WORDS_H */